#include "ro/render/effect_cross_spikes.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <set>
#include <span>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_life_table.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_seed.h"
#include "ro/render/effect_node_world.h"

using Catch::Approx;
using ro::render::ComposeCrossSpikeQuads;
using ro::render::EffectCrossSpikeRequest;
using ro::render::EffectCrossSpikeRequestOf;
using ro::render::EffectNode;
using ro::render::EffectWorldQuad;
using ro::render::kCrossSpikeDrawnColor;
using ro::render::kCrossSpikeFaces;
using ro::render::kCrossSpikeTextureStep;
using ro::render::kEffectRingUpright;

namespace {

constexpr glm::vec3 kAtTheOrigin{0.0F, 0.0F, 0.0F};

// Le demi-tour en degrés, pour passer d'un angle en radians à un cap entier.
constexpr double kHalfTurnDegrees = 180.0;

[[nodiscard]] float QuadArea(const EffectWorldQuad& quad) {
  const glm::vec3 ab = quad.corners[1] - quad.corners[0];
  const glm::vec3 ac = quad.corners[2] - quad.corners[0];
  return glm::length(glm::cross(ab, ac)) * 0.5F;
}

[[nodiscard]] EffectCrossSpikeRequest OneCross() {
  EffectCrossSpikeRequest request;
  request.half_side = 3.0F;
  request.apex_height = 18.0F;
  request.tint = {200, 210, 220, 250};
  return request;
}

}  // namespace

TEST_CASE("la croix compte QUATRE triangles dressés", "[render][cross_spikes]") {
  const std::vector<EffectWorldQuad> quads =
      ComposeCrossSpikeQuads(OneCross(), kAtTheOrigin, kEffectRingUpright);
  REQUIRE(quads.size() == kCrossSpikeFaces);
  for (const EffectWorldQuad& quad : quads) {
    // ⚠ UN TRIANGLE PORTÉ PAR UN QUAD : le sommet est répété, comme le client le
    // fait. Chercher un quatrième point distinct n'en trouverait pas.
    CHECK(quad.corners[2] == quad.corners[3]);
    CHECK(QuadArea(quad) > 0.0F);
  }
}

TEST_CASE("les quatre faces partagent le sommet et couvrent les quatre côtés",
          "[render][cross_spikes]") {
  const EffectCrossSpikeRequest request = OneCross();
  const std::vector<EffectWorldQuad> quads =
      ComposeCrossSpikeQuads(request, kAtTheOrigin, kEffectRingUpright);
  REQUIRE(quads.size() == kCrossSpikeFaces);

  // Un seul sommet pour les quatre.
  for (const EffectWorldQuad& quad : quads) {
    CHECK(quad.corners[2] == quads.front().corners[2]);
  }
  // ⚠ ET LE SOMMET MONTE, il ne part pas de côté. C'est ce que la comparaison
  // en jeu a trouvé en premier : le pic de terre sortait du sol À
  // L'HORIZONTALE. Le client écrit son sommet en Z et le redresse par la
  // matrice du nœud ; nous replions ce quart de tour dans la figure, et le
  // sommet monte donc en Y — comme toutes les autres figures de la famille.
  CHECK(quads.front().corners[2].y == Approx(request.apex_height));
  CHECK(quads.front().corners[2].x == Approx(0.0F));
  CHECK(quads.front().corners[2].z == Approx(0.0F));

  // ET LA BASE EST AU SOL : les quatre coins ont l'ordonnée nulle.
  for (const EffectWorldQuad& quad : quads) {
    CHECK(quad.corners[0].y == Approx(0.0F));
    CHECK(quad.corners[1].y == Approx(0.0F));
  }

  // ⚠ LES QUATRE CÔTÉS, CHACUN UNE FOIS — ET CE N'EST PAS UN TOUR. Le client ne
  // parcourt pas le carré : ses arêtes sortent dans l'ordre bas, gauche, haut,
  // droite, et la première part du même coin que la deuxième. Compter les
  // premiers coins en trouverait TROIS, ce qui est juste et ne prouve rien ; ce
  // qui se prouve est que les quatre CÔTÉS y sont, sans répétition.
  const auto number = [](const glm::vec3& corner) {
    return std::pair<int, int>{corner.x > 0.0F ? 1 : -1, corner.z > 0.0F ? 1 : -1};
  };
  std::set<std::pair<std::pair<int, int>, std::pair<int, int>>> aretes;
  for (const EffectWorldQuad& quad : quads) {
    // L'arête est prise SANS SON SENS : le client en écrit une à rebours des
    // trois autres, et c'est son ordre à lui, pas une figure différente.
    const std::pair<int, int> from = number(quad.corners[0]);
    const std::pair<int, int> vers = number(quad.corners[1]);
    aretes.insert(from < vers ? std::pair{from, vers} : std::pair{vers, from});
    // Deux coins VOISINS, jamais opposés : leur distance est un côté, pas une
    // diagonale. Les prendre en diagonale croiserait les faces.
    const float cote = glm::length(quad.corners[1] - quad.corners[0]);
    CHECK(cote == Approx(2.0F * request.half_side));
  }
  CHECK(aretes.size() == kCrossSpikeFaces);
}

