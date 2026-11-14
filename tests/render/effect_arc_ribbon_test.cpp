#include "ro/render/effect_arc_ribbon.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_track.h"

using ro::render::ComposeArcRibbonQuads;
using ro::render::EffectArcRibbonStyleFor;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;

namespace {

// Les cases que cette figure emploie, telles qu'elle les nomme chez elle.
constexpr std::size_t kSlotOfSpinDegrees = 8;
constexpr std::size_t kSlotOfShape = 10;
constexpr std::size_t kMarkOfTexture = 0;
constexpr std::size_t kMarkOfStyle = 2;

// La marque la plus simple qui dessine : une couche blanche, l'ordre des coins
// gardé, aucun décalage.
constexpr std::uint8_t kPlainMark = 0;
// Une marque qui ne dessine rien.
constexpr std::uint8_t kSilentMark = 1;

EffectTrack LivingRibbon(std::uint8_t mark = kPlainMark) {
  EffectTrack track;
  track.active = true;
  track.alpha = 200;
  track.radius_at_anchor = 10.0F;
  track.degrees = 0;
  track.mark[kMarkOfStyle] = mark;
  return track;
}

float AreaOf(const EffectWorldQuad& quad) {
  return glm::length(
      glm::cross(quad.corners[1] - quad.corners[0], quad.corners[3] - quad.corners[0]));
}

}  // namespace

TEST_CASE("Arc de ruban : une marque MUETTE ne dessine rien", "[render][arcribbon]") {
  // Dix-neuf marques sur trente-trois ne dessinent rien du tout. Poser un quad
  // blanc à leur place montrerait ce que le client cache, et un quad blanc
  // ressemble à une figure ratée bien plus qu'à une absence voulue.
  CHECK(EffectArcRibbonStyleFor(kSilentMark).layers.empty());
  CHECK(EffectArcRibbonStyleFor(25).layers.empty());
  CHECK(ComposeArcRibbonQuads(LivingRibbon(kSilentMark), 0, 0).empty());
}

TEST_CASE("Arc de ruban : une piste éteinte ou transparente ne dessine rien",
          "[render][arcribbon]") {
  EffectTrack faded = LivingRibbon();
  faded.active = false;
  CHECK(ComposeArcRibbonQuads(faded, 0, 0).empty());

  EffectTrack transparente = LivingRibbon();
  transparente.alpha = 0;
  CHECK(ComposeArcRibbonQuads(transparente, 0, 0).empty());
}

TEST_CASE("Arc de ruban : la marque décide COMBIEN DE COUCHES", "[render][arcribbon]") {
  // C'est tout le propos de ce dessinateur. Le client enchaîne ses cas sans
  // rupture : chacun ajoute sa couleur par-dessus toutes les suivantes, et c'est
  // ainsi qu'une même figure va du discret à l'éclatant sans qu'aucun alpha ne
  // change. Lire le switch comme un choix d'une couleur parmi six donnerait six
  // figures pâles au lieu d'une progression.
  CHECK(EffectArcRibbonStyleFor(15).layers.size() == 1);
  CHECK(EffectArcRibbonStyleFor(12).layers.size() == 2);
  CHECK(EffectArcRibbonStyleFor(10).layers.size() == 3);
  CHECK(EffectArcRibbonStyleFor(5).layers.size() == 4);
  CHECK(EffectArcRibbonStyleFor(4).layers.size() == 5);
  CHECK(EffectArcRibbonStyleFor(2).layers.size() == 6);

  // Et la pile est la MÊME : la marque basse prend la haute, plus la sienne.
  CHECK(EffectArcRibbonStyleFor(2).layers.back().color ==
        EffectArcRibbonStyleFor(15).layers.back().color);
  CHECK(ComposeArcRibbonQuads(LivingRibbon(2), 0, 0).size() == 6);
}

TEST_CASE("Arc de ruban : deux marques écrasent au lieu de s'ajouter", "[render][arcribbon]") {
  // Les marques zéro et trois passent le mode un — le mélange ordinaire — là où
  // toutes les autres additionnent. Les uniformiser rendrait transparentes deux
  // figures que le client peint pleines, ou l'inverse.
  CHECK(EffectArcRibbonStyleFor(0).layers.front().blend_mode == 1);
  CHECK(EffectArcRibbonStyleFor(3).layers.front().blend_mode == 1);
  CHECK(EffectArcRibbonStyleFor(16).layers.front().blend_mode == 1);
  CHECK(EffectArcRibbonStyleFor(17).layers.front().blend_mode == 0);

  const std::vector<EffectWorldQuad> ordinary = ComposeArcRibbonQuads(LivingRibbon(0), 0, 0);
  REQUIRE(ordinary.size() == 1);
  CHECK(ordinary.front().blend_destination == 6);

  const std::vector<EffectWorldQuad> additif = ComposeArcRibbonQuads(LivingRibbon(17), 0, 0);
  REQUIRE(additif.size() == 1);
  CHECK(additif.front().blend_destination == 2);
}

