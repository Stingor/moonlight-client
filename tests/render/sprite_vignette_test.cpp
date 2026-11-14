#include "ro/render/sprite_vignette.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/act.h"
#include "ro/formats/image.h"
#include "ro/formats/spr.h"
#include "ro/render/actor_layers.h"
#include "ro/render/actor_sprite.h"
#include "ro/render/sprite_frame.h"

#include "support/sprite_builder.h"

using ro::formats::Image;
using ro::render::ActorOutfit;
using ro::render::ActorPart;
using ro::render::ActorPiece;
using ro::render::ComposedActor;
using ro::render::DrawVignette;
using ro::render::FrameVignette;
using ro::render::kAnimationIdle;
using ro::render::kDirectionCount;
using ro::render::MeasureDirectionSpan;
using ro::render::MeasureOutfitSpan;
using ro::render::MeasurePoseSpan;
using ro::render::OutfitSpan;
using ro::render::SpriteBounds;
using ro::render::SpriteQuad;
using ro::render::VignetteBox;
using ro::render::VignetteFit;
using ro::render::VignetteFraming;
using ro::test_support::ActActionSpec;
using ro::test_support::ActAnchorSpec;
using ro::test_support::ActBuildOptions;
using ro::test_support::ActFrameSpec;
using ro::test_support::ActLayerSpec;
using ro::test_support::BuildAct;
using ro::test_support::BuildSpr;
using ro::test_support::SprBuildOptions;

namespace {

// Le côté d'une image d'essai : deux pixels, centrés sur l'origine du calque.
// Un quad va donc de moins un à plus un autour du décalage de son calque.
constexpr float kHalfImage = 1.0F;

// Une emprise écrite à la main, pour éprouver le cadrage sans fichier.
SpriteBounds Box(float min_x, float min_y, float max_x, float max_y) {
  SpriteBounds bounds;
  bounds.min = glm::vec2(min_x, min_y);
  bounds.max = glm::vec2(max_x, max_y);
  return bounds;
}

// Un personnage haut de cent pixels, les pieds sur l'origine, avec ce qu'il
// porte débordant à gauche et à droite de `side`.
OutfitSpan Silhouette(float side) {
  OutfitSpan span;
  span.all = Box(-side, -100.0F, side, 0.0F);
  span.body = Box(-5.0F, -100.0F, 5.0F, 0.0F);
  span.measured = true;
  return span;
}

// Un sprite de deux images de deux pixels sur deux, palettisé.
std::vector<std::uint8_t> SpriteBytes() {
  SprBuildOptions options;
  options.indexed = {{2, 2, {1, 1, 1, 1}}, {2, 2, {2, 2, 2, 2}}};
  options.palette = {{0, 0, 0}, {255, 0, 0}, {0, 255, 0}};
  return BuildSpr(options);
}

// Une animation d'un rang : huit actions, une par direction, dont le calque se
// décale de `step_x` par direction et de `aside` en plus, toutes directions
// confondues. C'est ce qui fait une enveloppe plus large que n'importe laquelle
// des poses.
std::vector<std::uint8_t> SidewaysBytes(float step_x, std::size_t frames, float aside = 0.0F) {
  ActBuildOptions options;
  for (std::size_t direction = 0; direction < kDirectionCount; ++direction) {
    ActActionSpec action;
    for (std::size_t frame = 0; frame < frames; ++frame) {
      ActFrameSpec frame_spec;
      ActLayerSpec layer;
      layer.frame_index = 0;
      layer.offset_x = aside + step_x * static_cast<float>(direction);
      // L'image fait DESCENDRE le calque : une enveloppe qui ne regarderait
      // que la première le dirait plus haut qu'il ne va.
      layer.offset_y = static_cast<float>(frame);
      frame_spec.layers.push_back(layer);
      frame_spec.anchors.push_back(ActAnchorSpec{});
      action.frames.push_back(std::move(frame_spec));
    }
    options.actions.push_back(std::move(action));
    options.speeds.push_back(4.0F);
  }
  return BuildAct(options);
}

}  // namespace

