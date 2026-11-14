#include "ro/render/terrain_mesh.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gnd.h"
#include "ro/formats/image.h"
#include "ro/render/lightmap_atlas.h"

#include "support/map_builder.h"

using ro::formats::GndMap;
using ro::render::BuildTerrainMesh;
using ro::render::kWorldUnitsPerCube;
using ro::render::TerrainMesh;
using ro::test_support::BuildGnd;
using ro::test_support::GndBuildOptions;
using ro::test_support::GndCubeSpec;

namespace {

bool NearlyEqual(float actual, float expected) { return std::abs(actual - expected) < 1e-4F; }

// Une grille 2 × 2 dont chaque cube porte une face du dessus, et des hauteurs
// distinctes par coin pour que toute permutation se voie.
GndBuildOptions FlatGrid() {
  GndBuildOptions options;
  options.width = 2;
  options.height = 2;
  options.tile_count = 1;
  options.cubes = {
      {{1.0F, 2.0F, 3.0F, 4.0F}, 0, -1, -1},
      {{5.0F, 6.0F, 7.0F, 8.0F}, 0, -1, -1},
      {{9.0F, 10.0F, 11.0F, 12.0F}, 0, -1, -1},
      {{13.0F, 14.0F, 15.0F, 16.0F}, 0, -1, -1},
  };
  return options;
}

TerrainMesh MeshOf(const GndBuildOptions& options) {
  auto gnd = GndMap::Parse(BuildGnd(options));
  REQUIRE(gnd.has_value());
  return BuildTerrainMesh(*gnd, ro::render::BuildLightmapAtlas(*gnd));
}

// Deux cubes côte à côte, le premier portant la paroi qui les sépare : à droite
// de lui (`along_x`) ou vers la rangée suivante. Seule la face du dessus du
// premier est émise, pour que les seules normales horizontales soient la paroi.
GndBuildOptions WallBetween(bool along_x, const std::array<float, 4>& carrier,
                            const std::array<float, 4>& neighbour) {
  GndBuildOptions options;
  options.width = along_x ? 2 : 1;
  options.height = along_x ? 1 : 2;
  options.tile_count = 1;
  options.cubes = {
      {carrier, 0, along_x ? -1 : 0, along_x ? 0 : -1},
      {neighbour, -1, -1, -1},
  };
  return options;
}

// La normale de la paroi d'un maillage, ou un vecteur nul s'il n'y en a pas ou
// si ses sommets ne s'accordent pas.
std::array<float, 3> WallNormal(const TerrainMesh& mesh) {
  std::array<float, 3> found{};
  bool seen = false;
  for (const auto& vertex : mesh.vertices) {
    if (vertex.normal[1] != 0.0F) {
      continue;
    }
    if (seen && vertex.normal != found) {
      return {};
    }
    found = vertex.normal;
    seen = true;
  }
  return found;
}

constexpr std::array<float, 4> kHigh{0.0F, 0.0F, 0.0F, 0.0F};
// Les hauteurs du GND comptent vers le BAS : vingt unités sous kHigh.
constexpr std::array<float, 4> kLow{20.0F, 20.0F, 20.0F, 20.0F};
constexpr std::array<float, 3> kEast{1.0F, 0.0F, 0.0F};
constexpr std::array<float, 3> kWest{-1.0F, 0.0F, 0.0F};
// La rangée suivante est plus au fond, vers les z décroissants.
constexpr std::array<float, 3> kTowardNextRow{0.0F, 0.0F, -1.0F};
constexpr std::array<float, 3> kTowardCarrierRow{0.0F, 0.0F, 1.0F};

}  // namespace

TEST_CASE("Terrain : deux triangles par face", "[render][terrain]") {
  const TerrainMesh mesh = MeshOf(FlatGrid());
  // Quatre cubes, une face du dessus chacun, six sommets par face.
  CHECK(mesh.vertices.size() == 4 * 6);
  REQUIRE(mesh.surfaces.size() == 1);
  CHECK(mesh.surfaces[0].vertex_count == 24);
  CHECK(mesh.surfaces[0].first_vertex == 0);
}