TEST_CASE("le sommet retombe sur le bord GAUCHE de sa bande", "[render][cross_spikes]") {
  // ⚠ SOMMET PAR SOMMET, comme le dessinateur de la croix du client : la
  // base court d'un bord à l'autre de la bande, et la pointe prend l'abscisse du
  // PREMIER coin. La bande se pose en triangle rectangle. Lui donner l'autre
  // bord mirait la face, et c'est ce qui se voyait en jeu.
  const std::vector<EffectWorldQuad> quads =
      ComposeCrossSpikeQuads(OneCross(), kAtTheOrigin, kEffectRingUpright);
  REQUIRE(quads.size() == kCrossSpikeFaces);
  for (std::size_t face = 0; face < kCrossSpikeFaces; ++face) {
    const float band = static_cast<float>(face) * kCrossSpikeTextureStep;
    CHECK(quads[face].texture_uv[0].x == Approx(band));
    CHECK(quads[face].texture_uv[1].x == Approx(band + kCrossSpikeTextureStep));
    // Le sommet, celui que les deux derniers coins répètent.
    CHECK(quads[face].texture_uv[2].x == Approx(band));
    CHECK(quads[face].texture_uv[3].x == Approx(band));
    CHECK(quads[face].texture_uv[2].y == Approx(0.0F));
  }
}

TEST_CASE("la bande de texture avance d'une face à l'autre", "[render][cross_spikes]") {
  // ⚠ CHAQUE FACE PREND SA BANDE, deux dixièmes plus loin que la précédente.
  // Leur donner l'image entière effacerait ce dégradé.
  const std::vector<EffectWorldQuad> quads =
      ComposeCrossSpikeQuads(OneCross(), kAtTheOrigin, kEffectRingUpright);
  REQUIRE(quads.size() == kCrossSpikeFaces);
  for (std::size_t face = 0; face < kCrossSpikeFaces; ++face) {
    INFO("face " << face);
    CHECK(quads[face].texture_uv[0].x == Approx(static_cast<float>(face) * kCrossSpikeTextureStep));
  }
  CHECK(quads.back().texture_uv[0].x > quads.front().texture_uv[0].x);
}

TEST_CASE("une croix sans côté ne se dessine pas", "[render][cross_spikes]") {
  EffectCrossSpikeRequest request = OneCross();
  request.half_side = 0.0F;
  CHECK(ComposeCrossSpikeQuads(request, kAtTheOrigin, kEffectRingUpright).empty());
}

TEST_CASE("la marque de piste fait basculer le mélange", "[render][cross_spikes]") {
  // ⚠ UN BÂTISSEUR SUR NEUF POSE CETTE MARQUE, et c'est la seule piste que cette
  // figure lise. Les huit autres se mélangent ; celui-ci s'ajoute.
  EffectCrossSpikeRequest ordinary = OneCross();
  EffectCrossSpikeRequest additive = OneCross();
  additive.draws_its_own_color = true;
  CHECK(ComposeCrossSpikeQuads(ordinary, kAtTheOrigin, kEffectRingUpright)
            .front()
            .blend_destination == 6);
  CHECK(ComposeCrossSpikeQuads(additive, kAtTheOrigin, kEffectRingUpright)
            .front()
            .blend_destination == 2);
}

TEST_CASE("la croix se lit dans les champs du NŒUD", "[render][cross_spikes]") {
  // ⚠ C'EST CE QUI LA REND SI BON MARCHÉ : ses neuf bâtisseurs n'écrivent aucun
  // champ de piste, et tout vient du nœud.
  EffectNode node;
  node.sprite_scale = 3.2F;
  node.half_height.half_size = 18.0F;
  node.alpha = 250.0F;
  node.color = {200.0F, 100.0F, 50.0F};

  const EffectCrossSpikeRequest request = EffectCrossSpikeRequestOf(node, 0);
  CHECK(request.half_side == Approx(3.2F));
  CHECK(request.apex_height == Approx(18.0F));
  CHECK(request.tint == std::array<std::uint8_t, 4>{200, 100, 50, 250});
  CHECK_FALSE(request.draws_its_own_color);
  CHECK(EffectCrossSpikeRequestOf(node, kCrossSpikeDrawnColor).draws_its_own_color);
}

TEST_CASE("l'orientation place la croix", "[render][cross_spikes]") {
  const EffectCrossSpikeRequest request = OneCross();
  const glm::vec3 elsewhere{40.0F, 5.0F, 90.0F};
  const std::vector<EffectWorldQuad> here =
      ComposeCrossSpikeQuads(request, kAtTheOrigin, kEffectRingUpright);
  const std::vector<EffectWorldQuad> over_there =
      ComposeCrossSpikeQuads(request, elsewhere, kEffectRingUpright);
  REQUIRE(here.size() == over_there.size());
  for (std::size_t face = 0; face < here.size(); ++face) {
    CHECK(over_there[face].corners[0].x == Approx(here[face].corners[0].x + elsewhere.x));
    CHECK(over_there[face].corners[0].z == Approx(here[face].corners[0].z + elsewhere.z));
  }
}

