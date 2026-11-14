#include "ro/render/effect_node_world.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node.h"
#include "ro/render/sprite_billboard.h"

// LE TROISIÈME CHEMIN DE COMPOSITION : celui qui travaille dans le MONDE.
//
// Quatre dessinateurs y vivent. Leur table est dans
// `scripts/client_effect_node_rings.json`.
using ro::render::AdvanceEffectNode;
using ro::render::ComposeEffectNodeRingBand;
using ro::render::EffectNode;
using ro::render::EffectWorldQuad;
using ro::render::kEffectNodeDefaultRingStepDegrees;
using ro::render::kEffectRingFullTurnDegrees;
using ro::render::kEffectRingSegmentsPerTextureSpan;

namespace {

// L'anneau de Heal, tel que le client le pose : deux rayons égaux de quatre et
// demi, une colonne de trente-cinq, et le pas angulaire du constructeur.
EffectNode HealRing() {
  EffectNode node;
  node.radius_at_anchor.half_size = 4.5F;
  node.radius_at_height.half_size = 4.5F;
  node.half_height.half_size = 35.0F;
  node.alpha = 90.0F;
  return node;
}

// La distance d'un point à l'axe vertical de l'ancre.
float RadiusOf(glm::vec3 point, glm::vec3 anchor) {
  const float dx = point.x - anchor.x;
  const float dz = point.z - anchor.z;
  return std::sqrt(dx * dx + dz * dz);
}

}  // namespace

TEST_CASE("Anneau : le tour se découpe selon le pas angulaire", "[render][effectnode]") {
  // Trente-six degrés par défaut, donc dix segments — et c'est le CONSTRUCTEUR
  // du client qui le pose, pas ses effets.
  const EffectNode node = HealRing();
  CHECK(node.ring_step_degrees == Catch::Approx(kEffectNodeDefaultRingStepDegrees));

  const std::vector<EffectWorldQuad> quads =
      ComposeEffectNodeRingBand(node, glm::vec3{0.0F, 0.0F, 0.0F}, ro::render::kEffectRingUpright);
  CHECK(quads.size() == 10);

  EffectNode fin = node;
  fin.ring_step_degrees = 10.0F;
  CHECK(ComposeEffectNodeRingBand(fin, glm::vec3{0.0F, 0.0F, 0.0F}, ro::render::kEffectRingUpright)
            .size() == 36);
}

TEST_CASE("Anneau : il SE FERME, le dernier segment rejoint le premier", "[render][effectnode]") {
  // Un anneau qui ne se ferme pas laisse une fente, et une fente dans une
  // colonne de lumière se voit d'un bout à l'autre de l'écran.
  const glm::vec3 anchor{10.0F, 2.0F, -4.0F};
  const std::vector<EffectWorldQuad> quads =
      ComposeEffectNodeRingBand(HealRing(), anchor, ro::render::kEffectRingUpright);
  REQUIRE(quads.size() == 10);

  const glm::vec3 first = quads.front().corners[0];
  const glm::vec3 last = quads.back().corners[1];
  CHECK(last.x == Catch::Approx(first.x).margin(1e-4F));
  CHECK(last.y == Catch::Approx(first.y).margin(1e-4F));
  CHECK(last.z == Catch::Approx(first.z).margin(1e-4F));
}

TEST_CASE("Anneau : deux cercles, et la hauteur MONTE", "[render][effectnode]") {
  // Le client compte sa hauteur en ordonnée négative parce que son repère
  // descend. Le nôtre monte : reprendre son signe enfoncerait la colonne de
  // Heal sous le sol au lieu de la dresser.
  const glm::vec3 anchor{0.0F, 1.0F, 0.0F};
  const std::vector<EffectWorldQuad> quads =
      ComposeEffectNodeRingBand(HealRing(), anchor, ro::render::kEffectRingUpright);
  REQUIRE_FALSE(quads.empty());

  const EffectWorldQuad& quad = quads.front();
  // Les deux premiers coins sont au plan du nœud, les deux autres à la hauteur.
  CHECK(quad.corners[0].y == Catch::Approx(anchor.y));
  CHECK(quad.corners[1].y == Catch::Approx(anchor.y));
  CHECK(quad.corners[2].y == Catch::Approx(anchor.y + 35.0F));
  CHECK(quad.corners[3].y == Catch::Approx(anchor.y + 35.0F));

  for (const glm::vec3& corner : quad.corners) {
    CHECK(RadiusOf(corner, anchor) == Catch::Approx(4.5F).margin(1e-4F));
  }
}

TEST_CASE("Anneau : des rayons qui diffèrent font un CÔNE", "[render][effectnode]") {
  // Sonic Blow monte de quatre à sept : c'est un évasement, et le confondre
  // avec un cylindre lui ôterait sa forme.
  EffectNode node;
  node.radius_at_anchor.half_size = 4.0F;
  node.radius_at_height.half_size = 7.0F;
  node.half_height.half_size = 3.5F;

  const glm::vec3 anchor{0.0F, 0.0F, 0.0F};
  const std::vector<EffectWorldQuad> quads =
      ComposeEffectNodeRingBand(node, anchor, ro::render::kEffectRingUpright);
  REQUIRE_FALSE(quads.empty());

  CHECK(RadiusOf(quads.front().corners[0], anchor) == Catch::Approx(4.0F).margin(1e-4F));
  CHECK(RadiusOf(quads.front().corners[3], anchor) == Catch::Approx(7.0F).margin(1e-4F));
}

