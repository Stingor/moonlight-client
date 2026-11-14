// LA SUPPRESSION D'UN PERSONNAGE, EN DEUX TEMPS, sur un serveur de bouclage.
//
// 🔴 CE QUI SE VÉRIFIE ICI TIENT EN DEUX PHRASES.
//
// La première : LE SERVEUR RÉPOND PAR UN VERDICT, JAMAIS PAR UNE LISTE. Aux
// trois étapes — réserver, annuler, supprimer — il accuse et se tait. C'est au
// client de poser l'échéance, de l'effacer, et de retirer le personnage ; sans
// cela la tablée ment sur ce qu'elle montre.
//
// La seconde : UN REFUS DE SUPPRESSION N'EST PAS UN REFUS DE PHASE. Un
// personnage en guilde reste à sa place, et le joueur va quitter sa guilde.
// Couper la liaison le renverrait au formulaire de connexion.

#include <cstdint>
#include <ctime>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/session.h"
#include "ro/proto/manual_packets.h"

#include "support/char_session.h"
#include "support/loopback_server.h"

using ro::net::CharacterPhase;
using ro::net::DeletionState;
using ro::net::DeletionStep;
using ro::net::LoginPhase;
using ro::net::PhaseState;
using ro::net::ServerAddress;
using ro::test::LoopbackServer;
using ro::test_support::BareAccountId;
using ro::test_support::CharacterList;
using ro::test_support::Churn;
using ro::test_support::kEnterRequestSize;
using ro::test_support::kLoopbackWireIp;
using ro::test_support::kRoundsPastTheList;
using ro::test_support::SettleUntil;

namespace {

// Alice, emplacement 0, telle que `CharacterList` la donne.
constexpr std::uint32_t kAlice = 150'000;
// Le délai que le serveur annonce : une heure, comme en production.
constexpr std::int32_t kDelaySeconds = 3600;
// Un délai déjà écoulé : le serveur l'envoie négatif.
constexpr std::int32_t kElapsedSeconds = -120;

// L'horloge du mur, celle que la phase lit pour ancrer un délai.
[[nodiscard]] std::int64_t WallNow() { return static_cast<std::int64_t>(std::time(nullptr)); }

// Amène une phase jusqu'au choix, la liste `list` en main.
[[nodiscard]] ro::core::Result<CharacterPhase> AtTheTable(LoopbackServer& server,
                                                          const std::vector<std::uint8_t>& list) {
  LoginPhase::Account account;
  account.account_id = 2'000'123;
  auto phase = CharacterPhase::Start(ServerAddress{kLoopbackWireIp, server.port()}, account);
  if (!phase.has_value()) {
    return phase;
  }
  if (!server.Accept() || server.Receive(kEnterRequestSize).size() != kEnterRequestSize ||
      !server.Send(BareAccountId(account.account_id)) || !server.Send(list)) {
    return phase;
  }
  static_cast<void>(SettleUntil(*phase, CharacterPhase::Step::kAwaitingSelection));
  return phase;
}

// Amène une phase jusqu'au choix, la liste ordinaire en main.
[[nodiscard]] ro::core::Result<CharacterPhase> AtTheTable(LoopbackServer& server) {
  return AtTheTable(server, CharacterList());
}

// Alice et Bob, Alice condamnée depuis `alice_delay_seconds` tel que le fil
// le porte.
[[nodiscard]] std::vector<std::uint8_t> DoomedAliceList(std::int32_t alice_delay_seconds) {
  ro::proto::HcAcceptEnter list;
  list.total = 2;
  ro::proto::CharacterInfo alice;
  alice.GID = kAlice;
  alice.name = "Alice";
  alice.CharNum = 0;
  alice.DelRevDate = alice_delay_seconds;
  ro::proto::CharacterInfo bob;
  bob.GID = kAlice + 1;
  bob.name = "Bob";
  bob.CharNum = 1;
  list.characters = {alice, bob};
  return Encode(list);
}

// L'échéance que porte le personnage `character_id` dans la copie locale.
[[nodiscard]] std::int32_t DeadlineOf(const CharacterPhase& phase, std::uint32_t character_id) {
  for (const ro::proto::CharacterInfo& who : phase.characters()) {
    if (who.GID == character_id) {
      return who.DelRevDate;
    }
  }
  return -1;
}

}  // namespace