TEST_CASE("les faces d'une croix se peignent de l'ARRIÈRE vers l'AVANT", "[render][cross_spikes]") {
  // ⚠ SIGNALÉ EN JEU, ET PAS AVANT : les faces arrière du pic de terre
  // recouvraient les avant. Les quads se peignent sans épreuve de profondeur,
  // si bien que le dernier peint gagne, et le renderer ne triait que les LOTS —
  // rien à l'intérieur de l'un d'eux. Toutes les figures portées jusque-là sont
  // OUVERTES et ne se recouvrent pas elles-mêmes ; la croix est la première
  // figure FERMÉE, et elle l'a trouvé tout de suite.
  const EffectCrossSpikeRequest request = OneCross();
  const std::vector<EffectWorldQuad> quads =
      ComposeCrossSpikeQuads(request, kAtTheOrigin, kEffectRingUpright);
  REQUIRE(quads.size() == kCrossSpikeFaces);

  // L'œil au loin sur l'axe des x positifs : les faces dont les coins sont du
  // côté négatif doivent passer EN PREMIER.
  const glm::vec3 oeil{100.0F, 20.0F, 0.0F};
  const std::vector<std::size_t> order = ro::render::EffectWorldQuadsBackToFront(quads, oeil);
  REQUIRE(order.size() == quads.size());

  // Aucun rang ne se répète, et tous y sont.
  std::set<std::size_t> seen(order.begin(), order.end());
  CHECK(seen.size() == quads.size());

  // Et l'ordre est bien DÉCROISSANT en distance : chaque quad est au moins
  // aussi loin que le suivant. ⚠ Par son premier sommet, un coin de la base,
  // comme la file range le triangle — pas par le centre de ses coins.
  float previous = std::numeric_limits<float>::max();
  for (const std::size_t rang : order) {
    CHECK(quads[rang].sort_point == ro::render::EffectQuadSortPoint::kFirstCorner);
    const float far_point = glm::length(ro::render::EffectWorldQuadSortPoint(quads[rang]) - oeil);
    CHECK(far_point <= previous + 0.0001F);
    previous = far_point;
  }

  // ⚠ ET L'ORDRE SUIT L'ŒIL. Vu de l'autre côté, la première face peinte n'est
  // plus la même — sinon le tri ne trierait rien.
  const std::vector<std::size_t> on_other_side =
      ro::render::EffectWorldQuadsBackToFront(quads, glm::vec3{-100.0F, 20.0F, 0.0F});
  CHECK(on_other_side.front() != order.front());
}

TEST_CASE("le tri des quads est STABLE à profondeur égale", "[render][cross_spikes]") {
  // ⚠ DEUX QUADS COPLANAIRES — les segments d'un ruban — doivent garder l'ordre
  // que leur figure a voulu. Un tri instable les échangerait d'une image à
  // l'autre, et le ruban scintillerait.
  std::vector<EffectWorldQuad> plats(4);
  for (std::size_t rang = 0; rang < plats.size(); ++rang) {
    const auto place = static_cast<float>(rang);
    // Tous à la MÊME distance de l'œil, et pourtant distincts.
    plats[rang].corners = {glm::vec3{place, 0.0F, 0.0F}, glm::vec3{-place, 0.0F, 0.0F},
                           glm::vec3{0.0F, place, 0.0F}, glm::vec3{0.0F, -place, 0.0F}};
  }
  const std::vector<std::size_t> order =
      ro::render::EffectWorldQuadsBackToFront(plats, glm::vec3{0.0F, 0.0F, 50.0F});
  for (std::size_t rang = 0; rang < order.size(); ++rang) {
    CHECK(order[rang] == rang);
  }
}

TEST_CASE("le tri dans des tampons ne garde RIEN de l'appel précédent", "[render][cross_spikes]") {
  // Le renderer trie chaque lot dans les MÊMES tampons, lot après lot. Un
  // tampon qui sort d'un lot plus grand porte encore ses rangs : aucun ne doit
  // survivre, sans quoi on peindrait un quad d'un autre lot — ou hors de celui-ci.
  const EffectCrossSpikeRequest request = OneCross();
  const std::vector<EffectWorldQuad> quads =
      ComposeCrossSpikeQuads(request, kAtTheOrigin, kEffectRingUpright);
  const glm::vec3 oeil{100.0F, 20.0F, 0.0F};

  std::vector<std::size_t> order(quads.size() * 2, quads.size() + 7);
  std::vector<float> brouillon(quads.size() * 2, -1.0F);
  ro::render::EffectWorldQuadsBackToFront(quads, oeil, order, brouillon);
  CHECK(order == ro::render::EffectWorldQuadsBackToFront(quads, oeil));
}

TEST_CASE("la GRILLE de vingt-cinq pointes est CENTRÉE sur le point", "[render][cross_spikes]") {
  // ⚠ LE CLIENT EN POSE VINGT-CINQ, ET NOUS EN POSIONS UNE. Deux boucles
  // imbriquées de moins dix à dix par pas de cinq : cinq rangs de cinq. La
  // comparaison en jeu l'a montré d'un coup — la figure et la place étaient
  // justes, et l'effet méconnaissable.
  const std::span<const ro::render::EffectNodePattern> grid =
      ro::render::EffectNodePatternsFor(142);
  REQUIRE(grid.size() == 1);
  CHECK(grid.front().ring.instance_count == 25);
  CHECK(grid.front().ring.instance_placement ==
        ro::render::EffectNodeInstancePlacement::kSquareGrid);
  CHECK(grid.front().ring.instance_spacing == Approx(5.0F));

  // Les vingt-cinq places, et le carré qu'elles dessinent.
  std::uint32_t generator = 7U;
  float leftmost = 1000.0F;
  float rightmost = -1000.0F;
  std::set<std::pair<int, int>> places;
  for (std::uint16_t rang = 0; rang < 25; ++rang) {
    const glm::vec3 place =
        ro::render::EffectNodeInstanceOffset(ro::render::EffectNodeInstancePlacement::kSquareGrid,
                                             25, rang, 5.0F, 0.0F, 0.0F, generator);
    leftmost = std::min(leftmost, place.x);
    rightmost = std::max(rightmost, place.x);
    places.insert({static_cast<int>(place.x), static_cast<int>(place.z)});
    // ⚠ L'ORDONNÉE RESTE NULLE : le client en écrit une — dix —, et nous ne
    // savons pas encore qui la lit. Poser une élévation non mesurée
    // soulèverait ou enfoncerait vingt-cinq pointes sur une supposition.
    CHECK(place.y == Approx(0.0F));
  }
  // Vingt-cinq places DISTINCTES, et une grille CENTRÉE : autant à gauche qu'à
  // droite. La poser à partir de zéro la mettrait tout entière d'un côté.
  CHECK(places.size() == 25);
  CHECK(leftmost == Approx(-10.0F));
  CHECK(rightmost == Approx(10.0F));

  // ⚠ ET LA GRILLE NE TIRE RIEN : son générateur ne doit pas avoir bougé, sans
  // quoi elle décalerait les tirages de tout ce qui la suit.
  CHECK(generator == 7U);
}

