#include "ro/render/effect_panel_depth.h"

#include <array>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

using Catch::Approx;
using ro::render::EffectPanelDepthCorners;
using ro::render::kEffectPanelGroundClearance;

namespace {

// Une caméra qui plonge à quarante-cinq degrés sur un point au sol.
const glm::vec3 kGround{0.0F, 0.0F, 0.0F};
const glm::vec3 kEye{0.0F, 100.0F, 100.0F};
const glm::vec3 kForward = glm::normalize(kGround - kEye);
// Le haut et la droite du panneau face à cette caméra.
const glm::vec3 kUp = glm::normalize(glm::vec3{0.0F, 1.0F, -1.0F});
const glm::vec3 kRight{1.0F, 0.0F, 0.0F};

float DepthOf(glm::vec3 point) { return glm::dot(point - kEye, kForward); }

std::array<glm::vec3, 4> Panel(float centre_above_ground, float half_size) {
  const glm::vec3 centre = kGround + kUp * centre_above_ground;
  return {
      centre - kRight * half_size + kUp * half_size, centre + kRight * half_size + kUp * half_size,
      centre + kRight * half_size - kUp * half_size, centre - kRight * half_size - kUp * half_size};
}

// Vrai quand `placed` se projette au même point de l'écran que `corner`.
bool OnSameRay(glm::vec3 corner, glm::vec3 placed) {
  const glm::vec3 a = glm::normalize(corner - kEye);
  const glm::vec3 b = glm::normalize(placed - kEye);
  return glm::dot(a, b) == Approx(1.0F).margin(1e-6F);
}

}  // namespace

TEST_CASE("Un panneau qui ne descend pas sous le sol garde sa profondeur, moins la marge",
          "[render][effect]") {
  const auto corners = Panel(10.0F, 5.0F);
  const auto placed = EffectPanelDepthCorners(corners, kGround, kEye, kForward);
  for (std::size_t corner = 0; corner < corners.size(); ++corner) {
    CHECK(DepthOf(placed[corner]) ==
          Approx(DepthOf(corners[0]) - kEffectPanelGroundClearance).margin(1e-3F));
    CHECK(OnSameRay(corners[corner], placed[corner]));
  }
}

TEST_CASE("Un panneau qui descend sous le sol passe devant le terrain qu'il recouvre",
          "[render][effect]") {
  const auto corners = Panel(0.0F, 20.0F);
  const auto placed = EffectPanelDepthCorners(corners, kGround, kEye, kForward);

  // Le terrain que vise le coin le plus bas : là où son rayon croise le sol.
  const glm::vec3 lowest = corners[2];
  const float part = (kEye.y - kGround.y) / (kEye.y - lowest.y);
  const float terrain_depth = DepthOf(kEye + (lowest - kEye) * part);
  REQUIRE(terrain_depth < DepthOf(lowest));

  for (std::size_t corner = 0; corner < corners.size(); ++corner) {
    CHECK(DepthOf(placed[corner]) ==
          Approx(terrain_depth - kEffectPanelGroundClearance).margin(1e-3F));
    CHECK(OnSameRay(corners[corner], placed[corner]));
  }
}

TEST_CASE("Un mur dressé devant le panneau reste plus proche que lui", "[render][effect]") {
  // Une statue haute à deux cellules devant le point d'appui : son sommet
  // recouvre l'aura à l'écran, et doit la cacher.
  constexpr float kCellsInFront = 10.0F;
  const glm::vec3 statue_top{0.0F, 15.0F, kCellsInFront};
  const auto placed = EffectPanelDepthCorners(Panel(8.0F, 6.0F), kGround, kEye, kForward);
  CHECK(DepthOf(statue_top) < DepthOf(placed[0]));
}
