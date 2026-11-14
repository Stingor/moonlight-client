// LA LIGNE D'UN SERTISSAGE : la demande retenue, l'accusé du serveur, et la
// phrase qui nomme la carte et la pièce d'avant — en liens d'objet.
//
// 🔴 CE BANC PORTE CE QU'UNE SESSION NE MONTRERAIT PAS SANS LONGUE MISE EN
// SCÈNE :
//
//   · la pièce se nomme SANS la carte qu'elle vient de recevoir ;
//   · un accusé qu'aucune demande n'attend retombe sur la phrase générale ;
//   · deux sertissages d'un même lot font deux lignes, et le second part de la
//     pièce qui porte déjà la première carte.

#include "items/card_insert_line.h"

#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/card_composition.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/card_composition.h"
#include "ro/sim/inventory.h"
#include "ro/sim/world.h"
#include "ro/vfs/item_link.h"
#include "ro/vfs/item_name.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/item_words.h"

#include "chat/chat_item_link.h"

using moonclient::CardInsertLine;
using moonclient::CardInsertSlotRank;
using moonclient::DecorateChatLine;
using moonclient::DecoratedChatLine;
using moonclient::ItemTagOf;
using ro::net::ApplyCardCompositionFrame;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::sim::CardInsertReport;
using ro::sim::InventoryItem;
using ro::sim::World;

namespace {

constexpr std::uint16_t kCardSlot = 29;
constexpr std::uint16_t kKnifeSlot = 33;
constexpr std::uint16_t kPotionSlot = 40;
constexpr std::uint32_t kCardId = 4001;
constexpr std::uint32_t kKnifeId = 1201;
constexpr std::uint32_t kPotionId = 501;
constexpr std::uint8_t kCardType = 6;
constexpr std::uint8_t kWeaponType = 5;
constexpr std::uint8_t kKnifeSlots = 3;
constexpr std::uint8_t kInserted = ro::net::kCardInserted;
constexpr std::uint8_t kRefused = ro::net::kCardRefused;

[[nodiscard]] ro::vfs::ItemTable SmallTable() {
  ro::vfs::ItemTable table;
  table.Put(kKnifeId, ro::vfs::ItemEntry{.name = "Knife", .card_slots = kKnifeSlots});
  table.Put(kCardId, ro::vfs::ItemEntry{.name = "Poring Card"});
  table.Put(kPotionId, ro::vfs::ItemEntry{.name = "Red Potion"});
  return table;
}

[[nodiscard]] ro::vfs::ItemWords SmallWords() {
  ro::vfs::ItemWords words;
  words.cards[kCardId] = ro::vfs::CardWord{.text = "Poring", .after_name = false};
  words.card_inserted = "L'insertion de la carte a réussi.";
  words.card_refused = "L'insertion de la carte a échoué.";
  return words;
}

// Le lien d'un objet du sac, réduit à ce que la phrase éprouve : l'objet, son
// genre et ses cartes.
[[nodiscard]] ItemTagOf TagsFrom(const ro::vfs::ItemTable& table) {
  return [&table](const InventoryItem& item) {
    ro::vfs::ItemLink link;
    link.item_id = item.item_id;
    link.equipable = ro::vfs::ItemTypeIsDecorated(item.type);
    link.refine_level = item.refine_level;
    link.cards = item.cards;
    const ro::vfs::ItemEntry* entry = table.Find(item.item_id);
    return ro::vfs::BuildItemLink(link, entry != nullptr ? entry->card_slots : 0);
  };
}

[[nodiscard]] World BagWithCardAndKnife() {
  World world;
  world.inventory().Put(
      InventoryItem{.client_slot = kCardSlot, .item_id = kCardId, .type = kCardType, .amount = 3});
  world.inventory().Put(InventoryItem{
      .client_slot = kKnifeSlot, .item_id = kKnifeId, .type = kWeaponType, .amount = 1});
  return world;
}

[[nodiscard]] Frame AckFrame(std::uint16_t equip, std::uint16_t card, std::uint8_t result) {
  std::vector<std::uint8_t> bytes;
  const auto put_u16 = [&bytes](std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFF));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8));
  };
  put_u16(ro::proto::ZcAckItemcomposition::kOpcode);
  put_u16(equip);
  put_u16(card);
  bytes.push_back(result);
  return Frame{.opcode = ro::proto::ZcAckItemcomposition::kOpcode, .bytes = bytes};
}

// Le texte que le joueur lit : les balises devenues des noms.
[[nodiscard]] std::string Shown(const std::string& line, const ro::vfs::ItemTable& table,
                                const ro::vfs::ItemWords& words) {
  DecoratedChatLine out;
  if (!DecorateChatLine(line, table, words, &out)) {
    return line;
  }
  return out.text;
}

// Les lignes de tous les accusés en attente, dans l'ordre.
[[nodiscard]] std::vector<std::string> Lines(World& world, const ro::vfs::ItemTable& table,
                                             const ro::vfs::ItemWords& words) {
  std::vector<std::string> lines;
  for (const CardInsertReport& report : world.composition().TakeReports()) {
    lines.push_back(Shown(CardInsertLine(report, &table, words, TagsFrom(table)), table, words));
  }
  return lines;
}

}  // namespace

