#include "ro/render/effect_frame.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/formats/str.h"

#include "support/effect_builder.h"

using ro::formats::StrEffect;
using ro::render::EffectLayerAt;
using ro::render::PreparedEffect;
using ro::render::PreparedEffectLayer;
using ro::render::PrepareEffect;
using ro::test_support::BuildStr;
using ro::test_support::StrBuildOptions;
using ro::test_support::StrKeyFrameSpec;
using ro::test_support::StrLayerSpec;

namespace {

// Une clé posée : un état complet, à cette image.
StrKeyFrameSpec Posed(std::int32_t frame, float offset_x, float alpha = 255.0F) {
  StrKeyFrameSpec key;
  key.frame_index = frame;
  key.type = 0;
  key.offset_x = offset_x;
  key.color = {255.0F, 255.0F, 255.0F, alpha};
  return key;
}

// Une clé de déformation : elle ne pose rien, elle dit que la précédente se
// déforme vers la suivante.
StrKeyFrameSpec Morph(std::int32_t frame) {
  StrKeyFrameSpec key;
  key.frame_index = frame;
  key.type = 1;
  return key;
}

StrEffect Built(const StrLayerSpec& layer, std::int32_t last_frame = 100) {
  StrBuildOptions options;
  options.last_frame = last_frame;
  options.layers = {layer};
  auto effect = StrEffect::Parse(BuildStr(options));
  REQUIRE(effect.has_value());
  return std::move(*effect);
}

}  // namespace

TEST_CASE("Effet : les clés de déformation quittent la liste", "[render][effect]") {
  // Elles ne portent aucune pose : les laisser ferait lire des états vides.
  // Leur seule information — « la précédente se déforme » — passe sur celle-ci.
  StrLayerSpec layer;
  layer.texture_names = {"a.bmp"};
  layer.key_frames = {Posed(0, 10.0F), Morph(1), Posed(10, 30.0F)};

  const ro::render::PreparedEffect prepared = PrepareEffect(Built(layer));
  REQUIRE(prepared.layers.size() == 1);
  REQUIRE(prepared.layers[0].keys.size() == 2);
  CHECK(prepared.layers[0].keys[0].morphs);
  CHECK_FALSE(prepared.layers[0].keys[1].morphs);
}

TEST_CASE("Effet : une clé posée à la même image coupe net", "[render][effect]") {
  // C'est l'exception du format : une déformation aussitôt suivie d'une pose à
  // la même image n'est pas un fondu, c'est une coupure.
  StrLayerSpec layer;
  layer.texture_names = {"a.bmp"};
  layer.key_frames = {Posed(0, 10.0F), Morph(5), Posed(5, 30.0F), Posed(10, 50.0F)};

  const ro::render::PreparedEffect prepared = PrepareEffect(Built(layer));
  REQUIRE(prepared.layers[0].keys.size() == 3);
  CHECK_FALSE(prepared.layers[0].keys[0].morphs);
}

TEST_CASE("Effet : une couche se déforme d'une clé à l'autre", "[render][effect]") {
  StrLayerSpec layer;
  layer.texture_names = {"a.bmp"};
  layer.key_frames = {Posed(0, 0.0F, 255.0F), Morph(1), Posed(10, 100.0F, 0.0F)};
  const ro::render::PreparedEffect prepared = PrepareEffect(Built(layer));

  const auto start = EffectLayerAt(prepared.layers[0], 0, prepared.last_frame);
  REQUIRE(start.has_value());
  CHECK(start->offset_x == Catch::Approx(0.0F));

  const auto middle = EffectLayerAt(prepared.layers[0], 5, prepared.last_frame);
  REQUIRE(middle.has_value());
  CHECK(middle->offset_x == Catch::Approx(50.0F));
  // La couleur se déforme comme le reste : c'est ainsi qu'un effet s'éteint.
  CHECK(middle->color[3] == Catch::Approx(127.5F));

  const auto arrival = EffectLayerAt(prepared.layers[0], 10, prepared.last_frame);
  REQUIRE(arrival.has_value());
  CHECK(arrival->offset_x == Catch::Approx(100.0F));
}

TEST_CASE("Effet : une clé qui ne se déforme pas TIENT sa pose", "[render][effect]") {
  StrLayerSpec layer;
  layer.texture_names = {"a.bmp"};
  layer.key_frames = {Posed(0, 10.0F), Posed(10, 90.0F)};
  const ro::render::PreparedEffect prepared = PrepareEffect(Built(layer));

  const auto held = EffectLayerAt(prepared.layers[0], 7, prepared.last_frame);
  REQUIRE(held.has_value());
  CHECK(held->offset_x == Catch::Approx(10.0F));
}

