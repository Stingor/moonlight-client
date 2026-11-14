#include "ro/render/model_mesh.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "ro/formats/rsm.h"

#include "support/model_builder.h"

using ro::formats::RsmModel;
using ro::formats::RswModel;
using ro::render::BuildModelMesh;
using ro::render::ModelInstanceMatrix;
using ro::render::ModelMesh;
using ro::render::ModelVertex;
using ro::render::TransformedNormal;
using ro::test_support::BuildRsm;
using ro::test_support::RsmBuildOptions;
using ro::test_support::RsmFaceSpec;
using ro::test_support::RsmMeshSpec;

namespace {

bool NearlyEqual(float actual, float expected) { return std::abs(actual - expected) < 1e-4F; }

// Un triangle unitaire, sommets distincts pour que toute permutation se voie.
RsmMeshSpec Triangle(const std::string& name) {
  RsmMeshSpec mesh;
  mesh.name = name;
  mesh.texture_indices = {0};
  mesh.textures = {"mur.bmp"};
  mesh.vertices = {{0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F}};
  mesh.texture_vertices = {{0.0F, 0.0F}, {1.0F, 0.0F}, {0.0F, 1.0F}};
  mesh.faces = {RsmFaceSpec{.vertex_ids = {0, 1, 2}, .texture_vertex_ids = {0, 1, 2}}};
  return mesh;
}

ModelMesh MeshOf(const RsmBuildOptions& options) {
  auto model = RsmModel::Parse(BuildRsm(options));
  REQUIRE(model.has_value());
  return BuildModelMesh(*model);
}

}  // namespace

TEST_CASE("Modèle : un triangle donne trois sommets", "[render][model]") {
  RsmBuildOptions options;
  options.textures = {"mur.bmp"};
  options.main_mesh_names = {"corps"};
  options.meshes = {Triangle("corps")};

  const ModelMesh mesh = MeshOf(options);
  REQUIRE(mesh.vertices.size() == 3);
  REQUIRE(mesh.surfaces.size() == 1);
  CHECK(mesh.surfaces[0].vertex_count == 3);
  REQUIRE(mesh.textures.size() == 1);
  CHECK(mesh.textures[0] == "mur.bmp");
}

TEST_CASE("Modèle : la position globale du nœud décale ses sommets", "[render][model]") {
  RsmMeshSpec mesh = Triangle("corps");
  mesh.global_position = {100.0F, 200.0F, 300.0F};

  RsmBuildOptions options;
  options.textures = {"mur.bmp"};
  options.main_mesh_names = {"corps"};
  options.meshes = {mesh};

  const ModelMesh built = MeshOf(options);
  REQUIRE_FALSE(built.vertices.empty());
  CHECK(NearlyEqual(built.vertices[0].position[0], 100.0F));
  CHECK(NearlyEqual(built.vertices[0].position[1], 200.0F));
  CHECK(NearlyEqual(built.vertices[0].position[2], 300.0F));
}

TEST_CASE("Modèle : un enfant hérite de la transformation de son parent", "[render][model]") {
  // Le cœur du format : l'arbre de nœuds. Un enfant dont on oublie d'hériter la
  // transformation reste à l'origine, et le décor se retrouve empilé au même
  // endroit sans qu'aucun compte de sommets ne change.
  RsmMeshSpec parent = Triangle("parent");
  parent.global_position = {1000.0F, 0.0F, 0.0F};

  RsmMeshSpec child = Triangle("enfant");
  child.parent_name = "parent";
  child.global_position = {7.0F, 0.0F, 0.0F};

  RsmBuildOptions options;
  options.textures = {"mur.bmp"};
  options.main_mesh_names = {"parent"};
  options.meshes = {parent, child};

  const ModelMesh built = MeshOf(options);
  REQUIRE(built.vertices.size() == 6);

  // L'emprise doit atteindre 1007, somme des deux décalages.
  CHECK(NearlyEqual(built.bounds_max[0], 1008.0F));
  CHECK(NearlyEqual(built.bounds_min[0], 1000.0F));
}

TEST_CASE("Modèle : l'échelle du nœud s'applique", "[render][model]") {
  RsmMeshSpec mesh = Triangle("corps");
  mesh.global_scale = {3.0F, 1.0F, 1.0F};

  RsmBuildOptions options;
  options.textures = {"mur.bmp"};
  options.main_mesh_names = {"corps"};
  options.meshes = {mesh};

  const ModelMesh built = MeshOf(options);
  CHECK(NearlyEqual(built.bounds_max[0], 3.0F));
}