TEST_CASE("le BALAYAGE DE NAISSANCE avance avec le tic, sur son cercle", "[render][cross_spikes]") {
  // Sight : quinze de rayon, moins cinq degrés par tic. Au tic zéro le nœud naît
  // sur la PROFONDEUR — le client y met le cosinus —, et dix-huit tics plus tard
  // il a fait un quart de tour DU CÔTÉ DES ABSCISSES NÉGATIVES.
  //
  // ⚠ LE SIGNE, ET NON LA SEULE DISTANCE. Cet essai prenait la valeur absolue de
  // la profondeur, ce qui est vrai des DEUX sens : il ne pouvait pas voir que le
  // cercle se parcourait à l'envers, et le joueur l'a vu en jeu.
  const glm::vec3 start = ro::render::EffectBirthSweepOffset(15.0F, -5.0F, 0);
  CHECK(start.x == Approx(0.0F).margin(0.001F));
  CHECK(start.y == Approx(0.0F));
  CHECK(start.z == Approx(15.0F));
  // ⚠ MOINS QUATRE-VINGT-DIX SE LIT DEUX CENT SOIXANTE ET ONZE dans les tables
  // du client : le quart de tour s'arrête un degré avant l'axe.
  const glm::vec3 quart = ro::render::EffectBirthSweepOffset(15.0F, -5.0F, 18);
  constexpr float kTableRadians = 271.0F * 3.14159265F / 180.0F;
  CHECK(quart.x == Approx(15.0F * std::sin(kTableRadians)).margin(0.001F));
  CHECK(quart.z == Approx(15.0F * std::cos(kTableRadians)).margin(0.001F));
  CHECK(quart.z > 0.2F);
  // Toujours sur le cercle, quel que soit le tic.
  for (const std::int32_t tick : {1, 7, 33, 100}) {
    const glm::vec3 place = ro::render::EffectBirthSweepOffset(15.0F, -5.0F, tick);
    CHECK(glm::length(glm::vec2{place.x, place.z}) == Approx(15.0F).margin(0.001F));
  }
  // Et un rayon nul ne déplace rien.
  CHECK(glm::length(ro::render::EffectBirthSweepOffset(0.0F, -5.0F, 9)) == Approx(0.0F));
}

TEST_CASE("la PAIRE EN MIROIR se pose de part et d'autre, le long de la normale",
          "[render][cross_spikes]") {
  // Les murs d'EF_GLASSWALL : le rang pair d'un côté, l'impair de l'autre, à
  // l'écart donné — et le long de la NORMALE du nœud, celle que son cap tourne.
  std::uint32_t generator = 5U;
  const glm::vec3 right = ro::render::EffectNodeInstanceOffset(
      ro::render::EffectNodeInstancePlacement::kMirroredPair, 2, 0, 3.0F, 0.0F, 0.0F, generator);
  const glm::vec3 left = ro::render::EffectNodeInstanceOffset(
      ro::render::EffectNodeInstancePlacement::kMirroredPair, 2, 1, 3.0F, 0.0F, 0.0F, generator);
  CHECK(right.x == Approx(3.0F));
  CHECK(right.y == Approx(0.0F));
  CHECK(right.z == Approx(0.0F).margin(0.001F));
  CHECK(left.x == Approx(-right.x));
  CHECK(left.z == Approx(-right.z).margin(0.001F));

  // ⚠ TOURNÉE D'UN QUART DE TOUR, LA PAIRE PASSE SUR LA PROFONDEUR : c'est ce
  // qui fait que deux paires ferment une boîte au lieu de s'empiler sur un axe.
  const glm::vec3 devant = ro::render::EffectNodeInstanceOffset(
      ro::render::EffectNodeInstancePlacement::kMirroredPair, 2, 0, 2.6F, 0.0F, 90.0F, generator);
  CHECK(devant.x == Approx(0.0F).margin(0.001F));
  CHECK(std::abs(devant.z) == Approx(2.6F));

  // Et rien ne se tire : le générateur n'a pas bougé.
  CHECK(generator == 5U);
}

TEST_CASE("la COURONNE de six pointes tire chacun de ses angles", "[render][cross_spikes]") {
  const std::span<const ro::render::EffectNodePattern> pic = ro::render::EffectNodePatternsFor(79);
  REQUIRE(pic.size() == 2);
  // La première est SEULE, au centre ; la seconde en fait six en couronne.
  CHECK(pic[0].ring.instance_count == 1);
  CHECK(pic[1].ring.instance_count == 6);
  CHECK(pic[1].ring.instance_placement == ro::render::EffectNodeInstancePlacement::kRing);
  CHECK(pic[1].ring.instance_spacing == Approx(3.5F));

  // ⚠ LES SIX NE SONT PAS ÉGALEMENT ÉCARTÉES. Le client tire l'angle de
  // chacune sur le tour entier : les poser à soixante degrés d'intervalle
  // ferait une couronne trop régulière pour être la sienne.
  std::uint32_t generator = 11U;
  std::set<int> caps;
  for (std::uint16_t rang = 0; rang < 6; ++rang) {
    const glm::vec3 place = ro::render::EffectNodeInstanceOffset(
        ro::render::EffectNodeInstancePlacement::kRing, 6, rang, 3.5F, 0.0F, 0.0F, generator);
    // Toutes sur le cercle de rayon trois et demi.
    CHECK(glm::length(glm::vec2{place.x, place.z}) == Approx(3.5F).margin(0.001F));
    CHECK(place.y == Approx(0.0F));
    caps.insert(static_cast<int>(
        std::lround(std::atan2(static_cast<double>(-place.z), static_cast<double>(place.x)) *
                    kHalfTurnDegrees / std::numbers::pi_v<double>)));
  }
  // Le générateur A bougé, lui : six tirages.
  CHECK(generator != 11U);
  CHECK(caps.size() > 1);
}

