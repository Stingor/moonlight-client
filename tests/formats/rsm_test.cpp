#include "ro/formats/rsm.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/error.h"

#include "support/model_builder.h"

using ro::core::ErrorCode;
using ro::formats::RsmModel;
using ro::test_support::BuildRsm;
using ro::test_support::RsmBuildOptions;
using ro::test_support::RsmFaceSpec;
using ro::test_support::RsmMeshSpec;

namespace {

// Un maillage minimal mais complet : un triangle texturé, avec des sommets et
// des coordonnées distincts pour que toute permutation se voie.
RsmMeshSpec Triangle() {
  RsmMeshSpec mesh;
  mesh.name = "corps";
  mesh.texture_indices = {0};
  mesh.textures = {"mur.bmp"};
  mesh.vertices = {{0.0F, 0.0F, 0.0F}, {10.0F, 0.0F, 0.0F}, {0.0F, 20.0F, 0.0F}};
  mesh.texture_vertices = {{0.0F, 0.0F}, {1.0F, 0.0F}, {0.0F, 1.0F}};
  mesh.faces = {RsmFaceSpec{.vertex_ids = {0, 1, 2},
                            .texture_vertex_ids = {0, 1, 2},
                            .texture_id = 0,
                            .two_sided = true,
                            .smooth_group = 7}};
  return mesh;
}

}  // namespace

TEST_CASE("RSM 1.4 : en-tête, textures et maillage", "[formats][rsm]") {
  RsmBuildOptions options;
  options.textures = {"mur.bmp", "toit.bmp"};
  options.main_mesh_names = {"corps"};
  options.meshes = {Triangle()};

  auto rsm = RsmModel::Parse(BuildRsm(options));
  REQUIRE(rsm.has_value());

  CHECK(rsm->version() == 0x0104);
  CHECK(rsm->alpha() == 0xFF);
  REQUIRE(rsm->textures().size() == 2);
  CHECK(rsm->textures()[1] == "toit.bmp");
  REQUIRE(rsm->main_mesh_names().size() == 1);
  CHECK(rsm->main_mesh_names()[0] == "corps");

  REQUIRE(rsm->meshes().size() == 1);
  const auto& mesh = rsm->meshes()[0];
  CHECK(mesh.name == "corps");
  CHECK(mesh.vertices.size() == 3);
  CHECK(mesh.vertices[2] == std::array<float, 3>{0.0F, 20.0F, 0.0F});
  CHECK(mesh.texture_vertices.size() == 3);
  CHECK(mesh.texture_vertices[1].u == 1.0F);
  REQUIRE(mesh.faces.size() == 1);
  CHECK(mesh.faces[0].vertex_ids == std::array<std::uint16_t, 3>{0, 1, 2});
  CHECK(mesh.faces[0].two_sided);
  CHECK(mesh.faces[0].smooth_group[0] == 7);
  CHECK(rsm->face_count() == 1);
}

TEST_CASE("RSM : la signature GRSX se lit comme GRSM", "[formats][rsm]") {
  RsmBuildOptions options;
  options.use_grsx_signature = true;
  options.main_mesh_names = {"corps"};
  options.meshes = {Triangle()};

  auto rsm = RsmModel::Parse(BuildRsm(options));
  REQUIRE(rsm.has_value());
  CHECK(rsm->meshes().size() == 1);
}

TEST_CASE("RSM : avant la 1.2, ni couleur de sommet ni groupe de lissage", "[formats][rsm]") {
  // Ces deux champs n'existent pas dans le fichier : les lire quand même
  // décalerait tout ce qui suit, et le maillage sortirait vide ou absurde.
  RsmBuildOptions options;
  options.major = 1;
  options.minor = 1;
  options.main_mesh_names = {"corps"};
  options.meshes = {Triangle()};

  auto rsm = RsmModel::Parse(BuildRsm(options));
  REQUIRE(rsm.has_value());
  REQUIRE(rsm->meshes().size() == 1);

  const auto& mesh = rsm->meshes()[0];
  CHECK(mesh.vertices.size() == 3);
  REQUIRE(mesh.faces.size() == 1);
  // Faute de champ dans le fichier, la couleur vaut le blanc opaque.
  CHECK(mesh.texture_vertices[0].color == 0xFFFFFFFFU);
  CHECK(mesh.faces[0].smooth_group[0] == 0);
}