TEST_CASE("Vignette : l'échelle fait rentrer l'enveloppe, les pieds viennent du corps",
          "[render][vignette]") {
  const VignetteBox box{.width = 100, .height = 100};
  const VignetteFraming framing = FrameVignette(Silhouette(20.0F), box);

  REQUIRE(framing.usable());
  // La hauteur borne : cent pixels de silhouette dans cent pixels de boîte.
  CHECK(framing.scale == 1.0F);
  // Centré en abscisse, les pieds au bas de la boîte.
  CHECK(framing.origin.x == 50.0F);
  CHECK(framing.origin.y == 100.0F);
}

TEST_CASE("Vignette : un petit sprite grandit jusqu'à remplir sa boîte", "[render][vignette]") {
  // La fiche d'un monstre ajuste le sprite à son cadre, dans les deux sens : un
  // Little Poring de trente pixels ne se perd plus au fond d'une case de cent.
  const VignetteBox box{.width = 300, .height = 300};
  const VignetteFraming framing = FrameVignette(Silhouette(20.0F), box);
  REQUIRE(framing.usable());
  CHECK(framing.scale == 3.0F);
  CHECK(framing.origin.y == 300.0F);
}

TEST_CASE("Vignette : ce qui déborde sur les côtés commande l'échelle", "[render][vignette]") {
  const VignetteBox box{.width = 100, .height = 100};
  // Une coiffe posée à côté du personnage double l'enveloppe : la LARGEUR
  // décide, et le personnage rétrécit.
  const VignetteFraming framing = FrameVignette(Silhouette(100.0F), box);

  REQUIRE(framing.usable());
  CHECK(framing.scale == 0.5F);
}

TEST_CASE("Vignette : le plancher de stature l'emporte sur le cadre", "[render][vignette]") {
  // Le même débordement, mais on refuse un personnage plus petit que la moitié
  // de la boîte : ce qui dépasse sur les côtés sera coupé.
  const VignetteBox box{.width = 100, .height = 100, .min_body_height = 80.0F};
  const VignetteFraming framing = FrameVignette(Silhouette(100.0F), box);

  REQUIRE(framing.usable());
  CHECK(framing.scale == 0.8F);
}

TEST_CASE("Vignette : l'échelle imposée ne rétrécit pas ce qui déborde", "[render][vignette]") {
  // 🔴 C'EST TOUTE LA RAISON D'ÊTRE DU CHAMP, ET LE PIÈGE D'UNE SCÈNE. Deux
  // voisins de même stature : l'un nu, l'autre portant quelque chose de large.
  // À l'ajustement ils sortent à DEUX échelles — le second réduit de moitié
  // pour que son encombrement rentre —, ce qui se voit comme un défaut de
  // sprite. Imposée, l'échelle est la même et c'est ce qui dépasse qui sort du
  // cadre.
  const VignetteBox box{.width = 100, .height = 100, .scale = 1.0F};
  const VignetteFraming plain = FrameVignette(Silhouette(5.0F), box);
  const VignetteFraming laden = FrameVignette(Silhouette(100.0F), box);

  REQUIRE(plain.usable());
  REQUIRE(laden.usable());
  CHECK(plain.scale == 1.0F);
  CHECK(laden.scale == 1.0F);
  // Et sans elle, le second serait deux fois plus petit que le premier.
  CHECK(FrameVignette(Silhouette(100.0F), VignetteBox{.width = 100, .height = 100}).scale == 0.5F);
}

TEST_CASE("Vignette : l'échelle imposée centre sur le corps, pas sur l'enveloppe",
          "[render][vignette]") {
  // ⚠ CE N'EST PAS UN DÉTAIL SÉPARÉ. Une enveloppe qui déborde d'UN SEUL côté
  // — une arme tenue à droite, un compagnon posé à côté — déplacerait le
  // personnage vers la gauche si l'on centrait dessus, au lieu de laisser
  // sortir ce qui dépasse. Le corps est le seul point fixe.
  OutfitSpan span;
  span.all = Box(-5.0F, -100.0F, 95.0F, 0.0F);  // déborde de 90 à DROITE
  span.body = Box(-5.0F, -100.0F, 5.0F, 0.0F);  // centré sur zéro
  span.measured = true;

  const VignetteFraming framing =
      FrameVignette(span, VignetteBox{.width = 100, .height = 100, .scale = 1.0F});

  REQUIRE(framing.usable());
  CHECK(framing.origin.x == 50.0F);
  CHECK(framing.origin.y == 100.0F);
}

