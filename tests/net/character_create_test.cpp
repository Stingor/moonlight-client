// LA CRÉATION D'UN PERSONNAGE, sur un serveur de bouclage.
//
// 🔴 CE QUI SE VÉRIFIE ICI TIENT EN UNE PHRASE : un refus de CRÉATION n'est pas
// un refus de PHASE. Tous les autres refus du serveur de personnages mettent
// fin à la session ; celui-ci laisse le joueur devant sa tablée. Les
// confondre — ce qu'un portage cohérent fait naturellement, puisque tous les
// autres refus se traitent pareil — déconnecterait quelqu'un pour une faute de
// frappe.

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/session.h"
#include "ro/proto/manual_packets.h"

#include "support/char_session.h"
#include "support/loopback_server.h"

using ro::net::CharacterPhase;
using ro::net::CreationState;
using ro::net::LoginPhase;
using ro::net::PhaseState;
using ro::net::ServerAddress;
using ro::test::LoopbackServer;
using ro::test_support::BareAccountId;
using ro::test_support::CharacterList;
using ro::test_support::Churn;
using ro::test_support::kBriefWaitMs;
using ro::test_support::kEnterRequestSize;
using ro::test_support::kLoopbackWireIp;
using ro::test_support::kRoundsPastTheList;
using ro::test_support::SettleUntil;

namespace {

// Amène une phase jusqu'au choix, la liste ordinaire en main.
[[nodiscard]] ro::core::Result<CharacterPhase> AtTheTable(LoopbackServer& server,
                                                          std::uint32_t account_id) {
  LoginPhase::Account account;
  account.account_id = account_id;
  auto phase = CharacterPhase::Start(ServerAddress{kLoopbackWireIp, server.port()}, account);
  if (!phase.has_value()) {
    return phase;
  }
  if (!server.Accept() || server.Receive(kEnterRequestSize).size() != kEnterRequestSize ||
      !server.Send(BareAccountId(account_id)) || !server.Send(CharacterList())) {
    return phase;
  }
  static_cast<void>(SettleUntil(*phase, CharacterPhase::Step::kAwaitingSelection));
  return phase;
}

}  // namespace

TEST_CASE("Création : le brouillon part tel quel sur le fil", "[net][création]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  auto phase = AtTheTable(server, 2'000'123);
  REQUIRE(phase.has_value());
  REQUIRE(phase->step() == CharacterPhase::Step::kAwaitingSelection);
  CHECK(phase->creation() == CreationState::kIdle);

  ro::net::CharacterDraft draft;
  draft.name = "Carole";
  draft.slot = 2;
  draft.hair_style = 7;
  draft.hair_palette = 3;
  draft.sex = 0;
  phase->Create(draft);
  CHECK(phase->creation() == CreationState::kPending);

  ro::proto::ChMakeChar sent;
  REQUIRE(Decode(server.Receive(ro::proto::ChMakeChar::kWireSize), sent));
  CHECK(sent.name == "Carole");
  CHECK(sent.slot == 2);
  CHECK(sent.hair_style == 7);
  CHECK(sent.hair_color == 3);
  CHECK(sent.sex == 0);
  // ⚠ LE NOVICE, ET RIEN D'AUTRE : le serveur n'accepte que deux classes de
  // départ (`allowed_job_flag`), et l'on ne sait pas laquelle il autorise en
  // production. Proposer l'autre sans savoir ferait refuser sans le dire.
  CHECK(sent.job == 0);
}

