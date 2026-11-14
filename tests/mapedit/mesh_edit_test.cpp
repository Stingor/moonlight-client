// LA RETOUCHE DU MAILLAGE D'UN MODÈLE : des sommets déplacés, vérifiés un à un.

#include <array>
#include <string>
#include <string_view>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/rsm.h"
#include "ro/formats/rsw.h"
#include "ro/mapedit/map_edit.h"

#include "support/map_builder.h"
#include "support/model_builder.h"

using ro::mapedit::MapEdit;
using ro::mapedit::MeshEdit;
using ro::mapedit::VertexMove;

namespace {

std::string SameName(std::string_view raw) { return std::string(raw); }
constexpr ro::mapedit::FileNameCodec kSameNames{.to_utf8 = &SameName, .to_raw = &SameName};

// Un bloc d'un seul nœud, « Box04 », à trois sommets.
ro::formats::RsmModel Block() {
  ro::test_support::RsmBuildOptions options;
  ro::test_support::RsmMeshSpec mesh;
  mesh.name = "Box04";
  mesh.vertices = {{0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F}};
  mesh.texture_vertices = {{0.0F, 0.0F}};
  mesh.faces = {ro::test_support::RsmFaceSpec{.vertex_ids = {0, 1, 2}}};
  options.meshes = {mesh};
  options.main_mesh_names = {"Box04"};
  auto model = ro::formats::RsmModel::Parse(ro::test_support::BuildRsm(options));
  REQUIRE(model.has_value());
  return std::move(*model);
}

VertexMove Lowered(std::size_t vertex, const std::array<float, 3>& origin) {
  return VertexMove{.node = "Box04",
                    .vertex = vertex,
                    .origin = origin,
                    .position = {origin[0], origin[1] - 0.5F, origin[2]}};
}

}  // namespace

TEST_CASE("Maillage : un sommet désigné se déplace, les autres restent", "[mapedit]") {
  ro::formats::RsmModel block = Block();
  MeshEdit edit{.model_index = 27, .model_file = "falaise.rsm"};
  edit.SetVertex(Lowered(2, {0.0F, 1.0F, 0.0F}));
  edit.why = "le dessus coupe les planches";
  const ro::mapedit::MapEditReport report = ro::mapedit::ApplyMeshEdit(edit, block, kSameNames);
  CHECK(report.applied == 1);
  CHECK(report.refused.empty());
  CHECK(block.meshes()[0].vertices[2] == std::array<float, 3>{0.0F, 0.5F, 0.0F});
  CHECK(block.meshes()[0].vertices[1] == std::array<float, 3>{1.0F, 0.0F, 0.0F});
}

TEST_CASE("Maillage : un sommet qui ne correspond plus ne bouge pas", "[mapedit]") {
  ro::formats::RsmModel block = Block();
  MeshEdit edit{.model_index = 27, .model_file = "falaise.rsm"};
  // Un nœud absent, un rang hors du nœud, une position d'origine fausse.
  edit.SetVertex(VertexMove{.node = "Box05", .vertex = 0});
  edit.SetVertex(Lowered(9, {0.0F, 0.0F, 0.0F}));
  edit.SetVertex(Lowered(1, {5.0F, 0.0F, 0.0F}));
  const ro::mapedit::MapEditReport report = ro::mapedit::ApplyMeshEdit(edit, block, kSameNames);
  CHECK(report.applied == 0);
  CHECK(report.refused.size() == 3);
  CHECK(block.meshes()[0].vertices[1] == std::array<float, 3>{1.0F, 0.0F, 0.0F});
}

TEST_CASE("Maillage : la cible se vérifie comme une retouche de modèle", "[mapedit]") {
  ro::test_support::RswBuildOptions options;
  options.models = {
      {.name = "falaise", .model_name = "falaise.rsm", .position = {1.0F, 2.0F, 3.0F}}};
  auto rsw = ro::formats::RswScene::Parse(ro::test_support::BuildRsw(options));
  REQUIRE(rsw.has_value());
  MeshEdit edit{.model_index = 0, .model_file = "falaise.rsm", .origin = {1.0F, 2.0F, 3.0F}};
  CHECK(ro::mapedit::MeshTargetRefusal(edit, *rsw, kSameNames).empty());
  edit.model_file = "autre.rsm";
  CHECK_FALSE(ro::mapedit::MeshTargetRefusal(edit, *rsw, kSameNames).empty());
  edit.model_file = "falaise.rsm";
  edit.model_index = 4;
  CHECK_FALSE(ro::mapedit::MeshTargetRefusal(edit, *rsw, kSameNames).empty());
}

TEST_CASE("Maillage : le document écrit se relit à l'identique", "[mapedit]") {
  MapEdit edit;
  MeshEdit mesh{.model_index = 27,
                .model_file = "대만\\대만_절벽1.rsm",
                .origin = {62.172F, -8.0F, -468.02F}};
  mesh.all_poses = true;
  mesh.SetVertex(Lowered(3, {20.0F, 0.0F, 20.0F}));
  mesh.SetVertex(Lowered(1, {-20.0F, 0.0F, 20.0F}));
  mesh.why = "le dessus coupe les planches du pont";
  edit.SetMesh(mesh);
  REQUIRE(edit.MeshAt(27) != nullptr);
  // Rangés par rang de sommet, quel que soit l'ordre de pose.
  CHECK(edit.MeshAt(27)->vertices[0].vertex == 1);
  auto read_back =
      ro::mapedit::ParseMapEdit(ro::mapedit::MapEditToYaml(edit, "gonryun"), "gonryun");
  REQUIRE(read_back.has_value());
  CHECK(*read_back == edit);
  // Un maillage sans sommets dits est refusé.
  CHECK_FALSE(ro::mapedit::ParseMapEdit("version: 1\nmap: gonryun\nmeshes:\n  - index: 1\n"
                                        "    file: 'a.rsm'\n    origin: [0, 0, 0]\n"
                                        "    why: \"x\"\n",
                                        "gonryun")
                  .has_value());
}

TEST_CASE("Maillage : un sommet reposé remplace l'ancien, un retiré disparaît", "[mapedit]") {
  MeshEdit mesh;
  mesh.SetVertex(Lowered(2, {0.0F, 1.0F, 0.0F}));
  VertexMove again = Lowered(2, {0.0F, 1.0F, 0.0F});
  again.position[1] = -3.0F;
  mesh.SetVertex(again);
  REQUIRE(mesh.vertices.size() == 1);
  CHECK(mesh.VertexAt("Box04", 2)->position[1] == -3.0F);
  mesh.RemoveVertex("Box04", 2);
  CHECK(mesh.vertices.empty());
  MapEdit edit;
  edit.SetMesh(MeshEdit{.model_index = 5});
  edit.RemoveMesh(5);
  CHECK(edit.empty());
}
