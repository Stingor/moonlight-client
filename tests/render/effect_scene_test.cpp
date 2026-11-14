#include "ro/render/effect_scene.h"

#include <cmath>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/formats/str.h"
#include "ro/render/effect_node.h"

#include "support/effect_builder.h"

using ro::formats::StrEffect;
using ro::render::ComposeEffect;
using ro::render::ComposeEffectNodeBillboard;
using ro::render::EffectNode;
using ro::render::EffectQuad;
using ro::render::kEffectNodeAnchorsAtTop;
using ro::render::PrepareEffect;
using ro::test_support::BuildStr;
using ro::test_support::StrBuildOptions;
using ro::test_support::StrKeyFrameSpec;
using ro::test_support::StrLayerSpec;

namespace {

// Une clé posée au centre exact de l'origine d'écran du format : les coins
// tombent alors autour de zéro, ce qui rend la géométrie lisible.
StrKeyFrameSpec Centered() {
  StrKeyFrameSpec key;
  key.frame_index = 0;
  key.offset_x = ro::formats::kStrOriginX;
  key.offset_y = ro::formats::kStrOriginY;
  // Quatre abscisses puis quatre ordonnées : un carré de vingt de côté.
  key.corners = {-10.0F, 10.0F, 10.0F, -10.0F, -10.0F, -10.0F, 10.0F, 10.0F};
  key.uv = {0.0F, 0.0F, 1.0F, 1.0F, 0.0F, 0.0F, 1.0F, 1.0F};
  key.color = {255.0F, 255.0F, 255.0F, 255.0F};
  return key;
}

ro::render::PreparedEffect Prepared(const std::vector<StrLayerSpec>& layers) {
  StrBuildOptions options;
  options.last_frame = 10;
  options.layers = layers;
  auto effect = StrEffect::Parse(BuildStr(options));
  REQUIRE(effect.has_value());
  return PrepareEffect(*effect);
}

}  // namespace

TEST_CASE("Scène d'effet : les coins suivent l'ordre d'un quad de sprite", "[render][effect]") {
  // Le fichier range les quatre ABSCISSES avant les quatre ordonnées. Les lire
  // par sommet donnerait un quad replié sur lui-même.
  StrLayerSpec layer;
  layer.texture_names = {"a.bmp"};
  layer.key_frames = {Centered()};

  const std::vector<ro::render::EffectQuad> quads = ComposeEffect(Prepared({layer}), 0);
  REQUIRE(quads.size() == 1);
  // Haut-gauche, haut-droit, bas-droit, bas-gauche, l'ordonnée descendant.
  CHECK(quads[0].corners[0].x == Catch::Approx(-10.0F));
  CHECK(quads[0].corners[0].y == Catch::Approx(-10.0F));
  CHECK(quads[0].corners[1].x == Catch::Approx(10.0F));
  CHECK(quads[0].corners[1].y == Catch::Approx(-10.0F));
  CHECK(quads[0].corners[2].x == Catch::Approx(10.0F));
  CHECK(quads[0].corners[2].y == Catch::Approx(10.0F));
  CHECK(quads[0].corners[3].x == Catch::Approx(-10.0F));
  CHECK(quads[0].corners[3].y == Catch::Approx(10.0F));
}

TEST_CASE("Scène d'effet : le centre est RELATIF à l'origine du format", "[render][effect]") {
  // Le fichier donne des positions absolues dans un écran de référence ; c'est
  // cette origine qu'on retranche pour les ramener autour du point où l'effet
  // est joué. L'oublier poserait tout effet à trois cents pixels de sa cible.
  StrKeyFrameSpec key = Centered();
  key.offset_x = ro::formats::kStrOriginX + 40.0F;
  key.offset_y = ro::formats::kStrOriginY - 25.0F;
  StrLayerSpec layer;
  layer.texture_names = {"a.bmp"};
  layer.key_frames = {key};

  const std::vector<ro::render::EffectQuad> quads = ComposeEffect(Prepared({layer}), 0);
  REQUIRE(quads.size() == 1);
  CHECK(quads[0].corners[0].x == Catch::Approx(30.0F));
  CHECK(quads[0].corners[0].y == Catch::Approx(-35.0F));
}

TEST_CASE("Scène d'effet : l'angle tourne dans le sens de l'écran", "[render][effect]") {
  // L'ordonnée descend : un quart de tour amène donc le coin haut-gauche à
  // droite du centre. Tourner à l'envers se voit sur toute flamme orientée.
  StrKeyFrameSpec key = Centered();
  // Un quart de tour, dans l'unité du fichier.
  key.angle_units = 256.0F;
  StrLayerSpec layer;
  layer.texture_names = {"a.bmp"};
  layer.key_frames = {key};

  const std::vector<ro::render::EffectQuad> quads = ComposeEffect(Prepared({layer}), 0);
  REQUIRE(quads.size() == 1);
  CHECK(quads[0].corners[0].x == Catch::Approx(10.0F).margin(0.001F));
  CHECK(quads[0].corners[0].y == Catch::Approx(-10.0F).margin(0.001F));
}

