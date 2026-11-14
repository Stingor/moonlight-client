// L'ATELIER, décodé — et ses quatre réponses, sur le fil.
//
// 🔴 CE BANC PORTE CE QU'AUCUNE SESSION NE MONTRERAIT VITE :
//
//   · la liste de fabrication du genre des flèches répond par le paquet des
//     flèches, QUEL QUE SOIT l'opcode qui l'a portée ;
//   · une réponse d'affinage hors des quatre connues n'a AUCUN effet ;
//   · l'annulation de la forge est ZÉRO, celle des deux autres chemins moins un ;
//   · ce que le joueur envoie — skill, objet — se note au départ, et l'objet se
//     résout PAR SA PLACE avant que le serveur ne le consomme.

#include "ro/net/craft_session.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/proto/manual_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/craft_session.h"
#include "ro/sim/inventory.h"
#include "ro/sim/world.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/wire_bytes.h"

using ro::net::ApplyCraftFrame;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::sim::MakeListKind;
using ro::sim::RefineOutcome;
using ro::sim::World;
using ro::test::Connect;
using ro::test::LoopbackServer;
using ro::test_support::PutU16;
using ro::test_support::PutU32;
using ro::test_support::ReadU16;
using ro::test_support::ReadU32;

namespace {

constexpr std::uint32_t kIronArrow = 1770;
constexpr std::uint32_t kSteelArrow = 1753;
constexpr std::uint32_t kRedPotion = 501;
constexpr std::uint32_t kSword = 1101;
constexpr std::uint32_t kPhracon = 1010;
constexpr std::uint16_t kFirstCard = 4001;

// Une liste à longueur variable : l'opcode, la longueur, un en-tête propre, puis
// les entrées.
[[nodiscard]] Frame ListFrame(std::uint16_t opcode, const std::vector<std::uint8_t>& head,
                              const std::vector<std::uint8_t>& entries) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, opcode);
  PutU16(bytes, static_cast<std::uint16_t>(4 + head.size() + entries.size()));
  bytes.insert(bytes.end(), head.begin(), head.end());
  bytes.insert(bytes.end(), entries.begin(), entries.end());
  return Frame{.opcode = opcode, .bytes = bytes};
}

[[nodiscard]] std::vector<std::uint8_t> Ids(const std::vector<std::uint32_t>& ids) {
  std::vector<std::uint8_t> bytes;
  for (const std::uint32_t id : ids) {
    PutU32(bytes, id);
  }
  return bytes;
}

}  // namespace

TEST_CASE("Atelier : la liste des flèches se lit, avec le genre des flèches", "[net][atelier]") {
  World world;
  const Frame frame =
      ListFrame(ro::proto::ZcMakingarrowList::kOpcode, {}, Ids({kIronArrow, kSteelArrow}));
  REQUIRE(ApplyCraftFrame(frame, world) == FrameOutcome::kApplied);
  const ro::sim::MakeList& list = world.craft().make_list();
  CHECK(list.kind == MakeListKind::kArrow);
  CHECK(list.make_type == ro::sim::kArrowMakeType);
  CHECK(list.item_ids == std::vector<std::uint32_t>{kIronArrow, kSteelArrow});
  CHECK(world.craft().make_list_seq() == 1);
}

TEST_CASE("Atelier : le genre des flèches choisit la réponse, pas l'opcode", "[net][atelier]") {
  World world;
  std::vector<std::uint8_t> head;
  PutU16(head, ro::sim::kArrowMakeType);
  REQUIRE(ApplyCraftFrame(ListFrame(ro::proto::ZcMakingitemList::kOpcode, head, Ids({kIronArrow})),
                          world) == FrameOutcome::kApplied);
  CHECK(world.craft().make_list().kind == MakeListKind::kArrow);

  std::vector<std::uint8_t> cooking;
  PutU16(cooking, ro::sim::kCookingMakeType);
  REQUIRE(ApplyCraftFrame(ListFrame(ro::proto::ZcMakingitemList::kOpcode, cooking, {}), world) ==
          FrameOutcome::kApplied);
  CHECK(world.craft().make_list().kind == MakeListKind::kMaking);
  CHECK(world.craft().make_list().make_type == ro::sim::kCookingMakeType);
  CHECK(world.craft().make_list().item_ids.empty());
  CHECK(world.craft().make_list_seq() == 2);
}

