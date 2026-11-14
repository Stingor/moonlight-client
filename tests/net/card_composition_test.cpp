// LE SERTISSAGE DES CARTES, décodé — et les trois demandes, sur le fil.
//
// 🔴 CE BANC PORTE CE QU'AUCUNE SESSION NE MONTRERAIT VITE :
//
//   · la liste des pièces NE RAPPELLE PAS LA CARTE : c'est la demande qui la
//     note, et la réponse l'y retrouve ;
//   · l'accusé réussi est le SEUL mot du serveur sur le sac : il retire la carte
//     sans paquet de retrait, et c'est ici qu'elle part et que la pièce la
//     reçoit ;
//   · un résultat ni 0 ni 1 n'a AUCUN effet, pas même une ligne ;
//   · une réponse du sertissage rapide qui porte sur une autre pièce se jette.

#include "ro/net/card_composition.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/card_composition.h"
#include "ro/sim/inventory.h"
#include "ro/sim/world.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/wire_bytes.h"

using ro::net::ApplyCardCompositionFrame;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::sim::CardInsertVerdict;
using ro::sim::InventoryItem;
using ro::sim::World;
using ro::test::Connect;
using ro::test::LoopbackServer;
using ro::test_support::PutU16;
using ro::test_support::ReadU16;

namespace {

constexpr std::uint16_t kCardSlot = 29;
constexpr std::uint16_t kSwordSlot = 33;
constexpr std::uint16_t kShieldSlot = 40;
constexpr std::uint32_t kCardId = 4035;
constexpr std::uint32_t kSwordId = 1101;
constexpr std::uint8_t kCardType = 6;
constexpr std::uint8_t kWeaponType = 5;

[[nodiscard]] Frame CandidatesFrame(const std::vector<std::uint16_t>& slots) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, ro::net::kCardCandidatesOpcode);
  PutU16(bytes, static_cast<std::uint16_t>(ro::net::kCardCandidatesFixedSize + slots.size() * 2));
  for (const std::uint16_t slot : slots) {
    PutU16(bytes, slot);
  }
  return Frame{.opcode = ro::net::kCardCandidatesOpcode, .bytes = bytes};
}

[[nodiscard]] Frame AckFrame(std::uint16_t equip, std::uint16_t card, std::uint8_t result) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, ro::proto::ZcAckItemcomposition::kOpcode);
  PutU16(bytes, equip);
  PutU16(bytes, card);
  bytes.push_back(result);
  return Frame{.opcode = ro::proto::ZcAckItemcomposition::kOpcode, .bytes = bytes};
}

[[nodiscard]] Frame CompatibleFrame(std::uint16_t equip, std::int16_t count,
                                    const std::vector<std::uint16_t>& cards) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, ro::proto::ZcMoonlightCompatCards::kOpcode);
  PutU16(bytes, static_cast<std::uint16_t>(ro::net::kCompatibleCardsFixedSize + cards.size() * 2));
  PutU16(bytes, equip);
  PutU16(bytes, static_cast<std::uint16_t>(count));
  for (const std::uint16_t card : cards) {
    PutU16(bytes, card);
  }
  return Frame{.opcode = ro::proto::ZcMoonlightCompatCards::kOpcode, .bytes = bytes};
}

// Un sac avec trois cartes identiques et une épée à trois cases vides.
[[nodiscard]] World BagWithCardAndSword() {
  World world;
  world.inventory().Put(
      InventoryItem{.client_slot = kCardSlot, .item_id = kCardId, .type = kCardType, .amount = 3});
  world.inventory().Put(InventoryItem{
      .client_slot = kSwordSlot, .item_id = kSwordId, .type = kWeaponType, .amount = 1});
  return world;
}

}  // namespace

TEST_CASE("Sertissage : la liste ouvre la session sur la carte DEMANDÉE", "[net][sertissage]") {
  World world = BagWithCardAndSword();
  world.composition().Ask(kCardSlot);
  REQUIRE(ApplyCardCompositionFrame(CandidatesFrame({kSwordSlot, kShieldSlot}), world) ==
          FrameOutcome::kApplied);
  CHECK(world.composition().open());
  CHECK(world.composition().offer().card_slot == kCardSlot);
  CHECK(world.composition().offer().equip_slots ==
        std::vector<std::uint16_t>{kSwordSlot, kShieldSlot});
  CHECK(world.composition().offer_seq() == 1);
}