TEST_CASE("Suppression : la réservation pose l'échéance CHEZ NOUS", "[net][suppression]") {
  // 🔴 LE SERVEUR NE RENVOIE PAS LA LISTE. Il pose `delete_date` en base et
  // répond par un verdict et une date. Sans l'écrire dans notre copie, le
  // personnage n'aurait aucun compte à rebours — donc aucun bouton
  // « Annuler », et aucun moyen de savoir quand « Supprimer » s'allume.
  LoopbackServer server;
  REQUIRE(server.listening());
  auto phase = AtTheTable(server);
  REQUIRE(phase.has_value());
  REQUIRE(phase->step() == CharacterPhase::Step::kAwaitingSelection);
  CHECK(phase->deletion() == DeletionState::kIdle);
  CHECK(DeadlineOf(*phase, kAlice) == 0);

  phase->ReserveDeletion(kAlice);
  CHECK(phase->deletion_step() == DeletionStep::kReserve);
  CHECK(phase->deletion() == DeletionState::kPending);

  ro::proto::ChDeleteChar3Reserved sent;
  REQUIRE(Decode(server.Receive(ro::proto::ChDeleteChar3Reserved::kWireSize), sent));
  CHECK(sent.CID == kAlice);

  ro::proto::HcDeleteChar3Reserved reserved;
  reserved.CID = kAlice;
  reserved.result = 1;  // succès
  reserved.date = static_cast<std::uint32_t>(kDelaySeconds);
  const std::int64_t before = WallNow();
  REQUIRE(server.Send(Encode(reserved)));
  Churn(*phase, kRoundsPastTheList);
  const std::int64_t after = WallNow();

  CHECK(phase->deletion() == DeletionState::kDone);
  // 🔴 LE FIL PORTE UN DÉLAI, LA COPIE UNE DATE : une heure après la réception.
  CHECK(DeadlineOf(*phase, kAlice) >= before + kDelaySeconds);
  CHECK(DeadlineOf(*phase, kAlice) <= after + kDelaySeconds);
  // Et personne n'a quitté la table.
  CHECK(phase->characters().size() == 2);
}

TEST_CASE("Suppression : un UN veut dire OUI, et le ZÉRO veut dire NON", "[net][suppression]") {
  // 🔴 CES DEUX RÉPONSES PORTENT UN RÉSULTAT, PAS UN CODE D'ERREUR. Zéro y
  // veut dire « déjà programmée », et lire le succès dans le zéro habituel
  // écrirait une échéance ABERRANTE sur un refus — le personnage passerait
  // pour condamné sans l'être.
  LoopbackServer server;
  REQUIRE(server.listening());
  auto phase = AtTheTable(server);
  REQUIRE(phase.has_value());

  phase->ReserveDeletion(kAlice);
  REQUIRE(server.Receive(ro::proto::ChDeleteChar3Reserved::kWireSize).size() ==
          ro::proto::ChDeleteChar3Reserved::kWireSize);

  ro::proto::HcDeleteChar3Reserved reserved;
  reserved.CID = kAlice;
  reserved.result = 4;  // en guilde
  reserved.date = 0;
  REQUIRE(server.Send(Encode(reserved)));
  Churn(*phase, kRoundsPastTheList);

  CHECK(phase->deletion() == DeletionState::kRefused);
  CHECK(phase->deletion_refusal_text().find("GUILDE") != std::string::npos);
  // ⚠ ET LA PHASE VIT TOUJOURS : quitter une guilde se fait en jeu, donc sur
  // cette liaison-là.
  CHECK(phase->Advance() == PhaseState::kWaiting);
  CHECK(phase->step() == CharacterPhase::Step::kAwaitingSelection);
  // L'échéance n'a pas été posée.
  CHECK(DeadlineOf(*phase, kAlice) == 0);
}

TEST_CASE("Suppression : l'annulation efface l'échéance", "[net][suppression]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  auto phase = AtTheTable(server);
  REQUIRE(phase.has_value());

  phase->ReserveDeletion(kAlice);
  REQUIRE(server.Receive(ro::proto::ChDeleteChar3Reserved::kWireSize).size() ==
          ro::proto::ChDeleteChar3Reserved::kWireSize);
  ro::proto::HcDeleteChar3Reserved reserved;
  reserved.CID = kAlice;
  reserved.result = 1;
  reserved.date = static_cast<std::uint32_t>(kDelaySeconds);
  REQUIRE(server.Send(Encode(reserved)));
  Churn(*phase, kRoundsPastTheList);
  REQUIRE(DeadlineOf(*phase, kAlice) > 0);

  phase->CancelDeletion(kAlice);
  CHECK(phase->deletion_step() == DeletionStep::kCancel);
  ro::proto::ChDeleteChar3Cancel sent;
  REQUIRE(Decode(server.Receive(ro::proto::ChDeleteChar3Cancel::kWireSize), sent));
  CHECK(sent.CID == kAlice);

  ro::proto::HcDeleteChar3Cancel cancelled;
  cancelled.CID = kAlice;
  cancelled.result = 1;  // succès
  REQUIRE(server.Send(Encode(cancelled)));
  Churn(*phase, kRoundsPastTheList);

  CHECK(phase->deletion() == DeletionState::kDone);
  CHECK(DeadlineOf(*phase, kAlice) == 0);
  CHECK(phase->characters().size() == 2);
}