TEST_CASE("Terrain : la hauteur est NÉGATIVE dans le monde", "[render][terrain]") {
  // Le format compte les hauteurs vers le bas, la scène vers le haut. Oublier
  // la négation retourne le relief : les creux deviennent des bosses, et
  // aucune vérification de taille ne s'en aperçoit.
  const TerrainMesh mesh = MeshOf(FlatGrid());

  // Toutes les hauteurs du GND sont positives (1 à 16), donc toutes les
  // ordonnées du maillage doivent être négatives.
  for (const auto& vertex : mesh.vertices) {
    CHECK(vertex.position[1] < 0.0F);
  }
  CHECK(mesh.bounds_min[1] == -16.0F);
  CHECK(mesh.bounds_max[1] == -1.0F);
}

TEST_CASE("Terrain : la première rangée du GND est au FOND", "[render][terrain]") {
  // Le GND range sa rangée 0 au fond de la scène : Z se compte à rebours de la
  // hauteur totale, et chaque cellule s'étend d'un pas VERS l'observateur.
  //
  // Pour une grille de deux rangées, la rangée 0 occupe [20, 30] et la rangée 1
  // occupe [10, 20] : l'emprise totale est donc [10, 30], et non [0, 20]. Ce
  // décalage d'un pas est celui de GndRenderer, et il faut le conserver — les
  // positions d'objets du RSW sont exprimées dans le même repère.
  GndBuildOptions options = FlatGrid();
  options.height = 2;
  const TerrainMesh mesh = MeshOf(options);

  const float depth = 2.0F * kWorldUnitsPerCube;
  CHECK(mesh.bounds_min[2] == depth - kWorldUnitsPerCube);
  CHECK(mesh.bounds_max[2] == depth + kWorldUnitsPerCube);
}

TEST_CASE("Terrain : chaque coin va à sa place", "[render][terrain]") {
  // Un seul cube, quatre hauteurs distinctes : on vérifie que le coin 2
  // (haut-gauche) est bien au fond à gauche et le coin 1 (bas-droit) devant à
  // droite. C'est la correspondance que GRFEditor établit dans GndRenderer.
  GndBuildOptions options;
  options.width = 1;
  options.height = 1;
  options.tile_count = 1;
  options.cubes = {{{1.0F, 2.0F, 3.0F, 4.0F}, 0, -1, -1}};

  const TerrainMesh mesh = MeshOf(options);
  REQUIRE(mesh.vertices.size() == 6);

  const auto has_vertex = [&mesh](float x, float y, float z) {
    for (const auto& vertex : mesh.vertices) {
      if (vertex.position[0] == x && vertex.position[1] == y && vertex.position[2] == z) {
        return true;
      }
    }
    return false;
  };

  // Grille 1 × 1 : la cellule s'étend de z = 10 (fond) à z = 20 (devant).
  const float z_back = kWorldUnitsPerCube;
  const float z_front = z_back + kWorldUnitsPerCube;
  CHECK(has_vertex(0.0F, -3.0F, z_back));                 // coin 2, fond-gauche
  CHECK(has_vertex(kWorldUnitsPerCube, -4.0F, z_back));   // coin 3, fond-droit
  CHECK(has_vertex(0.0F, -1.0F, z_front));                // coin 0, devant-gauche
  CHECK(has_vertex(kWorldUnitsPerCube, -2.0F, z_front));  // coin 1, devant-droit
}

