// LA TEINTURE DES EFFETS DE CHAPEAU, TIRÉE DU LIVRE DES STYLES : le calque 8
// sous le gid du porteur, appliqué aux seuls numéros qu'il affiche.

#include "effects/hat_effect_dyes.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_dye.h"
#include "ro/sim/actor.h"
#include "ro/sim/style.h"
#include "ro/sim/world.h"

namespace {

constexpr std::uint32_t kBearerId = 2000011;
constexpr std::uint16_t kGlowOfNewYear = 20;
constexpr std::uint16_t kSnow = 6;
constexpr std::uint16_t kNotWorn = 33;
constexpr std::uint8_t kEffectProfile = 4;
constexpr std::int16_t kHue = 120;
constexpr std::int8_t kDarker = -40;

[[nodiscard]] ro::sim::StyleLayerRecipe HatEffect(std::uint32_t number, std::uint8_t profile) {
  ro::sim::StyleLayerRecipe recipe;
  recipe.layer = ro::sim::StyleLayerKind::kHatEffect;
  recipe.key = number;
  recipe.profile = profile;
  recipe.adjusts[0].hue = kHue;
  recipe.adjusts[0].value = kDarker;
  recipe.adjusts[0].absolute = true;
  return recipe;
}

[[nodiscard]] ro::sim::World Bearing(std::vector<ro::sim::StyleLayerRecipe> layers) {
  ro::sim::World world;
  ro::sim::Actor bearer;
  bearer.id = kBearerId;
  bearer.kind = ro::sim::ActorKind::kPlayer;
  bearer.hat_effects = {kGlowOfNewYear, kSnow};
  world.Spawn(bearer);
  world.styles().RememberLayers(kBearerId, std::move(layers));
  return world;
}

}  // namespace

TEST_CASE("La teinture d'un effet affiché vient du calque 8 du livre", "[hat_effect_dyes]") {
  const ro::sim::World world = Bearing({HatEffect(kGlowOfNewYear, kEffectProfile)});
  const std::vector<moonclient::WornHatEffectDye> worn = moonclient::WornHatEffectDyes(world);
  REQUIRE(worn.size() == 2);
  for (const moonclient::WornHatEffectDye& line : worn) {
    CHECK(line.bearer_id == kBearerId);
    if (line.hat_effect_id == kGlowOfNewYear) {
      REQUIRE(line.dye.has_value());
      // « Assombrir » voyage dans l'octet de l'absolu.
      CHECK(*line.dye == ro::render::MakeEffectDye(kHue, 0, kDarker, true));
    } else {
      // La neige n'a pas de teinture : elle s'ôte.
      CHECK(line.hat_effect_id == kSnow);
      CHECK_FALSE(line.dye.has_value());
    }
  }
}

TEST_CASE("Une ligne sous un numéro non affiché ou d'un autre profil ne teint rien",
          "[hat_effect_dyes]") {
  const ro::sim::World world =
      Bearing({HatEffect(kNotWorn, kEffectProfile), HatEffect(kGlowOfNewYear, 1)});
  for (const moonclient::WornHatEffectDye& line : moonclient::WornHatEffectDyes(world)) {
    CHECK(line.hat_effect_id != kNotWorn);
    CHECK_FALSE(line.dye.has_value());
  }
}

TEST_CASE("Un porteur que le livre ne connaît pas n'a aucune teinture", "[hat_effect_dyes]") {
  ro::sim::World world;
  ro::sim::Actor bearer;
  bearer.id = kBearerId;
  bearer.hat_effects = {kGlowOfNewYear};
  world.Spawn(bearer);
  const std::vector<moonclient::WornHatEffectDye> worn = moonclient::WornHatEffectDyes(world);
  REQUIRE(worn.size() == 1);
  CHECK_FALSE(worn.front().dye.has_value());
}
