#include "ro/render/water_plane.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gnd.h"
#include "ro/formats/rsw.h"
#include "ro/render/terrain_mesh.h"

#include "support/map_builder.h"

using ro::formats::GndMap;
using ro::formats::GndWaterZone;
using ro::formats::RswScene;
using ro::render::BuildWaterPlane;
using ro::render::kWaterFrameCount;
using ro::render::kWaterTicksPerSecond;
using ro::render::kWorldUnitsPerCube;
using ro::render::WaterFrameAt;
using ro::render::WaterPhaseAt;
using ro::render::WaterPlane;
using ro::render::WaterTexturePath;
using ro::render::WaterVertex;
using ro::test_support::BuildGnd;
using ro::test_support::BuildRsw;
using ro::test_support::GndBuildOptions;
using ro::test_support::GndCubeSpec;
using ro::test_support::RswBuildOptions;

namespace {

// UNE GRILLE DE QUATRE CUBES DE CÔTÉ, et ce n'est pas un chiffre au hasard :
// la texture d'eau pave par carrés de quatre, donc une grille plus petite ne
// montrerait jamais le raccord.
constexpr std::int32_t kSide = 4;

// Le niveau de la nappe dans les essais, en unités de FICHIER — donc positif
// alors que la nappe se pose SOUS l'origine du monde. Cinq, et non zéro : un
// zéro rendrait le signe de l'axe vertical invisible.
constexpr float kLevel = 5.0F;

// Une profondeur franche : chaque coin est bien SOUS la nappe.
constexpr float kDeep = 40.0F;
// Et une hauteur franche : chaque coin est bien AU-DESSUS.
constexpr float kDry = -40.0F;

// Le GND que la plupart des essais emploient : seize cubes creusés, aucune eau
// à lui. ⚠ VERSION 1.7 : au-delà, c'est LE GND qui porte l'eau, et le RSW
// n'aurait plus son mot à dire.
GndBuildOptions PlainGround() {
  GndBuildOptions options;
  options.major = 1;
  options.minor = 7;
  options.width = kSide;
  options.height = kSide;
  options.tile_count = 1;
  options.cubes.assign(static_cast<std::size_t>(kSide) * kSide,
                       GndCubeSpec{{kDeep, kDeep, kDeep, kDeep}, 0, -1, -1});
  return options;
}

RswBuildOptions PondScene() {
  RswBuildOptions options;
  options.water_level = kLevel;
  options.water_type = 3;
  options.wave_height = 1.0F;
  options.wave_speed = 2.0F;
  options.wave_pitch = 50.0F;
  options.texture_cycling = 3;
  return options;
}

WaterPlane PlaneOf(const GndBuildOptions& ground, const RswBuildOptions& scene) {
  auto gnd = GndMap::Parse(BuildGnd(ground));
  REQUIRE(gnd.has_value());
  auto rsw = RswScene::Parse(BuildRsw(scene));
  REQUIRE(rsw.has_value());
  return BuildWaterPlane(*gnd, *rsw);
}

// Les sommets de la nappe qui couvrent un cube donné.
//
// ⚠ ILS SE CHERCHENT PAR LOTS DE SIX, et non un par un : deux cubes voisins
// PARTAGENT une arête, donc leurs sommets partagent une coordonnée. Retenir
// tout sommet qui tombe dans l'emprise en ramasserait trois fois trop, et un
// essai de vide passerait alors pour un essai plein. C'est le quadrilatère —
// les six sommets consécutifs — qui appartient à un cube, pas le sommet.
std::vector<WaterVertex> VerticesOnCube(const WaterPlane& plane, std::int32_t x, std::int32_t y) {
  const float left = static_cast<float>(x) * kWorldUnitsPerCube;
  const float depth = static_cast<float>(kSide) * kWorldUnitsPerCube;
  const float z_back = depth - static_cast<float>(y) * kWorldUnitsPerCube;
  constexpr std::size_t kQuad = 6;

  for (std::size_t base = 0; base + kQuad <= plane.vertices.size(); base += kQuad) {
    float low_x = 1e9F;
    float low_z = 1e9F;
    for (std::size_t step = 0; step < kQuad; ++step) {
      low_x = std::min(low_x, plane.vertices[base + step].position[0]);
      low_z = std::min(low_z, plane.vertices[base + step].position[2]);
    }
    if (std::abs(low_x - left) < 0.01F && std::abs(low_z - z_back) < 0.01F) {
      return {plane.vertices.begin() + static_cast<std::ptrdiff_t>(base),
              plane.vertices.begin() + static_cast<std::ptrdiff_t>(base + kQuad)};
    }
  }
  return {};
}

}  // namespace