TEST_CASE("Terrain : la face du dessus se plie du fond-gauche au devant-droit",
          "[render][terrain]") {
  // Quatre coins hors d'un même plan : c'est la diagonale qui décide du relief.
  // Le jeu plie selon celle qui joint le coin 2 au coin 1 ; plier selon l'autre
  // étire les bosses en biais, et leur texture avec.
  GndBuildOptions options;
  options.width = 1;
  options.height = 1;
  options.tile_count = 1;
  options.cubes = {{{1.0F, 2.0F, 3.0F, 4.0F}, 0, -1, -1}};

  const TerrainMesh mesh = MeshOf(options);
  constexpr std::size_t kCornersPerTriangle = 3;
  constexpr std::size_t kTriangles = 2;
  REQUIRE(mesh.vertices.size() == kCornersPerTriangle * kTriangles);

  const float z_back = kWorldUnitsPerCube;
  const float z_front = z_back + kWorldUnitsPerCube;
  for (std::size_t triangle = 0; triangle < kTriangles; ++triangle) {
    const std::span<const ro::render::TerrainVertex> corners(
        mesh.vertices.data() + triangle * kCornersPerTriangle, kCornersPerTriangle);
    const auto holds = [corners](float x, float y, float z) {
      return std::ranges::any_of(corners, [x, y, z](const ro::render::TerrainVertex& vertex) {
        return vertex.position[0] == x && vertex.position[1] == y && vertex.position[2] == z;
      });
    };
    CHECK(holds(0.0F, -3.0F, z_back));                 // coin 2, fond-gauche
    CHECK(holds(kWorldUnitsPerCube, -2.0F, z_front));  // coin 1, devant-droit
  }
}

TEST_CASE("Terrain : chaque coin reçoit SES coordonnées de texture", "[render][terrain]") {
  // Le test qui manquait, et dont l'absence a laissé passer une géométrie en
  // miroir vérifiée correcte par ailleurs.
  //
  // Les positions étaient justes, le nombre de sommets aussi, l'emprise aussi :
  // seul l'APPARIEMENT position ↔ UV était faux, et rien ne le regardait. Il
  // n'était visible qu'en comparant un rendu à une capture du client.
  //
  // Le constructeur écrit u = {0, 1, 2, 3} et v = {0, 2, 4, 6}, donc chaque
  // coin porte un couple unique : toute permutation se voit.
  GndBuildOptions options;
  options.width = 1;
  options.height = 1;
  options.tile_count = 1;
  options.cubes = {{{1.0F, 2.0F, 3.0F, 4.0F}, 0, -1, -1}};

  const TerrainMesh mesh = MeshOf(options);
  REQUIRE(mesh.vertices.size() == 6);

  const auto uv_at = [&mesh](float x, float z) {
    for (const auto& vertex : mesh.vertices) {
      if (vertex.position[0] == x && vertex.position[2] == z) {
        return vertex.texture_uv;
      }
    }
    return std::array<float, 2>{-1.0F, -1.0F};
  };

  const float z_back = kWorldUnitsPerCube;
  const float z_front = z_back + kWorldUnitsPerCube;
  CHECK(uv_at(0.0F, z_back) == std::array<float, 2>{2.0F, 4.0F});                 // coin 2
  CHECK(uv_at(kWorldUnitsPerCube, z_back) == std::array<float, 2>{3.0F, 6.0F});   // coin 3
  CHECK(uv_at(0.0F, z_front) == std::array<float, 2>{0.0F, 0.0F});                // coin 0
  CHECK(uv_at(kWorldUnitsPerCube, z_front) == std::array<float, 2>{1.0F, 2.0F});  // coin 1
}

TEST_CASE("Terrain : un cube sans face du dessus ne produit rien", "[render][terrain]") {
  GndBuildOptions options = FlatGrid();
  for (GndCubeSpec& cube : options.cubes) {
    cube.tile_up = -1;
  }
  const TerrainMesh mesh = MeshOf(options);
  CHECK(mesh.empty());
  CHECK(mesh.surfaces.empty());
  // Une emprise nulle plutôt que les sentinelles de calcul, qui donneraient un
  // centre absurde à l'appelant.
  CHECK(mesh.center() == std::array<float, 3>{0.0F, 0.0F, 0.0F});
}