TEST_CASE("Anneau : trois colonnes emboîtées se rangent par leur cercle du SOL",
          "[render][effectnode]") {
  // EF_FIREPILLARON : trois bandes, du tour un au tour trois, de rayons
  // `tour + 1,5` à l'ancre et `tour + 3` en haut, de hauteur
  // `(4 − tour)(6 − 2·tour) + 3,5` — la plus étroite est la plus haute.
  struct Band {
    float radius_at_anchor;
    float radius_at_height;
    float height;
  };
  constexpr std::array<Band, 3> kBands = {
      {{2.5F, 4.0F, 27.5F}, {3.5F, 5.0F, 15.5F}, {4.5F, 6.0F, 7.5F}}};
  const glm::vec3 anchor{0.0F, 0.0F, 0.0F};
  std::vector<EffectWorldQuad> quads;
  for (const Band& band : kBands) {
    EffectNode node;
    node.radius_at_anchor.half_size = band.radius_at_anchor;
    node.radius_at_height.half_size = band.radius_at_height;
    node.half_height.half_size = band.height;
    const std::vector<EffectWorldQuad> ring =
        ComposeEffectNodeRingBand(node, anchor, ro::render::kEffectRingUpright);
    quads.insert(quads.end(), ring.begin(), ring.end());
  }
  // La caméra du jeu, à quarante-cinq degrés et deux cents unités, côté −z.
  const glm::vec3 eye{0.0F, 141.4F, -141.4F};
  const std::vector<std::size_t> order = ro::render::EffectWorldQuadsBackToFront(quads, eye);

  // 🔴 TOUT SEGMENT ARRIÈRE PASSE AVANT TOUT SEGMENT AVANT, d'une colonne à
  // l'autre. Rangés par leur centre, les segments arrière de la haute colonne,
  // levés de quatorze unités vers l'œil, passaient après l'avant des deux
  // courtes : la grande colonne recouvrait les petites.
  const auto is_back = [&quads](std::size_t rank) {
    const EffectWorldQuad& quad = quads[rank];
    return (quad.corners[0].z + quad.corners[1].z) / 2.0F > 0.0F;
  };
  const auto first_front = std::ranges::find_if_not(order, is_back);
  CHECK(std::ranges::none_of(first_front, order.end(), is_back));
  CHECK(quads.front().sort_point == ro::render::EffectQuadSortPoint::kFirstEdge);
}

TEST_CASE("Anneau : la texture se répète tous les QUATRE segments", "[render][effectnode]") {
  // L'abscisse avance d'un quart par segment et repart de zéro dès qu'elle
  // atteint un : la texture fait donc plusieurs tours par tour d'anneau, et
  // non un seul.
  const std::vector<EffectWorldQuad> quads = ComposeEffectNodeRingBand(
      HealRing(), glm::vec3{0.0F, 0.0F, 0.0F}, ro::render::kEffectRingUpright);
  REQUIRE(quads.size() == 10);

  for (std::size_t index = 0; index < quads.size(); ++index) {
    const auto within = static_cast<float>(index % kEffectRingSegmentsPerTextureSpan);
    const float expected = within / static_cast<float>(kEffectRingSegmentsPerTextureSpan);
    CHECK(quads[index].texture_uv[0].x == Catch::Approx(expected));
    CHECK(quads[index].texture_uv[1].x == Catch::Approx(expected + 0.25F));
  }

  // L'ordonnée vaut UN au plan du nœud et ZÉRO à la hauteur : la texture se lit
  // à l'envers de ce qu'on attendrait, et c'est le client qui le veut.
  CHECK(quads.front().texture_uv[0].y == Catch::Approx(1.0F));
  CHECK(quads.front().texture_uv[3].y == Catch::Approx(0.0F));
}

TEST_CASE("Anneau : la teinte et l'alpha du nœud le colorent", "[render][effectnode]") {
  EffectNode node = HealRing();
  node.color = {200.0F, 100.0F, 50.0F};
  node.alpha = 90.0F;

  const std::vector<EffectWorldQuad> quads =
      ComposeEffectNodeRingBand(node, glm::vec3{0.0F, 0.0F, 0.0F}, ro::render::kEffectRingUpright);
  REQUIRE_FALSE(quads.empty());
  CHECK(quads.front().tint[0] == 200);
  CHECK(quads.front().tint[1] == 100);
  CHECK(quads.front().tint[2] == 50);
  CHECK(quads.front().tint[3] == 90);
}

TEST_CASE("Anneau : un pas nul ne rend RIEN plutôt qu'un tour sans fin", "[render][effectnode]") {
  // Le client ne rencontre jamais ce cas parce que son constructeur pose
  // toujours trente-six degrés. Chez nous, un patron qui oublierait le pas
  // ferait tourner la boucle pour toujours.
  EffectNode node = HealRing();
  node.ring_step_degrees = 0.0F;
  CHECK(ComposeEffectNodeRingBand(node, glm::vec3{0.0F, 0.0F, 0.0F}, ro::render::kEffectRingUpright)
            .empty());

  node.ring_step_degrees = -36.0F;
  CHECK(ComposeEffectNodeRingBand(node, glm::vec3{0.0F, 0.0F, 0.0F}, ro::render::kEffectRingUpright)
            .empty());
}

TEST_CASE("Anneau : l'orientation place le plan et la hauteur", "[render][effectnode]") {
  // Le client refait sa matrice à chaque tic depuis deux angles ; tant que ce
  // mouvement n'est pas porté, l'anneau la reçoit. Une orientation couchée sur
  // le flanc doit donc coucher la colonne.
  const ro::render::EffectRingOrientation on_its_side{
      glm::vec3{1.0F, 0.0F, 0.0F}, glm::vec3{0.0F, 0.0F, 1.0F}, glm::vec3{0.0F, 1.0F, 0.0F},
      glm::vec3{0.0F, 0.0F, 0.0F}};

  const glm::vec3 anchor{0.0F, 0.0F, 0.0F};
  const std::vector<EffectWorldQuad> quads =
      ComposeEffectNodeRingBand(HealRing(), anchor, on_its_side);
  REQUIRE_FALSE(quads.empty());

  // La hauteur part maintenant en z, et le plan du cercle contient l'axe y.
  CHECK(quads.front().corners[3].z == Catch::Approx(35.0F));
  CHECK(quads.front().corners[3].y == Catch::Approx(0.0F).margin(1e-4F));
}

