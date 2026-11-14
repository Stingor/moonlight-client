// LES LUMIÈRES DE LA NUIT DE L'ARÈNE : la lanterne du héros paraît à 7:00 et
// grandit jusqu'à 15:00 ; chaque objet au sol porte sa lueur.

#include "survivors/arena_night_lights.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/actor_layers.h"
#include "ro/render/terrain_mesh.h"

#include "survivors/arena_night.h"

using moonclient::ArenaHeroCell;
using moonclient::ArenaItemHaloAlpha;
using moonclient::ArenaItemHalos;
using moonclient::ArenaLantern;
using moonclient::ArenaLanternRadiusCells;
using moonclient::HordeBodyKind;
using moonclient::HordePose;
using moonclient::HordeTag;
using moonclient::kArenaItemHaloDayAlpha;
using moonclient::kArenaItemHaloNightAlpha;
using moonclient::kArenaItemHaloScreenPixels;
using moonclient::kArenaLanternDuskRadiusCells;
using moonclient::kArenaLanternNightRadiusCells;
using moonclient::kArenaMidnightMs;
using moonclient::kArenaMsPerMinute;
using moonclient::kArenaNightFallsMs;
using ro::render::DrawnActor;

namespace {

constexpr std::uint64_t kOneMs = 1;
constexpr std::uint64_t kNightSteps = 8;
constexpr std::uint64_t kLongAfterMs = 20 * kArenaMsPerMinute;
// LE RAYON VOULU : environ huit cases à la nuit entière.
constexpr float kWantedNightRadiusCells = 8.0F;
constexpr glm::vec3 kHeroFeet{100.0F, -4.0F, 250.0F};
constexpr std::uint8_t kHalfAlpha = 128;

[[nodiscard]] DrawnActor ActorAt(glm::vec3 feet) {
  DrawnActor actor;
  actor.ground_position = feet;
  return actor;
}

}  // namespace

TEST_CASE("Lanterne de l'arène : éteinte avant 7:00", "[app][survivors]") {
  CHECK(ArenaLanternRadiusCells(0) == 0.0F);
  CHECK(ArenaLanternRadiusCells(kArenaNightFallsMs - kOneMs) == 0.0F);
  CHECK_FALSE(ArenaLantern(kArenaNightFallsMs - kOneMs, kHeroFeet).has_value());
}

TEST_CASE("Lanterne de l'arène : son rayon grandit palier après palier", "[app][survivors]") {
  CHECK(ArenaLanternRadiusCells(kArenaNightFallsMs) == Catch::Approx(kArenaLanternDuskRadiusCells));
  float previous = 0.0F;
  for (std::uint64_t step = 0; step <= kNightSteps; ++step) {
    const float radius = ArenaLanternRadiusCells(kArenaNightFallsMs + step * kArenaMsPerMinute);
    CHECK(radius > previous);
    previous = radius;
  }
  CHECK(ArenaLanternRadiusCells(kArenaMidnightMs) == Catch::Approx(kWantedNightRadiusCells));
  CHECK(kArenaLanternNightRadiusCells == Catch::Approx(kWantedNightRadiusCells));
  CHECK(ArenaLanternRadiusCells(kLongAfterMs) == Catch::Approx(kWantedNightRadiusCells));
}

TEST_CASE("Lanterne de l'arène : au-dessus du héros, sans ombre, plus vive la nuit",
          "[app][survivors]") {
  const std::optional<ro::render::PointLight> dusk = ArenaLantern(kArenaNightFallsMs, kHeroFeet);
  const std::optional<ro::render::PointLight> night = ArenaLantern(kArenaMidnightMs, kHeroFeet);
  REQUIRE(dusk.has_value());
  REQUIRE(night.has_value());
  CHECK(night->world_position.x == kHeroFeet.x);
  CHECK(night->world_position.z == kHeroFeet.z);
  CHECK(night->world_position.y > kHeroFeet.y);
  CHECK_FALSE(night->casts_shadow);
  CHECK(night->range_world ==
        Catch::Approx(kWantedNightRadiusCells * ro::render::kWorldUnitsPerCell));
  CHECK(night->color.r > dusk->color.r);
  CHECK(dusk->color.r > 0.0F);
}

TEST_CASE("Lanterne de l'arène : elle suit la case du héros", "[app][survivors]") {
  constexpr glm::vec2 kHeroCell{40.0F, 60.0F};
  std::array<HordePose, 2> poses = {HordePose{.kind = HordeBodyKind::kHero, .cell = kHeroCell},
                                    HordePose{.kind = HordeBodyKind::kFoe}};
  const std::optional<glm::vec2> cell = ArenaHeroCell(poses);
  REQUIRE(cell.has_value());
  CHECK(*cell == kHeroCell);
  CHECK_FALSE(ArenaHeroCell(std::span(poses).subspan(1)).has_value());
  CHECK_FALSE(ArenaHeroCell({}).has_value());
}

TEST_CASE("Lueur des objets : une par gemme et par objet au sol, additive", "[app][survivors]") {
  std::array<DrawnActor, 4> drawn = {ActorAt(kHeroFeet), ActorAt(glm::vec3(1.0F)),
                                     ActorAt(glm::vec3(2.0F)), ActorAt(glm::vec3(3.0F))};
  drawn[3].tint = {255, 255, 255, kHalfAlpha};
  const std::array<HordeTag, 4> tags = {
      HordeTag{.kind = HordeBodyKind::kHero}, HordeTag{.kind = HordeBodyKind::kFoe},
      HordeTag{.kind = HordeBodyKind::kGem}, HordeTag{.kind = HordeBodyKind::kPickup}};
  std::vector<DrawnActor> halos(1);
  ArenaItemHalos(drawn, tags, kArenaMidnightMs, halos);
  REQUIRE(halos.size() == 2);
  CHECK(halos[0].ground_position == drawn[2].ground_position);
  CHECK(halos[1].ground_position == drawn[3].ground_position);
  for (const DrawnActor& halo : halos) {
    CHECK(halo.additive_parts == ro::render::kAllActorParts);
    CHECK(halo.grow_screen_pixels == kArenaItemHaloScreenPixels);
    CHECK(halo.shadow == nullptr);
    CHECK_FALSE(halo.fogged);
  }
  // UN OBJET À DEMI FONDU porte une lueur à demi.
  CHECK(halos[0].tint[3] > halos[1].tint[3]);
  CHECK(halos[1].tint[3] > 0);
}

TEST_CASE("Lueur des objets : discrète de jour, franche à la nuit", "[app][survivors]") {
  CHECK(ArenaItemHaloAlpha(0) == Catch::Approx(kArenaItemHaloDayAlpha));
  CHECK(ArenaItemHaloAlpha(kArenaNightFallsMs) == Catch::Approx(kArenaItemHaloDayAlpha));
  CHECK(ArenaItemHaloAlpha(kArenaMidnightMs) == Catch::Approx(kArenaItemHaloNightAlpha));
  CHECK(kArenaItemHaloNightAlpha > kArenaItemHaloDayAlpha);
  CHECK(kArenaItemHaloDayAlpha > 0.0F);
}
