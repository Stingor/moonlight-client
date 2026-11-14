#include "ro/render/effect_node_sprite.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/act.h"
#include "ro/formats/spr.h"
#include "ro/render/effect_node.h"

#include "support/sprite_builder.h"

// LE DESSINATEUR D'EFFETS LE PLUS EMPLOYÉ du client : celui qui pose un sprite.
// Trente-quatre des soixante-quatorze effets relevés en dépendent — Sight,
// Heal, Blessing, Aspersio, Soul Strike.
using ro::formats::ActAnimation;
using ro::formats::SprSprite;
using ro::render::ComposeEffectNodeSprite;
using ro::render::EffectNode;
using ro::render::kEffectNodeKeepsLayerColor;
using ro::render::SpriteQuad;
using ro::test_support::ActActionSpec;
using ro::test_support::ActBuildOptions;
using ro::test_support::ActFrameSpec;
using ro::test_support::ActLayerSpec;
using ro::test_support::BuildAct;
using ro::test_support::BuildSpr;
using ro::test_support::SprBuildOptions;
using ro::test_support::SprIndexedFrameSpec;
using ro::test_support::SpriteColor;

namespace {

bool NearlyEqual(float actual, float expected) { return std::abs(actual - expected) < 1e-4F; }

// Une image de 4 sur 6 : dimensions PAIRES et DIFFÉRENTES, pour que toute
// confusion entre les deux axes se voie.
SprSprite OneSprite() {
  SprIndexedFrameSpec frame{.width = 4, .height = 6};
  frame.indices.assign(24, 1);
  SprBuildOptions options;
  options.indexed = {frame};
  options.palette = {SpriteColor{}, SpriteColor{.red = 255}};
  auto sprite = SprSprite::Parse(BuildSpr(options));
  REQUIRE(sprite.has_value());
  return std::move(*sprite);
}

ActAnimation AnimationOf(const ActBuildOptions& options) {
  auto animation = ActAnimation::Parse(BuildAct(options));
  REQUIRE(animation.has_value());
  return std::move(*animation);
}

ActBuildOptions OneLayerAt(const ActLayerSpec& layer) {
  ActBuildOptions options;
  options.actions = {ActActionSpec{.frames = {ActFrameSpec{.layers = {layer}}}}};
  options.speeds = {6.0F};
  return options;
}

// La largeur et la hauteur de la boîte qui contient un quad, tourné ou non.
glm::vec2 ExtentOf(const SpriteQuad& quad) {
  glm::vec2 bottom = quad.corners[0];
  glm::vec2 top = quad.corners[0];
  for (const glm::vec2& corner : quad.corners) {
    bottom = glm::min(bottom, corner);
    top = glm::max(top, corner);
  }
  return top - bottom;
}

}  // namespace

TEST_CASE("Nœud en sprite : son échelle multiplie tout", "[render][effectnode]") {
  // Le décalage d'un calque comme sa taille : le client range son échelle à
  // part de celle des calques et les compose. Ne grandir que la taille
  // laisserait les pièces à leur place et les ferait se chevaucher.
  const SprSprite sprite = OneSprite();
  const ActAnimation animation =
      AnimationOf(OneLayerAt(ActLayerSpec{.offset_x = 10.0F, .offset_y = 0.0F, .frame_index = 0}));

  EffectNode node;
  node.sprite_scale = 1.0F;
  const std::vector<SpriteQuad> plain = ComposeEffectNodeSprite(node, sprite, animation);
  REQUIRE(plain.size() == 1);
  CHECK(NearlyEqual(plain[0].corners[0].x, 8.0F));
  CHECK(NearlyEqual(plain[0].corners[2].x, 12.0F));

  node.sprite_scale = 3.0F;
  const std::vector<SpriteQuad> grown = ComposeEffectNodeSprite(node, sprite, animation);
  REQUIRE(grown.size() == 1);
  CHECK(NearlyEqual(grown[0].corners[0].x, 24.0F));
  CHECK(NearlyEqual(grown[0].corners[2].x, 36.0F));
  // La HAUTEUR grandit d'autant : une échelle qui ne toucherait qu'un axe
  // écraserait le sprite.
  CHECK(NearlyEqual(grown[0].corners[2].y - grown[0].corners[0].y, 18.0F));
}