TEST_CASE("le PENCHANT est l'écart au quart de tour, pas l'angle du client",
          "[render][cross_spikes]") {
  // ⚠ LE CLIENT POSE QUATRE-VINGTS À QUATRE-VINGT-DIX-NEUF DEGRÉS, et le quart
  // de tour est déjà replié dans la figure. Ce qui reste est le penchant : de
  // moins dix à plus neuf. Porter l'angle brut coucherait la figure une
  // seconde fois — c'est exactement le défaut qu'on vient de corriger.
  const std::span<const ro::render::EffectNodePattern> grid =
      ro::render::EffectNodePatternsFor(142);
  REQUIRE(!grid.empty());
  CHECK(grid.front().ring.lean_degrees == Approx(-10.0F));
  CHECK(grid.front().ring.lean_degrees_high == Approx(9.0F));
  CHECK(grid.front().ring.draws_its_yaw);

  // SANS PENCHANT NI LACET, l'orientation laisse la pointe DEBOUT : c'est
  // l'identité, et les figures de la famille la promettent déjà.
  const EffectCrossSpikeRequest request = OneCross();
  const std::vector<EffectWorldQuad> right = ComposeCrossSpikeQuads(
      request, kAtTheOrigin, ro::render::EffectNodeLeaningOrientation(0.0F, 0.0F));
  CHECK(right.front().corners[2].y == Approx(request.apex_height));
  CHECK(right.front().corners[2].x == Approx(0.0F).margin(0.0001F));

  // AVEC PENCHANT, le sommet s'écarte — et pas de plus que son angle.
  const std::vector<EffectWorldQuad> penchee = ComposeCrossSpikeQuads(
      request, kAtTheOrigin, ro::render::EffectNodeLeaningOrientation(10.0F, 0.0F));
  CHECK(penchee.front().corners[2].y < request.apex_height);
  CHECK(std::abs(penchee.front().corners[2].x) > 0.0F);
  // La HAUTEUR du sommet ne change pas : une rotation conserve les longueurs.
  CHECK(glm::length(penchee.front().corners[2]) == Approx(request.apex_height).margin(0.001F));
}

TEST_CASE("les vingt-cinq pointes ne s'éteignent PAS ensemble", "[render][table]") {
  // ⚠ SIGNALÉ EN JEU : dans le client, les pointes de la grille retombent
  // l'une après l'autre. Mesuré : le bâtisseur écrit
  // `vie = vie_de_l_instance - 10 + tirage(30)`. Une vie unique les fait
  // toutes disparaître au même instant, ce qui est le contraire d'une figure
  // qui retombe.
  const std::span<const ro::render::EffectNodePattern> grid =
      ro::render::EffectNodePatternsFor(142);
  REQUIRE(grid.size() == 1);
  const ro::render::EffectNodePattern& pattern = grid.front();
  CHECK(pattern.life_ticks_high > pattern.life_ticks);
  // ⚠ LA FENÊTRE EST MESURÉE : trente tics d'écart, et c'est cela seul que le
  // bâtisseur écrit en clair.
  CHECK(pattern.life_ticks_high - pattern.life_ticks == 29);

  // 🔴 ET LA BASE VIENT DE L'EFFET. Le bâtisseur écrit
  // `vie = durée de l'instance − 10 + tirage(30)` : le patron porte cette
  // dépendance — voir `EffectNodePattern::life_from_effect`.
  CHECK(pattern.life_from_effect);
  // 🔴 TROIS CENTS TICS, ET NON DEUX CENT CINQUANTE. Le bâtisseur AJOUTE
  // cinquante à la durée de son instance, et c'est la seule écriture de cette
  // espèce sur les trois cent un bâtisseurs. Ne lire que l'affectation ferait
  // perdre aux pointes cinquante tics de vie.
  const std::int32_t base = ro::render::EffectNodeLifeFor(142);
  CHECK(base == 300);
  // 🔴 ET LE BÂTISSEUR EN RETRANCHE DIX : `vie du nœud = durée de l'instance −
  // 10 + tirage(30)`. Remplacer la base par la vie résolue emporterait ces dix
  // tics avec elle ; ils ont leur colonne.
  CHECK(pattern.life_offset_from_effect == -10);
  const std::int32_t start = base + pattern.life_offset_from_effect;

  // Vingt-cinq exemplaires, et des vies DIFFÉRENTES.
  std::set<std::int32_t> vies;
  std::uint32_t generator = 3U;
  for (std::uint16_t exemplaire = 0; exemplaire < pattern.ring.instance_count; ++exemplaire) {
    const ro::render::EffectNode node = ro::render::EffectNodeFromPattern(pattern, generator);
    vies.insert(node.life_ticks);
    CHECK(node.life_ticks >= start);
    CHECK(node.life_ticks <= start + (pattern.life_ticks_high - pattern.life_ticks));
  }
  CHECK(vies.size() > 1);
  // ⚠ ET LA PLUS COURTE MEURT AVANT SON EFFET : c'est ce qui fait que dix
  // pointes sur vingt-cinq s'éteignent sans avoir plongé, chez le client comme
  // chez nous. Une base sans son écart les ferait toutes plonger.
  CHECK(*vies.begin() < base);
}