TEST_CASE("Scène d'effet : la portion de texture est une origine et une ÉTENDUE",
          "[render][effect]") {
  // Les deux premiers nombres disent où l'on commence, les deux suivants de
  // combien on avance — et non le coin opposé.
  StrKeyFrameSpec key = Centered();
  key.uv = {0.25F, 0.5F, 0.5F, 0.25F, 0.0F, 0.0F, 0.0F, 0.0F};
  StrLayerSpec layer;
  layer.texture_names = {"a.bmp"};
  layer.key_frames = {key};

  const std::vector<ro::render::EffectQuad> quads = ComposeEffect(Prepared({layer}), 0);
  REQUIRE(quads.size() == 1);
  CHECK(quads[0].uv_min.x == Catch::Approx(0.25F));
  CHECK(quads[0].uv_min.y == Catch::Approx(0.5F));
  CHECK(quads[0].uv_max.x == Catch::Approx(0.75F));
  CHECK(quads[0].uv_max.y == Catch::Approx(0.75F));
}

TEST_CASE("Scène d'effet : ce qui ne se voit pas ne se peint pas", "[render][effect]") {
  // Une couche sans texture, une couche éteinte, une couche qui n'a pas
  // commencé : trois façons de ne rien montrer, et autant de quads épargnés.
  StrLayerSpec without_texture;
  without_texture.key_frames = {Centered()};

  StrKeyFrameSpec faded = Centered();
  faded.color[3] = 0.0F;
  StrLayerSpec invisible;
  invisible.texture_names = {"a.bmp"};
  invisible.key_frames = {faded};

  StrKeyFrameSpec later = Centered();
  later.frame_index = 5;
  StrLayerSpec not_yet;
  not_yet.texture_names = {"a.bmp"};
  not_yet.key_frames = {later};

  StrLayerSpec shown;
  shown.texture_names = {"a.bmp"};
  shown.key_frames = {Centered()};

  const ro::render::PreparedEffect prepared =
      Prepared({without_texture, invisible, not_yet, shown});
  CHECK(ComposeEffect(prepared, 0).size() == 1);
  // À l'image cinq, celle qui attendait entre en scène — et celle de l'image
  // zéro en est sortie : sa dernière clé ne se déforme pas, elle ne paraît donc
  // qu'à son image. Une quatrième façon de ne rien montrer, et la seule qui se
  // voyait en jeu : c'est elle qui laissait les éclairs de StormGust allumés
  // après la fin de la tempête.
  CHECK(ComposeEffect(prepared, 5).size() == 1);
}

TEST_CASE("Scène d'effet : chaque couche désigne SA texture dans la liste à plat",
          "[render][effect]") {
  // Les couches numérotent leurs textures pour elles-mêmes ; le renderer, lui,
  // n'en tient qu'une liste. Confondre les deux montrerait la texture d'une
  // couche sur une autre.
  StrKeyFrameSpec second = Centered();
  second.texture_index = 1.0F;

  StrLayerSpec first_layer;
  first_layer.texture_names = {"a.bmp", "b.bmp"};
  first_layer.key_frames = {second};

  StrLayerSpec other;
  other.texture_names = {"c.bmp"};
  other.key_frames = {Centered()};

  const ro::render::PreparedEffect prepared = Prepared({first_layer, other});
  REQUIRE(prepared.texture_names.size() == 3);
  CHECK(prepared.texture_names[2] == "c.bmp");

  const std::vector<ro::render::EffectQuad> quads = ComposeEffect(prepared, 0);
  REQUIRE(quads.size() == 2);
  CHECK(quads[0].texture == 1);
  CHECK(quads[1].texture == 2);
}