TEST_CASE("Arc de ruban : deux marques seulement GARDENT l'ordre des coins",
          "[render][arcribbon]") {
  // L'échange des paires est une autre figure, pas un détail : la texture s'y
  // pose retournée.
  CHECK_FALSE(EffectArcRibbonStyleFor(0).swaps_corners);
  CHECK_FALSE(EffectArcRibbonStyleFor(3).swaps_corners);
  CHECK(EffectArcRibbonStyleFor(2).swaps_corners);
  CHECK(EffectArcRibbonStyleFor(16).swaps_corners);

  EffectTrack gardee = LivingRibbon(0);
  EffectTrack echangee = LivingRibbon(17);
  const std::vector<EffectWorldQuad> one = ComposeArcRibbonQuads(gardee, 0, 0);
  const std::vector<EffectWorldQuad> other = ComposeArcRibbonQuads(echangee, 0, 0);
  REQUIRE(!one.empty());
  REQUIRE(!other.empty());
  CHECK(one.front().corners[0].x == Catch::Approx(other.front().corners[1].x));
  CHECK(one.front().corners[1].x == Catch::Approx(other.front().corners[0].x));
}

TEST_CASE("Arc de ruban : le QUATRIÈME CÔTÉ est un arc plus large", "[render][arcribbon]") {
  // C'est de là que vient le nom. Trois écarts choisis par la piste, et le
  // quatrième côté prend ce qui reste du tour : un quad régulier ne rendrait pas
  // cette figure.
  EffectTrack cerf_volant = LivingRibbon();
  cerf_volant.value[kSlotOfShape] = 1.0F;
  const std::vector<EffectWorldQuad> quads = ComposeArcRibbonQuads(cerf_volant, 0, 0);
  REQUIRE(!quads.empty());

  const std::array<glm::vec3, 4>& corners = quads.front().corners;
  const float ferme = glm::length(corners[1] - corners[0]);
  const float large = glm::length(corners[0] - corners[3]);
  CHECK(large > ferme);
}

TEST_CASE("Arc de ruban : les trois FORMES diffèrent", "[render][arcribbon]") {
  // Le bâtisseur pose zéro, un ou deux, et rien d'autre. Une valeur inconnue
  // laisserait les quatre coins au même angle, donc un quad sans aire : le carré
  // sert de défaut plutôt que de laisser la figure disparaître.
  EffectTrack world_quad = LivingRibbon();
  EffectTrack cerf_volant = LivingRibbon();
  cerf_volant.value[kSlotOfShape] = 1.0F;
  EffectTrack aplati = LivingRibbon();
  aplati.value[kSlotOfShape] = 2.0F;
  EffectTrack inconnue = LivingRibbon();
  inconnue.value[kSlotOfShape] = 7.0F;

  const float square_area = AreaOf(ComposeArcRibbonQuads(world_quad, 0, 0).front());
  const float kite_area = AreaOf(ComposeArcRibbonQuads(cerf_volant, 0, 0).front());
  const float flat_area = AreaOf(ComposeArcRibbonQuads(aplati, 0, 0).front());
  const float default_area = AreaOf(ComposeArcRibbonQuads(inconnue, 0, 0).front());

  CHECK(square_area > 0.0F);
  CHECK(kite_area > 0.0F);
  CHECK(flat_area > 0.0F);
  CHECK(kite_area != Catch::Approx(square_area));
  CHECK(flat_area != Catch::Approx(square_area));
  CHECK(default_area == Catch::Approx(square_area));
}