TEST_CASE("Eau : une carte sans eau ne produit aucune nappe", "[render][water]") {
  // La section eau du RSW a déménagé en 2.6 : au-delà, le champ lu ici n'existe
  // plus, et le GND de cet essai est trop ancien pour en porter.
  RswBuildOptions moved = PondScene();
  moved.major = 2;
  moved.minor = 6;
  const WaterPlane plane = PlaneOf(PlainGround(), moved);

  CHECK(plane.empty());
  CHECK(plane.zones.empty());
}

TEST_CASE("Eau : le GND prime sur le RSW quand les deux la portent", "[render][water]") {
  // 🔴 L'ORDRE DES DEUX SOURCES, et c'est le défaut qui se verrait le moins :
  // une carte récente porte les DEUX sections, et celle du RSW y est un reste
  // qui ne suit plus. La lire donnerait une nappe à la bonne place mais au
  // mauvais niveau, ce qui ne se remarque que sur les cartes à plusieurs bassins.
  GndBuildOptions ground = PlainGround();
  ground.minor = 9;
  ground.water_levels = {12.0F};
  const WaterPlane plane = PlaneOf(ground, PondScene());

  REQUIRE(plane.zones.size() == 1);
  CHECK(plane.zones[0].settings.level == Catch::Approx(12.0F));
  // Et tout le reste des réglages vient du GND lui aussi.
  CHECK(plane.zones[0].settings.type == 2);
  CHECK(plane.zones[0].settings.wave_speed == Catch::Approx(3.0F));
}

TEST_CASE("Eau : la nappe se pose SOUS l'origine, à l'opposé du fichier", "[render][water]") {
  // 🔴 LE SIGNE DE L'AXE VERTICAL. Le fichier compte vers le bas, la scène vers
  // le haut — c'est la même inversion que le terrain, et la rater ne donne pas
  // une nappe inclinée : elle la met au-dessus des toits, ou sous le monde.
  const WaterPlane plane = PlaneOf(PlainGround(), PondScene());
  REQUIRE_FALSE(plane.empty());

  for (const WaterVertex& vertex : plane.vertices) {
    CHECK(vertex.position[1] == Catch::Approx(-kLevel));
  }
}

TEST_CASE("Eau : un cube entièrement au-dessus du creux n'en reçoit pas", "[render][water]") {
  GndBuildOptions ground = PlainGround();
  // Un promontoire au milieu de la grille : ses quatre coins sont au-dessus de
  // la nappe, donc l'eau ne l'atteint jamais, même au creux de la vague.
  ground.cubes[1 * kSide + 1] = GndCubeSpec{{kDry, kDry, kDry, kDry}, 0, -1, -1};
  const WaterPlane plane = PlaneOf(ground, PondScene());

  CHECK(VerticesOnCube(plane, 1, 1).empty());
  // Et son voisin, lui, est bien couvert : l'essai ne passe pas simplement
  // parce que la nappe serait vide.
  CHECK(VerticesOnCube(plane, 2, 1).size() == 6);
}

TEST_CASE("Eau : un cube au ras du creux en reçoit quand même", "[render][water]") {
  // ⚠ LA BORNE EST LE CREUX DE LA HOULE, PAS LE NIVEAU DE REPOS. Un fond posé
  // entre les deux n'est découvert qu'une partie du temps ; le retrancher
  // laisserait un anneau sec au bord de chaque bassin.
  GndBuildOptions ground = PlainGround();
  const float between = kLevel - 0.5F;  // sous le repos, au-dessus du creux
  ground.cubes[2 * kSide + 2] = GndCubeSpec{{between, between, between, between}, 0, -1, -1};
  const WaterPlane plane = PlaneOf(ground, PondScene());

  CHECK(VerticesOnCube(plane, 2, 2).size() == 6);
}

TEST_CASE("Eau : un cube sans face du dessus n'en reçoit pas", "[render][water]") {
  // Un trou creusé par le mappeur n'est pas un bassin : le recouvrir poserait
  // une nappe au-dessus du vide, qu'on verrait flotter depuis le dessous.
  GndBuildOptions ground = PlainGround();
  ground.cubes[3 * kSide + 0] = GndCubeSpec{{kDeep, kDeep, kDeep, kDeep}, -1, -1, -1};
  const WaterPlane plane = PlaneOf(ground, PondScene());

  CHECK(VerticesOnCube(plane, 0, 3).empty());
}

