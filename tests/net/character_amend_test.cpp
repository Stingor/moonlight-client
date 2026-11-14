// LES RETOUCHES D'UN PERSONNAGE — DÉPLACER, RENOMMER —, sur un serveur de
// bouclage.
//
// 🔴 CE QUI SE VÉRIFIE ICI : LES ACCUSÉS NE DISENT NI QUI NI OÙ. Le déplacement
// répond par une raison et un compteur, le renommage par un résultat seul. C'est
// au client de reporter SA demande dans sa copie de la liste — la liste que le
// serveur renvoie ensuite la remplace, mais elle peut arriver plus tard.
//
// Et comme pour la suppression : UN REFUS NE COUPE PAS LA PHASE.

#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/session.h"
#include "ro/proto/common_packets.h"
#include "ro/proto/manual_packets.h"

#include "support/char_session.h"
#include "support/loopback_server.h"

using ro::net::AmendState;
using ro::net::AmendStep;
using ro::net::CharacterPhase;
using ro::net::LoginPhase;
using ro::net::PhaseState;
using ro::net::ServerAddress;
using ro::test::LoopbackServer;
using ro::test_support::BareAccountId;
using ro::test_support::Churn;
using ro::test_support::kEnterRequestSize;
using ro::test_support::kLoopbackWireIp;
using ro::test_support::kRoundsPastTheList;
using ro::test_support::SettleUntil;

namespace {

constexpr std::uint32_t kAlice = 150'000;
constexpr std::uint32_t kBob = 150'001;
// Les déplacements qu'Alice a en réserve : deux coupons utilisés en jeu.
constexpr std::int32_t kAliceMoves = 2;
// L'emplacement libre où l'on envoie Alice.
constexpr std::uint8_t kFreeSlot = 4;

// Alice (0) peut se déplacer et se renommer ; Bob (1) ne peut ni l'un ni
// l'autre.
[[nodiscard]] std::vector<std::uint8_t> AmendableList() {
  ro::proto::HcAcceptEnter list;
  list.total = 2;
  ro::proto::CharacterInfo alice;
  alice.GID = kAlice;
  alice.name = "Alice";
  alice.CharNum = 0;
  alice.chr_slot_changeCnt = kAliceMoves;
  alice.chr_name_changeCnt = 1;
  ro::proto::CharacterInfo bob;
  bob.GID = kBob;
  bob.name = "Bob";
  bob.CharNum = 1;
  list.characters = {alice, bob};
  return Encode(list);
}

[[nodiscard]] ro::core::Result<CharacterPhase> AtTheTable(LoopbackServer& server) {
  LoginPhase::Account account;
  account.account_id = 2'000'123;
  auto phase = CharacterPhase::Start(ServerAddress{kLoopbackWireIp, server.port()}, account);
  if (!phase.has_value()) {
    return phase;
  }
  if (!server.Accept() || server.Receive(kEnterRequestSize).size() != kEnterRequestSize ||
      !server.Send(BareAccountId(account.account_id)) || !server.Send(AmendableList())) {
    return phase;
  }
  static_cast<void>(SettleUntil(*phase, CharacterPhase::Step::kAwaitingSelection));
  return phase;
}

[[nodiscard]] const ro::proto::CharacterInfo* Find(const CharacterPhase& phase,
                                                   std::uint32_t character_id) {
  for (const ro::proto::CharacterInfo& who : phase.characters()) {
    if (who.GID == character_id) {
      return &who;
    }
  }
  return nullptr;
}

}  // namespace

TEST_CASE("Déplacement : la place change CHEZ NOUS dès l'accusé", "[net][retouche]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  auto phase = AtTheTable(server);
  REQUIRE(phase.has_value());
  REQUIRE(phase->step() == CharacterPhase::Step::kAwaitingSelection);

  phase->MoveCharacter(0, kFreeSlot);
  CHECK(phase->amend_step() == AmendStep::kMove);
  CHECK(phase->amend() == AmendState::kPending);
  CHECK(phase->amend_target_slot() == kFreeSlot);

  ro::proto::ChReqChangeCharacterSlot sent;
  REQUIRE(Decode(server.Receive(ro::proto::ChReqChangeCharacterSlot::kWireSize), sent));
  CHECK(sent.slot_before == 0);
  CHECK(sent.slot_after == kFreeSlot);
  CHECK(static_cast<std::int32_t>(sent.remaining) == kAliceMoves);

  ro::proto::HcAckChangeCharacterSlot moved;
  moved.reason = 0;
  moved.moves = static_cast<std::uint16_t>(kAliceMoves - 1);
  REQUIRE(server.Send(Encode(moved)));
  Churn(*phase, kRoundsPastTheList);

  CHECK(phase->amend() == AmendState::kDone);
  const ro::proto::CharacterInfo* alice = Find(*phase, kAlice);
  REQUIRE(alice != nullptr);
  CHECK(alice->CharNum == kFreeSlot);
  // 🔴 LE COMPTEUR VIENT DE L'ACCUSÉ, pas d'une soustraction de notre cru :
  // un serveur réglé en illimité ne décompte rien.
  CHECK(alice->chr_slot_changeCnt == kAliceMoves - 1);
  CHECK(Find(*phase, kBob)->CharNum == 1);
}