TEST_CASE("Effet : rien avant la première clé d'une couche", "[render][effect]") {
  // Une couche qui n'a pas commencé ne se dessine pas — c'est ainsi que les
  // couches d'un même effet entrent l'une après l'autre.
  StrLayerSpec layer;
  layer.texture_names = {"a.bmp"};
  layer.key_frames = {Posed(20, 10.0F)};
  const ro::render::PreparedEffect prepared = PrepareEffect(Built(layer));

  CHECK_FALSE(EffectLayerAt(prepared.layers[0], 0, prepared.last_frame).has_value());
  CHECK(EffectLayerAt(prepared.layers[0], 20, prepared.last_frame).has_value());
}

TEST_CASE("Effet : les modes d'animation défilent les textures", "[render][effect]") {
  // Aucun de ces modes ne regarde la texture de la clé SUIVANTE : ils avancent
  // depuis celle de la clé courante, au rythme de son délai.
  const auto texture_at = [](std::int32_t animation, float delay, float from, std::int32_t frame) {
    StrKeyFrameSpec key = Posed(0, 0.0F);
    key.animation = animation;
    key.delay = delay;
    key.texture_index = from;
    StrLayerSpec layer;
    layer.texture_names = {"a.bmp", "b.bmp", "c.bmp", "d.bmp"};
    layer.key_frames = {key, Morph(1)};
    const ro::render::PreparedEffect prepared = PrepareEffect(Built(layer));
    const auto shown = EffectLayerAt(prepared.layers[0], frame, prepared.last_frame);
    REQUIRE(shown.has_value());
    return shown->texture_index;
  };

  // Une fois : jusqu'à la dernière, puis on y reste.
  CHECK(texture_at(2, 1.0F, 0.0F, 2) == 2);
  CHECK(texture_at(2, 1.0F, 0.0F, 9) == 3);
  // Boucle : elle repart à zéro.
  CHECK(texture_at(3, 1.0F, 0.0F, 5) == 1);
  // Boucle inverse : elle descend, et repasse par le haut.
  CHECK(texture_at(4, 1.0F, 0.0F, 1) == 3);
  // Aller-retour : sans répéter les extrémités.
  CHECK(texture_at(5, 1.0F, 0.0F, 3) == 3);
  CHECK(texture_at(5, 1.0F, 0.0F, 4) == 2);
  CHECK(texture_at(5, 1.0F, 0.0F, 6) == 0);
}

TEST_CASE("Effet : une couche sans texture ne fait pas déborder l'index", "[render][effect]") {
  // Chaque fichier de l'installation en porte une : la première couche n'a
  // aucune texture. Un modulo par zéro y suffirait à faire tomber le client.
  StrKeyFrameSpec key = Posed(0, 0.0F);
  key.animation = 3;
  key.delay = 1.0F;
  StrLayerSpec layer;
  layer.key_frames = {key, Morph(1)};
  const ro::render::PreparedEffect prepared = PrepareEffect(Built(layer));

  const auto shown = EffectLayerAt(prepared.layers[0], 5, prepared.last_frame);
  REQUIRE(shown.has_value());
  CHECK(shown->texture_index == 0);
}

TEST_CASE("Horloge d'un effet : un pas de seize millisecondes, et la fin", "[render][effect]") {
  // Une image par TIC, et le tic fait seize millisecondes — soit 62,5 images
  // par seconde, ce que le client joue réellement.
  ro::render::PreparedEffect effect;
  effect.last_frame = 10;

  CHECK(ro::render::EffectFrameAt(effect, 0.0F) == 0);
  CHECK(ro::render::EffectFrameAt(effect, 15.0F) == 0);
  CHECK(ro::render::EffectFrameAt(effect, 16.0F) == 1);
  CHECK(ro::render::EffectFrameAt(effect, 100.0F) == 6);
  // La dernière image se joue ; passé elle, l'effet n'est plus là.
  CHECK(ro::render::EffectFrameAt(effect, 175.0F) == 10);
  CHECK_FALSE(ro::render::EffectFrameAt(effect, 176.0F).has_value());

  // Une durée qui remonte le temps vaut le début : une image négative se
  // lirait comme une couche PAS ENCORE commencée, donc comme rien du tout.
  CHECK(ro::render::EffectFrameAt(effect, -50.0F) == 0);
}