TEST_CASE("Nœud en sprite : l'angle du calque tourne son quad", "[render][effectnode]") {
  // 🔴 LE DESSINATEUR DES SPRITES D'EFFET AJOUTE L'ANGLE DU CALQUE À CELUI DU
  // NŒUD, et la file de rendu tourne le quad de leur somme dès qu'elle n'est
  // pas nulle. Ce banc a longtemps tenu l'inverse : il prenait pour mesuré que
  // la somme retombait, parce que le rectangle remis à la file de rendu est
  // aligné aux axes — mais l'angle voyage à côté, et la file le lit.
  const SprSprite sprite = OneSprite();
  const ActAnimation animation =
      AnimationOf(OneLayerAt(ActLayerSpec{.frame_index = 0, .rotation_degrees = 90}));

  EffectNode node;
  node.draws_layer_angles = true;
  const std::vector<SpriteQuad> rotated = ComposeEffectNodeSprite(node, sprite, animation);
  REQUIRE(rotated.size() == 1);
  // Quatre de large et six de haut deviennent six de large et quatre de haut.
  CHECK(NearlyEqual(ExtentOf(rotated[0]).x, 6.0F));
  CHECK(NearlyEqual(ExtentOf(rotated[0]).y, 4.0F));

  // ET L'ANGLE DU NŒUD S'AJOUTE À CELUI DU CALQUE : un quart de tour de plus
  // fait un demi-tour, et l'image retrouve ses proportions.
  node.draws_rotated = true;
  node.spin_degrees = 90.0F;
  const std::vector<SpriteQuad> half_turn = ComposeEffectNodeSprite(node, sprite, animation);
  REQUIRE(half_turn.size() == 1);
  CHECK(NearlyEqual(ExtentOf(half_turn[0]).x, 4.0F));
  CHECK(NearlyEqual(ExtentOf(half_turn[0]).y, 6.0F));
}

TEST_CASE("Nœud en sprite : hors d'un effet, les calques restent droits", "[render][effectnode]") {
  // Un projectile tiré vers sa cible est un ACTEUR, calques redressés : seul
  // l'angle de son vol le tourne. Un nœud qui n'est pas semé par le patron d'un
  // effet garde donc ses calques d'aplomb.
  const SprSprite sprite = OneSprite();
  const ActAnimation animation =
      AnimationOf(OneLayerAt(ActLayerSpec{.frame_index = 0, .rotation_degrees = 90}));

  EffectNode node;
  node.spin_degrees = 45.0F;
  const std::vector<SpriteQuad> quads = ComposeEffectNodeSprite(node, sprite, animation);
  REQUIRE(quads.size() == 1);
  CHECK(NearlyEqual(quads[0].corners[0].x, -2.0F));
  CHECK(NearlyEqual(quads[0].corners[0].y, -3.0F));
  CHECK(NearlyEqual(quads[0].corners[2].x, 2.0F));
  CHECK(NearlyEqual(quads[0].corners[2].y, 3.0F));
}

