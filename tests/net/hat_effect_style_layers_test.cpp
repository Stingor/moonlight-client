// LE CALQUE 8 DU FIL : la teinture d'un effet de chapeau, sous la clé de son
// numéro, dans le CZ 0x0F5E et dans le bloc du porteur du ZC 0x0F5F.

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
using ro::net::kStyleLayerEffectProfile;
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
constexpr std::uint8_t kHatEffectWire = 8;
constexpr std::uint8_t kTailFlag = 0x02;
// Le halo du Nouvel An, et un porteur.
constexpr std::uint32_t kGlowOfNewYear = 20;
constexpr std::uint32_t kBearerGid = 2000001;
constexpr std::int16_t kHue = 120;
constexpr std::int8_t kDarker = -40;

[[nodiscard]] StyleLayerRecipe HatEffect(std::uint32_t number,
                                         std::uint8_t profile = kStyleLayerEffectProfile) {
  StyleLayerRecipe recipe;
  recipe.layer = StyleLayerKind::kHatEffect;
  recipe.key = number;
  recipe.profile = profile;
  return recipe;
}

// Un bloc d'une entrée d'effet de chapeau, un réglage au rang 0.
[[nodiscard]] std::vector<std::uint8_t> HatEffectBlock(std::uint32_t gid, std::uint32_t number,
                                                       std::int16_t hue, std::uint8_t darken) {
  std::vector<std::uint8_t> raw;
  PutU32(raw, gid);
  raw.push_back(1);
  raw.push_back(static_cast<std::uint8_t>(kEntryHead + kAdjustBytes));
  raw.push_back(kHatEffectWire);
  PutU32(raw, number);
  raw.push_back(kStyleLayerEffectProfile);
  raw.push_back(0);
  PutU16(raw, 1);
  PutU16(raw, static_cast<std::uint16_t>(hue));
  raw.push_back(0);
  raw.push_back(static_cast<std::uint8_t>(kDarker));
  raw.push_back(darken);
  return raw;
}

}  // namespace

TEST_CASE("Un effet de chapeau part au calque 8, sous son numéro, sans emplacement",
          "[net][style_layers][hat_effect]") {
  StyleLayerOrder order{.recipe = HatEffect(kGlowOfNewYear)};
  order.recipe.adjusts[0].hue = kHue;
  order.recipe.adjusts[0].absolute = true;
  const auto frame = EncodeStyleLayers(std::span(&order, 1), /*clear_character=*/false);
  REQUIRE(frame.has_value());
  const std::vector<std::uint8_t>& bytes = *frame;
  const std::size_t at = kRequestHead;
  CHECK(bytes[at] == kEntryHead + kAdjustBytes);
  CHECK(bytes[at + 1] == kHatEffectWire);
  CHECK(ReadU32(bytes, at + 2) == kGlowOfNewYear);
  CHECK(bytes[at + 6] == kStyleLayerEffectProfile);
  CHECK(bytes[at + 7] == 0);
  CHECK(ReadU16(bytes, at + 8) == 1);
  // « Assombrir » voyage dans l'octet de l'absolu.
  CHECK(bytes[at + kEntryHead + 4] == 1);
  CHECK(at + bytes[at] == bytes.size());
}