TEST_CASE("Suppression : une heure PASSÉE reste une suppression programmée", "[net][suppression]") {
  // 🔴 LE SERVEUR ENVOIE UN DÉLAI NÉGATIF une fois l'heure écoulée. Lu comme
  // une date, il donnait « pas programmée » : le personnage perdait son rouge
  // et son bouton « Annuler » au moment précis où sa suppression devenait
  // possible.
  LoopbackServer server;
  REQUIRE(server.listening());
  const std::int64_t before = WallNow();
  auto phase = AtTheTable(server, DoomedAliceList(kElapsedSeconds));
  REQUIRE(phase.has_value());
  REQUIRE(phase->step() == CharacterPhase::Step::kAwaitingSelection);
  const std::int64_t after = WallNow();

  CHECK(DeadlineOf(*phase, kAlice) > 0);
  CHECK(DeadlineOf(*phase, kAlice) >= before + kElapsedSeconds);
  CHECK(DeadlineOf(*phase, kAlice) <= after + kElapsedSeconds);
  // ⚠ ET BOB, QUE RIEN NE MENACE, GARDE SON ZÉRO.
  CHECK(DeadlineOf(*phase, kAlice + 1) == 0);
}

TEST_CASE("Suppression : le délai de la liste s'ancre à la réception", "[net][suppression]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  const std::int64_t before = WallNow();
  auto phase = AtTheTable(server, DoomedAliceList(kDelaySeconds));
  REQUIRE(phase.has_value());
  REQUIRE(phase->step() == CharacterPhase::Step::kAwaitingSelection);
  const std::int64_t after = WallNow();

  CHECK(DeadlineOf(*phase, kAlice) >= before + kDelaySeconds);
  CHECK(DeadlineOf(*phase, kAlice) <= after + kDelaySeconds);
}

TEST_CASE("Suppression : l'accusé est VIDE, et c'est nous qui savons qui part",
          "[net][suppression]") {
  // 🔴 `HC_ACCEPT_DELETECHAR` FAIT DEUX OCTETS : un opcode, et rien. Le
  // serveur ne redit pas QUI il vient de supprimer, et ne renvoie pas la
  // liste. Sans retenir la cible de notre propre demande, le client retirerait
  // le mauvais personnage — ou aucun.
  LoopbackServer server;
  REQUIRE(server.listening());
  auto phase = AtTheTable(server);
  REQUIRE(phase.has_value());

  phase->ConfirmDeletion(kAlice, "joueur@example.com");
  CHECK(phase->deletion_step() == DeletionStep::kConfirm);
  CHECK(phase->deletion_character_id() == kAlice);

  ro::proto::ChDeleteChar sent;
  REQUIRE(Decode(server.Receive(ro::proto::ChDeleteChar::kWireSize), sent));
  CHECK(sent.CID == kAlice);
  CHECK(sent.key == "joueur@example.com");

  REQUIRE(server.Send(Encode(ro::proto::HcAcceptDeletechar{})));
  Churn(*phase, kRoundsPastTheList);

  CHECK(phase->deletion() == DeletionState::kDone);
  REQUIRE(phase->characters().size() == 1);
  // C'est bien Alice qui est partie, et Bob qui reste.
  CHECK(phase->characters()[0].name == "Bob");
  CHECK(ro::net::IndexOfSlot(phase->characters(), 0) == -1);
  CHECK(ro::net::IndexOfSlot(phase->characters(), 1) == 0);
}

TEST_CASE("Suppression : le refus final NE COUPE PAS la phase", "[net][suppression]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  auto phase = AtTheTable(server);
  REQUIRE(phase.has_value());

  phase->ConfirmDeletion(kAlice, "faux@example.com");
  REQUIRE(server.Receive(ro::proto::ChDeleteChar::kWireSize).size() ==
          ro::proto::ChDeleteChar::kWireSize);

  ro::proto::HcRefuseDeletechar refused;
  refused.error = 0;  // « adresse email incorrecte », et trois autres causes
  REQUIRE(server.Send(Encode(refused)));
  Churn(*phase, kRoundsPastTheList);

  CHECK(phase->deletion() == DeletionState::kRefused);
  CHECK(phase->Advance() == PhaseState::kWaiting);
  CHECK(phase->step() == CharacterPhase::Step::kAwaitingSelection);
  // Personne n'est parti.
  CHECK(phase->characters().size() == 2);
  // ⚠ ET LE MOTIF NE PROMET PAS QUE C'EST L'ADRESSE : le serveur tombe sous ce
  // même code quand l'heure n'est pas atteinte.
  CHECK(phase->deletion_refusal_text().find("heure") != std::string::npos);
}

TEST_CASE("Suppression : rien ne part avant la liste", "[net][suppression]") {
  LoopbackServer server;
  REQUIRE(server.listening());

  auto phase =
      CharacterPhase::Start(ServerAddress{kLoopbackWireIp, server.port()}, LoginPhase::Account());
  REQUIRE(phase.has_value());
  REQUIRE(server.Accept());
  REQUIRE(server.Receive(kEnterRequestSize).size() == kEnterRequestSize);

  phase->ReserveDeletion(kAlice);
  phase->CancelDeletion(kAlice);
  phase->ConfirmDeletion(kAlice, "joueur@example.com");
  CHECK(phase->deletion() == DeletionState::kIdle);
  CHECK(server.Receive(1, ro::test_support::kBriefWaitMs).empty());
}