TEST_CASE("Anneau : la quatrième colonne de l'orientation DÉPLACE l'anneau",
          "[render][effectnode]") {
  const ro::render::EffectRingOrientation ecarte{
      glm::vec3{1.0F, 0.0F, 0.0F}, glm::vec3{0.0F, 1.0F, 0.0F}, glm::vec3{0.0F, 0.0F, 1.0F},
      glm::vec3{3.0F, -2.0F, 1.0F}};

  const glm::vec3 anchor{0.0F, 0.0F, 0.0F};
  const std::vector<EffectWorldQuad> straight =
      ComposeEffectNodeRingBand(HealRing(), anchor, ro::render::kEffectRingUpright);
  const std::vector<EffectWorldQuad> shifted =
      ComposeEffectNodeRingBand(HealRing(), anchor, ecarte);
  REQUIRE(straight.size() == shifted.size());

  CHECK(shifted.front().corners[0].x == Catch::Approx(straight.front().corners[0].x + 3.0F));
  CHECK(shifted.front().corners[0].y == Catch::Approx(straight.front().corners[0].y - 2.0F));
  CHECK(shifted.front().corners[0].z == Catch::Approx(straight.front().corners[0].z + 1.0F));
}

TEST_CASE("Couronne : elle est PLATE, et creusée jusqu'au rayon de l'ancre",
          "[render][effectnode]") {
  // L'onde de Magnum Break : elle part du lanceur et s'écarte de presque deux
  // unités par tic, creusée sur cinq d'épaisseur.
  EffectNode node;
  node.ring_radius.half_size = 20.0F;
  node.radius_at_anchor.half_size = 5.0F;

  const glm::vec3 anchor{0.0F, 3.0F, 0.0F};
  const std::vector<EffectWorldQuad> quads =
      ro::render::ComposeEffectNodeRingCylinder(node, anchor, ro::render::kEffectRingUpright);
  REQUIRE(quads.size() == 10);

  const EffectWorldQuad& quad = quads.front();
  // LES QUATRE COINS SONT AU MÊME PLAN : le client multiplie par zéro les
  // colonnes qui porteraient la hauteur, et confondre cette figure avec un
  // cylindre la dresserait.
  for (const glm::vec3& corner : quad.corners) {
    CHECK(corner.y == Catch::Approx(anchor.y));
  }
  // Le bord intérieur est à vingt moins cinq, l'extérieur à vingt.
  CHECK(RadiusOf(quad.corners[0], anchor) == Catch::Approx(15.0F).margin(1e-4F));
  CHECK(RadiusOf(quad.corners[3], anchor) == Catch::Approx(20.0F).margin(1e-4F));
}

TEST_CASE("Couronne : tant que le rayon n'a pas dépassé le plafond, c'est un DISQUE",
          "[render][effectnode]") {
  // La largeur est le PLUS PETIT des deux, et le client la recalcule à chaque
  // tic : une onde naissante est pleine, et se creuse en s'écartant.
  EffectNode node;
  node.radius_at_anchor.half_size = 5.0F;
  node.ring_radius.half_size = 3.0F;

  const glm::vec3 anchor{0.0F, 0.0F, 0.0F};
  const std::vector<EffectWorldQuad> quads =
      ro::render::ComposeEffectNodeRingCylinder(node, anchor, ro::render::kEffectRingUpright);
  REQUIRE_FALSE(quads.empty());
  // Bord intérieur au centre : le trou s'est refermé.
  CHECK(RadiusOf(quads.front().corners[0], anchor) == Catch::Approx(0.0F).margin(1e-4F));
  CHECK(RadiusOf(quads.front().corners[3], anchor) == Catch::Approx(3.0F).margin(1e-4F));
}

TEST_CASE("Couronne : son rayon S'ÉTEND au fil des tics", "[render][effectnode]") {
  // Sans cela, l'onde resterait un point : la plupart des effets ne posent que
  // son PAS, et laissent le rayon partir de zéro.
  EffectNode node;
  node.ring_radius.growth_per_tick = 1.75F;
  node.radius_at_anchor.half_size = 5.0F;
  node.life_ticks = 20;

  for (int tick = 0; tick < 4; ++tick) {
    CHECK(ro::render::AdvanceEffectNode(node));
  }
  CHECK(node.ring_radius.half_size == Catch::Approx(4.0F * 1.75F));
}

TEST_CASE("Plans croisés : DEUX quads perpendiculaires, pas un", "[render][effectnode]") {
  // Une croix de deux plans se voit sous presque tous les angles, là où un seul
  // disparaît quand on le regarde par la tranche. Un seul quad ferait une
  // étincelle qui s'éteint dès que la caméra tourne.
  EffectNode node;
  node.half_height.half_size = 3.0F;
  node.half_width.half_size = 5.0F;

  const glm::vec3 anchor{0.0F, 0.0F, 0.0F};
  const std::vector<EffectWorldQuad> quads =
      ro::render::ComposeEffectNodeCrossedPlanes(node, anchor, ro::render::kEffectRingUpright);
  REQUIRE(quads.size() == 2);

  // Le premier s'étend en ABSCISSE, le second en ORDONNÉE, et les deux le long
  // de l'axe qu'ils partagent.
  for (const glm::vec3& corner : quads[0].corners) {
    CHECK(std::abs(corner.x) == Catch::Approx(3.0F));
    CHECK(corner.y == Catch::Approx(0.0F));
    CHECK(std::abs(corner.z) == Catch::Approx(5.0F));
  }
  for (const glm::vec3& corner : quads[1].corners) {
    CHECK(corner.x == Catch::Approx(0.0F));
    CHECK(std::abs(corner.y) == Catch::Approx(3.0F));
    CHECK(std::abs(corner.z) == Catch::Approx(5.0F));
  }
}

