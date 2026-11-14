#include "ro/render/scene_light.h"

#include <cmath>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/rsw.h"

using ro::formats::RswLight;
using ro::render::MakeSceneLight;
using ro::render::SceneLight;

namespace {

// Comparaison tolérante : les rotations passent par des sinus et cosinus.
bool NearlyEqual(float actual, float expected) { return std::abs(actual - expected) < 1e-5F; }

}  // namespace

TEST_CASE("SceneLight reprend les couleurs du RSW", "[render][light]") {
  RswLight declared;
  declared.ambient = {0.55F, 0.50F, 0.50F};
  declared.diffuse = {1.0F, 0.9F, 0.8F};

  const SceneLight light = MakeSceneLight(declared);
  CHECK(light.ambient.r == 0.55F);
  CHECK(light.ambient.g == 0.50F);
  CHECK(light.diffuse.b == 0.8F);
}

TEST_CASE("SceneLight : direction sans rotation", "[render][light]") {
  RswLight declared;
  declared.longitude = 0;
  declared.latitude = 0;

  const SceneLight light = MakeSceneLight(declared);
  CHECK(NearlyEqual(light.direction.x, 0.0F));
  CHECK(NearlyEqual(light.direction.y, 1.0F));
  CHECK(NearlyEqual(light.direction.z, 0.0F));
}

TEST_CASE("SceneLight : la latitude incline autour de X", "[render][light]") {
  // Rotation de +90° autour de X appliquée à l'axe vertical : la direction
  // bascule vers +Z, c'est-à-dire vers l'observateur.
  //
  // Le signe ne vient PAS de la référence, qui ne peut pas trancher — voir
  // MakeSceneLight. Il a été mesuré contre une capture du client : l'inverse
  // place le soleil de l'autre côté, et les façades exposées sortent alors
  // sombres tandis que celles à l'ombre s'éclairent. Invisible sur un terrain
  // plat, flagrant sur le moindre bâtiment.
  RswLight declared;
  declared.longitude = 0;
  declared.latitude = 90;

  const SceneLight light = MakeSceneLight(declared);
  CHECK(NearlyEqual(light.direction.x, 0.0F));
  CHECK(NearlyEqual(light.direction.y, 0.0F));
  CHECK(NearlyEqual(light.direction.z, 1.0F));
}

TEST_CASE("SceneLight : la longitude fait TOURNER la direction", "[render][light]") {
  // Le test qui manquait. Avec les rotations composées dans le mauvais ordre,
  // la longitude s'appliquait à l'axe vertical AVANT son inclinaison —
  // c'est-à-dire à rien — et toutes les cartes recevaient la même lumière quelle
  // que soit leur longitude déclarée. Rien ne le signalait : la direction
  // restait normalisée et plausible.
  RswLight east;
  east.longitude = 0;
  east.latitude = 45;

  RswLight south;
  south.longitude = 90;
  south.latitude = 45;

  const SceneLight first = MakeSceneLight(east);
  const SceneLight second = MakeSceneLight(south);

  // À longitude nulle la direction reste dans le plan X = 0 ; un quart de tour
  // l'en sort forcément.
  CHECK(NearlyEqual(first.direction.x, 0.0F));
  CHECK(std::abs(second.direction.x) > 0.5F);
  // L'inclinaison, elle, ne dépend pas de la longitude.
  CHECK(NearlyEqual(first.direction.y, second.direction.y));
}

TEST_CASE("SceneLight : la direction est normalisée", "[render][light]") {
  RswLight declared;
  declared.longitude = 37;
  declared.latitude = 61;

  const SceneLight light = MakeSceneLight(declared);
  CHECK(NearlyEqual(glm::length(light.direction), 1.0F));
}