TEST_CASE("Vignette : le plancher de stature ne s'applique pas à une échelle imposée",
          "[render][vignette]") {
  // Le plancher est le recours de l'AJUSTEMENT contre une enveloppe trop
  // large. Le laisser mordre ici reviendrait à réintroduire, par une autre
  // porte, l'échelle par personnage que l'imposition supprime.
  const VignetteBox box{.width = 100, .height = 100, .min_body_height = 80.0F, .scale = 0.25F};
  const VignetteFraming framing = FrameVignette(Silhouette(100.0F), box);

  REQUIRE(framing.usable());
  CHECK(framing.scale == 0.25F);
}

TEST_CASE("Vignette : une cape qui descend plus bas que les bottes n'enfonce pas les pieds",
          "[render][vignette]") {
  OutfitSpan span;
  // La cape descend vingt pixels sous les pieds ; le corps, lui, s'arrête à
  // l'origine.
  span.all = Box(-10.0F, -100.0F, 10.0F, 20.0F);
  span.body = Box(-5.0F, -100.0F, 5.0F, 0.0F);
  span.measured = true;

  const VignetteFraming framing = FrameVignette(span, VignetteBox{.width = 100, .height = 100});

  REQUIRE(framing.usable());
  // Les pieds tombent au bas de la boîte, et la cape sort dessous.
  CHECK(framing.origin.y == 100.0F);
}

TEST_CASE("Vignette : une enveloppe non mesurée ne se cadre pas", "[render][vignette]") {
  OutfitSpan span;
  span.all = Box(-10.0F, -100.0F, 10.0F, 0.0F);
  span.body = span.all;
  CHECK_FALSE(FrameVignette(span, VignetteBox{.width = 100, .height = 100}).usable());

  span.measured = true;
  CHECK_FALSE(FrameVignette(span, VignetteBox{.width = 0, .height = 100}).usable());
}

TEST_CASE("Vignette : l'enveloppe couvre les huit directions et toutes les images",
          "[render][vignette]") {
  auto sprite = ro::formats::SprSprite::Parse(SpriteBytes());
  REQUIRE(sprite.has_value());
  constexpr float kStepX = 10.0F;
  constexpr std::size_t kFrames = 3;
  auto animation = ro::formats::ActAnimation::Parse(SidewaysBytes(kStepX, kFrames));
  REQUIRE(animation.has_value());

  ActorPiece body;
  body.sprite = &*sprite;
  body.animation = &*animation;
  body.part = ActorPart::kBody;
  ActorOutfit outfit(std::vector<ActorPiece>{body});

  const OutfitSpan span = MeasureOutfitSpan(outfit, kAnimationIdle);

  REQUIRE(span.measured);
  // La direction zéro ne va pas plus loin que plus un ; la septième, elle,
  // emmène le calque à soixante-dix.
  CHECK(span.all.min.x == -kHalfImage);
  CHECK(span.all.max.x == kStepX * static_cast<float>(kDirectionCount - 1) + kHalfImage);
  // La troisième image descend de deux pixels de plus que la première.
  CHECK(span.all.max.y == static_cast<float>(kFrames - 1) + kHalfImage);
  // Sans rien de porté, le corps EST la silhouette.
  CHECK(span.body.max.x == span.all.max.x);
}