TEST_CASE("Atelier : la liste de forge ne garde que les objets", "[net][atelier]") {
  World world;
  // Seize octets par entrée : l'objet, puis trois matériaux toujours nuls.
  std::vector<std::uint8_t> entries = Ids({kRedPotion, 0, 0, 0, kSword, 0, 0, 0});
  REQUIRE(ApplyCraftFrame(ListFrame(ro::proto::ZcMakableitemlist::kOpcode, {}, entries), world) ==
          FrameOutcome::kApplied);
  CHECK(world.craft().make_list().kind == MakeListKind::kProduce);
  CHECK(world.craft().make_list().item_ids == std::vector<std::uint32_t>{kRedPotion, kSword});
}

TEST_CASE("Atelier : le résultat d'une fabrication, pair pour la réussite", "[net][atelier]") {
  World world;
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, ro::proto::ZcAckReqmakingitem::kOpcode);
  PutU16(bytes, 3);
  PutU32(bytes, kRedPotion);
  REQUIRE(ApplyCraftFrame(Frame{.opcode = ro::proto::ZcAckReqmakingitem::kOpcode, .bytes = bytes},
                          world) == FrameOutcome::kApplied);
  CHECK_FALSE(world.craft().make_result().succeeded());
  CHECK(world.craft().make_result().item_id == kRedPotion);
  CHECK(world.craft().make_result_seq() == 1);
}

TEST_CASE("Atelier : la maîtrise culinaire de ce serveur", "[net][atelier]") {
  World world;
  CHECK(world.craft().cook_mastery() == -1);
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, ro::proto::ZcMoonlightCookMastery::kOpcode);
  PutU16(bytes, ro::proto::ZcMoonlightCookMastery::kWireSize);
  PutU16(bytes, 812);
  REQUIRE(ApplyCraftFrame(
              Frame{.opcode = ro::proto::ZcMoonlightCookMastery::kOpcode, .bytes = bytes}, world) ==
          FrameOutcome::kApplied);
  CHECK(world.craft().cook_mastery() == 812);
}

TEST_CASE("Atelier : la liste d'affinage porte place, raffinement et cartes", "[net][atelier]") {
  World world;
  std::vector<std::uint8_t> entry;
  PutU16(entry, 7);
  PutU32(entry, kSword);
  entry.push_back(4);
  PutU32(entry, kFirstCard);
  PutU32(entry, 0);
  PutU32(entry, 0);
  PutU32(entry, 0);
  REQUIRE(entry.size() == ro::proto::ZcNotifyWeaponitemlistSub::kWireSize);
  REQUIRE(ApplyCraftFrame(ListFrame(ro::proto::ZcNotifyWeaponitemlist::kOpcode, {}, entry),
                          world) == FrameOutcome::kApplied);
  const auto& weapons = world.craft().refine_list();
  REQUIRE(weapons.size() == 1);
  CHECK(weapons[0].index == 7);
  CHECK(weapons[0].item_id == kSword);
  CHECK(weapons[0].refine == 4);
  CHECK(weapons[0].cards[0] == kFirstCard);
}

TEST_CASE("Atelier : une réponse d'affinage inconnue n'a aucun effet", "[net][atelier]") {
  World world;
  const auto answer = [&](std::int32_t result, std::uint32_t item_id) {
    std::vector<std::uint8_t> bytes;
    PutU16(bytes, ro::proto::ZcAckWeaponrefine::kOpcode);
    PutU32(bytes, static_cast<std::uint32_t>(result));
    PutU32(bytes, item_id);
    return ApplyCraftFrame(Frame{.opcode = ro::proto::ZcAckWeaponrefine::kOpcode, .bytes = bytes},
                           world);
  };
  // 🔴 POUR UN MINERAI MANQUANT, L'OBJET EST LE MINERAI.
  REQUIRE(answer(3, kPhracon) == FrameOutcome::kApplied);
  CHECK(world.craft().refine_result().outcome == RefineOutcome::kMissingOre);
  CHECK(world.craft().refine_result().item_id == kPhracon);
  CHECK(answer(4, kSword) == FrameOutcome::kIgnored);
  CHECK(world.craft().refine_result_seq() == 1);
}