TEST_CASE("Nœud en sprite : un quad tourne autour de SON CENTRE", "[render][effectnode]") {
  // 🔴 C'EST LA FILE DE RENDU QUI TOURNE, PAS LE CALCUL DES COINS.
  // La file de rendu compose le centre du quad —
  // `(haut + demi_hauteur)`, `(gauche + demi_largeur)` — puis pose les quatre
  // coins à ce centre plus ou moins la demi-étendue tournée. Tourner autour de
  // l'ANCRE du sprite déplace donc un quad que son calque décale : les boules de
  // Fire Ball, dont le calque est seize pixels sous l'ancre, remontaient
  // d'autant dès qu'elles se couchaient sur leur vol.
  const SprSprite sprite = OneSprite();
  const ActAnimation animation =
      AnimationOf(OneLayerAt(ActLayerSpec{.offset_x = 0.0F, .offset_y = 9.0F, .frame_index = 0}));

  EffectNode node;
  node.draws_rotated = true;
  node.spin_degrees = 90.0F;
  const std::vector<SpriteQuad> quads = ComposeEffectNodeSprite(node, sprite, animation);
  REQUIRE(quads.size() == 1);

  glm::vec2 centre{0.0F, 0.0F};
  for (const glm::vec2& corner : quads[0].corners) {
    centre += corner;
  }
  centre /= 4.0F;
  // LE CENTRE NE BOUGE PAS : il valait (0, 9) avant la rotation, le calque
  // portant l'image neuf pixels sous l'ancre.
  CHECK(NearlyEqual(centre.x, 0.0F));
  CHECK(NearlyEqual(centre.y, 9.0F));

  // ET L'IMAGE A BIEN TOURNÉ : quatre de large et six de haut deviennent six de
  // large et quatre de haut. Sans quoi le contrôle du centre passerait aussi
  // pour un quad qu'on aurait oublié de tourner.
  float left = quads[0].corners[0].x;
  float right = left;
  float top = quads[0].corners[0].y;
  float bottom = top;
  for (const glm::vec2& corner : quads[0].corners) {
    left = std::min(left, corner.x);
    right = std::max(right, corner.x);
    top = std::min(top, corner.y);
    bottom = std::max(bottom, corner.y);
  }
  CHECK(NearlyEqual(right - left, 6.0F));
  CHECK(NearlyEqual(bottom - top, 4.0F));
}

TEST_CASE("Nœud en sprite : la teinte du nœud recouvre celle des calques", "[render][effectnode]") {
  // C'est ce qui fait descendre un effet en fondu : sa rampe d'alpha s'applique
  // à toutes ses pièces d'un coup, sans toucher au fichier.
  const SprSprite sprite = OneSprite();
  const ActAnimation animation =
      AnimationOf(OneLayerAt(ActLayerSpec{.frame_index = 0, .tint = {10, 20, 30, 40}}));

  EffectNode node;
  node.color = {200.0F, 100.0F, 50.0F};
  node.alpha = 128.0F;
  const std::vector<SpriteQuad> painted = ComposeEffectNodeSprite(node, sprite, animation);
  REQUIRE(painted.size() == 1);
  CHECK(painted[0].tint[0] == 200);
  CHECK(painted[0].tint[1] == 100);
  CHECK(painted[0].tint[2] == 50);
  CHECK(painted[0].tint[3] == 128);

  // Sauf quand le nœud demande le contraire : l'effet laisse alors parler les
  // couleurs de son fichier.
  node.flags = kEffectNodeKeepsLayerColor;
  const std::vector<SpriteQuad> kept = ComposeEffectNodeSprite(node, sprite, animation);
  REQUIRE(kept.size() == 1);
  CHECK(kept[0].tint[0] == 10);
  CHECK(kept[0].tint[3] == 40);
}

TEST_CASE("Nœud en sprite : l'action et l'image viennent du nœud", "[render][effectnode]") {
  // Un nœud choisit SON action et SON image : c'est ainsi qu'une même feuille
  // de sprites sert à plusieurs effets, et qu'un effet avance ses images.
  const SprSprite sprite = OneSprite();
  ActBuildOptions options;
  options.actions = {
      ActActionSpec{.frames = {ActFrameSpec{.layers = {ActLayerSpec{.frame_index = 0}}}}},
      ActActionSpec{
          .frames = {ActFrameSpec{.layers = {}},
                     ActFrameSpec{.layers = {ActLayerSpec{.offset_x = 7.0F, .frame_index = 0}}}}}};
  options.speeds = {6.0F, 6.0F};
  const ActAnimation animation = AnimationOf(options);

  EffectNode node;
  node.action = 1;
  node.frame = 1;
  const std::vector<SpriteQuad> quads = ComposeEffectNodeSprite(node, sprite, animation);
  REQUIRE(quads.size() == 1);
  CHECK(NearlyEqual(quads[0].corners[0].x, 5.0F));

  // Une image vide ne rend rien, plutôt que de retomber sur une autre.
  node.frame = 0;
  CHECK(ComposeEffectNodeSprite(node, sprite, animation).empty());

  // Une action hors du fichier ne rend rien non plus : mieux vaut un effet
  // muet qu'une lecture hors bornes.
  node.action = 99;
  node.frame = 0;
  CHECK(ComposeEffectNodeSprite(node, sprite, animation).empty());
}
