// LES CALQUES 18 ET 19 DU FIL : la teinture de l'oiseau et du chariot du joueur,
// sous le numéro de leur image, dans le CZ 0x0F5E et dans le bloc du joueur du
// ZC 0x0F5F.

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/player_style_layers.h"
#include "ro/sim/style.h"

#include "support/wire_bytes.h"

using ro::net::EncodeStyleLayers;
using ro::net::ReadStyleLayerPayload;
using ro::net::StyleLayerOrder;
using ro::net::StyleSlot;
using ro::sim::StyleLayerKind;
using ro::sim::StyleLayerRecipe;
using ro::test_support::PutU16;
using ro::test_support::PutU32;
using ro::test_support::ReadU32;

namespace {

constexpr std::size_t kRequestHead = 6;
constexpr std::size_t kEntryHead = 10;
constexpr std::size_t kAdjustBytes = 5;
constexpr std::uint8_t kFalconWire = 18;
constexpr std::uint8_t kCartWire = 19;
// La chouette du ranger, le chariot de type 3, et le gid d'un joueur.
constexpr std::uint32_t kOwl = 3;
constexpr std::uint32_t kCartThree = 3;
constexpr std::uint32_t kPlayerGid = 2000042;

[[nodiscard]] StyleLayerRecipe Accessory(StyleLayerKind layer, std::uint32_t key,
                                         std::uint8_t profile = 1) {
  StyleLayerRecipe recipe;
  recipe.layer = layer;
  recipe.key = key;
  recipe.profile = profile;
  return recipe;
}

// Une entrée d'un réglage au rang 0, sous ce calque du fil.
void PutEntry(std::vector<std::uint8_t>& raw, std::uint8_t wire_layer, std::uint32_t key,
              std::int16_t hue) {
  raw.push_back(static_cast<std::uint8_t>(kEntryHead + kAdjustBytes));
  raw.push_back(wire_layer);
  PutU32(raw, key);
  raw.push_back(1);
  raw.push_back(0);
  PutU16(raw, 1);
  PutU16(raw, static_cast<std::uint16_t>(hue));
  raw.push_back(0);
  raw.push_back(0);
  raw.push_back(0);
}

}  // namespace

TEST_CASE("L'oiseau et le chariot partent sous leur clé, sans emplacement",
          "[net][style_layers][accessory]") {
  std::array orders{StyleLayerOrder{.recipe = Accessory(StyleLayerKind::kFalcon, kOwl)},
                    StyleLayerOrder{.recipe = Accessory(StyleLayerKind::kCart, kCartThree)}};
  orders[0].recipe.adjusts[0].hue = 90;
  orders[1].recipe.adjusts[0].hue = -90;
  const auto frame = EncodeStyleLayers(orders, /*clear_character=*/false);
  REQUIRE(frame.has_value());
  const std::vector<std::uint8_t>& bytes = *frame;
  std::size_t at = kRequestHead;
  CHECK(bytes[at + 1] == kFalconWire);
  CHECK(ReadU32(bytes, at + 2) == kOwl);
  at += bytes[at];
  CHECK(bytes[at + 1] == kCartWire);
  CHECK(ReadU32(bytes, at + 2) == kCartThree);
  CHECK(at + bytes[at] == bytes.size());
}

TEST_CASE("Un oiseau ou un chariot incohérent ne part pas", "[net][style_layers][accessory]") {
  const auto refused = [](const StyleLayerOrder& order) {
    return !EncodeStyleLayers(std::span(&order, 1), false).has_value();
  };
  // Sans clé : le fil réserve la clé zéro à la peau du visage.
  CHECK(refused(StyleLayerOrder{.recipe = Accessory(StyleLayerKind::kCart, 0)}));
  // 🔴 NI L'UN NI L'AUTRE NE SE DÉSIGNE PAR UN EMPLACEMENT.
  CHECK(refused(StyleLayerOrder{.recipe = Accessory(StyleLayerKind::kFalcon, kOwl),
                                .slot = StyleSlot::kHeadTop}));
  // Un profil inconnu ; le profil 2 des classes de teinte, lui, passe.
  CHECK(refused(StyleLayerOrder{.recipe = Accessory(StyleLayerKind::kFalcon, kOwl, 9)}));
  StyleLayerOrder classes{.recipe = Accessory(StyleLayerKind::kCart, kCartThree, 2)};
  classes.recipe.adjusts[12].hue = 30;
  CHECK(EncodeStyleLayers(std::span(&classes, 1), false).has_value());
  // Le même chariot deux fois.
  const std::array twice{StyleLayerOrder{.recipe = Accessory(StyleLayerKind::kCart, kCartThree)},
                         StyleLayerOrder{.recipe = Accessory(StyleLayerKind::kCart, kCartThree)}};
  CHECK_FALSE(EncodeStyleLayers(twice, false).has_value());
}

TEST_CASE("L'oiseau et le chariot se relisent dans le bloc du joueur",
          "[net][style_layers][accessory]") {
  std::vector<std::uint8_t> raw;
  PutU32(raw, kPlayerGid);
  raw.push_back(2);
  PutEntry(raw, kFalconWire, kOwl, 120);
  PutEntry(raw, kCartWire, kCartThree, -60);
  const auto blocks = ReadStyleLayerPayload(raw);
  REQUIRE(blocks.has_value());
  REQUIRE(blocks->size() == 1);
  CHECK((*blocks)[0].gid == kPlayerGid);
  REQUIRE((*blocks)[0].layers.size() == 2);
  CHECK((*blocks)[0].layers[0].layer == StyleLayerKind::kFalcon);
  CHECK((*blocks)[0].layers[0].key == kOwl);
  CHECK((*blocks)[0].layers[0].adjusts[0].hue == 120);
  CHECK((*blocks)[0].layers[1].layer == StyleLayerKind::kCart);
  CHECK((*blocks)[0].layers[1].adjusts[0].hue == -60);
}

TEST_CASE("Un calque inconnu entre deux se saute", "[net][style_layers][accessory]") {
  // Le 9 n'est pas un calque : un serveur plus récent peut l'employer, et
  // l'entrée s'enjambe sans emporter la suivante.
  constexpr std::uint8_t kUnknownWire = 9;
  std::vector<std::uint8_t> raw;
  PutU32(raw, kPlayerGid);
  raw.push_back(2);
  PutEntry(raw, kUnknownWire, kOwl, 10);
  PutEntry(raw, kFalconWire, kOwl, 20);
  const auto blocks = ReadStyleLayerPayload(raw);
  REQUIRE(blocks.has_value());
  REQUIRE((*blocks)[0].layers.size() == 1);
  CHECK((*blocks)[0].layers[0].layer == StyleLayerKind::kFalcon);
}
