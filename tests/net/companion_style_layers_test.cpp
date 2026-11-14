// LE CALQUE 7 DU FIL : la teinture d'un compagnon, sous la clé de sa classe,
// dans le CZ 0x0F5E et dans le bloc du gid du compagnon du ZC 0x0F5F.

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/net/player_style_layers.h"
#include "ro/net/world_update.h"
#include "ro/sim/style.h"
#include "ro/sim/world.h"

#include "support/wire_bytes.h"

using ro::net::ApplyStyleLayersFrame;
using ro::net::EncodeStyleLayers;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::net::ReadStyleLayerPayload;
using ro::net::StyleLayerOrder;
using ro::net::StyleSlot;
using ro::sim::StyleLayerKind;
using ro::sim::StyleLayerRecipe;
using ro::sim::World;
using ro::test_support::PutU16;
using ro::test_support::PutU32;
using ro::test_support::ReadU16;
using ro::test_support::ReadU32;

namespace {

constexpr std::uint16_t kLayersOpcode = 0x0F5F;
constexpr std::size_t kRequestHead = 6;
constexpr std::size_t kLayersHead = 7;
constexpr std::size_t kEntryHead = 10;
constexpr std::size_t kAdjustBytes = 5;
constexpr std::uint8_t kCompanionWire = 7;
// Le Lif, et un gid d'unité non joueur.
constexpr std::uint32_t kLif = 6001;
constexpr std::uint32_t kCompanionGid = 110000042;

[[nodiscard]] StyleLayerRecipe Companion(std::uint32_t class_id, std::uint8_t profile = 1) {
  StyleLayerRecipe recipe;
  recipe.layer = StyleLayerKind::kCompanion;
  recipe.key = class_id;
  recipe.profile = profile;
  return recipe;
}

// Un bloc d'une entrée de compagnon, un réglage au rang 0.
[[nodiscard]] std::vector<std::uint8_t> CompanionBlock(std::uint32_t gid, std::uint32_t class_id,
                                                       std::int16_t hue) {
  std::vector<std::uint8_t> raw;
  PutU32(raw, gid);
  raw.push_back(1);
  raw.push_back(static_cast<std::uint8_t>(kEntryHead + kAdjustBytes));
  raw.push_back(kCompanionWire);
  PutU32(raw, class_id);
  raw.push_back(1);
  raw.push_back(0);
  PutU16(raw, 1);
  PutU16(raw, static_cast<std::uint16_t>(hue));
  raw.push_back(0);
  raw.push_back(0);
  raw.push_back(0);
  return raw;
}

}  // namespace

TEST_CASE("Un compagnon part au calque 7, sous la classe attendue, sans emplacement",
          "[net][style_layers][companion]") {
  StyleLayerOrder order{.recipe = Companion(kLif)};
  order.recipe.adjusts[0].hue = 90;
  const auto frame = EncodeStyleLayers(std::span(&order, 1), /*clear_character=*/false);
  REQUIRE(frame.has_value());
  const std::vector<std::uint8_t>& bytes = *frame;
  const std::size_t at = kRequestHead;
  CHECK(bytes[at] == kEntryHead + kAdjustBytes);
  CHECK(bytes[at + 1] == kCompanionWire);
  CHECK(ReadU32(bytes, at + 2) == kLif);
  CHECK(ReadU16(bytes, at + 8) == 1);
  CHECK(at + bytes[at] == bytes.size());
}

TEST_CASE("Un compagnon incohérent ne part pas", "[net][style_layers][companion]") {
  const auto refused = [](const StyleLayerOrder& order) {
    return !EncodeStyleLayers(std::span(&order, 1), false).has_value();
  };
  // Sans classe.
  CHECK(refused(StyleLayerOrder{.recipe = Companion(0)}));
  // 🔴 UN COMPAGNON NE SE DÉSIGNE PAS PAR UN EMPLACEMENT : il n'y en a pas.
  CHECK(refused(StyleLayerOrder{.recipe = Companion(kLif), .slot = StyleSlot::kHeadTop}));
  // Un profil inconnu.
  CHECK(refused(StyleLayerOrder{.recipe = Companion(kLif, 9)}));
  // Le même compagnon deux fois.
  const std::array twice{StyleLayerOrder{.recipe = Companion(kLif)},
                         StyleLayerOrder{.recipe = Companion(kLif)}};
  CHECK_FALSE(EncodeStyleLayers(twice, false).has_value());
}

TEST_CASE("Le profil 2 des classes de teinte vaut pour un compagnon",
          "[net][style_layers][companion]") {
  StyleLayerOrder order{.recipe = Companion(kLif, 2)};
  order.recipe.adjusts[12].hue = 30;
  CHECK(EncodeStyleLayers(std::span(&order, 1), false).has_value());
}

TEST_CASE("Le bloc d'un compagnon se relit sous son gid", "[net][style_layers][companion]") {
  const std::vector<std::uint8_t> raw = CompanionBlock(kCompanionGid, kLif, 120);
  const auto blocks = ReadStyleLayerPayload(raw);
  REQUIRE(blocks.has_value());
  REQUIRE(blocks->size() == 1);
  CHECK((*blocks)[0].gid == kCompanionGid);
  REQUIRE((*blocks)[0].layers.size() == 1);
  CHECK((*blocks)[0].layers[0].layer == StyleLayerKind::kCompanion);
  CHECK((*blocks)[0].layers[0].key == kLif);
  CHECK((*blocks)[0].layers[0].adjusts[0].hue == 120);

  // Et il se range dans le livre, sous le gid du compagnon.
  Frame frame;
  frame.opcode = kLayersOpcode;
  PutU16(frame.bytes, kLayersOpcode);
  PutU16(frame.bytes, static_cast<std::uint16_t>(kLayersHead + raw.size()));
  frame.bytes.push_back(0);
  PutU16(frame.bytes, static_cast<std::uint16_t>(raw.size()));
  frame.bytes.insert(frame.bytes.end(), raw.begin(), raw.end());
  World world;
  REQUIRE(ApplyStyleLayersFrame(frame, world) == FrameOutcome::kApplied);
  const std::vector<StyleLayerRecipe>* layers = world.styles().FindLayers(kCompanionGid);
  REQUIRE(layers != nullptr);
  REQUIRE(layers->size() == 1);
  CHECK(layers->front().layer == StyleLayerKind::kCompanion);
}
