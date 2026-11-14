// LES LUMIÈRES PONCTUELLES DE L'IMAGE : celles du document, à l'intensité de
// l'heure.

#include "ro/render/map_point_lights.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/scene_light.h"

using ro::mapedit::AddedLight;
using ro::render::MapPointLights;

namespace {

// Un ciel qui laisse `daylight` du jour, sans nuit posée par le serveur.
ro::render::SceneLight Sky(float daylight) {
  ro::render::SceneLight sky;
  sky.daylight = glm::vec3(daylight);
  return sky;
}
// Une carte de dix carreaux de côté.
constexpr ro::render::MapEffectFrame kFrame{10, 10};

}  // namespace

TEST_CASE("Lumières de l'image : la nuit allume celles qui l'attendent", "[render][mapedit]") {
  const std::vector<AddedLight> lights = {
      AddedLight{.position = {0.0F, -10.0F, 0.0F},
                 .color = {1.0F, 0.5F, 0.25F},
                 .range = 30.0F,
                 .night_only = true,
                 .why = "lanterne"},
      AddedLight{.position = {20.0F, 0.0F, 0.0F},
                 .color = {1.0F, 1.0F, 1.0F},
                 .range = 10.0F,
                 .night_only = false,
                 .why = "feu"},
  };
  const glm::vec3 eye = kFrame.ToWorld(glm::vec3(0.0F));

  // À midi, la lanterne est éteinte : seul le feu éclaire.
  const ro::render::PointLightSet noon = MapPointLights(lights, kFrame, Sky(1.0F), false, eye);
  REQUIRE(noon.lights.size() == 1);
  CHECK(noon.lights[0].range_world == 10.0F);

  // À minuit, les deux ; la lanterne, plus proche, d'abord, à pleine couleur
  // et au-dessus du sol — un y négatif du RSW monte.
  const ro::render::PointLightSet midnight = MapPointLights(lights, kFrame, Sky(0.0F), false, eye);
  REQUIRE(midnight.lights.size() == 2);
  CHECK(midnight.lights[0].color == glm::vec3(1.0F, 0.5F, 0.25F));
  CHECK(midnight.lights[0].world_position.y > eye.y);
}

TEST_CASE("Lumières de l'image : la nuit du serveur et les cartes fermées allument aussi",
          "[render][mapedit]") {
  const std::vector<AddedLight> lantern = {AddedLight{.position = {},
                                                      .color = {1.0F, 1.0F, 1.0F},
                                                      .range = 30.0F,
                                                      .night_only = true,
                                                      .why = "lanterne"}};
  const glm::vec3 eye = kFrame.ToWorld(glm::vec3(0.0F));
  // Le ciel dit midi, mais le serveur pose la nuit par la teinte.
  ro::render::SceneLight server_night = Sky(1.0F);
  server_night.tint = glm::vec3(0.0F);
  CHECK(MapPointLights(lantern, kFrame, server_night, false, eye).lights.size() == 1);
  // Une carte fermée au ciel : rien n'y fait le jour.
  CHECK(MapPointLights(lantern, kFrame, Sky(1.0F), true, eye).lights.size() == 1);
  CHECK(MapPointLights(lantern, kFrame, Sky(1.0F), false, eye).lights.empty());
}

TEST_CASE("Lumières de l'image : une lumière qui vacille reste dans sa plage",
          "[render][mapedit]") {
  constexpr double kSampleStepMs = 37.0;
  constexpr int kSamples = 200;
  float lowest = 1.0F;
  float highest = 0.0F;
  for (int sample = 0; sample < kSamples; ++sample) {
    const float factor = ro::render::FlickerFactor(0, sample * kSampleStepMs);
    lowest = std::min(lowest, factor);
    highest = std::max(highest, factor);
  }
  CHECK(lowest >= 1.0F - ro::render::kFlickerDepth - 1e-5F);
  CHECK(highest <= 1.0F + 1e-5F);
  // Elle bouge vraiment, et deux lumières ne battent pas ensemble.
  CHECK(highest - lowest > ro::render::kFlickerDepth / 2.0F);
  CHECK(ro::render::FlickerFactor(0, 500.0) != ro::render::FlickerFactor(1, 500.0));
  // Déterministe.
  CHECK(ro::render::FlickerFactor(3, 1234.0) == ro::render::FlickerFactor(3, 1234.0));
}