TEST_CASE("Vignette : une pose FIGÉE se mesure sur elle seule", "[render][vignette]") {
  auto sprite = ro::formats::SprSprite::Parse(SpriteBytes());
  REQUIRE(sprite.has_value());
  constexpr float kStepX = 10.0F;
  constexpr std::size_t kFrames = 3;
  auto animation = ro::formats::ActAnimation::Parse(SidewaysBytes(kStepX, kFrames));
  REQUIRE(animation.has_value());

  ActorPiece body;
  body.sprite = &*sprite;
  body.animation = &*animation;
  body.part = ActorPart::kBody;
  ActorOutfit outfit(std::vector<ActorPiece>{body});

  const ComposedActor& composed = outfit.Composed(kAnimationIdle, 0, 0.0F);
  const OutfitSpan pose = MeasurePoseSpan(outfit, composed);

  REQUIRE(pose.measured);
  // 🔴 CE QUE L'ENVELOPPE DES HUIT DIRECTIONS COÛTERAIT : elle va jusqu'à
  // soixante-et-onze en abscisse, la pose de face jusqu'à un. Cadrer une case
  // de grille sur la première la montrerait trente-cinq fois plus petite
  // qu'elle ne doit l'être — c'est la même mécanique, en plus discret, qui
  // rapetissait une coiffure sur deux.
  CHECK(pose.all.max.x == kHalfImage);
  CHECK(pose.all.min.x == -kHalfImage);
  CHECK(MeasureOutfitSpan(outfit, kAnimationIdle).all.max.x >
        kStepX * static_cast<float>(kDirectionCount - 2));
  // Et sur la première image seulement : les suivantes descendent plus bas.
  CHECK(pose.all.max.y == kHalfImage);
}

TEST_CASE("Vignette : une pose sans corps ne se cadre pas", "[render][vignette]") {
  ActorOutfit empty(std::vector<ActorPiece>{});
  const ComposedActor nothing;
  CHECK_FALSE(MeasurePoseSpan(empty, nothing).measured);
}

TEST_CASE("Vignette : le corps se mesure seul, sans ce qu'il porte", "[render][vignette]") {
  auto sprite = ro::formats::SprSprite::Parse(SpriteBytes());
  REQUIRE(sprite.has_value());
  auto body_animation = ro::formats::ActAnimation::Parse(SidewaysBytes(0.0F, 1));
  REQUIRE(body_animation.has_value());
  // La coiffe est posée loin sur le côté, comme le sont les costumes qui
  // portent un compagnon : elle élargit la silhouette sans toucher la stature.
  constexpr float kAside = 40.0F;
  auto hat_sprite = ro::formats::SprSprite::Parse(SpriteBytes());
  REQUIRE(hat_sprite.has_value());
  auto aside_animation = ro::formats::ActAnimation::Parse(SidewaysBytes(0.0F, 1, kAside));
  REQUIRE(aside_animation.has_value());

  ActorPiece body;
  body.sprite = &*sprite;
  body.animation = &*body_animation;
  body.part = ActorPart::kBody;
  ActorPiece hat;
  hat.sprite = &*hat_sprite;
  hat.animation = &*aside_animation;
  hat.part = ActorPart::kHeadgearTop;
  ActorOutfit outfit(std::vector<ActorPiece>{body, hat});

  const OutfitSpan span = MeasureOutfitSpan(outfit, kAnimationIdle);

  REQUIRE(span.measured);
  CHECK(span.all.max.x == kAside + kHalfImage);
  CHECK(span.body.max.x == kHalfImage);
}