TEST_CASE("le tic de sortie SUIT la vie tirée", "[render][table]") {
  // ⚠ SANS CELA, UN NŒUD QUI VIT PLUS LONGTEMPS FOND BIEN AVANT SA FIN. Ce que
  // le patron dit est l'ÉCART entre la vie et la sortie ; c'est lui qui se
  // reporte, et non le tic lui-même.
  const ro::render::EffectNodePattern& pic = ro::render::EffectNodePatternsFor(79).front();
  REQUIRE(pic.fade_out_tick > 0);
  const std::int32_t offset = pic.life_ticks - pic.fade_out_tick;
  std::uint32_t generator = 9U;
  const ro::render::EffectNode node = ro::render::EffectNodeFromPattern(pic, generator);
  CHECK(node.life_ticks - node.fade_out_tick == offset);

  // Et une vie SANS tirage garde exactement celle du patron.
  CHECK(pic.life_ticks_high == 0);
  CHECK(node.life_ticks == pic.life_ticks);
}

TEST_CASE("la pointe jaillit le long de son axe, puis s'arrête net", "[render][table]") {
  // ⚠ LE MOUVEMENT EN BOUCLE DU CLIENT : le pas
  // multiplie la TROISIÈME LIGNE de la matrice du nœud et s'accumule. Onze tics
  // de montée à un pas qui s'accélère d'un centième, puis la seconde image-clé
  // le renverse à moins un et deux dixièmes, puis l'arrêt au quatorzième.
  const ro::render::EffectNodePattern& grid = ro::render::EffectNodePatternsFor(142).front();
  REQUIRE(grid.ring.motion_stop_tick == 14);
  REQUIRE(grid.ring.burial_depth == 10.0F);

  std::uint32_t generator = 5U;
  ro::render::EffectNode node = ro::render::EffectNodeFromPattern(grid, generator);
  REQUIRE(node.translates_along_axis);
  CHECK(node.axial_translation == 0.0F);

  // ONZE TICS DE MONTÉE : la somme des pas, un plus un centième de rang.
  for (int tick = 0; tick < 11; ++tick) {
    REQUIRE(ro::render::AdvanceEffectNode(node));
  }
  CHECK(node.axial_translation == Catch::Approx(11.66F).margin(0.001F));

  // PUIS ELLE REDESCEND, quatre tics à moins un et deux dixièmes.
  const float vertex = node.axial_translation;
  for (int tick = 0; tick < 4; ++tick) {
    REQUIRE(ro::render::AdvanceEffectNode(node));
  }
  CHECK(node.axial_translation < vertex);
  CHECK(node.axial_translation == Catch::Approx(vertex - 4.8F).margin(0.001F));

  // ⚠ ET L'ARRÊT FIGE LE PAS ET SA VARIATION ENSEMBLE. N'annuler que le pas le
  // laisserait repartir au tic suivant.
  CHECK(node.step_length == 0.0F);
  CHECK(node.step_length_change_per_tick == 0.0F);
  const float arretee = node.axial_translation;
  for (int tick = 0; tick < 10; ++tick) {
    REQUIRE(ro::render::AdvanceEffectNode(node));
  }
  CHECK(node.axial_translation == arretee);
}

TEST_CASE("la pointe PLONGE sous terre avant de disparaître", "[render][table]") {
  // ⚠ SIGNALÉ EN JEU : dans le client, les pointes rentrent sous terre au lieu
  // de s'éteindre sur place. Mesuré : à la vie MOINS TRENTE, le pas devient
  // moins un et sa variation moins un centième.
  const ro::render::EffectNodePattern& grid = ro::render::EffectNodePatternsFor(142).front();
  REQUIRE(grid.ring.sinking_step_length == -1.0F);
  REQUIRE(grid.ring.sinking_step_length_change == -0.01F);

  std::uint32_t generator = 11U;
  ro::render::EffectNode node = ro::render::EffectNodeFromPattern(grid, generator);
  const std::int32_t life = node.life_ticks;

  // ⚠ LE TIC SE COMPTE DEPUIS LA FIN, et la vie est TIRÉE : un tic figé au semis
  // ferait plonger les pointes les plus longues bien avant leur terme.
  while (node.age_ticks < life - ro::render::kEffectNodeSinkingLeadTicks) {
    REQUIRE(ro::render::AdvanceEffectNode(node));
  }
  const float placed = node.axial_translation;
  CHECK(placed > 0.0F);

  // ⚠ LA BASCULE POSE LE PAS À LA FIN DE SON TIC, comme le client : c'est le
  // tic SUIVANT qui lui ajoute sa variation. L'attendre déjà accéléré ferait
  // porter la plongée d'un cran trop tôt.
  REQUIRE(ro::render::AdvanceEffectNode(node));
  CHECK(node.step_length == Catch::Approx(-1.0F).margin(0.0001F));
  REQUIRE(ro::render::AdvanceEffectNode(node));
  CHECK(node.step_length == Catch::Approx(-1.01F).margin(0.0001F));
  while (ro::render::AdvanceEffectNode(node)) {
  }
  // Elle finit bien PLUS BAS que là où elle est née : l'enfouissement est de
  // dix, la montée d'un peu moins de sept, et la plongée de plus de trente.
  CHECK(node.axial_translation < placed);
  CHECK(node.axial_translation < -grid.ring.burial_depth);
}

