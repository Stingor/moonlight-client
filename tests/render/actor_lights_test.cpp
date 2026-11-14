// LES LUMIÈRES QUE PORTENT LES ACTEURS : le catalogue lu par la table des
// classes, la lumière de chaque porteur, et leur place dans le jeu de l'image.

#include "ro/render/actor_lights.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_lights.h"
#include "ro/render/map_point_lights.h"

using Catch::Approx;
using ro::render::ActorLightBearer;
using ro::render::ActorLightCatalogue;
using ro::render::ActorLightsOf;
using ro::render::PointLight;

namespace {

constexpr std::uint32_t kSkeletonWorker = 1169;
constexpr std::uint32_t kElusiveSkeletonWorker = 2660;
constexpr std::uint32_t kJakk = 1130;
constexpr std::uint32_t kPoring = 1002;
constexpr std::uint32_t kUnlistedJob = 99999;
constexpr std::uint32_t kMage = 2;
constexpr std::uint32_t kOptionSight = 0x1;
constexpr std::uint32_t kOptionRuwach = 0x2000;
constexpr std::uint32_t kOptionHide = 0x2;
constexpr float kLampHeightWorld = 13.0F;
constexpr float kLampRangeWorld = 45.0F;
constexpr double kNoonMs = 0.0;

[[nodiscard]] ActorLightCatalogue Catalogue() {
  const std::unordered_map<std::uint32_t, std::string> classes = {
      {kSkeletonWorker, "SKEL_WORKER"},
      // Une copie du même sprite, écrite autrement : la casse ne compte pas.
      {kElusiveSkeletonWorker, "skel_worker"},
      {kJakk, "JAKK"},
      {kPoring, "PORING"},
  };
  return ActorLightCatalogue::FromClassSprites(classes);
}

[[nodiscard]] ActorLightBearer Bearer(std::uint32_t job, std::uint32_t option = 0) {
  return ActorLightBearer{
      .seed = 7, .job = job, .option = option, .feet_world = glm::vec3(10.0F, 2.0F, 30.0F)};
}

[[nodiscard]] std::vector<PointLight> LightsOf(const ActorLightBearer& bearer) {
  std::vector<PointLight> out;
  ActorLightsOf({&bearer, 1}, Catalogue(), kNoonMs, out);
  return out;
}

}  // namespace

TEST_CASE("Lumières d'acteur : un monstre du catalogue porte la sienne, un autre non",
          "[render][actor_lights]") {
  const ActorLightCatalogue catalogue = Catalogue();
  REQUIRE(catalogue.OfJob(kSkeletonWorker) != nullptr);
  CHECK(catalogue.OfJob(kSkeletonWorker)->range_world == Approx(kLampRangeWorld));
  CHECK(catalogue.OfJob(kElusiveSkeletonWorker) == catalogue.OfJob(kSkeletonWorker));
  CHECK(catalogue.OfJob(kJakk) != nullptr);
  CHECK(catalogue.OfJob(kPoring) == nullptr);
  CHECK(catalogue.OfJob(kUnlistedJob) == nullptr);
  CHECK(catalogue.size() == 3);
}

TEST_CASE("Lumières d'acteur : la lampe se tient au-dessus des pieds, sans ombre",
          "[render][actor_lights]") {
  const std::vector<PointLight> lights = LightsOf(Bearer(kSkeletonWorker));
  REQUIRE(lights.size() == 1);
  CHECK(lights[0].world_position.x == Approx(10.0F));
  CHECK(lights[0].world_position.y == Approx(2.0F + kLampHeightWorld));
  CHECK(lights[0].world_position.z == Approx(30.0F));
  CHECK(lights[0].range_world == Approx(kLampRangeWorld));
  CHECK_FALSE(lights[0].casts_shadow);
  // La lampe du mineur ne vacille pas : sa couleur est celle du catalogue.
  CHECK(lights[0].color.r == Approx(1.0F));
}

TEST_CASE("Lumières d'acteur : un corps grandi porte sa lampe plus haut",
          "[render][actor_lights]") {
  ActorLightBearer giant = Bearer(kSkeletonWorker);
  giant.scale = 2.0F;
  const std::vector<PointLight> lights = LightsOf(giant);
  REQUIRE(lights.size() == 1);
  CHECK(lights[0].world_position.y == Approx(2.0F + 2.0F * kLampHeightWorld));
}