TEST_CASE("Modèle 2.2 et au-delà : la hiérarchie ne se compose pas deux fois", "[render][model]") {
  // À partir de 2.2, chaque nœud se pose par SA matrice du fichier : la
  // hiérarchie ne fait que s'y ramener. Sans décalage ni rotation, les deux
  // triangles restent donc l'un sur l'autre — composer à la manière de la
  // première famille les éparpillerait.
  RsmMeshSpec parent = Triangle("parent");
  RsmMeshSpec child = Triangle("enfant");
  child.parent_name = "parent";

  RsmBuildOptions options;
  options.major = 2;
  options.minor = 2;
  options.textures = {"mur.bmp"};
  options.main_mesh_names = {"parent"};
  options.meshes = {parent, child};

  const ModelMesh built = MeshOf(options);
  REQUIRE(built.vertices.size() == 6);
  // Les deux triangles se superposent exactement : aucune transformation.
  CHECK(NearlyEqual(built.bounds_max[0], 1.0F));
  CHECK(NearlyEqual(built.bounds_min[0], 0.0F));
}

TEST_CASE("Modèle : un enfant s'accroche au PREMIER homonyme de son parent", "[render][model]") {
  // Les modèles convertis de lasagna nomment tous leurs maillages comme le
  // modèle, et chacun se dit enfant de ce nom. Le client rend le premier
  // homonyme du fichier : le second maillage est donc l'enfant du premier, et
  // non une racine — sans quoi les façades d'une maison partent ailleurs.
  RsmMeshSpec first = Triangle("maison");
  first.global_position = {1000.0F, 0.0F, 0.0F};

  RsmMeshSpec second = Triangle("maison");
  second.parent_name = "maison";
  second.global_position = {7.0F, 0.0F, 0.0F};

  RsmBuildOptions options;
  options.textures = {"mur.bmp"};
  options.main_mesh_names = {"maison"};
  options.meshes = {first, second};

  const ModelMesh built = MeshOf(options);
  REQUIRE(built.vertices.size() == 6);
  CHECK(NearlyEqual(built.bounds_max[0], 1008.0F));
  CHECK(NearlyEqual(built.bounds_min[0], 1000.0F));
}

TEST_CASE("Modèle : un nœud orphelin n'est pas perdu", "[render][model]") {
  // Son parent n'existe pas dans le fichier. Le laisser de côté ferait
  // disparaître une partie du décor sans la moindre erreur.
  RsmMeshSpec orphan = Triangle("orphelin");
  orphan.parent_name = "un parent qui n'existe pas";

  RsmBuildOptions options;
  options.textures = {"mur.bmp"};
  options.main_mesh_names = {"orphelin"};
  options.meshes = {orphan};

  const ModelMesh built = MeshOf(options);
  CHECK(built.vertices.size() == 3);
}

TEST_CASE("Modèle : une face aux index invalides est ignorée", "[render][model]") {
  RsmMeshSpec mesh = Triangle("corps");
  mesh.faces.push_back(RsmFaceSpec{.vertex_ids = {0, 1, 99}, .texture_vertex_ids = {0, 1, 2}});

  RsmBuildOptions options;
  options.textures = {"mur.bmp"};
  options.main_mesh_names = {"corps"};
  options.meshes = {mesh};

  const ModelMesh built = MeshOf(options);
  // La face valide seule subsiste.
  CHECK(built.vertices.size() == 3);
}

TEST_CASE("Placement : la position est relative au CENTRE de la carte", "[render][model]") {
  // Un objet à l'origine du RSW se pose au milieu du terrain, pas à son coin.
  // L'oublier décale tout le décor d'un demi-plateau.
  RswModel placement;
  placement.position = {0.0F, 0.0F, 0.0F};

  ModelMesh mesh;
  mesh.bounds_min = {0.0F, 0.0F, 0.0F};
  mesh.bounds_max = {0.0F, 0.0F, 0.0F};

  const glm::mat4 matrix = ModelInstanceMatrix(placement, 100, 200, mesh, 0x0104);
  const glm::vec4 origin = matrix * glm::vec4{0.0F, 0.0F, 0.0F, 1.0F};

  // 5 × 100 en X ; en Z, le demi-plateau et le retrait, puis l'inversion d'axe.
  CHECK(NearlyEqual(origin.x, 500.0F));
  CHECK(NearlyEqual(origin.z, 1010.0F));
}