TEST_CASE("Plans croisés : les demi-extensions sont CROISÉES", "[render][effectnode]") {
  // Le champ que le client nomme demi-HAUTEUR porte l'extension du premier axe,
  // et la demi-LARGEUR celle de l'axe partagé, sur ses huit points — et les
  // intervertir rendrait chaque étincelle
  // rectangulaire dans le mauvais sens.
  EffectNode node;
  node.half_height.half_size = 1.0F;
  node.half_width.half_size = 9.0F;

  const std::vector<EffectWorldQuad> quads = ro::render::ComposeEffectNodeCrossedPlanes(
      node, glm::vec3{0.0F, 0.0F, 0.0F}, ro::render::kEffectRingUpright);
  REQUIRE(quads.size() == 2);
  CHECK(std::abs(quads[0].corners[0].x) == Catch::Approx(1.0F));
  CHECK(std::abs(quads[0].corners[0].z) == Catch::Approx(9.0F));
}

TEST_CASE("Plans croisés : les points du client passent dans notre repère",
          "[render][effectnode]") {
  // Le client pose le coin de texture (0, 1) en (−h, 0, +l) sur le premier plan
  // et en (0, −h, +l) sur le second. Notre repère retourne ordonnée et
  // profondeur : (−h, 0, −l) et (0, +h, −l). Les recopier mettait la texture
  // à l'envers sous la même orientation de nœud.
  EffectNode node;
  node.half_height.half_size = 2.0F;
  node.half_width.half_size = 7.0F;
  const std::vector<EffectWorldQuad> quads = ro::render::ComposeEffectNodeCrossedPlanes(
      node, glm::vec3{0.0F, 0.0F, 0.0F}, ro::render::kEffectRingUpright);
  REQUIRE(quads.size() == 2);
  CHECK(quads[0].corners[0].x == Catch::Approx(-2.0F));
  CHECK(quads[0].corners[0].z == Catch::Approx(-7.0F));
  CHECK(quads[1].corners[0].y == Catch::Approx(2.0F));
  CHECK(quads[1].corners[0].z == Catch::Approx(-7.0F));
}

TEST_CASE("Plans croisés : la texture couvre chaque plan en entier", "[render][effectnode]") {
  const std::vector<EffectWorldQuad> quads = ro::render::ComposeEffectNodeCrossedPlanes(
      [] {
        EffectNode node;
        node.half_height.half_size = 2.0F;
        node.half_width.half_size = 2.0F;
        return node;
      }(),
      glm::vec3{0.0F, 0.0F, 0.0F}, ro::render::kEffectRingUpright);
  REQUIRE(quads.size() == 2);
  for (const EffectWorldQuad& quad : quads) {
    CHECK(quad.texture_uv[0] == glm::vec2{0.0F, 1.0F});
    CHECK(quad.texture_uv[1] == glm::vec2{1.0F, 1.0F});
    CHECK(quad.texture_uv[2] == glm::vec2{1.0F, 0.0F});
    CHECK(quad.texture_uv[3] == glm::vec2{0.0F, 0.0F});
  }
}

TEST_CASE("Quad orienté : UN seul quad, centré sur son ancre", "[render][effectnode]") {
  // Magic Circle : trente sur trente, à plat autour du lanceur.
  EffectNode node;
  node.half_height.half_size = 30.0F;
  node.half_width.half_size = 30.0F;

  const glm::vec3 anchor{0.0F, 1.0F, 0.0F};
  const std::vector<EffectWorldQuad> quads =
      ro::render::ComposeEffectNodeOrientedQuad(node, anchor, ro::render::kEffectRingUpright);
  REQUIRE(quads.size() == 1);

  // Centré : autant au-dessus qu'au-dessous de l'ancre.
  CHECK(quads[0].corners[0].y == Catch::Approx(anchor.y + 30.0F));
  CHECK(quads[0].corners[2].y == Catch::Approx(anchor.y - 30.0F));
  CHECK(quads[0].texture_uv[0] == glm::vec2{0.0F, 0.0F});
  CHECK(quads[0].texture_uv[2] == glm::vec2{1.0F, 1.0F});
}

TEST_CASE("Quad orienté : un drapeau le pose AU-DESSUS de l'ancre", "[render][effectnode]") {
  // Glass Wall se dresse sur quarante au-dessus du sol ; centré, il en
  // enfoncerait la moitié.
  //
  // ⚠ Le repère du client DESCEND et le nôtre MONTE : ce qu'il pose en ordonnée
  // négative se pose chez nous en positive.
  EffectNode node;
  node.half_height.half_size = 40.0F;
  node.half_width.half_size = 3.0F;
  node.flags = ro::render::kEffectNodeQuadSitsAboveAnchor;

  const glm::vec3 anchor{0.0F, 0.0F, 0.0F};
  const std::vector<EffectWorldQuad> quads =
      ro::render::ComposeEffectNodeOrientedQuad(node, anchor, ro::render::kEffectRingUpright);
  REQUIRE(quads.size() == 1);
  CHECK(quads[0].corners[0].y == Catch::Approx(40.0F));
  CHECK(quads[0].corners[2].y == Catch::Approx(0.0F));
}