TEST_CASE("Horloge d'un effet : la cadence du fichier N'ENTRE PAS", "[render][effect]") {
  // Le fichier déclare vingt-quatre images par seconde ; à cette cadence, cent
  // vingt-cinq millisecondes en donneraient trois. Le jeu en montre SEPT, parce
  // qu'il compte des pas de seize millisecondes sans jamais lire ce champ.
  //
  // Le client incrémente le compteur de l'instance d'un par pas, et rien n'y
  // consulte la cadence.
  StrBuildOptions options;
  options.frames_per_second = 24;
  options.last_frame = 10;
  options.layers = {StrLayerSpec{.texture_names = {"halo.bmp"}, .key_frames = {Posed(0, 0.0F)}}};
  const auto effect = StrEffect::Parse(BuildStr(options));
  REQUIRE(effect.has_value());

  const ro::render::PreparedEffect prepared = PrepareEffect(*effect);
  CHECK(ro::render::EffectFrameAt(prepared, 125.0F) == 7);
}

TEST_CASE("Couche d'effet : elle s'éteint au bout de ses clés", "[render][effect]") {
  // Une couche dont la dernière clé NE se déforme PAS ne paraît qu'à l'image de
  // cette clé, et s'éteint ensuite. Tenir sa pose jusqu'au bout de l'effet fige
  // à l'écran ce qui finit opaque — les éclairs de StormGust restaient allumés
  // pendant que la tempête, elle, se dissipait d'elle-même.
  const StrEffect effect = Built(StrLayerSpec{.texture_names = {"eclair.bmp"},
                                              .key_frames = {Posed(10, 0.0F), Posed(20, 40.0F)}});
  const PreparedEffect prepared = PrepareEffect(effect);
  REQUIRE(prepared.layers.size() == 1);
  const PreparedEffectLayer& layer = prepared.layers.front();

  CHECK_FALSE(EffectLayerAt(layer, 9, prepared.last_frame).has_value());
  CHECK(EffectLayerAt(layer, 10, prepared.last_frame).has_value());
  // Entre deux clés, elle tient la pose de la première : c'est une coupure
  // nette, et non une extinction.
  CHECK(EffectLayerAt(layer, 15, prepared.last_frame).has_value());
  CHECK(EffectLayerAt(layer, 20, prepared.last_frame).has_value());
  CHECK_FALSE(EffectLayerAt(layer, 21, prepared.last_frame).has_value());
  CHECK_FALSE(EffectLayerAt(layer, prepared.last_frame, prepared.last_frame).has_value());
}

TEST_CASE("Couche d'effet : une dernière clé qui se déforme tient jusqu'au bout",
          "[render][effect]") {
  // C'est l'exception, et elle est nécessaire : une couche qui se déforme sans
  // clé d'arrivée se déforme vers ELLE-MÊME, posée à la dernière image. La
  // couper à sa clé escamoterait toute la fin de l'effet.
  const StrEffect effect = Built(
      StrLayerSpec{.texture_names = {"halo.bmp"}, .key_frames = {Posed(10, 0.0F), Morph(20)}});
  const PreparedEffect prepared = PrepareEffect(effect);
  REQUIRE(prepared.layers.size() == 1);
  const PreparedEffectLayer& layer = prepared.layers.front();

  CHECK(EffectLayerAt(layer, 10, prepared.last_frame).has_value());
  CHECK(EffectLayerAt(layer, 50, prepared.last_frame).has_value());
  CHECK(EffectLayerAt(layer, prepared.last_frame, prepared.last_frame).has_value());
}

TEST_CASE("Effet : la part d'image écoulée est le reste d'un tic entamé", "[render][effect]") {
  CHECK(ro::render::EffectFrameFraction(0.0F) == 0.0F);
  CHECK(ro::render::EffectFrameFraction(-5.0F) == 0.0F);
  CHECK(ro::render::EffectFrameFraction(16.0F) == 0.0F);
  CHECK(ro::render::EffectFrameFraction(24.0F) == Catch::Approx(0.5F));
  CHECK(ro::render::EffectFrameFraction(36.0F) == Catch::Approx(0.25F));
}