TEST_CASE("Placement : l'axe Y est inversé comme pour le terrain", "[render][model]") {
  RswModel placement;
  placement.position = {0.0F, 40.0F, 0.0F};

  ModelMesh mesh;
  const glm::mat4 matrix = ModelInstanceMatrix(placement, 0, 0, mesh, 0x0104);
  const glm::vec4 origin = matrix * glm::vec4{0.0F, 0.0F, 0.0F, 1.0F};

  CHECK(NearlyEqual(origin.y, -40.0F));
}

TEST_CASE("Placement : le modèle repose PAR SA BASE sur le sol", "[render][model]") {
  // Le décalage vertical s'applique avant l'inversion de l'axe Y : prendre la
  // mauvaise borne de l'emprise soulève chaque bâtiment de toute sa hauteur.
  // Aucune assertion de taille ni de position horizontale ne le détecte.
  //
  // Modèle dont la base est à y = 0 et le toit à y = -100 dans le fichier.
  ModelMesh mesh;
  mesh.bounds_min = {-5.0F, -100.0F, -5.0F};
  mesh.bounds_max = {5.0F, 0.0F, 5.0F};

  RswModel placement;
  const glm::mat4 matrix = ModelInstanceMatrix(placement, 0, 0, mesh, 0x0104);

  // Les deux extrémités verticales du modèle, une fois posées.
  const float base = (matrix * glm::vec4{0.0F, mesh.bounds_max[1], 0.0F, 1.0F}).y;
  const float roof = (matrix * glm::vec4{0.0F, mesh.bounds_min[1], 0.0F, 1.0F}).y;

  CHECK(NearlyEqual(base, 0.0F));
  // Le toit est AU-DESSUS de la base : le modèle n'est ni enterré ni retourné.
  CHECK(roof > base);
  CHECK(NearlyEqual(roof, 100.0F));
}

TEST_CASE("Placement : l'échelle du RSW s'applique", "[render][model]") {
  RswModel placement;
  placement.scale = {2.0F, 1.0F, 1.0F};

  ModelMesh mesh;
  const glm::mat4 matrix = ModelInstanceMatrix(placement, 0, 0, mesh, 0x0104);
  const glm::vec4 moved = matrix * glm::vec4{1.0F, 0.0F, 0.0F, 1.0F};
  const glm::vec4 origin = matrix * glm::vec4{0.0F, 0.0F, 0.0F, 1.0F};

  CHECK(NearlyEqual(moved.x - origin.x, 2.0F));
}

TEST_CASE("Placement : les instances hors carte sont écartées", "[render][model]") {
  // Les fichiers contiennent réellement des objets posés hors du terrain. Ne
  // pas les écarter les laisse flotter dans le vide autour de la carte, ce que
  // seul un rendu révèle.
  ModelMesh mesh;

  RswModel inside;
  inside.position = {0.0F, 0.0F, 0.0F};
  const glm::mat4 centered = ModelInstanceMatrix(inside, 100, 100, mesh, 0x0104);
  CHECK(ro::render::IsInstanceOnMap(centered, 100, 100));

  RswModel far_away;
  far_away.position = {100000.0F, 0.0F, 0.0F};
  const glm::mat4 outside = ModelInstanceMatrix(far_away, 100, 100, mesh, 0x0104);
  CHECK_FALSE(ro::render::IsInstanceOnMap(outside, 100, 100));

  RswModel behind;
  behind.position = {-100000.0F, 0.0F, 0.0F};
  const glm::mat4 negative = ModelInstanceMatrix(behind, 100, 100, mesh, 0x0104);
  CHECK_FALSE(ro::render::IsInstanceOnMap(negative, 100, 100));
}