TEST_CASE("Quad orienté : PENDU, sa demi-hauteur double", "[render][effectnode]") {
  // Le client n'y décale pas le centre : il double l'extension et ramène l'autre
  // borne à l'ancre.
  EffectNode node;
  node.half_height.half_size = 10.0F;
  node.half_width.half_size = 2.0F;
  node.flags = ro::render::kEffectNodeQuadHangsFromAnchor;

  const std::vector<EffectWorldQuad> quads = ro::render::ComposeEffectNodeOrientedQuad(
      node, glm::vec3{0.0F, 0.0F, 0.0F}, ro::render::kEffectRingUpright);
  REQUIRE(quads.size() == 1);
  CHECK(quads[0].corners[0].y == Catch::Approx(20.0F));
  CHECK(quads[0].corners[2].y == Catch::Approx(0.0F));
}

TEST_CASE("Quad orienté : sa texture peut n'être qu'une TRANCHE", "[render][effectnode]") {
  // C'est ainsi qu'une planche porte plusieurs motifs empilés : le quad n'en
  // prend qu'une bande, et prendre l'image entière les montrerait tous.
  EffectNode node;
  node.half_height.half_size = 4.0F;
  node.half_width.half_size = 4.0F;
  node.texture_v_min = 0.25F;
  node.texture_v_max = 0.5F;

  const std::vector<EffectWorldQuad> quads = ro::render::ComposeEffectNodeOrientedQuad(
      node, glm::vec3{0.0F, 0.0F, 0.0F}, ro::render::kEffectRingUpright);
  REQUIRE(quads.size() == 1);
  CHECK(quads[0].texture_uv[0].y == Catch::Approx(0.25F));
  CHECK(quads[0].texture_uv[2].y == Catch::Approx(0.5F));
  // L'abscisse, elle, prend toujours l'image entière.
  CHECK(quads[0].texture_uv[0].x == Catch::Approx(0.0F));
  CHECK(quads[0].texture_uv[1].x == Catch::Approx(1.0F));
}

TEST_CASE("Quad orienté : ses coins passent par S, largeur comprise", "[render][effectnode]") {
  // 🔴 LE COIN D'ABSCISSE DE TEXTURE NULLE, EN HAUT, a chez le client la largeur
  // POSITIVE : passé par `S`, il a chez nous la négative. Ne retourner que la
  // hauteur faisait du quad le miroir du sien, texture inversée de gauche à
  // droite.
  EffectNode node;
  node.half_height.half_size = 5.0F;
  node.half_width.half_size = 3.0F;
  const std::vector<EffectWorldQuad> quads = ro::render::ComposeEffectNodeOrientedQuad(
      node, glm::vec3{0.0F, 0.0F, 0.0F}, ro::render::kEffectRingUpright);
  REQUIRE(quads.size() == 1);
  CHECK(quads[0].texture_uv[0].x == Catch::Approx(0.0F));
  CHECK(quads[0].corners[0].y == Catch::Approx(5.0F));
  CHECK(quads[0].corners[0].z == Catch::Approx(-3.0F));
  CHECK(quads[0].corners[1].z == Catch::Approx(3.0F));
}

TEST_CASE("Quad orienté : face à la caméra, la texture garde son sens à l'écran",
          "[render][effectnode]") {
  // La base de la caméra est celle du client conjuguée par `S` : la largeur
  // locale va vers la gauche de l'écran, si bien que le coin d'abscisse de
  // texture nulle, passé par `S`, tombe en haut à DROITE — là où le client le
  // pose — et que la normale regarde l'œil.
  ro::render::Camera camera;
  camera.look_at = {0.0F, 0.0F, 0.0F};
  camera.distance = 100.0F;
  camera.yaw_degrees = 35.0F;
  camera.pitch_degrees = 50.0F;
  // L'altitude du client se lit un degré de travers : sa base est celle d'un
  // œil qui plonge d'un degré de moins. Voir `EffectNodeFacingCamera`.
  ro::render::Camera as_read = camera;
  as_read.pitch_degrees -= 1.0F;
  const ro::render::BillboardBasis basis = ro::render::MakeBillboardBasis(as_read);
  const ro::render::EffectRingOrientation facing = ro::render::EffectNodeFacingCamera(camera);
  CHECK(glm::determinant(glm::mat3(facing)) == Catch::Approx(1.0F));
  CHECK(glm::dot(facing[0], basis.forward) == Catch::Approx(-1.0F));

  EffectNode node;
  node.half_height.half_size = 5.0F;
  node.half_width.half_size = 3.0F;
  const std::vector<EffectWorldQuad> quads =
      ro::render::ComposeEffectNodeOrientedQuad(node, glm::vec3{0.0F, 0.0F, 0.0F}, facing);
  REQUIRE(quads.size() == 1);
  // Depuis le centre du quad : le dégagement du terrain le soulève d'un rien.
  const glm::vec3 centre = (quads[0].corners[0] + quads[0].corners[2]) / 2.0F;
  CHECK(glm::dot(quads[0].corners[0] - centre, basis.right) == Catch::Approx(3.0F));
  CHECK(glm::dot(quads[0].corners[0] - centre, basis.up) == Catch::Approx(5.0F));
}

TEST_CASE("Orientation du client : le roulis se nie comme le cap", "[render][effectnode]") {
  // 🔴 `S·Rz(r)·S = Rz(−r)` : la rotation autour de Z du client, ramenée dans
  // notre repère, tourne dans l'autre sens. L'inclinaison, elle, ne change pas.
  const ro::render::EffectRingOrientation client =
      ro::render::EffectNodeOrientedAsClient(0.0F, 0.0F, 90.0F);
  const ro::render::EffectRingOrientation expected =
      ro::render::EffectNodeOriented(0.0F, 0.0F, -90.0F);
  for (int column = 0; column < 3; ++column) {
    for (int row = 0; row < 3; ++row) {
      CHECK(client[column][row] == Catch::Approx(expected[column][row]).margin(1e-4F));
    }
  }
  // Le quad couché par un roulis de quatre-vingt-dix degrés a sa hauteur locale
  // vers +x chez nous.
  CHECK(client[1].x == Catch::Approx(1.0F).margin(1e-4F));
}