TEST_CASE("Vignette : l'image est en pixels de SPRITE, et la boîte coupe le reste",
          "[render][vignette]") {
  auto sprite = ro::formats::SprSprite::Parse(SpriteBytes());
  REQUIRE(sprite.has_value());
  auto animation = ro::formats::ActAnimation::Parse(SidewaysBytes(0.0F, 1));
  REQUIRE(animation.has_value());

  ActorPiece body;
  body.sprite = &*sprite;
  body.animation = &*animation;
  body.part = ActorPart::kBody;
  ActorOutfit outfit(std::vector<ActorPiece>{body});
  const ComposedActor& composed = outfit.Composed(kAnimationIdle, 0, 0.0F);
  REQUIRE_FALSE(composed.quads.empty());

  // 🔴 LA BOÎTE FAIT HUIT PIXELS D'ÉCRAN, L'IMAGE EN FERA QUATRE : à l'échelle
  // deux, on dessine à l'échelle UN sur une boîte ramenée en pixels de sprite,
  // et c'est la carte qui agrandira. Rastériser huit sur huit rendrait un
  // agrandissement au plus proche, que le filtre de la texture ne rattraperait
  // plus — c'est tout l'objet de ce cas.
  VignetteFraming framing;
  framing.scale = 2.0F;
  framing.origin = glm::vec2(4.0F, 4.0F);
  const Image image = DrawVignette(composed, VignetteBox{.width = 8, .height = 8}, framing);

  REQUIRE(image.width == 4);
  REQUIRE(image.height == 4);
  const auto opaque_at = [&image](std::int32_t x, std::int32_t y) {
    const std::size_t offset =
        (static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) +
         static_cast<std::size_t>(x)) *
        4;
    return image.pixels[offset + 3] != 0;
  };
  // L'origine tombe au milieu : elle aussi se ramène en pixels de sprite, et le
  // carré de deux pixels s'y centre.
  CHECK(opaque_at(1, 1));
  CHECK(opaque_at(2, 2));
  CHECK_FALSE(opaque_at(0, 0));
  CHECK_FALSE(opaque_at(3, 3));
}

TEST_CASE("Vignette : un canevas démesuré est rendu plus grossièrement, au même endroit",
          "[render][vignette]") {
  auto sprite = ro::formats::SprSprite::Parse(SpriteBytes());
  REQUIRE(sprite.has_value());
  auto animation = ro::formats::ActAnimation::Parse(SidewaysBytes(0.0F, 1));
  REQUIRE(animation.has_value());

  ActorPiece body;
  body.sprite = &*sprite;
  body.animation = &*animation;
  body.part = ActorPart::kBody;
  ActorOutfit outfit(std::vector<ActorPiece>{body});
  const ComposedActor& composed = outfit.Composed(kAnimationIdle, 0, 0.0F);
  REQUIRE_FALSE(composed.quads.empty());

  // Quatre mille pixels de boîte à l'échelle deux demanderaient un canevas de
  // deux mille : il n'en fait que le plafond, et le carré de deux pixels n'en
  // occupe plus qu'un, au même point de la boîte.
  constexpr std::int32_t kHugeBox = 4096;
  VignetteFraming framing;
  framing.scale = 2.0F;
  framing.origin = glm::vec2(2050.0F, 2050.0F);
  const Image image =
      DrawVignette(composed, VignetteBox{.width = kHugeBox, .height = kHugeBox}, framing);
  REQUIRE(image.width == static_cast<std::int32_t>(ro::render::kVignetteLargestCanvasPx));
  REQUIRE(image.height == static_cast<std::int32_t>(ro::render::kVignetteLargestCanvasPx));
  const auto opaque_at = [&image](std::int32_t x, std::int32_t y) {
    const std::size_t offset =
        (static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) +
         static_cast<std::size_t>(x)) *
        4;
    return image.pixels[offset + 3] != 0;
  };
  // 2050 pixels de boîte sur une échelle de canevas de quatre : 512,5.
  CHECK(opaque_at(512, 512));
  CHECK_FALSE(opaque_at(510, 510));
  CHECK_FALSE(opaque_at(514, 514));
}

