#include "ro/render/ground_pick.h"

#include <cmath>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gat.h"
#include "ro/formats/gnd.h"
#include "ro/render/camera.h"
#include "ro/render/terrain_mesh.h"

#include "support/map_builder.h"

using ro::formats::GatMap;
using ro::formats::GndMap;
using ro::render::Camera;
using ro::render::CellAtPosition;
using ro::render::CellCenterPosition;
using ro::render::kWorldUnitsPerCube;
using ro::render::MakeScreenRay;
using ro::render::PickGround;
using ro::render::ScreenRay;
using ro::test_support::BuildGat;
using ro::test_support::BuildGnd;
using ro::test_support::GatCellSpec;
using ro::test_support::GndBuildOptions;
using ro::test_support::GndCubeSpec;

namespace {

constexpr float kViewportWidth = 800.0F;
constexpr float kViewportHeight = 600.0F;
constexpr float kAspectRatio = kViewportWidth / kViewportHeight;

bool NearlyEqual(float actual, float expected, float tolerance = 1e-2F) {
  return std::abs(actual - expected) < tolerance;
}

// Une carte plate de dix cubes de côté, à hauteur nulle. Plate exprès : le point
// visé s'y calcule à la main, ce qui permet de vérifier le pointage lui-même
// plutôt que l'interpolation du relief.
GndMap FlatMap() {
  GndBuildOptions options;
  options.width = 10;
  options.height = 10;
  options.texture_paths = {"sol.bmp"};
  options.tile_count = 1;
  options.cubes.assign(100, GndCubeSpec{.corner_height = {0.0F, 0.0F, 0.0F, 0.0F}, .tile_up = 0});

  auto gnd = GndMap::Parse(BuildGnd(options));
  REQUIRE(gnd.has_value());
  return std::move(*gnd);
}

// Une caméra qui vise le point donné, de haut et de face.
Camera Looking(const std::array<float, 3>& target) {
  Camera camera;
  camera.look_at = glm::vec3(target[0], target[1], target[2]);
  camera.distance = 300.0F;
  camera.pitch_degrees = 45.0F;
  camera.yaw_degrees = 0.0F;
  camera.fov_degrees = 45.0F;
  return camera;
}

}  // namespace

TEST_CASE("Pointage : le rayon central passe par le point visé", "[render][pick]") {
  // C'est la propriété qui définit une caméra : ce qu'elle vise est au centre de
  // l'image. Si elle tombe, tout le reste du pointage vise à côté, et l'erreur
  // se lit comme un défaut de terrain.
  const Camera camera = Looking({50.0F, 0.0F, 50.0F});
  const ScreenRay ray = MakeScreenRay(camera, kAspectRatio, kViewportWidth / 2.0F,
                                      kViewportHeight / 2.0F, kViewportWidth, kViewportHeight);

  const glm::vec3 toward = glm::normalize(camera.look_at - camera.Position());
  CHECK(NearlyEqual(ray.direction.x, toward.x, 1e-3F));
  CHECK(NearlyEqual(ray.direction.y, toward.y, 1e-3F));
  CHECK(NearlyEqual(ray.direction.z, toward.z, 1e-3F));
}

TEST_CASE("Pointage : un viewport dégénéré ne fabrique pas de rayon", "[render][pick]") {
  // Une fenêtre réduite le temps d'une image donne une largeur nulle, et la
  // division par zéro produirait des NaN qui se propageraient jusqu'à une
  // destination absurde envoyée au serveur.
  const ScreenRay ray = MakeScreenRay(Looking({0.0F, 0.0F, 0.0F}), 1.0F, 0.0F, 0.0F, 0.0F, 0.0F);
  CHECK(std::isfinite(ray.direction.x));
  CHECK(std::isfinite(ray.direction.y));
  CHECK(std::isfinite(ray.direction.z));
}

TEST_CASE("Pointage : le centre de l'écran retrouve le point visé", "[render][pick]") {
  const GndMap gnd = FlatMap();
  const std::array<float, 3> target = CellCenterPosition(gnd, 8, 9);
  const Camera camera = Looking(target);

  const auto hit = PickGround(gnd, nullptr, camera, kAspectRatio, kViewportWidth / 2.0F,
                              kViewportHeight / 2.0F, kViewportWidth, kViewportHeight);
  REQUIRE(hit.has_value());
  CHECK(NearlyEqual((*hit)[0], target[0]));
  CHECK(NearlyEqual((*hit)[2], target[2]));
}