TEST_CASE("Sertissage : une demande sans réponse ne change pas la carte de la session",
          "[net][sertissage]") {
  World world = BagWithCardAndSword();
  world.composition().Ask(kCardSlot);
  REQUIRE(ApplyCardCompositionFrame(CandidatesFrame({kSwordSlot}), world) ==
          FrameOutcome::kApplied);
  // ⚠ UNE AUTRE CARTE DEMANDÉE, À LAQUELLE LE SERVEUR NE RÉPOND PAS — aucune
  // pièce ne lui convient : la session reste celle de la première.
  world.composition().Ask(kShieldSlot);
  CHECK(world.composition().offer().card_slot == kCardSlot);
}

TEST_CASE("Sertissage : l'accusé réussi retire la carte et la pose sur la pièce",
          "[net][sertissage]") {
  World world = BagWithCardAndSword();
  REQUIRE(ApplyCardCompositionFrame(AckFrame(kSwordSlot, kCardSlot, ro::net::kCardInserted),
                                    world) == FrameOutcome::kApplied);
  CHECK(world.inventory().Find(kCardSlot)->amount == 2);
  CHECK(world.inventory().Find(kSwordSlot)->cards[0] == kCardId);
  CHECK(world.composition().verdict() == CardInsertVerdict::kInserted);
  CHECK(world.composition().verdict_seq() == 1);

  // La SECONDE carte va dans la case suivante.
  REQUIRE(ApplyCardCompositionFrame(AckFrame(kSwordSlot, kCardSlot, ro::net::kCardInserted),
                                    world) == FrameOutcome::kApplied);
  CHECK(world.inventory().Find(kSwordSlot)->cards[1] == kCardId);
  CHECK(world.inventory().Find(kCardSlot)->amount == 1);
}

TEST_CASE("Sertissage : la dernière carte fait disparaître sa ligne", "[net][sertissage]") {
  World world = BagWithCardAndSword();
  world.inventory().SetAmount(kCardSlot, 1);
  REQUIRE(ApplyCardCompositionFrame(AckFrame(kSwordSlot, kCardSlot, ro::net::kCardInserted),
                                    world) == FrameOutcome::kApplied);
  CHECK(world.inventory().Find(kCardSlot) == nullptr);
  CHECK(world.inventory().Find(kSwordSlot)->cards[0] == kCardId);
}

TEST_CASE("Sertissage : un refus ne touche pas au sac, mais se dit", "[net][sertissage]") {
  World world = BagWithCardAndSword();
  REQUIRE(ApplyCardCompositionFrame(AckFrame(kSwordSlot, kCardSlot, ro::net::kCardRefused),
                                    world) == FrameOutcome::kApplied);
  CHECK(world.inventory().Find(kCardSlot)->amount == 3);
  CHECK(world.inventory().Find(kSwordSlot)->cards[0] == 0);
  CHECK(world.composition().verdict() == CardInsertVerdict::kRefused);
  CHECK(world.composition().verdict_seq() == 1);
}

TEST_CASE("Sertissage : un résultat inconnu n'a aucun effet", "[net][sertissage]") {
  World world = BagWithCardAndSword();
  constexpr std::uint8_t kUnknownResult = 2;
  CHECK(ApplyCardCompositionFrame(AckFrame(kSwordSlot, kCardSlot, kUnknownResult), world) ==
        FrameOutcome::kIgnored);
  CHECK(world.inventory().Find(kCardSlot)->amount == 3);
  CHECK(world.composition().verdict_seq() == 0);
}

TEST_CASE("Sertissage : un accusé sur une place vide se dit sans rien changer",
          "[net][sertissage]") {
  World world = BagWithCardAndSword();
  constexpr std::uint16_t kGoneSlot = 90;
  REQUIRE(ApplyCardCompositionFrame(AckFrame(kGoneSlot, kCardSlot, ro::net::kCardInserted),
                                    world) == FrameOutcome::kApplied);
  CHECK(world.inventory().Find(kCardSlot)->amount == 3);
  CHECK(world.composition().verdict_seq() == 1);
}

TEST_CASE("Sertissage rapide : la réponse sur la pièce demandée se pose", "[net][sertissage]") {
  World world = BagWithCardAndSword();
  world.composition().AskCompatible(kSwordSlot);
  CHECK_FALSE(world.composition().compatible().answered);
  REQUIRE(ApplyCardCompositionFrame(CompatibleFrame(kSwordSlot, 1, {kCardSlot}), world) ==
          FrameOutcome::kApplied);
  CHECK(world.composition().compatible().answered);
  CHECK(world.composition().compatible().card_slots == std::vector<std::uint16_t>{kCardSlot});
}

TEST_CASE("Sertissage rapide : une liste vide est une réponse", "[net][sertissage]") {
  World world = BagWithCardAndSword();
  world.composition().AskCompatible(kSwordSlot);
  REQUIRE(ApplyCardCompositionFrame(CompatibleFrame(kSwordSlot, 0, {}), world) ==
          FrameOutcome::kApplied);
  CHECK(world.composition().compatible().answered);
  CHECK(world.composition().compatible().card_slots.empty());
}