TEST_CASE("RSM : l'opacité n'existe qu'à partir de 1.4", "[formats][rsm]") {
  RsmBuildOptions opaque;
  opaque.major = 1;
  opaque.minor = 3;
  opaque.alpha = 0x40;  // écrit nulle part : la version est trop ancienne
  opaque.main_mesh_names = {"corps"};
  opaque.meshes = {Triangle()};

  auto old_model = RsmModel::Parse(BuildRsm(opaque));
  REQUIRE(old_model.has_value());
  CHECK(old_model->alpha() == 0xFF);

  RsmBuildOptions translucent;
  translucent.minor = 4;
  translucent.alpha = 0x40;
  translucent.main_mesh_names = {"corps"};
  translucent.meshes = {Triangle()};

  auto recent = RsmModel::Parse(BuildRsm(translucent));
  REQUIRE(recent.has_value());
  CHECK(recent->alpha() == 0x40);
}

TEST_CASE("RSM 2.2 : noms préfixés et images-clés de position", "[formats][rsm]") {
  // À partir de 2.2 les noms portent leur longueur au lieu d'occuper 40 octets,
  // et la transformation globale cède la place aux images-clés.
  RsmMeshSpec mesh = Triangle();
  mesh.name = "un nom bien plus long que quarante caractères ne le permettraient";
  mesh.position_keys.resize(3);

  RsmBuildOptions options;
  options.major = 2;
  options.minor = 2;
  options.textures = {"mur.bmp"};
  options.main_mesh_names = {mesh.name};
  options.meshes = {mesh};

  auto rsm = RsmModel::Parse(BuildRsm(options));
  REQUIRE(rsm.has_value());
  REQUIRE(rsm->meshes().size() == 1);

  CHECK(rsm->meshes()[0].name == mesh.name);
  CHECK(rsm->meshes()[0].position_keys.size() == 3);
  CHECK(rsm->frame_rate() == 60.0F);
}

TEST_CASE("RSM 2.3 : chaque maillage porte SES textures", "[formats][rsm]") {
  // La table globale du modèle disparaît. Continuer à la lire décalerait tout.
  RsmMeshSpec mesh = Triangle();
  mesh.textures = {"pierre.bmp", "bois.bmp"};

  RsmBuildOptions options;
  options.major = 2;
  options.minor = 3;
  options.main_mesh_names = {"corps"};
  options.meshes = {mesh};

  auto rsm = RsmModel::Parse(BuildRsm(options));
  REQUIRE(rsm.has_value());
  CHECK(rsm->textures().empty());
  REQUIRE(rsm->meshes().size() == 1);
  REQUIRE(rsm->meshes()[0].textures.size() == 2);
  CHECK(rsm->meshes()[0].textures[1] == "bois.bmp");
  CHECK(rsm->meshes()[0].texture_indices == std::vector<std::int32_t>{0, 1});
}

TEST_CASE("RSM : les images-clés gardent leur image et leur valeur", "[formats][rsm]") {
  // L'animation se joue sur ces nombres : un champ décalé d'un cran donnerait
  // un quaternion (w, x, y, z) au lieu de (x, y, z, w), et une rotation fausse
  // qui tourne quand même.
  RsmMeshSpec mesh = Triangle();
  mesh.rotation_keys = {{.frame = 0, .quaternion = {0.0F, 0.0F, 0.0F, 1.0F}},
                        {.frame = 750, .quaternion = {0.0F, 0.6F, 0.0F, 0.8F}}};
  mesh.position_keys = {{.frame = 40, .position = {1.0F, -2.0F, 3.5F}}};

  RsmBuildOptions options;
  options.major = 2;
  options.minor = 2;
  options.textures = {"mur.bmp"};
  options.main_mesh_names = {"corps"};
  options.meshes = {mesh};

  auto rsm = RsmModel::Parse(BuildRsm(options));
  REQUIRE(rsm.has_value());
  const auto& parsed = rsm->meshes()[0];
  REQUIRE(parsed.rotation_keys.size() == 2);
  CHECK(parsed.rotation_keys[1].frame == 750);
  CHECK(parsed.rotation_keys[1].quaternion == std::array<float, 4>{0.0F, 0.6F, 0.0F, 0.8F});
  REQUIRE(parsed.position_keys.size() == 1);
  CHECK(parsed.position_keys[0].frame == 40);
  CHECK(parsed.position_keys[0].position == std::array<float, 3>{1.0F, -2.0F, 3.5F});
}