TEST_CASE("Eau : la nappe couvre exactement l'emprise du terrain", "[render][water]") {
  // 🔴 LA CONVENTION PARTAGÉE AVEC LE TERRAIN, et c'est le vrai risque de ce
  // portage : l'axe Z remonte les rangées du fichier. Une nappe bâtie sur
  // l'autre sens serait en MIROIR de son sol — elle couvrirait le plateau et
  // laisserait le bassin à sec, sur une carte symétrique personne ne le verrait.
  auto gnd = GndMap::Parse(BuildGnd(PlainGround()));
  REQUIRE(gnd.has_value());
  auto rsw = RswScene::Parse(BuildRsw(PondScene()));
  REQUIRE(rsw.has_value());

  const ro::render::TerrainMesh terrain =
      ro::render::BuildTerrainMesh(*gnd, ro::render::BuildLightmapAtlas(*gnd));
  const WaterPlane plane = BuildWaterPlane(*gnd, *rsw);

  const auto span_of = [](const auto& vertices, auto&& select) {
    float low = 1e9F;
    float high = -1e9F;
    for (const auto& vertex : vertices) {
      low = std::min(low, select(vertex));
      high = std::max(high, select(vertex));
    }
    return std::array<float, 2>{low, high};
  };

  const auto terrain_x = span_of(terrain.vertices, [](const auto& v) { return v.position[0]; });
  const auto water_x = span_of(plane.vertices, [](const auto& v) { return v.position[0]; });
  const auto terrain_z = span_of(terrain.vertices, [](const auto& v) { return v.position[2]; });
  const auto water_z = span_of(plane.vertices, [](const auto& v) { return v.position[2]; });

  CHECK(water_x[0] == Catch::Approx(terrain_x[0]));
  CHECK(water_x[1] == Catch::Approx(terrain_x[1]));
  CHECK(water_z[0] == Catch::Approx(terrain_z[0]));
  CHECK(water_z[1] == Catch::Approx(terrain_z[1]));
}

TEST_CASE("Eau : la diagonale d'un sommet est la somme de ses indices", "[render][water]") {
  // La houle court en BIAIS : son argument est `x + y`, si bien que deux
  // sommets d'une même diagonale montent ensemble. Prendre `x - y` la ferait
  // courir dans l'autre sens, ce qui ne se voit qu'en côte à côte avec le
  // client — et prendre `x` seul la ferait courir le long d'un axe.
  const WaterPlane plane = PlaneOf(PlainGround(), PondScene());
  const std::vector<WaterVertex> cube = VerticesOnCube(plane, 2, 1);
  REQUIRE(cube.size() == 6);

  // Les quatre coins distincts d'un cube portent trois diagonales voisines :
  // celle du coin avant-gauche, celle des deux coins de la diagonale, celle du
  // coin arrière-droit.
  std::vector<float> steps;
  for (const WaterVertex& vertex : cube) {
    if (std::ranges::find(steps, vertex.wave_step) == steps.end()) {
      steps.push_back(vertex.wave_step);
    }
  }
  std::ranges::sort(steps);
  REQUIRE(steps.size() == 3);
  CHECK(steps[0] == Catch::Approx(2.0F));  // (2, 0)
  CHECK(steps[1] == Catch::Approx(3.0F));  // (2, 1) et (3, 0)
  CHECK(steps[2] == Catch::Approx(4.0F));  // (3, 1)
}

TEST_CASE("Eau : la texture pave par carrés de quatre cubes", "[render][water]") {
  const WaterPlane plane = PlaneOf(PlainGround(), PondScene());

  const auto u_span = [&plane](std::int32_t x) {
    const std::vector<WaterVertex> cube = VerticesOnCube(plane, x, 0);
    REQUIRE(cube.size() == 6);
    float low = 1e9F;
    float high = -1e9F;
    for (const WaterVertex& vertex : cube) {
      low = std::min(low, vertex.texture_uv[0]);
      high = std::max(high, vertex.texture_uv[0]);
    }
    return std::array<float, 2>{low, high};
  };

  CHECK(u_span(0)[0] == Catch::Approx(0.0F));
  CHECK(u_span(0)[1] == Catch::Approx(0.25F));
  CHECK(u_span(1)[0] == Catch::Approx(0.25F));
  // ⚠ LE DERNIER CUBE DU CARRÉ VA JUSQU'À UN, et c'est ce qui rend le pavage
  // sans couture : `(x & 3) + 1` et non `(x + 1) & 3`, qui y remettrait zéro et
  // replierait la texture sur elle-même tous les quatre cubes.
  CHECK(u_span(3)[0] == Catch::Approx(0.75F));
  CHECK(u_span(3)[1] == Catch::Approx(1.0F));
}

