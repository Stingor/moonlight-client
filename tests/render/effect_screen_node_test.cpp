#include "ro/render/effect_screen_node.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
using ro::render::AdvanceEffectScreenNode;
using ro::render::EffectScreenFlag;
using ro::render::EffectScreenNode;
using ro::render::EffectScreenQuadRequestOf;
using ro::render::kEffectScreenUnchanged;

namespace {

[[nodiscard]] std::uint32_t FixedSeed() { return 7U; }

// Avance le noeud, et dit s'il a survecu jusqu'au bout.
bool Advance(EffectScreenNode& node, std::int32_t ticks) {
  std::uint32_t seed = FixedSeed();
  bool alive = true;
  for (std::int32_t tick = 0; tick < ticks; ++tick) {
    alive = AdvanceEffectScreenNode(node, seed) && alive;
  }
  return alive;
}

[[nodiscard]] constexpr std::uint32_t Drapeau(EffectScreenFlag flag) {
  return static_cast<std::uint32_t>(flag);
}

}  // namespace

TEST_CASE("un noeud d'ecran sans rien pose garde les defauts du client",
          "[render][effect][screen]") {
  const EffectScreenNode node;
  // ⚠ CE NE SONT PAS DES ZÉROS. Un alpha omis vaut 254, une vie omise cent tics,
  // et les trois canaux 255. Les mettre à zéro rendrait le nœud invisible.
  CHECK(node.alpha == Approx(254.0F));
  CHECK(node.life_ticks == 100);
  CHECK(node.color[0] == 255);
  CHECK(node.fade_out_tick == -1);
  CHECK(node.second_step_tick == -1);
  CHECK(node.half_width_step_after == kEffectScreenUnchanged);
}

TEST_CASE("la taille gagne son pas, et le pas sa variation", "[render][effect][screen]") {
  EffectScreenNode node;
  node.half_width = 10.0F;
  node.half_width_step = 2.0F;
  node.half_width_acceleration = 1.0F;
  (void)Advance(node, 1);
  // Le pas gagne sa variation AVANT de servir : 2 + 1 = 3, donc 10 + 3.
  CHECK(node.half_width_step == Approx(3.0F));
  CHECK(node.half_width == Approx(13.0F));
  (void)Advance(node, 1);
  CHECK(node.half_width == Approx(17.0F));
}

TEST_CASE("la sentinelle veut dire ne change pas", "[render][effect][screen]") {
  EffectScreenNode node;
  node.second_step_tick = 3;
  node.half_width_step = 6.0F;
  node.half_height_step = 5.0F;
  // ⚠ L'UNE EST POSÉE, L'AUTRE EST LA SENTINELLE. La prendre pour une valeur
  // remplacerait un pas de cinq par un dix-millième.
  node.half_width_step_after = 0.5F;
  node.half_height_step_after = kEffectScreenUnchanged;
  // La quatrième mise à jour lit l'âge trois : l'âge ne croît qu'après.
  (void)Advance(node, 4);
  CHECK(node.half_width_step == Approx(0.5F));
  CHECK(node.half_height_step == Approx(5.0F));
}

TEST_CASE("le tic du second pas se compare par EGALITE", "[render][effect][screen]") {
  EffectScreenNode node;
  node.second_step_tick = 2;
  node.half_width_step = 6.0F;
  node.half_width_step_after = 0.5F;
  (void)Advance(node, 5);
  // La bascule a eu lieu une seule fois : un « au-delà de » aurait rebasculé à
  // chaque tic, et le pas serait resté figé à un demi.
  CHECK(node.half_width_step == Approx(0.5F));
}

TEST_CASE("l'arret de croissance couche les quatre champs", "[render][effect][screen]") {
  EffectScreenNode node;
  node.flags = Drapeau(EffectScreenFlag::kStopsGrowing);
  node.growth_stop_tick = 3;
  node.half_width_step = 4.0F;
  node.half_width_acceleration = 1.0F;
  node.half_height_step = 2.0F;
  node.half_height_acceleration = 1.0F;
  (void)Advance(node, 4);
  CHECK(node.half_width_step == Approx(0.0F));
  CHECK(node.half_width_acceleration == Approx(0.0F));
  CHECK(node.half_height_step == Approx(0.0F));
  CHECK(node.half_height_acceleration == Approx(0.0F));
  // La taille atteinte, elle, reste : l'arrêt fige, il ne rembobine pas.
  const float width = node.half_width;
  (void)Advance(node, 5);
  CHECK(node.half_width == Approx(width));
}

TEST_CASE("l'ordonnee d'ecran se soustrait", "[render][effect][screen]") {
  EffectScreenNode node;
  node.step = 10.0F;
  node.heading_degrees = 0.0F;
  (void)Advance(node, 1);
  // ⚠ UN CAP NUL MONTE À L'ÉCRAN, donc l'ordonnée DIMINUE. Recopier le signe
  // du monde ferait tomber ce qui doit s'élever.
  CHECK(node.travel.x == Approx(0.0F).margin(1e-5));
  CHECK(node.travel.y == Approx(-10.0F));
  // Et un quart de tour l'envoie à DROITE : l'abscisse prend le sinus du cap.
  EffectScreenNode on_right;
  on_right.step = 10.0F;
  on_right.heading_degrees = 90.0F;
  (void)Advance(on_right, 1);
  CHECK(on_right.travel.x == Approx(10.0F));
  CHECK(on_right.travel.y == Approx(0.0F).margin(1e-5));
}