TEST_CASE("Un effet de chapeau incohérent ne part pas", "[net][style_layers][hat_effect]") {
  const auto refused = [](const StyleLayerOrder& order) {
    return !EncodeStyleLayers(std::span(&order, 1), false).has_value();
  };
  // Sans numéro.
  CHECK(refused(StyleLayerOrder{.recipe = HatEffect(0)}));
  // 🔴 UN EFFET DE CHAPEAU NE SE DÉSIGNE PAS PAR UN EMPLACEMENT : le serveur cherche
  // lui-même l'objet porté qui le pose.
  CHECK(refused(StyleLayerOrder{.recipe = HatEffect(kGlowOfNewYear), .slot = StyleSlot::kHeadTop}));
  // Le profil d'une coiffe.
  CHECK(refused(StyleLayerOrder{.recipe = HatEffect(kGlowOfNewYear, 1)}));
  // Un second réglage : il n'y a que le rang 0.
  StyleLayerOrder two_ranks{.recipe = HatEffect(kGlowOfNewYear)};
  two_ranks.recipe.adjusts[1].hue = kHue;
  CHECK(refused(two_ranks));
  // Ni reste, ni retouche.
  StyleLayerOrder with_rest{.recipe = HatEffect(kGlowOfNewYear)};
  with_rest.recipe.rest[0].hue = kHue;
  CHECK(refused(with_rest));
  StyleLayerOrder with_retouch{.recipe = HatEffect(kGlowOfNewYear)};
  with_retouch.recipe.retouches.push_back({.index = 1, .red = 1, .green = 2, .blue = 3});
  CHECK(refused(with_retouch));
  // Le même effet deux fois.
  const std::array twice{StyleLayerOrder{.recipe = HatEffect(kGlowOfNewYear)},
                         StyleLayerOrder{.recipe = HatEffect(kGlowOfNewYear)}};
  CHECK_FALSE(EncodeStyleLayers(twice, false).has_value());
}

TEST_CASE("Le bloc d'un porteur se relit avec son effet de chapeau",
          "[net][style_layers][hat_effect]") {
  const std::vector<std::uint8_t> raw = HatEffectBlock(kBearerGid, kGlowOfNewYear, kHue, 1);
  const auto blocks = ReadStyleLayerPayload(raw);
  REQUIRE(blocks.has_value());
  REQUIRE(blocks->size() == 1);
  CHECK((*blocks)[0].gid == kBearerGid);
  REQUIRE((*blocks)[0].layers.size() == 1);
  const StyleLayerRecipe& read = (*blocks)[0].layers[0];
  CHECK(read.layer == StyleLayerKind::kHatEffect);
  CHECK(read.key == kGlowOfNewYear);
  CHECK(read.adjusts[0].hue == kHue);
  CHECK(read.adjusts[0].value == kDarker);
  CHECK(read.adjusts[0].absolute);

  Frame frame;
  frame.opcode = kLayersOpcode;
  PutU16(frame.bytes, kLayersOpcode);
  PutU16(frame.bytes, static_cast<std::uint16_t>(kLayersHead + raw.size()));
  frame.bytes.push_back(0);
  PutU16(frame.bytes, static_cast<std::uint16_t>(raw.size()));
  frame.bytes.insert(frame.bytes.end(), raw.begin(), raw.end());
  World world;
  REQUIRE(ApplyStyleLayersFrame(frame, world) == FrameOutcome::kApplied);
  const std::vector<StyleLayerRecipe>* layers = world.styles().FindLayers(kBearerGid);
  REQUIRE(layers != nullptr);
  REQUIRE(layers->size() == 1);
  CHECK(layers->front().layer == StyleLayerKind::kHatEffect);
}

TEST_CASE("Une queue sur un effet de chapeau reçu est ignorée, le réglage reste",
          "[net][style_layers][hat_effect]") {
  std::vector<std::uint8_t> raw = HatEffectBlock(kBearerGid, kGlowOfNewYear, kHue, 0);
  // La même entrée, annoncée avec une queue d'un reste vide et d'une retouche.
  constexpr std::size_t kEntryAt = 5;
  raw[kEntryAt + 7] = kTailFlag;
  const std::array<std::uint8_t, 8> tail = {0, 0, 1, 7, 10, 20, 30, 0};
  raw.insert(raw.end(), tail.begin(), tail.begin() + 7);
  raw[kEntryAt] = static_cast<std::uint8_t>(raw.size() - kEntryAt);
  const auto blocks = ReadStyleLayerPayload(raw);
  REQUIRE(blocks.has_value());
  REQUIRE((*blocks)[0].layers.size() == 1);
  CHECK((*blocks)[0].layers[0].retouches.empty());
  CHECK((*blocks)[0].layers[0].adjusts[0].hue == kHue);
}