TEST_CASE("Vignette : la pipette désigne le pixel affiché, jusqu'à son bord",
          "[render][vignette]") {
  auto sprite = ro::formats::SprSprite::Parse(SpriteBytes());
  REQUIRE(sprite.has_value());
  auto animation = ro::formats::ActAnimation::Parse(SidewaysBytes(0.0F, 1));
  REQUIRE(animation.has_value());

  ActorPiece body;
  body.sprite = &*sprite;
  body.animation = &*animation;
  body.part = ActorPart::kBody;
  ActorOutfit outfit(std::vector<ActorPiece>{body});
  const ComposedActor& composed = outfit.Composed(kAnimationIdle, 0, 0.0F);
  REQUIRE_FALSE(composed.quads.empty());

  // 🔴 UNE ORIGINE AU DEMI-PIXEL DE SPRITE : dix-huit pixels de boîte à
  // l'échelle quatre, soit 4,5 pixels du canevas. Le canevas lit chaque pixel
  // en son centre ; l'inverse brut du cadrage changeait de réponse au milieu
  // de chaque pixel affiché.
  constexpr float kScale = 4.0F;
  constexpr std::int32_t kBoxSide = 40;
  const VignetteBox box{.width = kBoxSide, .height = kBoxSide};
  VignetteFraming framing;
  framing.scale = kScale;
  framing.origin = glm::vec2(18.0F, 18.0F);
  const Image image = DrawVignette(composed, box, framing);
  REQUIRE(image.width == kBoxSide / static_cast<std::int32_t>(kScale));
  const auto opaque_at = [&image](std::int32_t x, std::int32_t y) {
    const std::size_t offset =
        (static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) +
         static_cast<std::size_t>(x)) *
        4;
    return image.pixels[offset + 3] != 0;
  };

  // L'image s'étire sur la boîte, comme dans l'éditeur. Chaque quart de pixel
  // de la boîte est survolé : la pipette répond là où l'image est peinte, et
  // seulement là.
  const glm::vec2 shown(static_cast<float>(kBoxSide), static_cast<float>(kBoxSide));
  constexpr float kStep = 0.25F;
  std::size_t painted_hits = 0;
  for (float y = 0.0F; y < shown.y; y += kStep) {
    for (float x = 0.0F; x < shown.x; x += kStep) {
      const std::optional<glm::vec2> at =
          ro::render::VignetteSampleUnder(box, framing, shown, glm::vec2(x, y));
      REQUIRE(at.has_value());
      const bool picked = ro::render::PaletteIndexAt(composed, *at).has_value();
      const bool painted =
          opaque_at(static_cast<std::int32_t>(x / kScale), static_cast<std::int32_t>(y / kScale));
      CHECK(picked == painted);
      painted_hits += painted ? 1 : 0;
    }
  }
  CHECK(painted_hits > 0);

  // Hors de l'image, rien.
  CHECK_FALSE(
      ro::render::VignetteSampleUnder(box, framing, shown, glm::vec2(-1.0F, 4.0F)).has_value());
  CHECK_FALSE(
      ro::render::VignetteSampleUnder(box, framing, shown, glm::vec2(4.0F, shown.y)).has_value());
}

TEST_CASE("Vignette : sans cadrage, rien n'est dessiné", "[render][vignette]") {
  ComposedActor composed;
  SpriteQuad quad;
  quad.image_index = 0;
  composed.quads.push_back(quad);
  CHECK(DrawVignette(composed, VignetteBox{.width = 4, .height = 4}, VignetteFraming{}).empty());
}

TEST_CASE("Vignette centrée : un petit sprite garde sa taille, au milieu de la boîte",
          "[render][vignette]") {
  // 🔴 LA TAILLE D'UN MONSTRE DIT SON GABARIT. Un Poring de trente pixels
  // agrandi jusqu'à remplir sa case se lirait comme un boss : il reste à
  // l'échelle un, centré dans les deux sens.
  OutfitSpan span;
  span.all = Box(-10.0F, -30.0F, 10.0F, 0.0F);
  span.measured = true;
  const VignetteBox box{.width = 64, .height = 64, .fit = VignetteFit::kCenteredShrinkOnly};

  const VignetteFraming framing = FrameVignette(span, box);

  REQUIRE(framing.usable());
  CHECK(framing.scale == 1.0F);
  // Le milieu de l'enveloppe, (0, -15), tombe au milieu de la boîte.
  CHECK(framing.origin.x == 32.0F);
  CHECK(framing.origin.y == 47.0F);
  // ⚠ DEBOUT, LE MÊME SPRITE GRANDIRAIT : c'est ce qui distingue les deux.
  span.body = span.all;
  CHECK(FrameVignette(span, VignetteBox{.width = 64, .height = 64}).scale > 1.0F);
}