TEST_CASE("Placement : une rotation autour de Y fait pivoter l'instance", "[render][model]") {
  RswModel placement;
  placement.rotation = {0.0F, 90.0F, 0.0F};

  ModelMesh mesh;
  const glm::mat4 matrix = ModelInstanceMatrix(placement, 0, 0, mesh, 0x0104);
  const glm::vec4 origin = matrix * glm::vec4{0.0F, 0.0F, 0.0F, 1.0F};
  const glm::vec4 moved = matrix * glm::vec4{1.0F, 0.0F, 0.0F, 1.0F};

  // Un quart de tour autour de Y : ce qui pointait vers X ne pointe plus vers X.
  CHECK(std::abs(moved.x - origin.x) < 1e-4F);
  CHECK(std::abs(moved.z - origin.z) > 0.5F);
}

TEST_CASE("Modèle : le décalage local s'applique APRÈS la matrice du nœud", "[render][model]") {
  // L'ordre décide de tout. La matrice 3 × 3 du nœud porte l'orientation et,
  // très souvent, une échelle. Appliquer le décalage AVANT elle le fait grandir
  // et tourner avec le maillage : le nœud part d'autant plus loin que son
  // échelle est grande. C'est ce qui suspendait le feuillage des palmiers de
  // gonryun loin au-dessus de leur tronc.
  //
  // Ici, la matrice double tout, et le décalage vaut dix. Le sommet posé à
  // l'origine doit finir à dix, pas à vingt.
  RsmBuildOptions options;
  options.textures = {"mur.bmp"};
  options.main_mesh_names = {"corps"};
  RsmMeshSpec mesh = Triangle("corps");
  mesh.transform = {2.0F, 0.0F, 0.0F, 0.0F, 2.0F, 0.0F, 0.0F, 0.0F, 2.0F};
  mesh.local_position = {10.0F, 0.0F, 0.0F};
  options.meshes = {mesh};

  const ModelMesh built = MeshOf(options);
  REQUIRE(built.vertices.size() == 3);
  CHECK(NearlyEqual(built.vertices[0].position[0], 10.0F));
  // Le deuxième sommet est à une unité sur x : la matrice le porte à deux, et
  // le décalage l'emmène à douze. C'est ce qui distingue les deux ordres, car
  // l'autre le placerait à vingt-deux.
  CHECK(NearlyEqual(built.vertices[1].position[0], 12.0F));
}

namespace {

using ro::test_support::RsmRotationKeySpec;

// Un quart de tour autour de Y, en quaternion (x, y, z, w).
constexpr float kHalfSqrt2 = 0.70710678F;
constexpr std::array<float, 4> kQuarterTurnAroundY{0.0F, kHalfSqrt2, 0.0F, kHalfSqrt2};
constexpr std::array<float, 4> kNoTurn{0.0F, 0.0F, 0.0F, 1.0F};

// Un nœud posé à cent unités, tourné d'un quart de tour au repos, et qui porte
// des images-clés partant de l'absence de rotation.
RsmBuildOptions TurningHub() {
  RsmMeshSpec hub = Triangle("moyeu");
  hub.global_position = {100.0F, 0.0F, 0.0F};
  hub.global_rotation_angle = 1.5707964F;
  hub.global_rotation_axis = {0.0F, 1.0F, 0.0F};
  hub.rotation_keys = {RsmRotationKeySpec{.frame = 0, .quaternion = kNoTurn},
                       RsmRotationKeySpec{.frame = 1000, .quaternion = kQuarterTurnAroundY}};
  RsmBuildOptions options;
  options.animation_length = 1000;
  options.textures = {"mur.bmp"};
  options.main_mesh_names = {"moyeu"};
  options.meshes = {hub};
  return options;
}

}  // namespace

TEST_CASE("Modèle : figé, un nœud prend sa pose de l'IMAGE ZÉRO", "[render][model]") {
  // Le natif pose tout objet à l'image zéro de ses images-clés, et n'emploie
  // l'angle au repos que faute d'images-clés. Cuire l'angle tournait ces nœuds
  // d'un quart de tour de trop.
  const ModelMesh built = MeshOf(TurningHub());
  REQUIRE(built.vertices.size() == 3);
  // Le sommet (1, 0, 0) reste sur l'axe X, décalé de cent : aucun quart de tour.
  CHECK(NearlyEqual(built.vertices[1].position[0], 101.0F));
  CHECK(NearlyEqual(built.vertices[1].position[2], 0.0F));
}