TEST_CASE("les six pointes du 79 sortent et RESTENT", "[render][table]") {
  // ⚠ LEUR BÂTISSEUR NE LÈVE QUE LE PREMIER DRAPEAU : elles s'arrêtent au
  // deuxième tic et rien ne les fait replonger. Leur donner la plongée de la
  // grille serait porter ce que le client n'écrit pas.
  const std::span<const ro::render::EffectNodePattern> pics = ro::render::EffectNodePatternsFor(79);
  REQUIRE(pics.size() == 2);
  const ro::render::EffectNodePattern& crown = pics[1];
  REQUIRE(crown.ring.instance_count == 6);
  CHECK(crown.ring.motion_stop_tick == 2);
  CHECK(crown.ring.sinking_step_length == ro::render::kEffectNodeKeyframeUnset);
  CHECK(crown.ring.burial_depth == 20.0F);

  std::uint32_t generator = 3U;
  ro::render::EffectNode node = ro::render::EffectNodeFromPattern(crown, generator);
  for (int tick = 0; tick < 3; ++tick) {
    REQUIRE(ro::render::AdvanceEffectNode(node));
  }
  const float sortie = node.axial_translation;
  CHECK(sortie == Catch::Approx(4.5F).margin(0.001F));
  while (ro::render::AdvanceEffectNode(node)) {
    CHECK(node.axial_translation == sortie);
  }
}

TEST_CASE("l'axe du penchant est celui que la figure dresse", "[render]") {
  // ⚠ UN SEUL AXE POUR LES DEUX EMPLOIS. Le sommet de la croix se pose le long
  // de la seconde colonne de l'orientation, et le pas déplace le nœud le long du
  // même axe — le client emploie la même ligne de matrice. Les tenir en deux
  // exemplaires les laisserait diverger.
  for (const float penchant : {-10.0F, 0.0F, 9.0F, 10.0F}) {
    for (const float lacet : {0.0F, 37.0F, 180.0F, 300.0F}) {
      const ro::render::EffectRingOrientation orientation =
          ro::render::EffectNodeLeaningOrientation(penchant, lacet);
      const glm::vec3 axe = ro::render::EffectNodeLeanAxis(penchant, lacet);
      CHECK(glm::length(axe) == Catch::Approx(1.0F).margin(0.0001F));
      for (glm::length_t rang = 0; rang < 3; ++rang) {
        CHECK(axe[rang] == Catch::Approx(orientation[1][rang]).margin(0.0001F));
      }
    }
  }
}

TEST_CASE("un nœud qui n'est pas une pointe garde son orbite", "[render]") {
  // ⚠ L'UN OU L'AUTRE, JAMAIS LES DEUX : le répartiteur du client choisit UN
  // mouvement par sous-type. Nourrir l'orbite ET l'axe ferait dériver une pointe
  // dans le plan par-dessus sa montée.
  ro::render::EffectNode orbitant;
  orbitant.life_ticks = 10;
  orbitant.step_length = 2.0F;
  orbitant.orbit_degrees = 0.0F;
  REQUIRE(ro::render::AdvanceEffectNode(orbitant));
  CHECK(orbitant.orbit_x == Catch::Approx(2.0F).margin(0.0001F));
  CHECK(orbitant.axial_translation == 0.0F);

  ro::render::EffectNode axial;
  axial.life_ticks = 10;
  axial.step_length = 2.0F;
  axial.translates_along_axis = true;
  REQUIRE(ro::render::AdvanceEffectNode(axial));
  CHECK(axial.axial_translation == Catch::Approx(2.0F).margin(0.0001F));
  CHECK(axial.orbit_x == 0.0F);
}

TEST_CASE("les CRISTAUX de Storm Gust naissent par vagues, sur un DISQUE",
          "[render][cross_spikes]") {
  // 🔴 QUATRE VAGUES DE DEUX, ET NON UN CRISTAL AU CENTRE. Le bâtisseur garde
  // son semis derrière `âge % 29 == 0 && 31 <= âge <= 150`, et sa boucle tourne
  // DEUX fois. Sans ses bornes, `horaire_du_patron` refuse une période qui
  // sèmerait sans fin, et le nœud naît UNE fois, au tic zéro — une pyramide
  // blanche plantée au milieu du blizzard.
  const std::span<const ro::render::EffectNodePattern> blizzard =
      ro::render::EffectNodePatternsFor(89);
  REQUIRE(!blizzard.empty());
  const ro::render::EffectNodePattern* cristal = nullptr;
  for (const ro::render::EffectNodePattern& pattern : blizzard) {
    if (pattern.shape == ro::render::EffectNodeShape::kCrossSpikes) {
      cristal = &pattern;
    }
  }
  REQUIRE(cristal != nullptr);

  // Les quatre moments, et rien entre eux.
  for (const std::int32_t tick : {58, 87, 116, 145}) {
    CHECK(ro::render::EffectNodePatternSowsAt(*cristal, tick));
  }
  for (const std::int32_t tick : {0, 29, 30, 31, 57, 59, 150, 174, 203}) {
    CHECK_FALSE(ro::render::EffectNodePatternSowsAt(*cristal, tick));
  }
  CHECK(cristal->ring.instance_count == 2);

  // ⚠ UN DISQUE, ET NON UNE COURONNE. Le client tire `rand() % 25 + 3` : de
  // trois à vingt-sept unités du centre, soit cinq cellules d'écart entre le
  // plus proche et le plus lointain. Un rayon moyen les alignerait tous sur le
  // même cercle.
  CHECK(cristal->ring.instance_placement == ro::render::EffectNodeInstancePlacement::kRing);
  CHECK(cristal->ring.instance_spacing == Catch::Approx(3.0F));
  CHECK(cristal->ring.instance_spacing_high == Catch::Approx(27.0F));

  std::uint32_t generator = 3U;
  std::set<int> radii;
  for (std::uint16_t rang = 0; rang < 8; ++rang) {
    const glm::vec3 place = ro::render::EffectNodeInstanceOffset(
        ro::render::EffectNodeInstancePlacement::kRing, 2, rang % 2, cristal->ring.instance_spacing,
        cristal->ring.instance_spacing_high, 0.0F, generator);
    const float radius = glm::length(glm::vec2{place.x, place.z});
    CHECK(radius >= 3.0F);
    CHECK(radius <= 27.0F);
    radii.insert(static_cast<int>(std::lround(radius)));
  }
  // Huit cristaux ne tombent pas tous à la même distance.
  CHECK(radii.size() > 1);

  // ⚠ ET ILS MEURENT ENSEMBLE. `484 = 215 - âge` est un RENDEZ-VOUS et non une
  // durée : les quatre vagues s'éteignent au même instant, avec un fondu que le
  // bâtisseur place dix tics avant la fin (tic de fondu = vie − 10).
  CHECK(cristal->life_until_tick == 215);
  CHECK(cristal->life_ticks - cristal->fade_out_tick == 10);

  // 🔴 ET ILS SORTENT DU SOL EN TROIS TICS, PUIS S'ARRÊTENT NET. Le bâtisseur
  // lève le premier drapeau et pose son tic d'arrêt à trois ; la mise à jour
  // du sous-type 9 remet alors le pas ET sa variation à zéro. Sans cet arrêt,
  // l'avance de un et demi par tic court sur les cent cinquante-sept tics du
  // premier cristal — plus de deux cents unités —, et les cristaux s'envolent
  // au-dessus du blizzard.
  CHECK(cristal->ring.motion_stop_tick == 3);
  CHECK((cristal->ring.flags & ro::render::kEffectNodeAnchorsAtTop) != 0);
  ro::render::EffectNode node = ro::render::EffectNodeFromPattern(*cristal);
  // La figure décide du mouvement : une croix de pointes avance le long de son
  // axe, et c'est ce qui rend l'arrêt visible.
  REQUIRE(node.translates_along_axis);
  for (std::int32_t tick = 0; tick < 40; ++tick) {
    REQUIRE(ro::render::AdvanceEffectNode(node));
  }
  // Trois tics d'avance à un et demi, et plus rien : le cristal sort de
  // quelques unités au lieu de monter de soixante en quarante tics.
  CHECK(node.step_length == 0.0F);
  CHECK(node.axial_translation > 0.0F);
  CHECK(node.axial_translation <= 6.0F);
}