TEST_CASE("le rouge reboucle la ou le vert et le bleu saturent", "[render][effect][screen]") {
  EffectScreenNode node;
  node.color = {250, 250, 250};
  node.color_step = {10, 10, 10};
  (void)Advance(node, 1);
  // ⚠ LES DEUX BORNES DU ROUGE SONT INVERSÉES, et c'est mesuré : 260 retombe à
  // ZÉRO, quand le vert et le bleu s'arrêtent à 255. Le même écrêtage pour les
  // trois ferait un dégradé là où le client fait clignoter.
  CHECK(node.color[0] == 0);
  CHECK(node.color[1] == 255);
  CHECK(node.color[2] == 255);

  EffectScreenNode below_zero;
  below_zero.color = {5, 5, 5};
  below_zero.color_step = {-10, -10, -10};
  (void)Advance(below_zero, 1);
  CHECK(below_zero.color[0] == 255);
  CHECK(below_zero.color[1] == 0);
  CHECK(below_zero.color[2] == 0);
}

TEST_CASE("le fondu de sortie vise zero pour la fin de la vie", "[render][effect][screen]") {
  EffectScreenNode node;
  node.alpha = 200.0F;
  node.life_ticks = 30;
  node.fade_out_tick = 20;
  (void)Advance(node, 21);
  // La vingt et unième mise à jour lit l'âge vingt : il reste dix tics, le pas
  // vaut donc moins vingt par tic.
  CHECK(node.alpha_step == Approx(-20.0F));
  CHECK(node.alpha == Approx(180.0F));
  (void)Advance(node, 9);
  CHECK(node.alpha == Approx(0.0F));
}

TEST_CASE("un noeud ne survit pas a celui qui le porte", "[render][effect][screen]") {
  EffectScreenNode node;
  node.alpha = 100.0F;
  node.life_ticks = 200;
  node.owner_remaining_ticks = 10;
  node.flags = Drapeau(EffectScreenFlag::kSwingsAlpha);
  (void)Advance(node, 1);
  // ⚠ IL LUI RESTE CENT QUATRE-VINGT-DIX-NEUF TICS ET AU PORTEUR DIX : le fondu
  // s'engage, et le va-et-vient est RETIRÉ — sans quoi l'alpha remonterait.
  CHECK(node.owner_fade_engaged);
  CHECK_FALSE(ro::render::HasEffectScreenFlag(node.flags, EffectScreenFlag::kSwingsAlpha));
  CHECK(node.alpha_step == Approx(-10.0F));
  // Et il ne s'engage qu'une fois : la marque l'en empêche.
  node.alpha_step = -1.0F;
  (void)Advance(node, 1);
  CHECK(node.alpha_step == Approx(-1.0F));
}

TEST_CASE("un porteur inconnu n'engage aucun fondu", "[render][effect][screen]") {
  EffectScreenNode node;
  node.alpha = 100.0F;
  node.life_ticks = 200;
  // Zéro veut dire « je ne sais pas » : `render` ne connaît pas l'état de jeu.
  node.owner_remaining_ticks = 0;
  (void)Advance(node, 5);
  CHECK_FALSE(node.owner_fade_engaged);
  CHECK(node.alpha == Approx(100.0F));
}

TEST_CASE("l'animation boucle ou tue le noeud", "[render][effect][screen]") {
  EffectScreenNode loop;
  loop.ticks_per_frame = 2;
  loop.texture_count = 3;
  loop.frame_loops = true;
  std::uint32_t seed = FixedSeed();
  for (int tick = 0; tick < 12; ++tick) {
    CHECK(AdvanceEffectScreenNode(loop, seed));
  }
  CHECK(loop.frame < 3);

  EffectScreenNode mortel;
  mortel.ticks_per_frame = 2;
  mortel.texture_count = 2;
  mortel.frame_loops = false;
  seed = FixedSeed();
  // Deux images, une toutes les deux tics, comptées sur l'âge AVANT sa
  // croissance : l'image avance aux âges zéro et deux, il meurt au troisième.
  CHECK(AdvanceEffectScreenNode(mortel, seed));
  CHECK(AdvanceEffectScreenNode(mortel, seed));
  CHECK_FALSE(AdvanceEffectScreenNode(mortel, seed));
}

TEST_CASE("un quad dont l'image boucle meurt par sa vie, une mise a jour apres elle",
          "[render][effect][screen]") {
  EffectScreenNode node;
  node.ticks_per_frame = 1;
  node.texture_count = 2;
  node.frame_loops = true;
  node.life_ticks = 5;
  std::uint32_t seed = FixedSeed();
  for (int tick = 0; tick < 5; ++tick) {
    REQUIRE(AdvanceEffectScreenNode(node, seed));
  }
  CHECK_FALSE(AdvanceEffectScreenNode(node, seed));
}

TEST_CASE("la position suit le porteur, sauf en absolu", "[render][effect][screen]") {
  EffectScreenNode node;
  node.screen_offset = {5.0F, -20.0F};
  node.half_width = 4.0F;
  node.half_height = 4.0F;
  const glm::vec2 bearer{300.0F, 400.0F};
  CHECK(EffectScreenQuadRequestOf(node, bearer).screen_position.x == Approx(305.0F));
  CHECK(EffectScreenQuadRequestOf(node, bearer).screen_position.y == Approx(380.0F));

  node.flags = Drapeau(EffectScreenFlag::kScreenAbsolute);
  CHECK(EffectScreenQuadRequestOf(node, bearer).screen_position.x == Approx(5.0F));
  CHECK(EffectScreenQuadRequestOf(node, bearer).screen_position.y == Approx(-20.0F));
}

TEST_CASE("la demande porte l'alpha du noeud dans sa teinte", "[render][effect][screen]") {
  EffectScreenNode node;
  node.alpha = 128.0F;
  node.color = {10, 20, 30};
  node.half_width = node.half_height = 2.0F;
  const std::array<std::uint8_t, 4> expected = {10, 20, 30, 128};
  CHECK(EffectScreenQuadRequestOf(node).tint == expected);
}