TEST_CASE("Effet : à part nulle, la couche est celle de l'image entière au bit près",
          "[render][effect]") {
  StrLayerSpec layer;
  layer.texture_names = {"a.bmp"};
  layer.key_frames = {Posed(0, 0.0F, 255.0F), Morph(1), Posed(10, 100.0F, 0.0F)};
  const PreparedEffect prepared = PrepareEffect(Built(layer));

  for (std::int32_t frame = 0; frame <= 10; ++frame) {
    const auto whole = EffectLayerAt(prepared.layers[0], frame, prepared.last_frame);
    const auto zero = EffectLayerAt(prepared.layers[0], frame, prepared.last_frame, 0.0F);
    REQUIRE(whole.has_value());
    REQUIRE(zero.has_value());
    CHECK(zero->offset_x == whole->offset_x);
    CHECK(zero->color == whole->color);
    CHECK(zero->corners == whole->corners);
    CHECK(zero->texture_index == whole->texture_index);
  }
}

TEST_CASE("Effet : entre deux images, la couche se déforme au milieu", "[render][effect]") {
  StrLayerSpec layer;
  layer.texture_names = {"a.bmp"};
  layer.key_frames = {Posed(0, 0.0F, 255.0F), Morph(1), Posed(10, 100.0F, 0.0F)};
  const PreparedEffect prepared = PrepareEffect(Built(layer));

  // À mi-chemin de l'image cinq et de l'image six : la place cinquante-cinq.
  const auto between = EffectLayerAt(prepared.layers[0], 5, prepared.last_frame, 0.5F);
  REQUIRE(between.has_value());
  CHECK(between->offset_x == Catch::Approx(55.0F));
  CHECK(between->color[3] == Catch::Approx(255.0F * 0.45F));
  // Dès la clé posée, la déformation commence : la moitié de l'image zéro.
  const auto from_key = EffectLayerAt(prepared.layers[0], 0, prepared.last_frame, 0.5F);
  REQUIRE(from_key.has_value());
  CHECK(from_key->offset_x == Catch::Approx(5.0F));
}

TEST_CASE("Effet : entre deux images, ce qui est discret reste à l'image entière",
          "[render][effect]") {
  // Une couche qui tient sa pose ne glisse pas vers la clé suivante.
  StrLayerSpec held;
  held.texture_names = {"a.bmp"};
  held.key_frames = {Posed(0, 10.0F), Posed(10, 90.0F)};
  const PreparedEffect held_prepared = PrepareEffect(Built(held));
  const auto still = EffectLayerAt(held_prepared.layers[0], 9, held_prepared.last_frame, 0.9F);
  REQUIRE(still.has_value());
  CHECK(still->offset_x == 10.0F);

  // L'image de texture d'un mode d'animation reste celle de l'image entière.
  StrKeyFrameSpec looping = Posed(0, 0.0F);
  looping.animation = 3;
  looping.delay = 1.0F;
  StrLayerSpec animated;
  animated.texture_names = {"a.bmp", "b.bmp", "c.bmp", "d.bmp"};
  animated.key_frames = {looping, Morph(1)};
  const PreparedEffect animated_prepared = PrepareEffect(Built(animated));
  const auto whole = EffectLayerAt(animated_prepared.layers[0], 2, animated_prepared.last_frame);
  const auto between =
      EffectLayerAt(animated_prepared.layers[0], 2, animated_prepared.last_frame, 0.9F);
  REQUIRE(whole.has_value());
  REQUIRE(between.has_value());
  CHECK(between->texture_index == whole->texture_index);

  // Une couche qui s'éteint à sa dernière clé s'éteint à la même image.
  StrLayerSpec ending;
  ending.texture_names = {"a.bmp"};
  ending.key_frames = {Posed(10, 0.0F), Posed(20, 0.0F)};
  const PreparedEffect ending_prepared = PrepareEffect(Built(ending));
  CHECK(EffectLayerAt(ending_prepared.layers[0], 20, ending_prepared.last_frame, 0.5F).has_value());
  CHECK_FALSE(
      EffectLayerAt(ending_prepared.layers[0], 21, ending_prepared.last_frame, 0.5F).has_value());
}

TEST_CASE("Effet : la déformation ne dépasse jamais la clé suivante", "[render][effect]") {
  // Une dernière clé qui se déforme vers elle-même, posée à la dernière image :
  // la part écoulée n'y ajoute rien.
  const StrEffect effect = Built(
      StrLayerSpec{.texture_names = {"halo.bmp"}, .key_frames = {Posed(10, 40.0F), Morph(20)}});
  const PreparedEffect prepared = PrepareEffect(effect);
  const auto last = EffectLayerAt(prepared.layers[0], prepared.last_frame, prepared.last_frame);
  const auto beyond =
      EffectLayerAt(prepared.layers[0], prepared.last_frame, prepared.last_frame, 0.75F);
  REQUIRE(last.has_value());
  REQUIRE(beyond.has_value());
  CHECK(beyond->offset_x == last->offset_x);
}