TEST_CASE("Terrain : les sommets sont groupés par texture", "[render][terrain]") {
  // Un lot de dessin par texture : c'est ce qui évite autant de changements
  // d'état que de carreaux.
  GndBuildOptions options = FlatGrid();
  options.tile_count = 3;
  options.tile_texture_indices = {7, 3, 7};
  options.cubes = {
      {{1.0F, 1.0F, 1.0F, 1.0F}, 0, -1, -1},
      {{1.0F, 1.0F, 1.0F, 1.0F}, 1, -1, -1},
      {{1.0F, 1.0F, 1.0F, 1.0F}, 2, -1, -1},
      {{1.0F, 1.0F, 1.0F, 1.0F}, 0, -1, -1},
  };

  const TerrainMesh mesh = MeshOf(options);
  REQUIRE(mesh.surfaces.size() == 2);

  // Triées par index de texture croissant.
  CHECK(mesh.surfaces[0].texture_index == 3);
  CHECK(mesh.surfaces[0].vertex_count == 6);  // un seul cube
  CHECK(mesh.surfaces[1].texture_index == 7);
  CHECK(mesh.surfaces[1].vertex_count == 18);  // trois cubes

  // Les lots se suivent sans trou ni chevauchement.
  CHECK(mesh.surfaces[0].first_vertex == 0);
  CHECK(mesh.surfaces[1].first_vertex == 6);
  CHECK(mesh.surfaces[1].first_vertex + mesh.surfaces[1].vertex_count == mesh.vertices.size());
}

TEST_CASE("Terrain : une face sans texture n'est pas émise", "[render][terrain]") {
  // Elle ne pourrait se peindre qu'avec la texture de repli, en magenta.
  GndBuildOptions options = FlatGrid();
  options.tile_count = 2;
  options.tile_texture_indices = {0, -1};
  options.cubes = {
      {{1.0F, 1.0F, 1.0F, 1.0F}, 0, -1, -1},
      {{1.0F, 1.0F, 1.0F, 1.0F}, 1, -1, -1},
      {{1.0F, 1.0F, 1.0F, 1.0F}, 0, -1, -1},
      {{1.0F, 1.0F, 1.0F, 1.0F}, 0, -1, -1},
  };
  const TerrainMesh mesh = MeshOf(options);
  REQUIRE(mesh.surfaces.size() == 1);
  CHECK(mesh.surfaces[0].texture_index == 0);
  CHECK(mesh.vertices.size() == 3 * 6);
}

TEST_CASE("Terrain : les faces latérales ferment les dénivelés", "[render][terrain]") {
  GndBuildOptions options;
  options.width = 2;
  options.height = 1;
  options.tile_count = 1;
  options.cubes = {
      {{0.0F, 0.0F, 0.0F, 0.0F}, 0, -1, 0},  // face latérale vers la droite
      {{20.0F, 20.0F, 20.0F, 20.0F}, 0, -1, -1},
  };

  const TerrainMesh mesh = MeshOf(options);
  // Deux faces du dessus plus une latérale.
  CHECK(mesh.vertices.size() == 3 * 6);
  // La face latérale descend jusqu'à la hauteur du voisin.
  CHECK(mesh.bounds_min[1] == -20.0F);
}

TEST_CASE("Terrain : une face latérale au bord est ignorée", "[render][terrain]") {
  // Sans voisin à droite, il n'y a pas de dénivelé à fermer, et lire le cube
  // suivant sortirait de la grille.
  GndBuildOptions options;
  options.width = 1;
  options.height = 1;
  options.tile_count = 1;
  options.cubes = {{{0.0F, 0.0F, 0.0F, 0.0F}, 0, 0, 0}};

  const TerrainMesh mesh = MeshOf(options);
  CHECK(mesh.vertices.size() == 6);  // la face du dessus, et elle seule
}

TEST_CASE("Terrain : la normale d'un cube plat pointe vers le haut", "[render][terrain]") {
  // Sans normale correcte, la lumière directionnelle du RSW ne peut pas
  // s'appliquer. Une normale INVERSÉE éclairerait la carte par en dessous —
  // invisible sur un terrain plat, flagrant sur un relief.
  GndBuildOptions options;
  options.width = 1;
  options.height = 1;
  options.tile_count = 1;
  options.cubes = {{{5.0F, 5.0F, 5.0F, 5.0F}, 0, -1, -1}};

  const TerrainMesh mesh = MeshOf(options);
  REQUIRE(mesh.vertices.size() == 6);
  for (const auto& vertex : mesh.vertices) {
    CHECK(vertex.normal[0] == 0.0F);
    CHECK(vertex.normal[1] == 1.0F);
    CHECK(vertex.normal[2] == 0.0F);
  }
}