TEST_CASE("Déplacement : une place occupée ÉCHANGE les deux convives", "[net][retouche]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  auto phase = AtTheTable(server);
  REQUIRE(phase.has_value());

  phase->MoveCharacter(0, 1);
  REQUIRE(server.Receive(ro::proto::ChReqChangeCharacterSlot::kWireSize).size() ==
          ro::proto::ChReqChangeCharacterSlot::kWireSize);
  ro::proto::HcAckChangeCharacterSlot moved;
  moved.moves = 1;
  REQUIRE(server.Send(Encode(moved)));
  Churn(*phase, kRoundsPastTheList);

  CHECK(phase->amend() == AmendState::kDone);
  CHECK(Find(*phase, kAlice)->CharNum == 1);
  CHECK(Find(*phase, kBob)->CharNum == 0);
}

TEST_CASE("Déplacement : le refus NE COUPE PAS la phase et ne déplace personne",
          "[net][retouche]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  auto phase = AtTheTable(server);
  REQUIRE(phase.has_value());

  phase->MoveCharacter(1, kFreeSlot);
  REQUIRE(server.Receive(ro::proto::ChReqChangeCharacterSlot::kWireSize).size() ==
          ro::proto::ChReqChangeCharacterSlot::kWireSize);
  ro::proto::HcAckChangeCharacterSlot refused;
  refused.reason = 1;
  REQUIRE(server.Send(Encode(refused)));
  Churn(*phase, kRoundsPastTheList);

  CHECK(phase->amend() == AmendState::kRefused);
  CHECK_FALSE(phase->amend_refusal_text().empty());
  CHECK(phase->Advance() == PhaseState::kWaiting);
  CHECK(phase->step() == CharacterPhase::Step::kAwaitingSelection);
  CHECK(Find(*phase, kBob)->CharNum == 1);
}

TEST_CASE("Renommage : le nom change dès l'accusé, puis la liste fait foi", "[net][retouche]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  auto phase = AtTheTable(server);
  REQUIRE(phase.has_value());

  phase->RenameCharacter(kAlice, "Alicia");
  CHECK(phase->amend_step() == AmendStep::kRename);
  CHECK(phase->amend() == AmendState::kPending);

  ro::proto::ChReqChangeCharname sent;
  REQUIRE(Decode(server.Receive(ro::proto::ChReqChangeCharname::kWireSize), sent));
  CHECK(sent.CID == kAlice);
  CHECK(sent.new_name == "Alicia");

  REQUIRE(server.Send(Encode(ro::proto::HcAckChangeCharname{})));
  Churn(*phase, kRoundsPastTheList);

  CHECK(phase->amend() == AmendState::kDone);
  CHECK(Find(*phase, kAlice)->name == "Alicia");
  CHECK(Find(*phase, kAlice)->chr_name_changeCnt == 0);

  // La liste que le serveur renvoie ensuite remplace la copie.
  ro::proto::HcAcceptEnter list;
  list.total = 2;
  ro::proto::CharacterInfo alice;
  alice.GID = kAlice;
  alice.name = "Alicia";
  alice.CharNum = 0;
  alice.chr_name_changeCnt = 1;
  list.characters = {alice};
  REQUIRE(server.Send(Encode(list)));
  Churn(*phase, kRoundsPastTheList);
  REQUIRE(phase->characters().size() == 1);
  CHECK(phase->characters()[0].chr_name_changeCnt == 1);
}

TEST_CASE("Renommage : le refus se dit et garde le nom", "[net][retouche]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  auto phase = AtTheTable(server);
  REQUIRE(phase.has_value());

  phase->RenameCharacter(kAlice, "Bob");
  REQUIRE(server.Receive(ro::proto::ChReqChangeCharname::kWireSize).size() ==
          ro::proto::ChReqChangeCharname::kWireSize);
  ro::proto::HcAckChangeCharname refused;
  refused.result = 4;  // nom déjà pris
  REQUIRE(server.Send(Encode(refused)));
  Churn(*phase, kRoundsPastTheList);

  CHECK(phase->amend() == AmendState::kRefused);
  CHECK(phase->amend_refusal_text().find("pris") != std::string::npos);
  CHECK(phase->Advance() == PhaseState::kWaiting);
  CHECK(Find(*phase, kAlice)->name == "Alice");
  CHECK(Find(*phase, kAlice)->chr_name_changeCnt == 1);

  phase->ForgetAmend();
  CHECK(phase->amend() == AmendState::kIdle);
  CHECK(phase->amend_refusal_text().empty());
}

TEST_CASE("Retouches : rien ne part avant la liste", "[net][retouche]") {
  LoopbackServer server;
  REQUIRE(server.listening());

  auto phase =
      CharacterPhase::Start(ServerAddress{kLoopbackWireIp, server.port()}, LoginPhase::Account());
  REQUIRE(phase.has_value());
  REQUIRE(server.Accept());
  REQUIRE(server.Receive(kEnterRequestSize).size() == kEnterRequestSize);

  phase->MoveCharacter(0, kFreeSlot);
  phase->RenameCharacter(kAlice, "Alicia");
  CHECK(phase->amend() == AmendState::kIdle);
  CHECK(server.Receive(1, ro::test_support::kBriefWaitMs).empty());
}