TEST_CASE("Gréement : les sommets restent dans le repère de leur nœud", "[render][model]") {
  // La pose se compose au dessin : la cuire ici la compterait deux fois.
  auto model = RsmModel::Parse(BuildRsm(TurningHub()));
  REQUIRE(model.has_value());
  const ModelMesh mesh = BuildModelMesh(*model);
  const ro::render::ModelRig rig = ro::render::BuildModelRig(*model, mesh);
  REQUIRE_FALSE(rig.empty());
  REQUIRE(rig.parts.size() == 1);
  REQUIRE(rig.vertices.size() == 3);
  CHECK(NearlyEqual(rig.vertices[1].position[0], 1.0F));
  // Les lots s'indexent comme ceux du maillage statique : ils en partagent les
  // textures téléversées.
  REQUIRE(rig.parts[0].surfaces.size() == 1);
  CHECK(mesh.textures[static_cast<std::size_t>(rig.parts[0].surfaces[0].texture_index)] ==
        "mur.bmp");
  CHECK(rig.animation_length == 1000);
}

TEST_CASE("Gréement : aucun sans deux images-clés de rotation", "[render][model]") {
  // Une seule image-clé pose le nœud mais ne le fait pas bouger.
  RsmBuildOptions options = TurningHub();
  options.meshes[0].rotation_keys.resize(1);
  auto single = RsmModel::Parse(BuildRsm(options));
  REQUIRE(single.has_value());
  CHECK(ro::render::BuildModelRig(*single, BuildModelMesh(*single)).empty());
}

TEST_CASE("Pose animée : le .rsw décide, et une vitesse trop lente fige", "[render][model]") {
  auto model = RsmModel::Parse(BuildRsm(TurningHub()));
  REQUIRE(model.has_value());
  ro::render::ModelInstances instances;
  instances.mesh = BuildModelMesh(*model);
  instances.rig = ro::render::BuildModelRig(*model, instances.mesh);

  RswModel looped;
  looped.animation_type = 2;
  looped.animation_speed = 1.0F;
  RswModel faster = looped;
  faster.animation_speed = 2.0F;
  RswModel still = looped;
  still.animation_type = 0;
  RswModel crawling = looped;
  crawling.animation_speed = 0.01F;

  CHECK(ro::render::AddInstance(instances, glm::mat4{1.0F}, looped));
  CHECK(ro::render::AddInstance(instances, glm::mat4{1.0F}, looped));
  CHECK(ro::render::AddInstance(instances, glm::mat4{1.0F}, faster));
  CHECK_FALSE(ro::render::AddInstance(instances, glm::mat4{1.0F}, still));
  CHECK_FALSE(ro::render::AddInstance(instances, glm::mat4{1.0F}, crawling));

  // Deux façons de jouer, donc deux groupes ; les figées vont au maillage cuit.
  REQUIRE(instances.animated.size() == 2);
  CHECK(instances.animated[0].transforms.size() == 2);
  CHECK(instances.animated[1].animation_speed == 2.0F);
  CHECK(instances.transforms.size() == 2);
}

TEST_CASE("Modèle : la normale suit l'ordre de lecture des sommets", "[render][model]") {
  // Le sens n'est pas une question de goût : il doit être celui de la
  // référence, faute de quoi l'éclairage se retourne. Ce qui décide du côté
  // éclairé est la DIRECTION de la lumière, pas ce produit vectoriel.
  //
  // Le triangle d'essai est posé dans le plan z = 0, sommets (0,0,0), (1,0,0)
  // et (0,1,0). Le produit (b - a) x (c - a) pointe donc vers les z POSITIFS.
  RsmBuildOptions options;
  options.textures = {"mur.bmp"};
  options.main_mesh_names = {"corps"};
  options.meshes = {Triangle("corps")};

  const ModelMesh built = MeshOf(options);
  REQUIRE(built.vertices.size() == 3);
  CHECK(NearlyEqual(built.vertices[0].normal[0], 0.0F));
  CHECK(NearlyEqual(built.vertices[0].normal[1], 0.0F));
  CHECK(NearlyEqual(built.vertices[0].normal[2], 1.0F));
}