TEST_CASE("Pointage : le point retrouvé désigne la case visée", "[render][pick]") {
  // Le tour complet : une case connue, son centre, l'écran, le sol, et de
  // nouveau la case. C'est ce chemin-là que parcourt un clic, et une inversion
  // de rangée ou de colonne s'y verrait.
  const GndMap gnd = FlatMap();
  for (const auto& [cell_x, cell_y] : {std::pair{3, 4}, std::pair{12, 1}, std::pair{15, 18}}) {
    const std::array<float, 3> target = CellCenterPosition(gnd, cell_x, cell_y);
    const Camera camera = Looking(target);

    const auto hit = PickGround(gnd, nullptr, camera, kAspectRatio, kViewportWidth / 2.0F,
                                kViewportHeight / 2.0F, kViewportWidth, kViewportHeight);
    REQUIRE(hit.has_value());
    const std::array<std::int32_t, 2> cell = CellAtPosition(gnd, (*hit)[0], (*hit)[2]);
    CHECK(cell[0] == cell_x);
    CHECK(cell[1] == cell_y);
  }
}

TEST_CASE("Pointage : un clic à côté de la carte ne désigne rien", "[render][pick]") {
  // Hors des bords, GroundHeightAt prolonge la hauteur du cube le plus proche :
  // sans contrôle, un clic sur le ciel rendrait une case au-delà de la carte, et
  // le serveur recevrait une destination qui n'existe pas.
  const GndMap gnd = FlatMap();
  Camera camera = Looking(CellCenterPosition(gnd, 10, 10));
  // Presque à l'horizontale : le haut de l'image regarde alors le ciel.
  camera.pitch_degrees = 2.0F;

  const auto hit = PickGround(gnd, nullptr, camera, kAspectRatio, kViewportWidth / 2.0F, 1.0F,
                              kViewportWidth, kViewportHeight);
  CHECK_FALSE(hit.has_value());
}

TEST_CASE("Cases : la conversion inverse celle du centre", "[render][terrain]") {
  // CellAtPosition doit défaire CellCenterPosition, sans quoi le personnage
  // marcherait vers la case d'à côté — un décalage d'une demi-case, invisible
  // sur une carte plate et flagrant contre un mur.
  const GndMap gnd = FlatMap();
  for (std::int32_t cell_y = 0; cell_y < 20; ++cell_y) {
    for (std::int32_t cell_x = 0; cell_x < 20; ++cell_x) {
      const std::array<float, 3> center = CellCenterPosition(gnd, cell_x, cell_y);
      const std::array<std::int32_t, 2> back = CellAtPosition(gnd, center[0], center[2]);
      CHECK(back[0] == cell_x);
      CHECK(back[1] == cell_y);
    }
  }
}

TEST_CASE("Cases : une cellule couvre la moitié d'un cube", "[render][terrain]") {
  // Deux cellules par cube, c'est la grille de franchissement du serveur. Se
  // tromper d'un facteur deux placerait tout le monde à la moitié de la carte.
  const GndMap gnd = FlatMap();
  const std::array<float, 3> first = CellCenterPosition(gnd, 0, 0);
  const std::array<float, 3> next = CellCenterPosition(gnd, 1, 0);
  CHECK(NearlyEqual(next[0] - first[0], kWorldUnitsPerCube / 2.0F));
}

TEST_CASE("Pointage : le clic vise le sol PRATICABLE, pas le terrain visible", "[render][pick]") {
  // Là où la carte fait marcher au-dessus du terrain — un pont, une arche —,
  // viser le terrain visible désigne la case qui passe DESSOUS, et le
  // personnage part ailleurs.
  //
  // La grille est ici tout entière vingt unités PLUS HAUT que le terrain, ce
  // qui suffit à distinguer les deux : le point touché doit être à cette
  // hauteur-là.
  const GndMap gnd = FlatMap();
  const std::vector<GatCellSpec> cells(
      400, GatCellSpec{.corner_height = {-20.0F, -20.0F, -20.0F, -20.0F}});
  auto gat = GatMap::Parse(BuildGat(20, 20, cells));
  REQUIRE(gat.has_value());

  const std::array<float, 3> target = CellCenterPosition(gnd, 8, 9);
  const Camera camera = Looking({target[0], 20.0F, target[2]});

  const auto visible = PickGround(gnd, nullptr, camera, kAspectRatio, kViewportWidth / 2.0F,
                                  kViewportHeight / 2.0F, kViewportWidth, kViewportHeight);
  REQUIRE(visible.has_value());
  CHECK(NearlyEqual((*visible)[1], 0.0F));

  const auto walkable = PickGround(gnd, &*gat, camera, kAspectRatio, kViewportWidth / 2.0F,
                                   kViewportHeight / 2.0F, kViewportWidth, kViewportHeight);
  REQUIRE(walkable.has_value());
  CHECK(NearlyEqual((*walkable)[1], 20.0F));
  // Et c'est bien la case visée, pas sa voisine.
  const std::array<std::int32_t, 2> cell = CellAtPosition(gnd, (*walkable)[0], (*walkable)[2]);
  CHECK(cell[0] == 8);
  CHECK(cell[1] == 9);
}
