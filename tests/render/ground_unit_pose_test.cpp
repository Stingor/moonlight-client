#include "ro/render/ground_unit_pose.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/model_mesh.h"
#include "ro/render/second_family_pose.h"

// L'IMAGE DU MODÈLE D'UN PIÈGE : figé sur son image de pose, puis joué une fois
// jusqu'à sa dernière image quand il se déclenche.
using ro::render::GroundUnitAtRest;
using ro::render::GroundUnitPlay;
using ro::render::GroundUnitRestFrame;
using ro::render::ModelRig;
using ro::render::StepGroundUnit;

namespace {

// Une animation de la première famille, en images : celle de deux pièges.
constexpr std::int32_t kFirstFamilyLength = 480;
// Une animation de la seconde famille : 30 images à 30 par seconde, soit une
// seconde.
constexpr std::int32_t kSecondFamilyLength = 30;
constexpr float kSecondFamilyRate = 30.0F;

ModelRig FirstFamilyRig() {
  ModelRig rig;
  rig.animation_length = kFirstFamilyLength;
  return rig;
}

ModelRig SecondFamilyRig() {
  ModelRig rig;
  rig.second_nodes.emplace_back();
  rig.animation_length = kSecondFamilyLength;
  rig.frame_rate = kSecondFamilyRate;
  return rig;
}

}  // namespace

TEST_CASE("Unité au sol : la première famille se pose à l'image 3", "[render][ground_unit]") {
  const ModelRig rig = FirstFamilyRig();
  CHECK(GroundUnitRestFrame(rig) == 3.0F);
  const GroundUnitPlay play = GroundUnitAtRest(rig);
  CHECK(play.frame == 3.0F);
  // Le compteur, lui, part de zéro.
  CHECK(play.counted_frame == 0);
}

TEST_CASE("Unité au sol : la seconde famille lit 3 comme des millisecondes",
          "[render][ground_unit]") {
  const ModelRig rig = SecondFamilyRig();
  // 30 images par seconde × 3 ms / 1000.
  CHECK(GroundUnitRestFrame(rig) == ro::render::SecondFamilyFrameAt(3, kSecondFamilyRate));
  CHECK(GroundUnitRestFrame(rig) > 0.08F);
  CHECK(GroundUnitRestFrame(rig) < 0.1F);
}

TEST_CASE("Unité au sol : sans ordre de jouer, le modèle reste figé", "[render][ground_unit]") {
  const ModelRig first = FirstFamilyRig();
  GroundUnitPlay play = GroundUnitAtRest(first);
  for (int step = 0; step < 10; ++step) {
    StepGroundUnit(first, false, 0, 16U * static_cast<std::uint32_t>(step), play);
  }
  CHECK(play.frame == 3.0F);

  const ModelRig second = SecondFamilyRig();
  GroundUnitPlay still = GroundUnitAtRest(second);
  StepGroundUnit(second, false, 0, 5000, still);
  CHECK(still.frame == GroundUnitRestFrame(second));
}

TEST_CASE("Unité au sol : déclenchée, la première famille joue une fois et s'arrête",
          "[render][ground_unit]") {
  const ModelRig rig = FirstFamilyRig();
  GroundUnitPlay play = GroundUnitAtRest(rig);
  // 33 images par pas, en partant de ZÉRO et non de l'image de pose.
  StepGroundUnit(rig, true, 0, 16, play);
  CHECK(play.frame == 33.0F);
  StepGroundUnit(rig, true, 0, 32, play);
  CHECK(play.frame == 66.0F);
  for (int step = 0; step < 20; ++step) {
    StepGroundUnit(rig, true, 0, 48, play);
  }
  // Jouée une fois : elle reste sur la dernière image, sans reboucler.
  CHECK(play.counted_frame == kFirstFamilyLength - 1);
  CHECK(play.frame == static_cast<float>(kFirstFamilyLength - 1));
}

TEST_CASE("Unité au sol : la seconde famille compte depuis que le modèle est bâti",
          "[render][ground_unit]") {
  const ModelRig rig = SecondFamilyRig();
  constexpr std::uint32_t kBuiltMs = 10'000;

  GroundUnitPlay play = GroundUnitAtRest(rig);
  StepGroundUnit(rig, true, kBuiltMs, kBuiltMs + 500, play);
  CHECK(play.frame == ro::render::SecondFamilyFrameAt(500, kSecondFamilyRate));

  // Un piège posé depuis plus longtemps que son animation saute droit à sa
  // dernière image quand il se déclenche.
  GroundUnitPlay late = GroundUnitAtRest(rig);
  StepGroundUnit(rig, true, kBuiltMs, kBuiltMs + 60'000, late);
  const double duration_ms =
      ro::render::SecondFamilyDurationMs(kSecondFamilyLength, kSecondFamilyRate);
  CHECK(late.frame == ro::render::SecondFamilyFrameAt(static_cast<std::uint32_t>(duration_ms - 1.0),
                                                      kSecondFamilyRate));
}

TEST_CASE("Unité au sol : Ankle Snare se tient déplié, et se referme quand il saute",
          "[render][ground_unit]") {
  CHECK(ro::render::GroundUnitRestsDeployed(ro::render::kAnkleSnareUnit));
  // Les autres pièges gardent leur image de pose.
  constexpr std::uint32_t kLandMineUnit = 147;
  CHECK_FALSE(ro::render::GroundUnitRestsDeployed(kLandMineUnit));

  const ModelRig rig = FirstFamilyRig();
  GroundUnitPlay play = ro::render::GroundUnitDeployed(rig);
  CHECK(play.frame == static_cast<float>(kFirstFamilyLength - 1));
  ro::render::StepDeployedGroundUnit(rig, false, play);
  CHECK(play.frame == static_cast<float>(kFirstFamilyLength - 1));
  ro::render::StepDeployedGroundUnit(rig, true, play);
  CHECK(play.frame == GroundUnitRestFrame(rig));
}
