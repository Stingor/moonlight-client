#include "ro/formats/str.h"

#include <cmath>
#include <limits>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "support/effect_builder.h"

using ro::formats::StrAnimationType;
using ro::formats::StrEffect;
using ro::test_support::BuildStr;
using ro::test_support::StrBuildOptions;
using ro::test_support::StrKeyFrameSpec;
using ro::test_support::StrLayerSpec;

namespace {

StrBuildOptions OneLayer() {
  StrKeyFrameSpec key;
  key.frame_index = 0;
  key.offset_x = 320.0F;
  key.offset_y = 290.0F;
  key.texture_index = 1.0F;
  key.animation = 3;
  key.delay = 2.5F;
  // Un quart de tour, dans l'unité du fichier.
  key.angle_units = 256.0F;

  StrLayerSpec layer;
  layer.texture_names = {"feu01.tga", "feu02.tga"};
  layer.key_frames = {key};

  StrBuildOptions options;
  options.frames_per_second = 60;
  options.last_frame = 12;
  options.layers = {layer};
  return options;
}

}  // namespace

TEST_CASE("STR : un effet se lit couche par couche", "[formats][str]") {
  const auto effect = StrEffect::Parse(BuildStr(OneLayer()));
  REQUIRE(effect.has_value());
  CHECK(effect->frames_per_second() == 60);
  CHECK(effect->last_frame() == 12);
  // Le fichier donne la DERNIÈRE image, pas leur nombre.
  CHECK(effect->frame_count() == 13);
  REQUIRE(effect->layers().size() == 1);

  const auto& layer = effect->layers()[0];
  REQUIRE(layer.texture_names.size() == 2);
  CHECK(layer.texture_names[0] == "feu01.tga");
  CHECK(layer.texture_names[1] == "feu02.tga");
  REQUIRE(layer.key_frames.size() == 1);

  const auto& key = layer.key_frames[0];
  CHECK(key.offset_x == Catch::Approx(320.0F));
  CHECK(key.texture_index == Catch::Approx(1.0F));
  CHECK(key.animation == StrAnimationType::kLoop);
  CHECK(key.delay == Catch::Approx(2.5F));
  CHECK(key.blend_source == 5);
  CHECK(key.blend_destination == 7);
}

TEST_CASE("STR : l'angle est ramené en degrés", "[formats][str]") {
  // Le fichier l'écrit en 1024e de tour. Le laisser tel quel ferait tourner les
  // effets de presque trois tours par degré attendu.
  StrBuildOptions options = OneLayer();
  options.layers[0].key_frames[0].angle_units = 256.0F;
  const auto effect = StrEffect::Parse(BuildStr(options));
  REQUIRE(effect.has_value());
  CHECK(effect->layers()[0].key_frames[0].angle_degrees == Catch::Approx(90.0F));
}

TEST_CASE("STR : un délai non numérique ne se propage pas", "[formats][str]") {
  // Des fichiers réels en portent. Laissé tel quel, il empoisonne toute
  // interpolation qui le touche, et la couche disparaît sans dire pourquoi.
  StrBuildOptions options = OneLayer();
  options.layers[0].key_frames[0].delay = std::numeric_limits<float>::infinity();
  const auto effect = StrEffect::Parse(BuildStr(options));
  REQUIRE(effect.has_value());
  CHECK(effect->layers()[0].key_frames[0].delay == Catch::Approx(0.0F));

  options.layers[0].key_frames[0].delay = std::numeric_limits<float>::quiet_NaN();
  const auto other = StrEffect::Parse(BuildStr(options));
  REQUIRE(other.has_value());
  CHECK(other->layers()[0].key_frames[0].delay == Catch::Approx(0.0F));
}

TEST_CASE("STR : un mot laissé à la place de la dernière image se recalcule", "[formats][str]") {
  // Certains fichiers y laissent les quatre octets du mot « Fram ». Le nombre
  // n'a alors aucun sens : la dernière image se retrouve sur les images clés.
  StrBuildOptions options = OneLayer();
  options.last_frame = 0x6d617246;
  options.layers[0].key_frames[0].frame_index = 7;
  StrKeyFrameSpec later = options.layers[0].key_frames[0];
  later.frame_index = 41;
  options.layers[0].key_frames.push_back(later);

  const auto effect = StrEffect::Parse(BuildStr(options));
  REQUIRE(effect.has_value());
  CHECK(effect->last_frame() == 41);
}

TEST_CASE("STR : une signature ou une fin qui manque se refuse", "[formats][str]") {
  // Ces fichiers viennent d'archives que l'on ne contrôle pas : l'échec est un
  // cas normal, pas un plantage.
  std::vector<std::uint8_t> wrong = BuildStr(OneLayer());
  wrong[0] = 'X';
  CHECK_FALSE(StrEffect::Parse(wrong).has_value());

  const std::vector<std::uint8_t> whole = BuildStr(OneLayer());
  CHECK_FALSE(StrEffect::Parse(std::span(whole).first(whole.size() / 2)).has_value());

  // Un en-tête qui annonce plus de couches qu'il n'en porte.
  StrBuildOptions lying = OneLayer();
  lying.declared_layers = 4;
  CHECK_FALSE(StrEffect::Parse(BuildStr(lying)).has_value());
}

TEST_CASE("STR : un effet sans couche se lit sans erreur", "[formats][str]") {
  // Un fichier vide est licite : il ne dessine rien.
  StrBuildOptions empty;
  const auto effect = StrEffect::Parse(BuildStr(empty));
  REQUIRE(effect.has_value());
  CHECK(effect->layers().empty());
}