TEST_CASE("Sertissage : la réussite nomme la carte, la pièce d'avant et l'emplacement",
          "[app][sertissage]") {
  const ro::vfs::ItemTable table = SmallTable();
  const ro::vfs::ItemWords words = SmallWords();
  World world = BagWithCardAndKnife();
  world.composition().AskInsert(kCardSlot, kKnifeSlot, world.inventory());
  REQUIRE(ApplyCardCompositionFrame(AckFrame(kKnifeSlot, kCardSlot, kInserted), world) ==
          FrameOutcome::kApplied);
  // La pièce a reçu la carte, mais la ligne la nomme comme elle était.
  REQUIRE(world.inventory().Find(kKnifeSlot)->cards[0] == kCardId);
  CHECK(Lines(world, table, words) ==
        std::vector<std::string>{"<Poring Card> sertie dans <Knife [3]> (emplacement 1/3)."});
  CHECK(world.composition().pending_asks() == 0);
}

TEST_CASE("Sertissage : les deux noms sont des liens d'objet", "[app][sertissage]") {
  const ro::vfs::ItemTable table = SmallTable();
  const ro::vfs::ItemWords words = SmallWords();
  World world = BagWithCardAndKnife();
  world.composition().AskInsert(kCardSlot, kKnifeSlot, world.inventory());
  REQUIRE(ApplyCardCompositionFrame(AckFrame(kKnifeSlot, kCardSlot, kInserted), world) ==
          FrameOutcome::kApplied);
  const std::vector<CardInsertReport> reports = world.composition().TakeReports();
  REQUIRE(reports.size() == 1);
  DecoratedChatLine out;
  REQUIRE(DecorateChatLine(CardInsertLine(reports[0], &table, words, TagsFrom(table)), table, words,
                           &out));
  REQUIRE(out.links.size() == 2);
  CHECK(out.carried[0].item.item_id == kCardId);
  CHECK(out.carried[1].item.item_id == kKnifeId);
  // ⚠ LE LIEN DE LA PIÈCE EST CELUI D'AVANT : il ne porte pas la carte.
  CHECK(out.carried[1].item.cards[0] == 0);
}

TEST_CASE("Sertissage : l'échec nomme la carte et la pièce, sans emplacement",
          "[app][sertissage]") {
  const ro::vfs::ItemTable table = SmallTable();
  const ro::vfs::ItemWords words = SmallWords();
  World world = BagWithCardAndKnife();
  world.composition().AskInsert(kCardSlot, kKnifeSlot, world.inventory());
  REQUIRE(ApplyCardCompositionFrame(AckFrame(kKnifeSlot, kCardSlot, kRefused), world) ==
          FrameOutcome::kApplied);
  CHECK(Lines(world, table, words) ==
        std::vector<std::string>{"<Poring Card> n'a pas pu être sertie dans <Knife [3]>."});
}

TEST_CASE("Sertissage : un accusé qu'aucune demande n'attend dit la phrase générale",
          "[app][sertissage]") {
  const ro::vfs::ItemTable table = SmallTable();
  const ro::vfs::ItemWords words = SmallWords();
  World world = BagWithCardAndKnife();
  REQUIRE(ApplyCardCompositionFrame(AckFrame(kKnifeSlot, kCardSlot, kInserted), world) ==
          FrameOutcome::kApplied);
  REQUIRE(ApplyCardCompositionFrame(AckFrame(kKnifeSlot, kCardSlot, kRefused), world) ==
          FrameOutcome::kApplied);
  CHECK(Lines(world, table, words) ==
        std::vector<std::string>{"L'insertion de la carte a réussi.",
                                 "L'insertion de la carte a échoué."});
}

TEST_CASE("Sertissage : un accusé sur d'autres places que la demande dit la phrase générale",
          "[app][sertissage]") {
  const ro::vfs::ItemTable table = SmallTable();
  const ro::vfs::ItemWords words = SmallWords();
  World world = BagWithCardAndKnife();
  world.composition().AskInsert(kCardSlot, kKnifeSlot, world.inventory());
  constexpr std::uint16_t kOtherSlot = 77;
  REQUIRE(ApplyCardCompositionFrame(AckFrame(kOtherSlot, kCardSlot, kInserted), world) ==
          FrameOutcome::kApplied);
  CHECK(Lines(world, table, words) ==
        std::vector<std::string>{"L'insertion de la carte a réussi."});
  // La demande attend toujours son propre accusé.
  CHECK(world.composition().pending_asks() == 1);
}