TEST_CASE("Modèle : deux faces d'un même groupe se lissent au sommet partagé", "[render][model]") {
  // Sans lissage, un modèle montre ses facettes : chaque triangle prend une
  // teinte franche, et deux pans voisins d'un toit sortent l'un sombre l'autre
  // clair pour quelques degrés d'écart. C'est ce qui rendait le décor de
  // gonryun charbonneux là où le client le montre lumineux.
  //
  // Deux triangles se rejoignent ici en angle droit. Dans le même groupe de
  // lissage, leur sommet commun reçoit la MOYENNE de leurs deux normales, à
  // mi-chemin des deux plans.
  RsmMeshSpec mesh;
  mesh.name = "corps";
  mesh.texture_indices = {0};
  mesh.textures = {"mur.bmp"};
  mesh.vertices = {{0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, 1.0F}};
  mesh.texture_vertices = {{0.0F, 0.0F}, {1.0F, 0.0F}, {0.0F, 1.0F}, {1.0F, 1.0F}};
  mesh.faces = {
      RsmFaceSpec{.vertex_ids = {0, 1, 2}, .texture_vertex_ids = {0, 1, 2}, .smooth_group = 1},
      RsmFaceSpec{.vertex_ids = {0, 3, 1}, .texture_vertex_ids = {0, 3, 1}, .smooth_group = 1},
  };

  RsmBuildOptions options;
  options.textures = {"mur.bmp"};
  options.main_mesh_names = {"corps"};
  options.shade_type = 2;
  options.meshes = {mesh};

  const ModelMesh built = MeshOf(options);
  REQUIRE(built.vertices.size() == 6);
  // Le sommet 0 appartient aux deux faces : sa normale n'est celle d'aucune des
  // deux, elle est entre les deux.
  const auto& shared = built.vertices[0].normal;
  CHECK_FALSE(NearlyEqual(shared[1], 0.0F));
  CHECK_FALSE(NearlyEqual(shared[2], 0.0F));
  CHECK(NearlyEqual(shared[1], shared[2]));
}

TEST_CASE("Modèle : deux groupes différents gardent leur arête vive", "[render][model]") {
  // Le lissage suit les GROUPES que le fichier déclare. Moyenner par sommet
  // seul arrondirait les arêtes que le modeleur a voulues nettes.
  RsmMeshSpec mesh;
  mesh.name = "corps";
  mesh.texture_indices = {0};
  mesh.textures = {"mur.bmp"};
  mesh.vertices = {{0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F}, {0.0F, 0.0F, 1.0F}};
  mesh.texture_vertices = {{0.0F, 0.0F}, {1.0F, 0.0F}, {0.0F, 1.0F}, {1.0F, 1.0F}};
  mesh.faces = {
      RsmFaceSpec{.vertex_ids = {0, 1, 2}, .texture_vertex_ids = {0, 1, 2}, .smooth_group = 1},
      RsmFaceSpec{.vertex_ids = {0, 3, 1}, .texture_vertex_ids = {0, 3, 1}, .smooth_group = 2},
  };

  RsmBuildOptions options;
  options.textures = {"mur.bmp"};
  options.main_mesh_names = {"corps"};
  options.shade_type = 2;
  options.meshes = {mesh};

  const ModelMesh built = MeshOf(options);
  REQUIRE(built.vertices.size() == 6);
  // La première face est dans le plan z = 0 : sa normale y reste franche.
  const auto& sharp = built.vertices[0].normal;
  CHECK(NearlyEqual(sharp[0], 0.0F));
  CHECK(NearlyEqual(sharp[1], 0.0F));
  CHECK(NearlyEqual(sharp[2], 1.0F));
}

TEST_CASE("Normale transformée : une échelle uniforme ne change pas sa direction",
          "[render][model]") {
  const glm::vec3 normal{0.6F, 0.8F, 0.0F};
  const glm::mat4 uniform = glm::scale(glm::mat4{1.0F}, glm::vec3{3.0F});
  const glm::vec3 moved = TransformedNormal(uniform, normal);
  CHECK(NearlyEqual(moved.x, 0.6F));
  CHECK(NearlyEqual(moved.y, 0.8F));
  CHECK(NearlyEqual(moved.z, 0.0F));
}