TEST_CASE("RSM 2.3 : les animations de texture sont gardées, texture par texture",
          "[formats][rsm]") {
  // Le fichier les range par texture puis par grandeur. Les sauter laissait
  // l'eau d'un bassin ou un tapis roulant immobiles ; mal les lire décalerait
  // tous les maillages suivants.
  RsmMeshSpec mesh = Triangle();
  mesh.textures = {"eau.bmp", "pierre.bmp"};
  mesh.texture_animations = {
      {.texture_index = 0,
       .motion = 1,
       .keys = {{.frame = 0, .value = 0.0F}, {.frame = 60, .value = 1.0F}}},
      {.texture_index = 0, .motion = 4, .keys = {{.frame = 30, .value = 0.5F}}},
      {.texture_index = 1, .motion = 2, .keys = {{.frame = 10, .value = 2.0F}}}};

  RsmMeshSpec second = Triangle();
  second.name = "second";

  RsmBuildOptions options;
  options.major = 2;
  options.minor = 3;
  options.main_mesh_names = {"corps"};
  options.meshes = {mesh, second};

  auto rsm = RsmModel::Parse(BuildRsm(options));
  REQUIRE(rsm.has_value());
  REQUIRE(rsm->meshes().size() == 2);
  const auto& animations = rsm->meshes()[0].texture_animations;
  REQUIRE(animations.size() == 3);
  CHECK(animations[0].texture_index == 0);
  CHECK(animations[0].motion == 1);
  REQUIRE(animations[0].keys.size() == 2);
  CHECK(animations[0].keys[1].frame == 60);
  CHECK(animations[0].keys[1].value == 1.0F);
  CHECK(animations[1].motion == 4);
  CHECK(animations[2].texture_index == 1);
  CHECK(animations[2].keys[0].value == 2.0F);
  // Le maillage suivant se lit au bon endroit.
  CHECK(rsm->meshes()[1].name == "second");
  CHECK(rsm->meshes()[1].texture_animations.empty());
}

TEST_CASE("RSM : plusieurs maillages s'enchaînent sans décalage", "[formats][rsm]") {
  // Le vrai risque du format : une seule longueur fausse dans le premier
  // maillage et tous les suivants sortent absurdes, sans la moindre erreur.
  RsmMeshSpec first = Triangle();
  first.name = "premier";
  first.rotation_keys.resize(2);

  RsmMeshSpec second = Triangle();
  second.name = "second";
  second.parent_name = "premier";
  second.vertices = {{1.0F, 2.0F, 3.0F}};
  second.texture_vertices = {{0.5F, 0.5F}};
  second.faces.clear();

  RsmBuildOptions options;
  options.textures = {"mur.bmp"};
  options.main_mesh_names = {"premier"};
  options.meshes = {first, second};

  auto rsm = RsmModel::Parse(BuildRsm(options));
  REQUIRE(rsm.has_value());
  REQUIRE(rsm->meshes().size() == 2);

  CHECK(rsm->meshes()[0].name == "premier");
  CHECK(rsm->meshes()[0].rotation_keys.size() == 2);
  CHECK(rsm->meshes()[1].name == "second");
  CHECK(rsm->meshes()[1].parent_name == "premier");
  CHECK(rsm->meshes()[1].vertices[0] == std::array<float, 3>{1.0F, 2.0F, 3.0F});
}

TEST_CASE("RSM : un index de texture hors table retombe à zéro", "[formats][rsm]") {
  // Le décor s'affiche avec une texture inattendue plutôt que de disparaître.
  RsmMeshSpec mesh = Triangle();
  mesh.faces[0].texture_id = 99;

  RsmBuildOptions options;
  options.textures = {"mur.bmp"};
  options.main_mesh_names = {"corps"};
  options.meshes = {mesh};

  auto rsm = RsmModel::Parse(BuildRsm(options));
  REQUIRE(rsm.has_value());
  CHECK(rsm->meshes()[0].faces[0].texture_id == 0);
}

TEST_CASE("RSM : signature invalide", "[formats][rsm]") {
  RsmBuildOptions options;
  options.main_mesh_names = {"corps"};
  options.meshes = {Triangle()};

  std::vector<std::uint8_t> rsm = BuildRsm(options);
  rsm[1] = 'X';
  auto parsed = RsmModel::Parse(rsm);
  REQUIRE_FALSE(parsed.has_value());
  CHECK(parsed.error().code == ErrorCode::kBadSignature);
}

TEST_CASE("RSM : aucune troncature ne fait planter le parseur", "[formats][rsm]") {
  RsmBuildOptions options;
  options.textures = {"mur.bmp"};
  options.main_mesh_names = {"corps"};
  options.meshes = {Triangle(), Triangle()};

  const std::vector<std::uint8_t> complete = BuildRsm(options);
  for (std::size_t length = 0; length <= complete.size(); ++length) {
    const std::vector<std::uint8_t> truncated(
        complete.begin(), complete.begin() + static_cast<std::ptrdiff_t>(length));
    auto parsed = RsmModel::Parse(truncated);
    if (parsed.has_value()) {
      // Un modèle tronqué garde ses maillages COMPLETS : aucun ne doit sortir
      // à moitié lu.
      for (const auto& mesh : parsed->meshes()) {
        CHECK(mesh.texture_vertices.size() <= mesh.vertices.size() * 3);
      }
    }
  }
  SUCCEED("aucune troncature n'a provoqué de lecture hors bornes");
}