TEST_CASE("Anneau : le tour complet reste dans ses bornes", "[render][effectnode]") {
  // Un pas qui ne divise pas trois cent soixante ne doit ni déborder du tour ni
  // laisser un segment de trop.
  EffectNode node = HealRing();
  node.ring_step_degrees = 50.0F;

  const std::vector<EffectWorldQuad> quads =
      ComposeEffectNodeRingBand(node, glm::vec3{0.0F, 0.0F, 0.0F}, ro::render::kEffectRingUpright);
  const auto attendus =
      static_cast<std::size_t>(std::floor((kEffectRingFullTurnDegrees - 50.0F) / 50.0F)) + 1;
  CHECK(quads.size() == attendus);
}

TEST_CASE("Quad orienté : face à la caméra sous son drapeau seulement", "[render][effectnode]") {
  // Le client ne bâtit la matrice depuis la caméra que sous le bit 0x40 ; sans lui, les angles du
  // nœud orientent le plan — les murs de Safety Wall, à 0x28, sont des plans FIXES.
  ro::render::Camera camera;
  camera.look_at = {0.0F, 0.0F, 0.0F};
  camera.distance = 100.0F;
  camera.yaw_degrees = 35.0F;
  camera.pitch_degrees = 50.0F;
  const ro::render::EffectRingOrientation fixed =
      ro::render::EffectOrientedQuadFacing(0x28U, camera, 30.0F, 0.0F, 0.0F);
  CHECK(fixed == ro::render::EffectNodeOriented(30.0F, 0.0F));
  CHECK(fixed != ro::render::EffectNodeFacingCamera(camera));
  const ro::render::EffectRingOrientation billboard =
      ro::render::EffectOrientedQuadFacing(0x40U, camera, 30.0F, 0.0F, 0.0F);
  CHECK(billboard == ro::render::EffectNodeFacingCamera(camera));
  // Deux murs tournés d'un quart de tour ont des normales PERPENDICULAIRES —
  // la première colonne —, et c'est ce qui ferme la boîte.
  const glm::vec3 normale_un =
      ro::render::EffectOrientedQuadFacing(0x28U, camera, 90.0F, 0.0F, 0.0F)[0];
  const glm::vec3 normal_two =
      ro::render::EffectOrientedQuadFacing(0x28U, camera, 0.0F, 0.0F, 0.0F)[0];
  CHECK(std::abs(glm::dot(normale_un, normal_two)) < 1e-5F);
}

TEST_CASE("Quad orienté : le ROULIS le couche au sol", "[render][effectnode]") {
  // 🔴 UN QUAD ORIENTÉ NAÎT DEBOUT : ses quatre coins vivent dans le plan de
  // l'abscisse locale nulle, et s'étendent en HAUTEUR et en profondeur. C'est le
  // troisième angle du nœud — le roulis — qui le couche : une rotation autour
  // de la profondeur tourne l'abscisse et la hauteur l'une dans l'autre, si bien
  // que la hauteur du quad devient horizontale.
  //
  // Sans lui, l'anneau du classement PVP se dresserait comme un panneau au lieu
  // d'être posé au sol.
  const ro::render::EffectRingOrientation debout = ro::render::EffectNodeOriented(0.0F, 0.0F, 0.0F);
  CHECK(debout[1].y == Catch::Approx(1.0F));

  const ro::render::EffectRingOrientation layer = ro::render::EffectNodeOriented(0.0F, 0.0F, 90.0F);
  // L'axe de la HAUTEUR du quad ne monte plus : il est dans le plan du sol.
  CHECK(layer[1].y == Catch::Approx(0.0F).margin(1e-5F));
  CHECK(std::abs(layer[1].x) == Catch::Approx(1.0F));
  // Et l'axe de sa largeur n'a pas bougé — la rotation laisse la profondeur.
  CHECK(layer[2].z == Catch::Approx(1.0F));

  // ⚠ UN ROULIS NUL NE DOIT RIEN CHANGER, sans quoi les dix-neuf quads qui ne
  // l'écrivent pas — les murs d'EF_GLASSWALL en tête — se coucheraient aussi.
  CHECK(ro::render::EffectNodeOriented(30.0F, 12.0F, 0.0F) ==
        ro::render::EffectNodeOriented(30.0F, 12.0F));

  // 🔴 ET LE ROULIS PASSE AVANT LE CAP : couché puis tourné
  // d'un quart de tour, le quad RESTE au sol. Composé dans l'autre ordre, il se
  // redresserait.
  const ro::render::EffectRingOrientation laid_and_turned =
      ro::render::EffectNodeOriented(90.0F, 0.0F, 90.0F);
  CHECK(laid_and_turned[1].y == Catch::Approx(0.0F).margin(1e-5F));
}