TEST_CASE("Atelier : les quatre réponses, octet par octet", "[net][atelier]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);

  REQUIRE(ro::net::RequestMakeArrow(link, ro::net::kMakeCancelItem).has_value());
  const std::vector<std::uint8_t> arrow = server.Receive(ro::proto::CzReqMakingarrow::kWireSize);
  REQUIRE(arrow.size() == ro::proto::CzReqMakingarrow::kWireSize);
  CHECK(ReadU16(arrow, 0) == 0x01AE);
  CHECK(ReadU32(arrow, 2) == 0xFFFFFFFFU);

  // ⚠ LE GENRE PRÉCÈDE L'OBJET.
  REQUIRE(ro::net::RequestMakeItem(link, ro::sim::kCookingMakeType, kRedPotion).has_value());
  const std::vector<std::uint8_t> making = server.Receive(ro::proto::CzReqMakingitem::kWireSize);
  REQUIRE(making.size() == ro::proto::CzReqMakingitem::kWireSize);
  CHECK(ReadU16(making, 0) == 0x025B);
  CHECK(ReadU16(making, 2) == ro::sim::kCookingMakeType);
  CHECK(ReadU32(making, 4) == kRedPotion);

  REQUIRE(ro::net::RequestProduce(link, kSword, {1000, 994, 0}).has_value());
  const std::vector<std::uint8_t> produce = server.Receive(ro::proto::CzReqmakingitem::kWireSize);
  REQUIRE(produce.size() == ro::proto::CzReqmakingitem::kWireSize);
  CHECK(ReadU16(produce, 0) == 0x018E);
  CHECK(ReadU32(produce, 2) == kSword);
  CHECK(ReadU32(produce, 6) == 1000);
  CHECK(ReadU32(produce, 10) == 994);
  CHECK(ReadU32(produce, 14) == 0);

  REQUIRE(ro::net::RequestWeaponRefine(link, ro::net::kWeaponRefineCancel).has_value());
  const std::vector<std::uint8_t> refine = server.Receive(6);
  REQUIRE(refine.size() == 6);
  CHECK(ReadU16(refine, 0) == 0x0222);
  CHECK(ReadU32(refine, 2) == 0xFFFFFFFFU);
}

TEST_CASE("Atelier : ce que le joueur envoie se note au départ", "[net][atelier]") {
  World world;
  constexpr std::uint16_t kFurnaceSlot = 12;
  constexpr std::uint32_t kMiniFurnace = 612;
  world.inventory().Put(
      ro::sim::InventoryItem{.client_slot = kFurnaceSlot, .item_id = kMiniFurnace, .amount = 5});

  std::vector<std::uint8_t> cast;
  PutU16(cast, ro::proto::SkillOnTargetRequest::kOpcode);
  PutU16(cast, 10);
  PutU16(cast, 147);
  PutU32(cast, 2000000);
  ro::net::ObserveCraftTrigger(cast, world, 1234);
  CHECK(world.craft().last_cast().id == 147);
  CHECK(world.craft().last_cast().level == 10);
  CHECK(world.craft().last_cast().at_ms == 1234);

  std::vector<std::uint8_t> use;
  PutU16(use, ro::proto::UseItemRequest::kOpcode);
  PutU16(use, kFurnaceSlot);
  PutU32(use, 2000000);
  ro::net::ObserveCraftTrigger(use, world, 5678);
  CHECK(world.craft().last_use_item() == kMiniFurnace);
  CHECK(world.craft().last_use().at_ms == 5678);
}