TEST_CASE("Eau : plusieurs zones, chacune à son niveau", "[render][water]") {
  // jor_dun02 porte une découpe 3 × 3 à deux altitudes. Une seule zone lue
  // poserait tout le souterrain au même niveau — la moitié des bassins à sec,
  // l'autre noyée.
  GndBuildOptions ground = PlainGround();
  ground.minor = 9;
  ground.water_split_width = 2;
  ground.water_split_height = 2;
  ground.water_levels = {1.0F, 2.0F, 3.0F, 4.0F};
  const WaterPlane plane = PlaneOf(ground, PondScene());

  REQUIRE(plane.zones.size() == 4);
  // Chaque zone couvre son quart : quatre cubes, six sommets chacun.
  for (const auto& zone : plane.zones) {
    CHECK(zone.vertex_count == 4 * 6);
  }
  // Et le cube du coin de chaque quart se pose bien au niveau de SA zone.
  CHECK(VerticesOnCube(plane, 0, 0)[0].position[1] == Catch::Approx(-1.0F));
  CHECK(VerticesOnCube(plane, 3, 0)[0].position[1] == Catch::Approx(-2.0F));
  CHECK(VerticesOnCube(plane, 0, 3)[0].position[1] == Catch::Approx(-3.0F));
  CHECK(VerticesOnCube(plane, 3, 3)[0].position[1] == Catch::Approx(-4.0F));
}

TEST_CASE("Eau : le chemin d'une image porte son type et deux chiffres", "[render][water]") {
  // Le client compose ce nom avec « water%d%02d.jpg » : le type sans
  // remplissage, l'image sur DEUX chiffres. Écrire « water30 » pour l'image 0
  // du type 3 chercherait l'image 30 du type nul.
  CHECK(WaterTexturePath(0, 0) == "data\\texture\\워터\\water000.jpg");
  CHECK(WaterTexturePath(3, 7) == "data\\texture\\워터\\water307.jpg");
  CHECK(WaterTexturePath(1, 31) == "data\\texture\\워터\\water131.jpg");
}

TEST_CASE("Eau : l'image avance d'une tous les `cycling` tics", "[render][water]") {
  GndWaterZone zone;
  zone.texture_cycling = 3;

  const auto at_tick = [](int ticks) {
    return static_cast<std::uint32_t>(static_cast<float>(ticks) / kWaterTicksPerSecond * 1000.0F);
  };

  CHECK(WaterFrameAt(zone, 0) == 0);
  CHECK(WaterFrameAt(zone, at_tick(2)) == 0);
  CHECK(WaterFrameAt(zone, at_tick(3)) == 1);
  CHECK(WaterFrameAt(zone, at_tick(6)) == 2);
  // Trente-deux images, puis on recommence : le client remet son compteur à
  // zéro dès qu'il atteint `32 × cycling`.
  CHECK(WaterFrameAt(zone, at_tick(3 * kWaterFrameCount - 3)) == kWaterFrameCount - 1);
  CHECK(WaterFrameAt(zone, at_tick(3 * kWaterFrameCount)) == 0);
}

TEST_CASE("Eau : une cadence absurde n'arrête pas l'animation", "[render][water]") {
  // Un fichier de carte n'est contrôlé par personne : une cadence nulle
  // diviserait par zéro, une négative rendrait un index négatif.
  //
  // ⚠ ET IL NE SUFFIT PAS QUE L'INDEX RESTE DANS LA PLAGE : borné sans être
  // ramené à un, il resterait collé sur la PREMIÈRE image, et la nappe serait
  // figée. Une eau immobile ressemble à un défaut de notre rendu, pas à une
  // carte mal écrite — d'où le compte d'images DISTINCTES.
  const auto distinct_frames = [](const GndWaterZone& zone) {
    std::vector<int> seen;
    for (std::uint32_t ms = 0; ms < 2000; ms += 17) {
      const int frame = WaterFrameAt(zone, ms);
      REQUIRE(frame >= 0);
      REQUIRE(frame < kWaterFrameCount);
      if (std::ranges::find(seen, frame) == seen.end()) {
        seen.push_back(frame);
      }
    }
    return seen.size();
  };

  GndWaterZone zone;
  zone.texture_cycling = 0;
  CHECK(distinct_frames(zone) > 4);

  zone.texture_cycling = -4;
  CHECK(distinct_frames(zone) > 4);
}