TEST_CASE("Quad orienté : couché, il DÉGAGE le terrain ; debout, il ne bouge pas",
          "[render][effectnode]") {
  // 🔴 LE CLIENT NE LÈVE RIEN, ET NOUS DEVONS LE FAIRE. Son récepteur passe un
  // décalage vertical nul, son bâtisseur n'écrit pas le champ de hauteur, son
  // dessinateur n'ajoute que la position du nœud : l'anneau du classement y est
  // exactement au sol. Mais nous éprouvons ces quads contre le terrain, et un
  // quad coplanaire perd cette épreuve — il s'enfonce, par morceaux.
  EffectNode node;
  node.half_width.half_size = 4.0F;
  node.half_height.half_size = 4.0F;
  const glm::vec3 anchor{0.0F, 0.0F, 0.0F};

  const std::vector<EffectWorldQuad> layer = ro::render::ComposeEffectNodeOrientedQuad(
      node, anchor, ro::render::EffectNodeOriented(0.0F, 0.0F, 90.0F));
  REQUIRE(layer.size() == 1);
  for (const glm::vec3& corner_index : layer.front().corners) {
    CHECK(corner_index.y == Catch::Approx(ro::render::kEffectFlatQuadClearance));
  }

  // ⚠ ET DEBOUT, RIEN : un mur qu'on soulève flotte, et les quatre d'EF_GLASSWALL
  // ne fermeraient plus leur boîte au ras du sol.
  const std::vector<EffectWorldQuad> debout = ro::render::ComposeEffectNodeOrientedQuad(
      node, anchor, ro::render::EffectNodeOriented(0.0F, 0.0F, 0.0F));
  REQUIRE(debout.size() == 1);
  float lowest = debout.front().corners[0].y;
  for (const glm::vec3& corner_index : debout.front().corners) {
    lowest = std::min(lowest, corner_index.y);
  }
  CHECK(lowest == Catch::Approx(-node.half_height.half_size));
}

namespace {

// La caméra des essais du quad orienté : un lacet et un tangage quelconques.
ro::render::Camera TiltedCamera() {
  ro::render::Camera camera;
  camera.look_at = {0.0F, 0.0F, 0.0F};
  camera.distance = 100.0F;
  camera.yaw_degrees = 35.0F;
  camera.pitch_degrees = 50.0F;
  return camera;
}

// LA BASE CONTINUE QUE LE CLIENT LIT pour `TiltedCamera` : ses angles sont
// entiers, le lacet plus le quart de tour reste positif, et l'altitude —
// négative — se lit un degré de travers.
ro::render::BillboardBasis TiltedBasisAsRead() {
  ro::render::Camera camera = TiltedCamera();
  camera.pitch_degrees -= 1.0F;
  return ro::render::MakeBillboardBasis(camera);
}

void CheckSameColumns(const ro::render::EffectRingOrientation& actual,
                      const ro::render::EffectRingOrientation& expected) {
  for (int column = 0; column < 3; ++column) {
    for (int row = 0; row < 3; ++row) {
      CHECK(actual[column][row] == Catch::Approx(expected[column][row]).margin(1e-4F));
    }
  }
}

}  // namespace

TEST_CASE("Quad face à la caméra : une inclinaison nulle rend la base de la caméra seule",
          "[render][effectnode]") {
  const ro::render::BillboardBasis basis = TiltedBasisAsRead();
  const ro::render::EffectRingOrientation facing =
      ro::render::EffectNodeFacingCamera(TiltedCamera(), 0.0F);
  CheckSameColumns(facing, ro::render::EffectRingOrientation{-basis.forward, basis.up, -basis.right,
                                                             glm::vec3{0.0F}});
  CHECK(facing[3] == glm::vec3{0.0F});
}

TEST_CASE("Quad face à la caméra : aux angles entiers lus sans travers, la base continue",
          "[render][effectnode]") {
  // Tangage négatif ou nul et lacet d'au plus un quart de tour : l'altitude du
  // client et son lacet plus le quart de tour sont positifs, et la table les
  // rend exacts.
  for (const float yaw : {-80.0F, 0.0F, 35.0F, 90.0F}) {
    for (const float pitch : {-30.0F, -1.0F, 0.0F}) {
      ro::render::Camera camera = TiltedCamera();
      camera.yaw_degrees = yaw;
      camera.pitch_degrees = pitch;
      const ro::render::BillboardBasis basis = ro::render::MakeBillboardBasis(camera);
      const ro::render::EffectRingOrientation facing = ro::render::EffectNodeFacingCamera(camera);
      const std::array<glm::vec3, 3> expected = {-basis.forward, basis.up, -basis.right};
      for (int column = 0; column < 3; ++column) {
        for (int row = 0; row < 3; ++row) {
          CHECK(facing[column][row] ==
                Catch::Approx(expected[static_cast<std::size_t>(column)][row]).margin(1e-5F));
        }
      }
    }
  }
}

TEST_CASE("Quad face à la caméra : les angles de la caméra se tronquent au degré",
          "[render][effectnode]") {
  // 🔴 C'EST LA SOMME du lacet du client et du quart de tour qui se tronque,
  // vers zéro : 90 − 35,3 = 54,7 se lit 54, comme 90 − 36 ; et l'altitude
  // −50,6 se lit −50, comme −50.
  ro::render::Camera fractional = TiltedCamera();
  fractional.yaw_degrees = 35.3F;
  fractional.pitch_degrees = 50.6F;
  ro::render::Camera whole = TiltedCamera();
  whole.yaw_degrees = 36.0F;
  CHECK(ro::render::EffectNodeFacingCamera(fractional, 20.0F) ==
        ro::render::EffectNodeFacingCamera(whole, 20.0F));
  CHECK(ro::render::EffectNodeFacingCamera(fractional) !=
        ro::render::EffectNodeFacingCamera(TiltedCamera()));
  // Négative, la somme se tronque aussi vers zéro : 90 − 120,4 = −30,4 se lit
  // −30, comme 90 − 120, et non −31.
  fractional.yaw_degrees = 120.4F;
  whole.yaw_degrees = 120.0F;
  CHECK(ro::render::EffectNodeFacingCamera(fractional) ==
        ro::render::EffectNodeFacingCamera(whole));
}