TEST_CASE("Sertissage : une place réattribuée depuis la demande dit la phrase générale",
          "[app][sertissage]") {
  const ro::vfs::ItemTable table = SmallTable();
  const ro::vfs::ItemWords words = SmallWords();
  World world = BagWithCardAndKnife();
  world.composition().AskInsert(kCardSlot, kKnifeSlot, world.inventory());
  // La carte part, et une potion prend sa place avant l'accusé.
  world.inventory().Take(kCardSlot, 3);
  world.inventory().Put(InventoryItem{.client_slot = kCardSlot, .item_id = kPotionId, .amount = 1});
  REQUIRE(ApplyCardCompositionFrame(AckFrame(kKnifeSlot, kCardSlot, kRefused), world) ==
          FrameOutcome::kApplied);
  CHECK(Lines(world, table, words) ==
        std::vector<std::string>{"L'insertion de la carte a échoué."});
}

TEST_CASE("Sertissage : deux demandes successives font deux lignes, chacune sur son avant",
          "[app][sertissage]") {
  const ro::vfs::ItemTable table = SmallTable();
  const ro::vfs::ItemWords words = SmallWords();
  World world = BagWithCardAndKnife();
  // Un lot de deux : les deux demandes partent avant le premier accusé, et
  // retiennent donc la même pièce vide.
  world.composition().AskInsert(kCardSlot, kKnifeSlot, world.inventory());
  world.composition().AskInsert(kCardSlot, kKnifeSlot, world.inventory());
  REQUIRE(ApplyCardCompositionFrame(AckFrame(kKnifeSlot, kCardSlot, kInserted), world) ==
          FrameOutcome::kApplied);
  REQUIRE(ApplyCardCompositionFrame(AckFrame(kKnifeSlot, kCardSlot, kInserted), world) ==
          FrameOutcome::kApplied);
  // ⚠ LE SECOND PART DE LA PIÈCE QUI PORTE DÉJÀ LA PREMIÈRE CARTE : c'est son
  // avant à lui, relu dans le sac à l'accusé.
  CHECK(
      Lines(world, table, words) ==
      std::vector<std::string>{"<Poring Card> sertie dans <Knife [3]> (emplacement 1/3).",
                               "<Poring Card> sertie dans <Poring Knife [3]> (emplacement 2/3)."});
}

TEST_CASE("Sertissage : une demande que le serveur a tue s'oublie au premier accusé suivant",
          "[app][sertissage]") {
  const ro::vfs::ItemTable table = SmallTable();
  const ro::vfs::ItemWords words = SmallWords();
  World world = BagWithCardAndKnife();
  world.inventory().Put(InventoryItem{
      .client_slot = kPotionSlot, .item_id = kKnifeId, .type = kWeaponType, .amount = 1});
  // La première demande n'aura jamais d'accusé : le serveur se tait sur ses
  // gardes de compatibilité.
  world.composition().AskInsert(kCardSlot, kPotionSlot, world.inventory());
  world.composition().AskInsert(kCardSlot, kKnifeSlot, world.inventory());
  REQUIRE(ApplyCardCompositionFrame(AckFrame(kKnifeSlot, kCardSlot, kInserted), world) ==
          FrameOutcome::kApplied);
  CHECK(Lines(world, table, words) ==
        std::vector<std::string>{"<Poring Card> sertie dans <Knife [3]> (emplacement 1/3)."});
  CHECK(world.composition().pending_asks() == 0);
}

TEST_CASE("Sertissage : le rang est le premier emplacement libre de la pièce",
          "[app][sertissage]") {
  InventoryItem equip;
  CHECK(CardInsertSlotRank(equip, kKnifeSlots) == 1);
  equip.cards = {kCardId, 0, 0, 0};
  CHECK(CardInsertSlotRank(equip, kKnifeSlots) == 2);
  equip.cards = {kCardId, 0, kCardId, 0};
  CHECK(CardInsertSlotRank(equip, kKnifeSlots) == 2);
  equip.cards = {kCardId, kCardId, kCardId, 0};
  // Pleine : la quatrième case n'est pas un emplacement de la pièce.
  CHECK(CardInsertSlotRank(equip, kKnifeSlots) == 0);
  CHECK(CardInsertSlotRank(InventoryItem{}, 0) == 0);
}

TEST_CASE("Sertissage : une réussite sans emplacement libre à nommer dit la phrase générale",
          "[app][sertissage]") {
  const ro::vfs::ItemTable table = SmallTable();
  const ro::vfs::ItemWords words = SmallWords();
  CardInsertReport report{.verdict = ro::sim::CardInsertVerdict::kInserted, .matched = true};
  report.card = InventoryItem{.client_slot = kCardSlot, .item_id = kCardId, .type = kCardType};
  report.equip = InventoryItem{.client_slot = kKnifeSlot, .item_id = kKnifeId, .type = kWeaponType};
  report.equip.cards = {kCardId, kCardId, kCardId, 0};
  CHECK(CardInsertLine(report, &table, words, TagsFrom(table)) ==
        "L'insertion de la carte a réussi.");
  // Sans table non plus : le total des emplacements est inconnu.
  report.equip.cards = {};
  CHECK(CardInsertLine(report, nullptr, words, TagsFrom(table)) ==
        "L'insertion de la carte a réussi.");
}