TEST_CASE("Vignette centrée : un grand sprite est réduit sur l'axe le plus serré",
          "[render][vignette]") {
  OutfitSpan span;
  // Deux cents de large, quatre cents de haut : c'est la hauteur qui borne.
  span.all = Box(0.0F, -400.0F, 200.0F, 0.0F);
  span.measured = true;

  const VignetteFraming framing =
      FrameVignette(span, VignetteBox{.width = 100,
                                      .height = 100,
                                      .min_body_height = 80.0F,
                                      .scale = 1.0F,
                                      .fit = VignetteFit::kCenteredShrinkOnly});

  REQUIRE(framing.usable());
  // ⚠ NI LE PLANCHER NI L'ÉCHELLE IMPOSÉE NE S'Y APPLIQUENT : seule la boîte
  // décide, et le centre de l'enveloppe, (100, -200), tombe au sien.
  CHECK(framing.scale == 0.25F);
  CHECK(framing.origin.x == 25.0F);
  CHECK(framing.origin.y == 100.0F);
}

TEST_CASE("Vignette centrée : une enveloppe plate ne se cadre pas", "[render][vignette]") {
  OutfitSpan span;
  span.all = Box(-10.0F, 0.0F, 10.0F, 0.0F);
  span.measured = true;
  CHECK_FALSE(
      FrameVignette(span,
                    VignetteBox{.width = 64, .height = 64, .fit = VignetteFit::kCenteredShrinkOnly})
          .usable());
  // Et le défaut reste le cadrage debout : rien ne change pour qui ne demande rien.
  CHECK(VignetteBox{}.fit == VignetteFit::kStanding);
}

TEST_CASE("Vignette centrée : l'enveloppe ne regarde qu'une direction", "[render][vignette]") {
  auto sprite = ro::formats::SprSprite::Parse(SpriteBytes());
  REQUIRE(sprite.has_value());
  constexpr float kStepX = 10.0F;
  constexpr std::size_t kFrames = 3;
  auto animation = ro::formats::ActAnimation::Parse(SidewaysBytes(kStepX, kFrames));
  REQUIRE(animation.has_value());

  ActorPiece body;
  body.sprite = &*sprite;
  body.animation = &*animation;
  body.part = ActorPart::kBody;
  ActorOutfit outfit(std::vector<ActorPiece>{body});

  const OutfitSpan facing = MeasureDirectionSpan(outfit, kAnimationIdle, 0);
  REQUIRE(facing.measured);
  // De face, le calque ne quitte pas l'origine ; mais toutes ses images
  // comptent, et la troisième descend de deux pixels.
  CHECK(facing.all.min.x == -kHalfImage);
  CHECK(facing.all.max.x == kHalfImage);
  CHECK(facing.all.max.y == static_cast<float>(kFrames - 1) + kHalfImage);
  const std::size_t last = kDirectionCount - 1;
  CHECK(MeasureDirectionSpan(outfit, kAnimationIdle, last).all.min.x ==
        kStepX * static_cast<float>(last) - kHalfImage);

  // 🔴 UNE DIRECTION LARGE AILLEURS NE RÉDUIT PAS CELLE QU'ON MONTRE : sur
  // l'enveloppe des huit, le même monstre rapetisserait dans sa case.
  const VignetteBox box{.width = 64, .height = 64, .fit = VignetteFit::kCenteredShrinkOnly};
  CHECK(FrameVignette(facing, box).scale == 1.0F);
  CHECK(FrameVignette(MeasureOutfitSpan(outfit, kAnimationIdle), box).scale < 1.0F);
}

TEST_CASE("Vignette centrée : sans pièce, rien à mesurer", "[render][vignette]") {
  ActorOutfit empty(std::vector<ActorPiece>{});
  CHECK_FALSE(MeasureDirectionSpan(empty, kAnimationIdle, 0).measured);
}