TEST_CASE("Lumières d'acteur : la lumière fond avec le corps, et s'éteint avec lui",
          "[render][actor_lights]") {
  ActorLightBearer fading = Bearer(kSkeletonWorker);
  fading.opacity = 0.5F;
  const std::vector<PointLight> half = LightsOf(fading);
  REQUIRE(half.size() == 1);
  CHECK(half[0].color.r == Approx(0.5F));

  fading.opacity = 0.0F;
  CHECK(LightsOf(fading).empty());
}

TEST_CASE("Lumières d'acteur : une flamme vacille sans dépasser sa couleur",
          "[render][actor_lights]") {
  const ro::render::ActorLightSpec* pumpkin = Catalogue().OfJob(kJakk);
  REQUIRE(pumpkin != nullptr);
  REQUIRE(pumpkin->flicker);
  for (double time_ms = 0.0; time_ms < 2000.0; time_ms += 137.0) {
    std::vector<PointLight> out;
    const ActorLightBearer bearer = Bearer(kJakk);
    ActorLightsOf({&bearer, 1}, Catalogue(), time_ms, out);
    REQUIRE(out.size() == 1);
    CHECK(out[0].color.r <= pumpkin->color.r + 1e-5F);
    CHECK(out[0].color.r >= pumpkin->color.r * (1.0F - ro::render::kFlickerDepth) - 1e-5F);
  }
}

TEST_CASE("Lumières d'acteur : Sight éclaire en orangé, Ruwach en bleuté, tant qu'ils tiennent",
          "[render][actor_lights]") {
  CHECK(LightsOf(Bearer(kMage)).empty());
  CHECK(LightsOf(Bearer(kMage, kOptionHide)).empty());

  const std::vector<PointLight> sight = LightsOf(Bearer(kMage, kOptionSight));
  REQUIRE(sight.size() == 1);
  CHECK(sight[0].color.r > sight[0].color.b);
  CHECK(sight[0].world_position.y > 2.0F);
  // Une lumière par lanceur : elle porte une ombre douce, comme un sort.
  CHECK(sight[0].casts_shadow);
  CHECK(sight[0].shadow_depth == Approx(ro::render::kEffectShadowDepth));

  const std::vector<PointLight> ruwach = LightsOf(Bearer(kMage, kOptionRuwach));
  REQUIRE(ruwach.size() == 1);
  CHECK(ruwach[0].color.b > ruwach[0].color.r);

  CHECK(LightsOf(Bearer(kMage, kOptionSight | kOptionRuwach)).size() == 2);
}

TEST_CASE("Lumières d'acteur : un monstre qui porte un état porte les deux lumières",
          "[render][actor_lights]") {
  const std::vector<PointLight> lights = LightsOf(Bearer(kSkeletonWorker, kOptionSight));
  REQUIRE(lights.size() == 2);
  CHECK_FALSE(lights[0].casts_shadow);
  CHECK(lights[1].casts_shadow);
}

TEST_CASE("Lumières d'acteur : au plus douze, les plus proches, après le reste du jeu",
          "[render][actor_lights]") {
  ro::render::PointLightSet before;
  before.lights.push_back(PointLight{
      .world_position = glm::vec3(500.0F), .color = glm::vec3(1.0F), .range_world = 10.0F});
  std::vector<PointLight> actors;
  constexpr std::size_t kCrowd = 20;
  for (std::size_t rank = 0; rank < kCrowd; ++rank) {
    // Rangés du plus loin au plus près, pour que le tri compte.
    actors.push_back(
        PointLight{.world_position = glm::vec3(static_cast<float>(kCrowd - rank), 0.0F, 0.0F),
                   .color = glm::vec3(1.0F),
                   .range_world = 10.0F});
  }
  const ro::render::PointLightSet merged =
      ro::render::WithActorLights(before, actors, glm::vec3(0.0F));
  REQUIRE(merged.lights.size() == 1 + ro::render::kMaxActorLights);
  CHECK(merged.lights[0].world_position.x == Approx(500.0F));
  CHECK(merged.lights[1].world_position.x == Approx(1.0F));
  CHECK(merged.lights.back().world_position.x ==
        Approx(static_cast<float>(ro::render::kMaxActorLights)));
}