TEST_CASE("Scène d'effet : composer dans un tampon le vide et garde sa mémoire",
          "[render][effect]") {
  // Le renderer compose chaque effet dans le MÊME tampon, image après image :
  // ce qui y restait ne doit pas s'ajouter au nouvel effet, et la mémoire du
  // plus grand doit servir aux suivants plutôt que d'être rendue.
  StrLayerSpec layer;
  layer.texture_names = {"a.bmp"};
  layer.key_frames = {Centered()};
  const ro::render::PreparedEffect three = Prepared({layer, layer, layer});
  const ro::render::PreparedEffect one = Prepared({layer});

  std::vector<EffectQuad> quads;
  ComposeEffect(three, 0, quads);
  REQUIRE(quads.size() == 3);
  const std::size_t capacity = quads.capacity();

  ComposeEffect(one, 0, quads);
  REQUIRE(quads.size() == 1);
  CHECK(quads.capacity() == capacity);
  const std::vector<EffectQuad> alone = ComposeEffect(one, 0);
  REQUIRE(alone.size() == 1);
  CHECK(quads[0].corners == alone[0].corners);
  CHECK(quads[0].texture == alone[0].texture);
  CHECK(quads[0].tint == alone[0].tint);
}

TEST_CASE("Nœud : le quad tient dans ses deux demi-extensions", "[render][effectnode]") {
  // ⚠ LE REPÈRE EST TOURNÉ D'UN QUART DE TOUR par rapport aux axes de la
  // texture : à angle nul la demi-LARGEUR s'étend en ordonnée et la
  // demi-HAUTEUR en abscisse. C'est ce que le client calcule ; cet essai
  // empêche de « redresser » le repère par intuition.
  EffectNode node;
  node.half_width.half_size = 10.0F;
  node.half_height.half_size = 4.0F;

  const EffectQuad quad = ComposeEffectNodeBillboard(node, glm::vec2{100.0F, 50.0F});
  float min_x = quad.corners[0].x;
  float max_x = quad.corners[0].x;
  float min_y = quad.corners[0].y;
  float max_y = quad.corners[0].y;
  for (const glm::vec2& corner : quad.corners) {
    min_x = std::fmin(min_x, corner.x);
    max_x = std::fmax(max_x, corner.x);
    min_y = std::fmin(min_y, corner.y);
    max_y = std::fmax(max_y, corner.y);
  }
  CHECK(min_x == Catch::Approx(96.0F));
  CHECK(max_x == Catch::Approx(104.0F));
  CHECK(min_y == Catch::Approx(40.0F));
  CHECK(max_y == Catch::Approx(60.0F));

  // La texture entière, et les coins dans l'ordre d'un quad de sprite.
  CHECK(quad.uv_min.x == Catch::Approx(0.0F));
  CHECK(quad.uv_max.x == Catch::Approx(1.0F));
}

TEST_CASE("Nœud : un quad ANCRÉ EN HAUT pend sous son point", "[render][effectnode]") {
  // Le client ne décale pas le centre : il double la demi-hauteur vers le bas.
  // Le quad garde donc sa taille et change seulement de côté.
  EffectNode node;
  node.half_width.half_size = 6.0F;
  node.half_height.half_size = 5.0F;
  node.flags = kEffectNodeAnchorsAtTop;

  const EffectQuad quad = ComposeEffectNodeBillboard(node, glm::vec2{0.0F, 0.0F});
  float min_x = quad.corners[0].x;
  float max_x = quad.corners[0].x;
  for (const glm::vec2& corner : quad.corners) {
    min_x = std::fmin(min_x, corner.x);
    max_x = std::fmax(max_x, corner.x);
  }
  // À angle nul, une demi-hauteur POSITIVE va vers la gauche ; pendu, le quad
  // n'en a plus que des négatives et bascule donc tout entier à droite de son
  // point, sans changer d'étendue.
  CHECK(max_x - min_x == Catch::Approx(10.0F));
  CHECK(min_x == Catch::Approx(0.0F));
  CHECK(max_x == Catch::Approx(10.0F));
}

TEST_CASE("Nœud : l'orbite déplace le quad, la couleur le teinte", "[render][effectnode]") {
  // L'orbite s'ajoute au point d'ancrage : c'est ce qui fait monter une
  // particule sans que l'appelant ait à la suivre.
  EffectNode node;
  node.half_width.half_size = 1.0F;
  node.half_height.half_size = 1.0F;
  node.orbit_x = 7.0F;
  node.orbit_y = -3.0F;
  node.color = {200.0F, 100.0F, 0.0F};
  node.alpha = 128.4F;

  const EffectQuad quad = ComposeEffectNodeBillboard(node, glm::vec2{10.0F, 10.0F});
  float sum_x = 0.0F;
  float sum_y = 0.0F;
  for (const glm::vec2& corner : quad.corners) {
    sum_x += corner.x;
    sum_y += corner.y;
  }
  CHECK(sum_x / 4.0F == Catch::Approx(17.0F));
  CHECK(sum_y / 4.0F == Catch::Approx(7.0F));

  CHECK(quad.tint[0] == 200);
  CHECK(quad.tint[1] == 100);
  CHECK(quad.tint[2] == 0);
  CHECK(quad.tint[3] == 128);
}