TEST_CASE("Quad orienté sous 0x400 : le lacet suit l'angle du corps, plus le pas du tic",
          "[render][effectnode]") {
  // Le même bit tue le nœud à l'arrivée : un bit, deux effets du même mouvement.
  STATIC_CHECK(ro::render::kEffectOrientedQuadYawFollowsBody == 0x400U);
  // L'angle du corps est du client, que `S` nie ; le pas est déjà nôtre.
  CHECK(ro::render::EffectOrientedQuadYawDegrees(0x400U, 77.0F, 30.0F, 2.0F) == -28.0F);
  CHECK(ro::render::EffectOrientedQuadYawDegrees(0x28U, 77.0F, 30.0F, 2.0F) == 77.0F);
  const ro::render::Camera camera = TiltedCamera();
  CHECK(ro::render::EffectOrientedQuadFacing(0x400U, camera, 77.0F, 10.0F, 0.0F, 30.0F, 2.0F) ==
        ro::render::EffectNodeOriented(-28.0F, 10.0F, 0.0F));
  // Sous 0x40, le lacet est réécrit depuis la caméra : le corps ne compte plus.
  CHECK(ro::render::EffectOrientedQuadFacing(0x440U, camera, 77.0F, 10.0F, 0.0F, 30.0F, 2.0F) ==
        ro::render::EffectNodeFacingCamera(camera, 10.0F));
}

TEST_CASE("Quad face à la caméra : l'inclinaison du nœud tourne ses axes locaux avant la caméra",
          "[render][effectnode]") {
  const ro::render::Camera camera = TiltedCamera();
  const ro::render::BillboardBasis basis = TiltedBasisAsRead();
  const glm::vec3 to_eye = -basis.forward;
  const glm::vec3 to_left = -basis.right;
  const glm::vec3 to_bottom = -basis.up;
  // Un quart de tour autour de l'abscisse locale : la normale reste vers l'œil,
  // la hauteur locale part vers la gauche de l'écran, la profondeur locale vers
  // le bas.
  CheckSameColumns(ro::render::EffectNodeFacingCamera(camera, 90.0F),
                   ro::render::EffectRingOrientation{to_eye, to_left, to_bottom, glm::vec3{0.0F}});
  // Trente degrés : la hauteur locale se partage entre le haut et la gauche de
  // l'écran, selon le cosinus et le sinus.
  const ro::render::EffectRingOrientation thirty =
      ro::render::EffectNodeFacingCamera(camera, 30.0F);
  const float cos_thirty = std::cos(glm::radians(30.0F));
  const float sin_thirty = std::sin(glm::radians(30.0F));
  CheckSameColumns(thirty, ro::render::EffectRingOrientation{
                               to_eye, cos_thirty * basis.up + sin_thirty * to_left,
                               cos_thirty * to_left - sin_thirty * basis.up, glm::vec3{0.0F}});
  CHECK(glm::determinant(glm::mat3(thirty)) == Catch::Approx(1.0F));
}

TEST_CASE("Quad orienté sous 0x200 : l'altitude de la caméra le penche, lue en radians",
          "[render][effectnode]") {
  ro::render::Camera camera = TiltedCamera();
  camera.pitch_degrees = 45.0F;
  // Calculé à la main : le client tourne autour de z de son altitude, moins
  // quarante-cinq, prise pour des RADIANS ; ramenée par `S`, c'est une rotation
  // de plus quarante-cinq radians autour de notre z. Angle du corps nul.
  constexpr float kAltitudeTakenAsRadians = 45.0F;
  const float cos_turn = std::cos(kAltitudeTakenAsRadians);
  const float sin_turn = std::sin(kAltitudeTakenAsRadians);
  CheckSameColumns(ro::render::EffectNodeLeaningByCameraAltitude(camera, 0.0F),
                   ro::render::EffectRingOrientation{glm::vec3{cos_turn, sin_turn, 0.0F},
                                                     glm::vec3{-sin_turn, cos_turn, 0.0F},
                                                     glm::vec3{0.0F, 0.0F, 1.0F}, glm::vec3{0.0F}});
  // Un angle du corps d'un quart de tour, dans le repère du client : la
  // profondeur locale, que la rotation autour de z laisse intacte, part vers −x
  // chez nous.
  const ro::render::EffectRingOrientation turned =
      ro::render::EffectNodeLeaningByCameraAltitude(camera, 90.0F);
  CHECK(turned[2].x == Catch::Approx(-1.0F).margin(1e-4F));
  CHECK(turned[2].y == Catch::Approx(0.0F).margin(1e-4F));
  CHECK(turned[2].z == Catch::Approx(0.0F).margin(1e-4F));
  // La caméra n'y entre que par son tangage : son lacet ne change rien.
  camera.yaw_degrees = -120.0F;
  CheckSameColumns(ro::render::EffectNodeLeaningByCameraAltitude(camera, 90.0F), turned);
}

TEST_CASE("Quad orienté : 0x200 l'emporte sur 0x40, et garde son autre sens ailleurs",
          "[render][effectnode]") {
  const ro::render::Camera camera = TiltedCamera();
  // Le même bit garde la teinte du calque pour un sprite : deux sens, un bit.
  STATIC_CHECK(ro::render::kEffectOrientedQuadLeansByCameraAltitude ==
               ro::render::kEffectNodeKeepsLayerColor);
  const ro::render::EffectRingOrientation both =
      ro::render::EffectOrientedQuadFacing(0x240U, camera, 30.0F, 20.0F, 10.0F, 0.0F);
  CHECK(both == ro::render::EffectNodeLeaningByCameraAltitude(camera, 0.0F));
  CHECK(both != ro::render::EffectNodeFacingCamera(camera, 20.0F));
  // Sous 0x40 seul, l'inclinaison du nœud compte.
  CHECK(ro::render::EffectOrientedQuadFacing(0x40U, camera, 30.0F, 20.0F, 10.0F) ==
        ro::render::EffectNodeFacingCamera(camera, 20.0F));
}