TEST_CASE("Normale transformée : sous (2, 1, 1), elle suit la partie 3 × 3 comme le client",
          "[render][model]") {
  // Une face à 45 degrés, de normale (1, 1, 0)/√2 et de tangente (1, −1, 0).
  // Le client étire la normale comme une position : (2, 1, 0), renormalisée.
  // La transposée de l'inverse donnerait (1/2, 1, 0) — la perpendiculaire à la
  // face étirée, que le client ne calcule pas.
  constexpr float kHalfSquareRoot = 0.70710678F;
  const glm::vec3 normal{kHalfSquareRoot, kHalfSquareRoot, 0.0F};
  const glm::mat4 stretched = glm::scale(glm::mat4{1.0F}, glm::vec3{2.0F, 1.0F, 1.0F});
  const glm::vec3 moved = TransformedNormal(stretched, normal);
  const glm::vec3 expected = glm::normalize(glm::vec3{2.0F, 1.0F, 0.0F});
  CHECK(NearlyEqual(moved.x, expected.x));
  CHECK(NearlyEqual(moved.y, expected.y));
  CHECK(NearlyEqual(moved.z, expected.z));
  CHECK(NearlyEqual(glm::length(moved), 1.0F));
  // La normale du client n'est PLUS perpendiculaire à la face étirée.
  const glm::vec3 stretched_tangent = glm::mat3(stretched) * glm::vec3{1.0F, -1.0F, 0.0F};
  CHECK_FALSE(NearlyEqual(glm::dot(moved, stretched_tangent), 0.0F));
}

TEST_CASE("Modèle : la normale naît dans le maillage, puis la pose du nœud l'étire",
          "[render][model]") {
  // Le triangle (0,0,0), (1,−1,0), (0,0,1) a pour normale (−1, −1, 0)/√2 dans
  // son maillage. L'échelle (2, 1, 1) du nœud l'étire en (−2, −1, 0), comme le
  // client. Mesurer la face déjà étirée donnerait (−1, −2, 0).
  RsmMeshSpec mesh = Triangle("corps");
  mesh.vertices = {{0.0F, 0.0F, 0.0F}, {1.0F, -1.0F, 0.0F}, {0.0F, 0.0F, 1.0F}};
  mesh.global_scale = {2.0F, 1.0F, 1.0F};

  RsmBuildOptions options;
  options.textures = {"mur.bmp"};
  options.main_mesh_names = {"corps"};
  options.meshes = {mesh};

  const ModelMesh built = MeshOf(options);
  REQUIRE(built.vertices.size() == 3);
  const glm::vec3 expected = glm::normalize(glm::vec3{-2.0F, -1.0F, 0.0F});
  const auto& normal = built.vertices[0].normal;
  CHECK(NearlyEqual(normal[0], expected.x));
  CHECK(NearlyEqual(normal[1], expected.y));
  CHECK(NearlyEqual(normal[2], expected.z));
}

TEST_CASE("Nœuds posés : un sommet tombe là où le maillage le dessine", "[render][model]") {
  ro::test_support::RsmBuildOptions options;
  ro::test_support::RsmMeshSpec node;
  node.name = "Box04";
  // Une rotation du nœud, et un décalage : ce que le maillage applique aussi.
  node.transform = {1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, -1.0F, 0.0F};
  node.local_position = {1.0F, 2.0F, 3.0F};
  node.vertices = {{0.0F, 0.0F, 0.0F}, {4.0F, 0.0F, 0.0F}, {0.0F, 5.0F, 0.0F}};
  node.texture_vertices = {{0.0F, 0.0F}};
  node.faces = {ro::test_support::RsmFaceSpec{.vertex_ids = {0, 1, 2}}};
  options.meshes = {node};
  options.main_mesh_names = {"Box04"};
  auto model = ro::formats::RsmModel::Parse(ro::test_support::BuildRsm(options));
  REQUIRE(model.has_value());
  const std::vector<ro::render::PosedNode> posed = ro::render::PosedNodes(*model);
  REQUIRE(posed.size() == 1);
  REQUIRE(posed[0].vertices.size() == 3);
  const ro::render::ModelMesh mesh = ro::render::BuildModelMesh(*model);
  // Chaque sommet posé est un des sommets que le maillage dessine.
  for (const glm::vec3& vertex : posed[0].vertices) {
    const bool drawn = std::ranges::any_of(mesh.vertices, [&](const ro::render::ModelVertex& at) {
      return glm::length(glm::vec3(at.position[0], at.position[1], at.position[2]) - vertex) <
             1e-4F;
    });
    CHECK(drawn);
  }
}