TEST_CASE("Terrain : une pente ne penche PAS la normale du sol", "[render][terrain]") {
  // Le sol de RO ne s'ombre pas selon son inclinaison : la direction du soleil
  // est déjà cuite dans la lightmap, et la rejouer par un produit scalaire
  // ombrerait le relief une seconde fois. Mesuré contre une capture du client
  // sur gonryun — voir kGroundNormal et docs/adr/0009.
  //
  // Ce test garde la règle : quelle que soit la pente, la face du dessus reste
  // verticale. Sans lui, remettre une normale géométrique passerait inaperçu à
  // la compilation et couvrirait le sol de taches carrées.
  GndBuildOptions options;
  options.width = 1;
  options.height = 1;
  options.tile_count = 1;
  // Une pente franche : les deux coins de droite sont dix unités plus bas.
  options.cubes = {{{0.0F, 10.0F, 0.0F, 10.0F}, 0, -1, -1}};

  const TerrainMesh mesh = MeshOf(options);
  REQUIRE_FALSE(mesh.vertices.empty());

  for (const auto& vertex : mesh.vertices) {
    CHECK(vertex.normal[0] == 0.0F);
    CHECK(vertex.normal[1] == 1.0F);
    CHECK(vertex.normal[2] == 0.0F);
  }
}

TEST_CASE("Terrain : une face latérale garde sa normale horizontale", "[render][terrain]") {
  // La règle précédente ne vaut que pour le dessus. Les parois qui ferment un
  // dénivelé sont verticales et gardent leur orientation, sans quoi elles
  // s'éclaireraient comme du sol plat.
  GndBuildOptions options;
  options.width = 2;
  options.height = 1;
  options.tile_count = 1;
  options.cubes = {
      {{0.0F, 0.0F, 0.0F, 0.0F}, 0, -1, 0},
      {{20.0F, 20.0F, 20.0F, 20.0F}, 0, -1, -1},
  };

  const TerrainMesh mesh = MeshOf(options);
  bool found_horizontal = false;
  for (const auto& vertex : mesh.vertices) {
    if (vertex.normal[1] == 0.0F) {
      found_horizontal = true;
      CHECK(std::abs(vertex.normal[0]) == 1.0F);
      CHECK(vertex.normal[2] == 0.0F);
    }
  }
  CHECK(found_horizontal);
}

TEST_CASE("Terrain : une paroi regarde le cube le plus bas", "[render][terrain]") {
  // C'est de ce côté qu'elle se voit, et que le soleil l'éclaire ou non. Une
  // normale tournée vers le cube haut retourne l'éclairage des parois : celle
  // au soleil sort sombre, celle à l'ombre claire.
  SECTION("paroi de droite, cube porteur en haut") {
    CHECK(WallNormal(MeshOf(WallBetween(true, kHigh, kLow))) == kEast);
  }
  SECTION("paroi de droite, voisin en haut") {
    CHECK(WallNormal(MeshOf(WallBetween(true, kLow, kHigh))) == kWest);
  }
  SECTION("paroi vers la rangée suivante, cube porteur en haut") {
    CHECK(WallNormal(MeshOf(WallBetween(false, kHigh, kLow))) == kTowardNextRow);
  }
  SECTION("paroi vers la rangée suivante, voisin en haut") {
    CHECK(WallNormal(MeshOf(WallBetween(false, kLow, kHigh))) == kTowardCarrierRow);
  }
}

TEST_CASE("Terrain : à égalité, une paroi regarde le cube qui la porte", "[render][terrain]") {
  CHECK(WallNormal(MeshOf(WallBetween(true, kHigh, kHigh))) == kWest);
  CHECK(WallNormal(MeshOf(WallBetween(false, kHigh, kHigh))) == kTowardCarrierRow);
}

TEST_CASE("Terrain : la paroi de droite compare le coin 2 du cube au coin 0 du voisin",
          "[render][terrain]") {
  // Le jeu compare cette paire pour les deux parois, même pour celle de droite
  // qui longe les coins 1 et 3. Une pente où les deux lectures divergent :
  // le coin 2 est au-dessus du voisin, le coin 1 au-dessous.
  constexpr std::array<float, 4> kSlope{0.0F, 30.0F, 0.0F, 30.0F};
  CHECK(WallNormal(MeshOf(WallBetween(true, kSlope, kLow))) == kEast);
}

