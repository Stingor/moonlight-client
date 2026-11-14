// La nuit d'une carte : un fondu du rouge et du vert de la diffuse, et la
// teinte qu'il donne au sol et au décor.

#include "ro/render/day_night_light.h"

#include <cmath>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/scene_light.h"

using ro::render::DayNightLight;
using ro::render::kDayNightFrameMs;
using ro::render::kNightDiffuseFloor;
using ro::render::SceneLight;

namespace {

bool NearlyEqual(float actual, float expected) { return std::abs(actual - expected) < 1e-4F; }

// Une carte de plein jour : diffuse pleine, ambiante 0,3.
SceneLight FullDay() {
  SceneLight light;
  light.ambient = {0.3F, 0.3F, 0.3F};
  light.diffuse = {1.0F, 1.0F, 1.0F};
  return light;
}

// Combien d'images de nuit avant que le rouge cesse de descendre.
int FramesToNight(DayNightLight& light) {
  int frames = 0;
  while (light.diffuse().r > kNightDiffuseFloor) {
    light.Step(true);
    ++frames;
  }
  return frames;
}

}  // namespace

TEST_CASE("Nuit : le rouge et le vert descendent au plancher, le bleu reste", "[render][nuit]") {
  DayNightLight light;
  light.Enter(FullDay());
  // 🔴 CENT UNE IMAGES, et la dernière passe SOUS le plancher : le pas tombe
  // au-dessus, et la garde ne regarde que l'avant.
  CHECK(FramesToNight(light) == 101);
  CHECK(light.diffuse().r < kNightDiffuseFloor);
  CHECK(NearlyEqual(light.diffuse().r, 0.495F));
  CHECK(light.diffuse().g == light.diffuse().r);
  CHECK(light.diffuse().b == 1.0F);

  // Au plancher, plus rien ne bouge.
  const float floor_red = light.diffuse().r;
  light.Step(true);
  CHECK(light.diffuse().r == floor_red);
}

TEST_CASE("Nuit : la teinte assombrit le rouge et le vert, jamais le bleu", "[render][nuit]") {
  DayNightLight light;
  light.Enter(FullDay());
  // Le jour, la teinte est neutre : le dessin de jour ne change pas.
  CHECK(light.Tint() == glm::vec3(1.0F, 1.0F, 1.0F));

  FramesToNight(light);
  const glm::vec3 tint = light.Tint();
  // 1 − 0,7 × (1 − 0,495), rapporté à la lumière du jour, qui vaut un.
  CHECK(NearlyEqual(tint.r, 0.6465F));
  CHECK(NearlyEqual(tint.g, 0.6465F));
  CHECK(tint.b == 1.0F);
  // Et la lumière à peindre la porte.
  CHECK(light.Lit(FullDay()).tint == tint);
}

TEST_CASE("Nuit : une diffuse déjà sombre ne descend pas", "[render][nuit]") {
  SceneLight dim = FullDay();
  dim.diffuse = {0.4F, 0.8F, 0.6F};
  DayNightLight light;
  light.Enter(dim);
  for (int frame = 0; frame < 200; ++frame) {
    light.Step(true);
  }
  CHECK(light.diffuse().r == 0.4F);
  CHECK(light.diffuse().g < kNightDiffuseFloor);
  CHECK(light.diffuse().b == 0.6F);
  CHECK(light.Tint().r == 1.0F);
}

TEST_CASE("Jour : la diffuse remonte vers celle du RSW, par crans", "[render][nuit]") {
  SceneLight map = FullDay();
  map.diffuse = {0.8F, 0.9F, 1.0F};
  DayNightLight light;
  light.Enter(map);
  for (int frame = 0; frame < 200; ++frame) {
    light.Step(true);
  }
  // UNE IMAGE DE JOUR ne rend qu'un cran : pas de retour d'un coup.
  const float night_red = light.diffuse().r;
  light.Step(false);
  CHECK(NearlyEqual(light.diffuse().r, night_red + ro::render::kDayNightStep));

  for (int frame = 0; frame < 200; ++frame) {
    light.Step(false);
  }
  // Rendue, à un cran près au plus, et jamais au-delà d'un.
  CHECK(light.diffuse().r >= 0.8F);
  CHECK(light.diffuse().r < 0.8F + ro::render::kDayNightStep + 1e-6F);
  CHECK(light.diffuse().g >= 0.9F);
  CHECK(light.diffuse().b == 1.0F);
  CHECK(NearlyEqual(light.Tint().r, 1.0F));
}

TEST_CASE("Nuit : une carte chargée repart du jour, sans fondu", "[render][nuit]") {
  DayNightLight light;
  light.Enter(FullDay());
  FramesToNight(light);
  light.Enter(FullDay());
  CHECK(light.diffuse() == glm::vec3(1.0F, 1.0F, 1.0F));
  CHECK(light.Tint() == glm::vec3(1.0F, 1.0F, 1.0F));
}

TEST_CASE("Nuit : le temps se compte en images du client", "[render][nuit]") {
  DayNightLight light;
  light.Enter(FullDay());
  // Moins d'une image : rien.
  light.Advance(true, kDayNightFrameMs * 0.5F);
  CHECK(light.diffuse().r == 1.0F);
  // La moitié restante en fait une.
  light.Advance(true, kDayNightFrameMs * 0.5F);
  CHECK(light.diffuse().r < 1.0F);
  // Deux secondes suffisent à toute la nuit.
  light.Advance(true, 2000.0F);
  CHECK(light.diffuse().r < kNightDiffuseFloor);
  // Un temps négatif ne rend pas le jour.
  light.Advance(false, -5000.0F);
  CHECK(light.diffuse().r < kNightDiffuseFloor);
}