TEST_CASE("Arc de ruban : le rayon RESPIRE avec l'angle propre", "[render][arcribbon]") {
  // Le battement de la figure, et il ne vient pas du mouvement mais d'ici : le
  // rayon s'écarte de lui-même au rythme de l'angle que le mouvement fait
  // tourner. Le retirer fige la figure sans rien casser d'autre, ce qui est
  // exactement le genre de perte qui ne se voit pas en test unitaire.
  EffectTrack au_repos = LivingRibbon();
  au_repos.value[kSlotOfSpinDegrees] = 0.0F;
  EffectTrack gonfle = LivingRibbon();
  gonfle.value[kSlotOfSpinDegrees] = 90.0F;
  EffectTrack creuse = LivingRibbon();
  creuse.value[kSlotOfSpinDegrees] = 270.0F;

  const float repos = AreaOf(ComposeArcRibbonQuads(au_repos, 0, 0).front());
  const float top = AreaOf(ComposeArcRibbonQuads(gonfle, 0, 0).front());
  const float bottom = AreaOf(ComposeArcRibbonQuads(creuse, 0, 0).front());
  CHECK(top > repos);
  CHECK(bottom < repos);
}

TEST_CASE("Arc de ruban : la marque trois prend LES DEUX décalages", "[render][arcribbon]") {
  // Le client la fait tomber d'un décalage dans l'autre, sans rupture. N'en
  // garder qu'un poserait la figure à côté de sa place, et rien ne le dirait.
  const std::vector<EffectWorldQuad> nue = ComposeArcRibbonQuads(LivingRibbon(0), 0, 0);
  const std::vector<EffectWorldQuad> decalee = ComposeArcRibbonQuads(LivingRibbon(3), 0, 0);
  REQUIRE(!nue.empty());
  REQUIRE(!decalee.empty());

  // Les décalages sont du repère du client, dont la profondeur est l'opposée de
  // la nôtre.
  CHECK(decalee.front().corners[0].x == Catch::Approx(nue.front().corners[0].x + 3.1F));
  CHECK(decalee.front().corners[0].z == Catch::Approx(nue.front().corners[0].z - 6.0F));
}

TEST_CASE("Arc de ruban : les marques hautes se soulèvent SANS se décaler", "[render][arcribbon]") {
  const std::vector<EffectWorldQuad> nue = ComposeArcRibbonQuads(LivingRibbon(17), 0, 0);
  const std::vector<EffectWorldQuad> basse = ComposeArcRibbonQuads(LivingRibbon(2), 0, 0);
  REQUIRE(!nue.empty());
  REQUIRE(!basse.empty());

  CHECK(nue.front().corners[0].x == Catch::Approx(basse.front().corners[0].x));
  CHECK(nue.front().corners[0].z == Catch::Approx(basse.front().corners[0].z - 6.0F));
}

TEST_CASE("Arc de ruban : la TEXTURE vient de la piste", "[render][arcribbon]") {
  // Le nœud porte un tableau de textures, et la piste dit laquelle. La figer à
  // zéro montrerait la même image pour toute la famille.
  EffectTrack track = LivingRibbon(2);
  track.mark[kMarkOfTexture] = 3;
  for (const EffectWorldQuad& quad : ComposeArcRibbonQuads(track, 0, 0)) {
    CHECK(quad.texture == 3);
  }
}

TEST_CASE("Arc de ruban : l'ANCRE porte la figure, et la caméra la tourne", "[render][arcribbon]") {
  EffectTrack track = LivingRibbon();
  track.anchor = glm::vec3{5.0F, 2.0F, -7.0F};
  const std::vector<EffectWorldQuad> pose = ComposeArcRibbonQuads(track, 0, 0);
  const std::vector<EffectWorldQuad> tournee = ComposeArcRibbonQuads(track, 90, 45);
  REQUIRE(!pose.empty());
  REQUIRE(!tournee.empty());

  glm::vec3 centre{0.0F, 0.0F, 0.0F};
  for (const glm::vec3& corner : pose.front().corners) {
    centre += corner;
  }
  centre /= 4.0F;
  CHECK(centre.x == Catch::Approx(5.0F).margin(1e-4));
  CHECK(centre.y == Catch::Approx(2.0F).margin(1e-4));

  CHECK(AreaOf(tournee.front()) == Catch::Approx(AreaOf(pose.front())));
  // ⚠ LE COIN TÉMOIN NE PEUT PAS ÊTRE LE PREMIER : il est posé à l'angle zéro,
  // donc sans profondeur, et l'inclinaison mêlant la hauteur à la profondeur
  // elle ne le bouge pas. Un test pris sur lui passerait quelle que soit la
  // caméra, et ne prouverait rien.
  CHECK(pose.front().corners[1].y != Catch::Approx(tournee.front().corners[1].y));
}