TEST_CASE("Lumières de l'image : une flamme monte et descend, par pas", "[render][mapedit]") {
  using ro::render::FlameSway;
  using ro::render::kFlameSwayStepMs;
  using ro::render::kFlameSwayWorld;
  constexpr int kSamples = 200;
  float widest = 0.0F;
  for (int sample = 0; sample < kSamples; ++sample) {
    const glm::vec3 sway = FlameSway(0, sample * kFlameSwayStepMs);
    // Jamais de côté : elle ne passe pas derrière le mur qui la porte.
    CHECK(sway.x == 0.0F);
    CHECK(sway.z == 0.0F);
    CHECK(std::abs(sway.y) <= kFlameSwayWorld + 1e-5F);
    widest = std::max(widest, std::abs(sway.y));
  }
  CHECK(widest > kFlameSwayWorld / 2.0F);
  // Tenu tout le long d'un pas : la carte d'ombre ne se repeint qu'au suivant.
  CHECK(FlameSway(2, kFlameSwayStepMs * 7.0) == FlameSway(2, kFlameSwayStepMs * 7.9));
  CHECK(FlameSway(2, kFlameSwayStepMs * 7.0) != FlameSway(2, kFlameSwayStepMs * 8.0));
}

TEST_CASE("Lumières de l'image : une flamme qui ne vacille pas reste en place",
          "[render][mapedit]") {
  std::vector<AddedLight> lights(2);
  lights[0].range = 20.0F;
  lights[1].range = 20.0F;
  lights[1].flicker = true;
  const ro::render::PointLightSet placed =
      MapPointLights(lights, kFrame, Sky(1.0F), true, glm::vec3(0.0F), 1234.0);
  REQUIRE(placed.lights.size() == 2);
  // Les deux gardent leur place de repos, et seule la flamme qui vacille se
  // balance autour.
  const glm::vec3 rest = kFrame.ToWorld(glm::vec3(0.0F));
  int swaying = 0;
  for (const ro::render::PointLight& light : placed.lights) {
    CHECK(light.world_position == rest);
    const bool sways = light.sway_world != glm::vec3(0.0F);
    swaying += sways ? 1 : 0;
    // Celle qui se balance a la pénombre d'une flamme ; l'autre, un bord net.
    CHECK(light.shadow_spread == (sways ? ro::render::kFlameShadowSpread : 0.0F));
  }
  CHECK(swaying == 1);
}

TEST_CASE("Lumières de l'image : un effet qui luit porte sa lumière", "[render][mapedit]") {
  ro::mapedit::MapEdit edit;
  edit.AddLight(AddedLight{.position = {}, .color = {1.0F, 1.0F, 1.0F}, .range = 5.0F, .why = "a"});
  edit.AddEffect(ro::mapedit::AddedEffect{.number = 47,
                                          .position = {3.0F, -4.0F, 5.0F},
                                          .night_only = true,
                                          .glow = ro::mapedit::EffectGlow{},
                                          .why = "flamme"});
  edit.AddEffect(ro::mapedit::AddedEffect{.number = 44, .position = {}, .why = "sans lueur"});
  const std::vector<AddedLight> lights = ro::render::DocumentLights(edit);
  REQUIRE(lights.size() == 2);
  // À la hauteur de la flamme, au-dessus de l'ancre : un y négatif monte.
  CHECK(lights[1].position ==
        std::array<float, 3>{3.0F, -4.0F - ro::render::kEffectGlowLiftWorld, 5.0F});
  CHECK(lights[1].night_only);
  CHECK(lights[1].flicker);
}