TEST_CASE("Sertissage rapide : une réponse en retard se jette", "[net][sertissage]") {
  World world = BagWithCardAndSword();
  world.composition().AskCompatible(kShieldSlot);
  CHECK(ApplyCardCompositionFrame(CompatibleFrame(kSwordSlot, 1, {kCardSlot}), world) ==
        FrameOutcome::kIgnored);
  CHECK_FALSE(world.composition().compatible().answered);
}

TEST_CASE("Sertissage rapide : un compte menteur est borné par la trame", "[net][sertissage]") {
  World world = BagWithCardAndSword();
  world.composition().AskCompatible(kSwordSlot);
  constexpr std::int16_t kClaimed = 50;
  REQUIRE(ApplyCardCompositionFrame(CompatibleFrame(kSwordSlot, kClaimed, {kCardSlot}), world) ==
          FrameOutcome::kApplied);
  CHECK(world.composition().compatible().card_slots.size() == 1);
}

TEST_CASE("Sertissage : les trames arrivent par l'entrée commune", "[net][sertissage]") {
  // 🔴 UN TRADUCTEUR QUE L'AIGUILLAGE N'APPELLE PAS NE FAIT RIEN, sans erreur :
  // ce banc passe par l'entrée que la partie emploie, pas par le traducteur.
  World world = BagWithCardAndSword();
  world.composition().Ask(kCardSlot);
  constexpr std::uint32_t kNowMs = 1000;
  CHECK(ro::net::ApplyFrame(CandidatesFrame({kSwordSlot}), world, kNowMs) ==
        FrameOutcome::kApplied);
  CHECK(world.composition().open());
  CHECK(ro::net::ApplyFrame(AckFrame(kSwordSlot, kCardSlot, ro::net::kCardInserted), world,
                            kNowMs) == FrameOutcome::kApplied);
  CHECK(world.inventory().Find(kSwordSlot)->cards[0] == kCardId);
  world.composition().AskCompatible(kSwordSlot);
  CHECK(ro::net::ApplyFrame(CompatibleFrame(kSwordSlot, 1, {kCardSlot}), world, kNowMs) ==
        FrameOutcome::kApplied);
  CHECK(world.composition().compatible().answered);
}

TEST_CASE("Sertissage : une trame étrangère n'est pas à lui", "[net][sertissage]") {
  World world;
  const Frame other{.opcode = ro::proto::ZcAckItemidentify::kOpcode, .bytes = {}};
  CHECK(ApplyCardCompositionFrame(other, world) == FrameOutcome::kUnhandled);
}

TEST_CASE("Sertissage : les trois demandes, sur le fil", "[net][sertissage]") {
  LoopbackServer server;
  ro::net::Connection link = Connect(server);

  REQUIRE(ro::net::RequestCardCandidates(link, kCardSlot).has_value());
  const std::vector<std::uint8_t> list =
      server.Receive(ro::proto::CzReqItemcompositionList::kWireSize);
  REQUIRE(list.size() == ro::proto::CzReqItemcompositionList::kWireSize);
  CHECK(ReadU16(list, 0) == 0x017A);
  CHECK(ReadU16(list, 2) == kCardSlot);

  // ⚠ LA CARTE D'ABORD, LA PIÈCE ENSUITE : c'est l'ordre du paquet.
  REQUIRE(ro::net::RequestInsertCard(link, kCardSlot, kSwordSlot).has_value());
  const std::vector<std::uint8_t> insert =
      server.Receive(ro::proto::CzReqItemcomposition::kWireSize);
  REQUIRE(insert.size() == ro::proto::CzReqItemcomposition::kWireSize);
  CHECK(ReadU16(insert, 0) == 0x017C);
  CHECK(ReadU16(insert, 2) == kCardSlot);
  CHECK(ReadU16(insert, 4) == kSwordSlot);

  // ⚠ LE CHAMP DE LONGUEUR EST REMPLI : six, la taille du paquet.
  REQUIRE(ro::net::RequestCompatibleCards(link, kSwordSlot).has_value());
  const std::vector<std::uint8_t> compatible =
      server.Receive(ro::proto::CzMoonlightReqCompatCards::kWireSize);
  REQUIRE(compatible.size() == ro::proto::CzMoonlightReqCompatCards::kWireSize);
  CHECK(ReadU16(compatible, 0) == 0x0F18);
  CHECK(ReadU16(compatible, 2) == ro::proto::CzMoonlightReqCompatCards::kWireSize);
  CHECK(ReadU16(compatible, 4) == kSwordSlot);
}