TEST_CASE("un rayon SANS borne haute ne tire rien", "[render][cross_spikes]") {
  // ⚠ SINON TOUTE COURONNE À RAYON FIXE DÉCALERAIT LES TIRAGES DE CE QUI SUIT.
  // Une borne nulle veut dire « fixe », et le générateur ne doit pas avancer
  // pour elle : c'est la même règle que pour les autres fourchettes du patron.
  std::uint32_t with = 21U;
  std::uint32_t sans = 21U;
  const glm::vec3 fixed = ro::render::EffectNodeInstanceOffset(
      ro::render::EffectNodeInstancePlacement::kRing, 1, 0, 4.0F, 0.0F, 0.0F, sans);
  const glm::vec3 drawn = ro::render::EffectNodeInstanceOffset(
      ro::render::EffectNodeInstancePlacement::kRing, 1, 0, 4.0F, 9.0F, 0.0F, with);
  CHECK(glm::length(glm::vec2{fixed.x, fixed.z}) == Catch::Approx(4.0F).margin(0.001F));
  // Le cap est le MÊME — un seul tirage avant le rayon —, et seul le rayon
  // sépare les deux.
  CHECK(glm::length(glm::vec2{drawn.x, drawn.z}) >= 4.0F);
  CHECK(with != sans);
}

TEST_CASE("une pointe qui PLONGE garde son alpha de bout en bout", "[render][cross_spikes]") {
  // 🔴 LE CLIENT NE MET PAS À JOUR L'ALPHA D'UN NŒUD QUI PLONGE. Son mouvement
  // en boucle pose la plongée sous le drapeau 2 puis RETOURNE ; la mise à jour
  // de la couleur par images-clés est sur l'autre branche. Un fondu ici ferait
  // s'effacer les pointes de terre AVANT qu'elles ne rentrent sous terre.
  ro::render::EffectNode node;
  node.life_ticks = 60;
  node.alpha = 255.0F;
  node.alpha_step_per_tick = -8.0F;
  node.fade_out_tick = 0;
  node.sinking_step_length = -1.2F;

  for (std::int32_t tick = 0; tick < 40; ++tick) {
    static_cast<void>(ro::render::AdvanceEffectNode(node));
  }
  CHECK(node.alpha == Approx(255.0F));

  // ET LE FONDU AVEC L'INSTANCE NE LE TOUCHE PAS NON PLUS : c'est la même règle,
  // au même endroit du client.
  node.fades_with_its_instance = true;
  node.instance_ticks_left = 10;
  static_cast<void>(ro::render::AdvanceEffectNode(node));
  CHECK(node.owner_fade_at_age == ro::render::kEffectNodeOwnerFadeUnjudged);
  CHECK(node.alpha == Approx(255.0F));
  CHECK(node.life_ticks == 60);
  CHECK(node.fade_out_tick == 0);

  // ⚠ ET LE MÊME NŒUD SANS PLONGÉE PÂLIT, lui : la garde ne désactive pas la
  // rampe pour tout le monde.
  ro::render::EffectNode ordinary = node;
  ordinary.sinking_step_length = ro::render::kEffectNodeKeyframeUnset;
  ordinary.age_ticks = 0;
  for (std::int32_t tick = 0; tick < 10; ++tick) {
    static_cast<void>(ro::render::AdvanceEffectNode(ordinary));
  }
  CHECK(ordinary.alpha < 255.0F);
}