TEST_CASE("Eau : la phase avance de `wave_speed` par tic et se replie", "[render][water]") {
  GndWaterZone zone;
  zone.wave_speed = 2.0F;

  const auto at_tick = [](int ticks) {
    return static_cast<std::uint32_t>(static_cast<float>(ticks) / kWaterTicksPerSecond * 1000.0F);
  };

  CHECK(WaterPhaseAt(zone, 0) == Catch::Approx(0.0F));
  CHECK(WaterPhaseAt(zone, at_tick(10)) == Catch::Approx(20.0F).margin(0.5));
  CHECK(WaterPhaseAt(zone, at_tick(90)) == Catch::Approx(180.0F).margin(0.5));
  // Au-delà d'un demi-tour, le client retranche un tour entier : la phase
  // repart du négatif au lieu de grandir sans fin.
  CHECK(WaterPhaseAt(zone, at_tick(100)) == Catch::Approx(-160.0F).margin(0.5));
  CHECK(WaterPhaseAt(zone, at_tick(180)) == Catch::Approx(0.0F).margin(0.5));
}

TEST_CASE("Eau : une journée de jeu n'arrête pas la houle", "[render][water]") {
  // L'horloge reçue part du lancement du client : après une nuit de jeu elle
  // vaut quatre-vingt-six millions. Ce que cet essai tient est que la houle
  // avance ENCORE à cette heure-là, et que l'image tourne toujours — pas que
  // le calcul soit au dernier degré près.
  GndWaterZone zone;
  zone.wave_speed = 2.0F;
  constexpr std::uint32_t kOneDayMs = 24U * 3600U * 1000U;

  // Un tic d'écart doit encore SE VOIR, après une journée comme au départ.
  const float now = WaterPhaseAt(zone, kOneDayMs);
  const float later = WaterPhaseAt(zone, kOneDayMs + 17);
  CHECK(std::abs(later - now) > 0.5F);

  // Et l'image tourne toujours.
  zone.texture_cycling = 3;
  std::vector<int> seen;
  for (std::uint32_t step = 0; step < 200; ++step) {
    const int frame = WaterFrameAt(zone, kOneDayMs + step * 17);
    if (seen.empty() || seen.back() != frame) {
      seen.push_back(frame);
    }
  }
  CHECK(seen.size() > 4);
}

TEST_CASE("Eau : un carreau TROUÉ n'en reçoit pas non plus", "[render][water]") {
  // 🔴 LE DÉFAUT DE hu_fild01, vu en jeu : une nappe bleue par-dessus le VIDE,
  // très exactement là où le client ne montre rien.
  //
  // Un carreau que sa texture rend tout transparent est un trou creusé par le
  // mappeur. Le terrain ne l'émet pas, et le client n'y pose pas d'eau : il ne
  // la pose que sur un cube DONT LE SOL A DESSINÉ UN QUAD — il en relit les
  // quatre coins pour décider. Le cube existe pourtant, et sa face du dessus est
  // bien déclarée : `tile_up` seul ne suffit donc pas à l'écarter.
  GndBuildOptions ground = PlainGround();
  ground.tile_count = 2;
  // Le carreau UN est le trou ; le zéro reste visible.
  ground.cubes[1 * kSide + 2] = GndCubeSpec{{kDeep, kDeep, kDeep, kDeep}, 1, -1, -1};

  auto gnd = GndMap::Parse(BuildGnd(ground));
  REQUIRE(gnd.has_value());
  auto rsw = RswScene::Parse(BuildRsw(PondScene()));
  REQUIRE(rsw.has_value());

  // Deux carreaux, le second invisible.
  const std::vector<std::uint8_t> invisible = {0, 1};
  const WaterPlane plane = ro::render::BuildWaterPlane(*gnd, *rsw, invisible);

  CHECK(VerticesOnCube(plane, 2, 1).empty());
  // Et son voisin, sur le carreau visible, est bien couvert : l'essai ne passe
  // pas simplement parce que la nappe serait vide.
  CHECK(VerticesOnCube(plane, 1, 1).size() == 6);
}