TEST_CASE("Terrain : le centre de l'emprise", "[render][terrain]") {
  const TerrainMesh mesh = MeshOf(FlatGrid());
  const std::array<float, 3> center = mesh.center();
  CHECK(center[0] == kWorldUnitsPerCube);
  // Emprise en Z de [10, 30] pour une grille de deux rangées.
  CHECK(center[2] == 2.0F * kWorldUnitsPerCube);
}

namespace {

// Une texture d'un seul texel, opaque ou non.
//
// Un texel suffit : les carreaux du constructeur d'essai couvrent l'image
// entière, et ce que la règle regarde est la présence d'AU MOINS un texel
// opaque, pas leur disposition.
ro::formats::Image SingleTexel(std::uint8_t alpha) {
  ro::formats::Image image;
  image.width = 1;
  image.height = 1;
  image.pixels = {255, 0, 255, alpha};
  return image;
}

TerrainMesh MeshOf(const GndBuildOptions& options, std::span<const ro::formats::Image> textures) {
  auto gnd = GndMap::Parse(BuildGnd(options));
  REQUIRE(gnd.has_value());
  return BuildTerrainMesh(*gnd, ro::render::BuildLightmapAtlas(*gnd),
                          ro::render::FindInvisibleTiles(*gnd, textures));
}

}  // namespace

TEST_CASE("Terrain : un carreau tout transparent n'est pas émis", "[render][terrain]") {
  // C'est ainsi qu'un mappeur TROUE une carte : il pose un morceau de texture
  // tout en magenta. Émettre le carreau et compter sur le nuanceur pour le
  // rejeter ne tient pas — le rejet lit l'alpha d'un mipmap, qui remonte au-
  // dessus du seuil dès que la caméra recule, et la carte se couvre alors de
  // traits clairs suspendus au-dessus du vide.
  const std::array<ro::formats::Image, 1> textures = {SingleTexel(0)};
  const TerrainMesh mesh = MeshOf(FlatGrid(), textures);
  CHECK(mesh.vertices.empty());
  CHECK(mesh.surfaces.empty());
}

TEST_CASE("Terrain : un carreau opaque est émis comme avant", "[render][terrain]") {
  const std::array<ro::formats::Image, 1> textures = {SingleTexel(255)};
  const TerrainMesh mesh = MeshOf(FlatGrid(), textures);
  CHECK(mesh.vertices.size() == 4 * 6);
}

TEST_CASE("Terrain : sans texture lue, aucun carreau n'est écarté", "[render][terrain]") {
  // Une texture illisible ne doit pas faire disparaître le sol : mieux vaut une
  // plaque magenta, qui se voit et se diagnostique, qu'un trou silencieux.
  const TerrainMesh mesh = MeshOf(FlatGrid(), {});
  CHECK(mesh.vertices.size() == 4 * 6);
}

TEST_CASE("Terrain : un seul texel opaque suffit à garder le carreau", "[render][terrain]") {
  // La règle est PRUDENTE : elle n'écarte que ce dont elle est sûre. Un carreau
  // à demi transparent reste du sol, et le nuanceur découpe le reste.
  ro::formats::Image half = SingleTexel(0);
  half.width = 2;
  half.pixels = {255, 0, 255, 0, 40, 80, 40, 255};
  const std::array<ro::formats::Image, 1> textures = {half};
  const TerrainMesh mesh = MeshOf(FlatGrid(), textures);
  CHECK(mesh.vertices.size() == 4 * 6);
}