TEST_CASE("Création : un refus NE COUPE PAS la phase", "[net][création]") {
  // 🔴 LE PIÈGE DE CE LOT, et il est dans le sens de la pente : tous les
  // autres refus de cette phase posent `kRefused` et rendent la main. Celui-ci
  // ne le doit pas — le joueur reste attablé et essaie un autre nom.
  LoopbackServer server;
  REQUIRE(server.listening());
  auto phase = AtTheTable(server, 2'000'123);
  REQUIRE(phase.has_value());

  phase->Create(ro::net::CharacterDraft{.name = "Alice", .slot = 2});
  REQUIRE(server.Receive(ro::proto::ChMakeChar::kWireSize).size() ==
          ro::proto::ChMakeChar::kWireSize);

  ro::proto::HcRefuseMakechar refused;
  refused.error = 0x00;  // 'Charname already exists'
  REQUIRE(server.Send(Encode(refused)));
  Churn(*phase, kRoundsPastTheList);

  // La phase ATTEND toujours : elle n'est ni refusée ni tombée.
  CHECK(phase->Advance() == PhaseState::kWaiting);
  CHECK(phase->step() == CharacterPhase::Step::kAwaitingSelection);
  CHECK(phase->creation() == CreationState::kRefused);
  CHECK(phase->creation_refusal_code() == 0x00);
  CHECK_FALSE(phase->creation_refusal_text().empty());
  // Et la liste n'a pas bougé.
  CHECK(phase->characters().size() == 2);
}

TEST_CASE("Création : le personnage créé s'ajoute à la liste", "[net][création]") {
  // 🔴 LE SERVEUR NE RENVOIE PAS LA LISTE, il envoie le personnage SEUL
  // (`chclif_createnewchar`). Sans cet ajout la tablée resterait telle qu'elle
  // était, le joueur croirait sa création perdue — et en relancerait une, que
  // le serveur refuserait pour un emplacement déjà pris.
  constexpr std::uint8_t kNewSlot = 2;
  LoopbackServer server;
  REQUIRE(server.listening());
  auto phase = AtTheTable(server, 2'000'123);
  REQUIRE(phase.has_value());

  phase->Create(ro::net::CharacterDraft{.name = "Carole", .slot = kNewSlot});
  REQUIRE(server.Receive(ro::proto::ChMakeChar::kWireSize).size() ==
          ro::proto::ChMakeChar::kWireSize);

  ro::proto::HcAcceptMakechar made;
  made.character.GID = 150'002;
  made.character.name = "Carole";
  made.character.CharNum = kNewSlot;
  REQUIRE(server.Send(Encode(made)));
  Churn(*phase, kRoundsPastTheList);

  CHECK(phase->creation() == CreationState::kCreated);
  CHECK(phase->created_slot() == kNewSlot);
  REQUIRE(phase->characters().size() == 3);
  // ⚠ ET IL SE RETROUVE PAR SON EMPLACEMENT, pas par son rang : il occupe le
  // deux, et il est le troisième de la liste. Voir `ro::net::IndexOfSlot`.
  const int index = ro::net::IndexOfSlot(phase->characters(), kNewSlot);
  REQUIRE(index == 2);
  CHECK(phase->characters()[static_cast<std::size_t>(index)].name == "Carole");

  // La réponse s'oublie quand on l'a montrée : sans cela, un refus lu une fois
  // resterait affiché sous la tentative suivante.
  phase->ForgetCreation();
  CHECK(phase->creation() == CreationState::kIdle);
  CHECK(phase->created_slot() == -1);
}

TEST_CASE("Création : créer avant d'avoir la liste ne fait rien", "[net][création]") {
  // Un emplacement dont on ignore s'il est libre ne veut rien dire, et le
  // serveur ferme la liaison sur tout paquet reçu trop tôt.
  LoopbackServer server;
  REQUIRE(server.listening());

  auto phase =
      CharacterPhase::Start(ServerAddress{kLoopbackWireIp, server.port()}, LoginPhase::Account());
  REQUIRE(phase.has_value());
  REQUIRE(server.Accept());
  REQUIRE(server.Receive(kEnterRequestSize).size() == kEnterRequestSize);

  phase->Create(ro::net::CharacterDraft{.name = "Carole"});
  CHECK(phase->creation() == CreationState::kIdle);
  CHECK(server.Receive(1, kBriefWaitMs).empty());
}
