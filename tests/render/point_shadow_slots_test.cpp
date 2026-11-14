#include <array>
#include <cstddef>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/render/point_lights.h"
#include "ro/render/point_shadow.h"

using ro::render::kPointShadowTierMargin;
using ro::render::kPointShadowTierMaxLayers;
using ro::render::kPointShadowTierReachWorld;
using ro::render::PointLight;
using ro::render::PointShadowSlots;
using ro::render::PointShadowTierOf;

namespace {

constexpr float kRangeWorld = 50.0F;

// Une lumière à `distance_world` de l'œil, posé à l'origine.
PointLight LightAt(float distance_world) {
  return PointLight{.world_position = glm::vec3(distance_world, 0.0F, 0.0F),
                    .color = glm::vec3(1.0F),
                    .range_world = kRangeWorld};
}

using Assigned = std::vector<std::optional<PointShadowSlots::Assignment>>;

}  // namespace

TEST_CASE("Le palier d'une ombre suit la distance à l'œil, avec une marge pour en sortir",
          "[render][point_shadow]") {
  const float first = kPointShadowTierReachWorld[0];
  CHECK(PointShadowTierOf(0.0F) == 0);
  CHECK(PointShadowTierOf(first * 0.9F) == 0);
  CHECK(PointShadowTierOf(first * 1.05F) == 1);
  CHECK(PointShadowTierOf(kPointShadowTierReachWorld[1] * 1.05F) == 2);
  // Tenu, le palier fin se garde dans sa marge, et se quitte au-delà.
  CHECK(PointShadowTierOf(first * (1.0F + (kPointShadowTierMargin / 2.0F)), 0) == 0);
  CHECK(PointShadowTierOf(first * (1.0F + (kPointShadowTierMargin * 2.0F)), 0) == 1);
  // Tenu, le palier moyen se garde un peu en deçà de sa borne proche.
  CHECK(PointShadowTierOf(first * (1.0F - (kPointShadowTierMargin / 2.0F)), 1) == 1);
  CHECK(PointShadowTierOf(first * (1.0F - (kPointShadowTierMargin * 2.0F)), 1) == 0);
}

TEST_CASE("Une lumière garde sa carte d'une image à l'autre", "[render][point_shadow]") {
  PointShadowSlots slots;
  const std::vector<PointLight> lights = {LightAt(10.0F), LightAt(20.0F)};
  const Assigned first = slots.Assign(lights, glm::vec3(0.0F));
  REQUIRE(first[0].has_value());
  REQUIRE(first[1].has_value());
  CHECK(first[0]->fresh);
  CHECK(first[1]->fresh);
  CHECK(first[0]->slot.layer != first[1]->slot.layer);
  // Dans l'autre ordre : chacune reprend la sienne, déjà peinte.
  const std::vector<PointLight> swapped = {lights[1], lights[0]};
  const Assigned again = slots.Assign(swapped, glm::vec3(0.0F));
  REQUIRE(again[0].has_value());
  REQUIRE(again[1].has_value());
  CHECK_FALSE(again[0]->fresh);
  CHECK(again[0]->slot.layer == first[1]->slot.layer);
  CHECK(again[1]->slot.layer == first[0]->slot.layer);
}

TEST_CASE("Une lumière qui revient retrouve sa carte peinte", "[render][point_shadow]") {
  PointShadowSlots slots;
  const std::vector<PointLight> lantern = {LightAt(10.0F)};
  const Assigned first = slots.Assign(lantern, glm::vec3(0.0F));
  static_cast<void>(slots.Assign({}, glm::vec3(0.0F)));
  const Assigned back = slots.Assign(lantern, glm::vec3(0.0F));
  REQUIRE(back[0].has_value());
  CHECK_FALSE(back[0]->fresh);
  CHECK(back[0]->slot.layer == first[0]->slot.layer);
}

TEST_CASE("Une lumière qui bouge est une autre lumière", "[render][point_shadow]") {
  PointShadowSlots slots;
  static_cast<void>(slots.Assign(std::vector<PointLight>{LightAt(10.0F)}, glm::vec3(0.0F)));
  const Assigned moved = slots.Assign(std::vector<PointLight>{LightAt(11.0F)}, glm::vec3(0.0F));
  REQUIRE(moved[0].has_value());
  CHECK(moved[0]->fresh);
}

TEST_CASE("Une flamme qui se balance garde sa carte", "[render][point_shadow]") {
  PointShadowSlots slots;
  const Assigned first = slots.Assign(std::vector<PointLight>{LightAt(10.0F)}, glm::vec3(0.0F));
  PointLight swayed = LightAt(10.0F);
  swayed.sway_world = glm::vec3(0.5F, 0.0F, -0.5F);
  const Assigned again = slots.Assign(std::vector<PointLight>{swayed}, glm::vec3(0.0F));
  REQUIRE(first[0].has_value());
  REQUIRE(again[0].has_value());
  CHECK_FALSE(again[0]->fresh);
  CHECK(again[0]->slot.layer == first[0]->slot.layer);
}

TEST_CASE("Un palier plein renvoie au suivant, et le dernier plein laisse sans ombre",
          "[render][point_shadow]") {
  PointShadowSlots slots;
  std::vector<PointLight> lights;
  std::size_t total = 0;
  for (const std::size_t layers : kPointShadowTierMaxLayers) {
    total += layers;
  }
  // Toutes proches : toutes demandent le palier le plus fin.
  for (std::size_t rank = 0; rank <= total; ++rank) {
    lights.push_back(LightAt(1.0F + static_cast<float>(rank) * 0.01F));
  }
  const Assigned assigned = slots.Assign(lights, glm::vec3(0.0F));
  std::array<std::size_t, ro::render::kPointShadowTierCount> by_tier{};
  for (std::size_t rank = 0; rank < total; ++rank) {
    REQUIRE(assigned[rank].has_value());
    ++by_tier.at(static_cast<std::size_t>(assigned[rank]->slot.tier));
  }
  for (std::size_t tier = 0; tier < ro::render::kPointShadowTierCount; ++tier) {
    CHECK(by_tier[tier] == kPointShadowTierMaxLayers[tier]);
    CHECK(slots.layers_used(tier) == kPointShadowTierMaxLayers[tier]);
  }
  // Les plus proches ont les cartes les plus fines.
  CHECK(assigned[0]->slot.tier == 0);
  CHECK_FALSE(assigned[total].has_value());
}

TEST_CASE("Une lumière sans ombre ne prend pas de carte", "[render][point_shadow]") {
  PointShadowSlots slots;
  PointLight unshadowed = LightAt(10.0F);
  unshadowed.casts_shadow = false;
  const Assigned assigned =
      slots.Assign(std::vector<PointLight>{unshadowed, LightAt(20.0F)}, glm::vec3(0.0F));
  CHECK_FALSE(assigned[0].has_value());
  REQUIRE(assigned[1].has_value());
  CHECK(slots.layers_used(0) == 1);
}
