// LE DÉCOR D'UNE SCÈNE SE REPOSE SELON SON ÉDITION, sans relire la carte.

#include <array>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gnd.h"
#include "ro/formats/rsw.h"
#include "ro/mapedit/map_edit.h"
#include "ro/render/map_scene.h"

#include "support/map_builder.h"

using ro::mapedit::MapEdit;
using ro::mapedit::ModelEdit;

TEST_CASE("Édition : le décor se repose sans relire la carte", "[render][mapedit]") {
  // Une carte de dix cubes de côté, dont le centre est l'origine du RSW.
  ro::test_support::GndBuildOptions ground;
  ground.width = 10;
  ground.height = 10;
  auto gnd = ro::formats::GndMap::Parse(ro::test_support::BuildGnd(ground));
  REQUIRE(gnd.has_value());
  ro::test_support::RswBuildOptions options;
  // Trois poses dans la carte, qui s'étend à ±50 du centre.
  options.models = {
      {.name = "pont", .model_name = "decor\\pont.rsm", .position = {0.0F, 0.0F, 0.0F}},
      {.name = "arbre", .model_name = "decor\\arbre.rsm", .position = {10.0F, 0.0F, 10.0F}},
      {.name = "maison", .model_name = "decor\\maison.rsm", .position = {-10.0F, 0.0F, -10.0F}},
  };
  auto rsw = ro::formats::RswScene::Parse(ro::test_support::BuildRsw(options));
  REQUIRE(rsw.has_value());
  ro::render::MapScene scene{.gnd = std::move(*gnd), .rsw = std::move(*rsw)};
  scene.unfixed_models = scene.rsw.models();
  // Deux fichiers chargés ; la maison ne s'est pas lue.
  scene.models.resize(2);
  scene.models[0].rsm_file = "decor\\pont.rsm";
  scene.models[1].rsm_file = "decor\\arbre.rsm";

  CHECK(ro::render::ReplaceSceneModels(scene, MapEdit()).applied == 0);
  CHECK(scene.models[0].rsw_ranks == std::vector<std::size_t>{0});
  CHECK(scene.models[1].rsw_ranks == std::vector<std::size_t>{1});
  CHECK(scene.unreadable_models == 1);
  CHECK(scene.model_instance_count == 2);
  const glm::mat4 bridge_before = scene.models[0].transforms[0];

  // L'arbre se masque, le pont se déplace.
  ModelEdit moved{.model_index = 0, .model_file = "decor\\pont.rsm", .origin = {}};
  moved.position = std::array<float, 3>{5.0F, 0.0F, 0.0F};
  moved.why = "déplacé";
  ModelEdit gone{
      .model_index = 1, .model_file = "decor\\arbre.rsm", .origin = {10.0F, 0.0F, 10.0F}};
  gone.hidden = true;
  gone.why = "masqué";
  MapEdit edit;
  edit.SetModel(moved);
  edit.SetModel(gone);
  const ro::mapedit::MapEditReport report = ro::render::ReplaceSceneModels(scene, edit);
  CHECK(report.applied == 2);
  CHECK(scene.hidden_models == std::vector<std::size_t>{1});
  CHECK(scene.models[1].transforms.empty());
  REQUIRE(scene.models[0].transforms.size() == 1);
  CHECK(scene.models[0].transforms[0][3].x - bridge_before[3].x == 5.0F);

  // Sans édition, tout revient : le placement d'origine a été gardé.
  CHECK(ro::render::ReplaceSceneModels(scene, MapEdit()).applied == 0);
  CHECK(scene.models[0].transforms[0] == bridge_before);
  CHECK(scene.models[1].rsw_ranks == std::vector<std::size_t>{1});

  // UNE POSE DÉPLACÉE GARDE LA RETOUCHE DE SON MAILLAGE : elle se reconnaît à
  // son placement d'origine, pas à celui que l'édition lui donne.
  ro::mapedit::MeshEdit mesh{.model_index = 0, .model_file = "decor\\pont.rsm", .origin = {}};
  mesh.SetVertex(ro::mapedit::VertexMove{
      .node = "planche", .vertex = 0, .origin = {}, .position = {0.0F, 1.0F, 0.0F}});
  mesh.why = "la planche s'enfonce";
  edit.SetMesh(mesh);
  const ro::mapedit::MapEditReport with_mesh = ro::render::ReplaceSceneModels(scene, edit);
  CHECK(with_mesh.refused.empty());
  CHECK(with_mesh.applied == 3);
  CHECK(scene.mesh_edits.size() == 1);
}

TEST_CASE("Édition : le relief se repose sur les hauteurs du fichier", "[render][mapedit]") {
  ro::test_support::GndBuildOptions ground;
  ground.width = 2;
  ground.height = 2;
  auto gnd = ro::formats::GndMap::Parse(ro::test_support::BuildGnd(ground));
  auto rsw = ro::formats::RswScene::Parse(ro::test_support::BuildRsw({}));
  REQUIRE(gnd.has_value());
  REQUIRE(rsw.has_value());
  ro::render::MapScene scene{.gnd = std::move(*gnd), .rsw = std::move(*rsw)};
  for (const ro::formats::GndCube& cube : scene.gnd.cubes()) {
    scene.file_corner_heights.push_back(cube.corner_height);
  }
  const std::vector<ro::mapedit::GroundEdit> lifted = {
      {.corner_x = 1, .corner_y = 1, .lift = 2.0F}};
  CHECK(ro::render::ReshapeSceneGround(scene, lifted).applied == 1);
  CHECK(scene.gnd.at(0, 0).corner_height[3] == -2.0F);
  // Reposer ne cumule pas : les hauteurs du fichier reviennent d'abord.
  CHECK(ro::render::ReshapeSceneGround(scene, lifted).applied == 1);
  CHECK(scene.gnd.at(0, 0).corner_height[3] == -2.0F);
  CHECK(scene.ground_edits == lifted);
  CHECK(ro::render::ReshapeSceneGround(scene, {}).applied == 0);
  CHECK(scene.gnd.at(0, 0).corner_height[3] == 0.0F);
  // Sur un relief aplati, aucune retouche ne se pose.
  scene.relief_level = 0.0F;
  CHECK(ro::render::ReshapeSceneGround(scene, lifted).applied == 0);
  CHECK(scene.ground_edits.empty());
  CHECK(scene.gnd.at(0, 0).corner_height[3] == 0.0F);
}