TEST_CASE("Terrain : la couleur du carreau se lit BLEU, VERT, ROUGE, ALPHA", "[render][terrain]") {
  // C'est l'ordre d'un pixel Windows, pas celui d'un nom de couleur. La lire
  // comme ARGB échange le rouge et le bleu, et prend l'alpha pour du rouge.
  //
  // Sur une carte dont tous les carreaux sont blancs — gonryun l'est — la faute
  // ne se voit pas du tout. Elle attend une carte teintée pour se montrer, et
  // teinte alors le sol de travers.
  //
  // Le constructeur d'essai écrit 0x11, 0x22, 0x33, 0xFF dans l'ordre du
  // fichier : bleu 0x11, vert 0x22, rouge 0x33, opaque.
  const TerrainMesh mesh = MeshOf(FlatGrid());
  REQUIRE_FALSE(mesh.vertices.empty());
  const auto& color = mesh.vertices[0].color;
  CHECK(NearlyEqual(color[0], 0x33 / 255.0F));
  CHECK(NearlyEqual(color[1], 0x22 / 255.0F));
  CHECK(NearlyEqual(color[2], 0x11 / 255.0F));
  CHECK(NearlyEqual(color[3], 1.0F));
}

TEST_CASE("Terrain : la couleur d'un carreau se lit au COIN, pas sur la face",
          "[render][terrain]") {
  // Le format range la couleur par carreau, mais le jeu la traite comme une
  // valeur posée à l'ANGLE de la cellule : chaque sommet prend celle d'un
  // voisin, et la carte graphique interpole. Donner la même aux quatre coins
  // découpe le sol en rectangles nets — un damier là où le jeu montre un
  // dégradé.
  GndBuildOptions options = FlatGrid();
  options.tile_count = 4;
  // Une teinte par cube, reconnaissable à son canal ROUGE — troisième octet du
  // fichier.
  options.tile_colors = {{0, 0, 10, 0xFF}, {0, 0, 20, 0xFF}, {0, 0, 30, 0xFF}, {0, 0, 40, 0xFF}};
  for (std::size_t cube = 0; cube < options.cubes.size(); ++cube) {
    options.cubes[cube].tile_up = static_cast<std::int32_t>(cube);
  }

  const TerrainMesh mesh = MeshOf(options);
  REQUIRE(mesh.vertices.size() == 4 * 6);

  // Le cube (0, 0) porte le carreau 0. La face du dessus émet fond-gauche,
  // fond-droit, avant-droit, puis fond-gauche, avant-droit, avant-gauche.
  const auto red = [&mesh](std::size_t vertex) { return mesh.vertices[vertex].color[0] * 255.0F; };
  // Fond gauche : le cube (0, 1), carreau 2.
  CHECK(NearlyEqual(red(0), 30.0F));
  // Fond droit : le cube (1, 1), carreau 3.
  CHECK(NearlyEqual(red(1), 40.0F));
  // Avant droit : le cube (1, 0), carreau 1.
  CHECK(NearlyEqual(red(2), 20.0F));
  // Avant gauche : LUI-MÊME, carreau 0. Il est le quatrième coin, que le
  // second triangle apporte.
  CHECK(NearlyEqual(red(5), 10.0F));
}

TEST_CASE("Terrain : au bord de la carte, un coin sans voisin reste blanc", "[render][terrain]") {
  // Sans voisin il n'y a rien à lire, et inventer une teinte assombrirait le
  // pourtour de toutes les cartes.
  GndBuildOptions options = FlatGrid();
  options.tile_count = 4;
  options.tile_colors = {{0, 0, 10, 0xFF}, {0, 0, 20, 0xFF}, {0, 0, 30, 0xFF}, {0, 0, 40, 0xFF}};
  for (std::size_t cube = 0; cube < options.cubes.size(); ++cube) {
    options.cubes[cube].tile_up = static_cast<std::int32_t>(cube);
  }

  const TerrainMesh mesh = MeshOf(options);
  // Le cube (1, 1) est au coin : ses voisins de droite et du fond n'existent
  // pas. Ses six sommets commencent au rang 18 : fond-gauche, fond-droit et
  // avant-droit n'y ont personne.
  const auto& corner = mesh.vertices;
  CHECK(NearlyEqual(corner[18].color[0], 1.0F));
  CHECK(NearlyEqual(corner[19].color[0], 1.0F));
  CHECK(NearlyEqual(corner[20].color[0], 1.0F));
  // Lui-même, l'avant-gauche, garde sa teinte.
  CHECK(NearlyEqual(corner[23].color[0] * 255.0F, 40.0F));
}
