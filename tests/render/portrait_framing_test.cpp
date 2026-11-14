// LE CADRAGE D'UN PORTRAIT, par des nombres : quelles boîtes une image peint,
// et où elles tombent dans le cadre.

#include "ro/render/portrait_framing.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/formats/act.h"
#include "ro/formats/spr.h"
#include "ro/render/actor_sprite.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/render/doll_animation.h"
#include "ro/render/sprite_frame.h"

#include "support/sprite_builder.h"

using Catch::Approx;
using ro::render::ActorPart;
using ro::render::FramePortrait;
using ro::render::MeasurePortraitBoxes;
using ro::render::PortraitAim;
using ro::render::PortraitBoxes;
using ro::render::SpriteBounds;
using ro::render::SpriteQuad;
using ro::render::VignetteFraming;

namespace {

// Un quad droit, de (min_x, min_y) à (max_x, max_y).
SpriteQuad Quad(float min_x, float min_y, float max_x, float max_y) {
  SpriteQuad quad;
  quad.corners = {glm::vec2(min_x, min_y), glm::vec2(max_x, min_y), glm::vec2(max_x, max_y),
                  glm::vec2(min_x, max_y)};
  return quad;
}

SpriteBounds Box(float min_x, float min_y, float max_x, float max_y) {
  return SpriteBounds{.min = glm::vec2(min_x, min_y), .max = glm::vec2(max_x, max_y)};
}

// Une tête de vingt pixels posée sur un corps de trente de haut, pieds à
// l'origine.
PortraitBoxes Character() {
  PortraitBoxes boxes;
  boxes.head = Box(-10.0F, -50.0F, 10.0F, -30.0F);
  boxes.body = Box(-15.0F, -30.0F, 15.0F, 0.0F);
  boxes.has_head = true;
  boxes.has_body = true;
  return boxes;
}

// Un cadre de cent pixels, dont la vignette couvre l'intérieur à un pixel du
// bord.
constexpr float kInset = 1.0F;
const glm::vec2 kDrawnBox(98.0F, 98.0F);

PortraitAim Aim(bool head_only) {
  PortraitAim aim;
  aim.head_only = head_only;
  aim.inset_px = kInset;
  return aim;
}

}  // namespace

TEST_CASE("Portrait : la tête et ses coiffes font une boîte, le corps une autre",
          "[render][portrait]") {
  const std::vector<SpriteQuad> quads = {
      Quad(-15.0F, -30.0F, 15.0F, 0.0F),    // le corps
      Quad(-10.0F, -50.0F, 10.0F, -30.0F),  // la tête
      Quad(-12.0F, -60.0F, 12.0F, -45.0F),  // la coiffe du haut
      Quad(20.0F, -40.0F, 60.0F, -20.0F),   // l'arme : ni tête ni corps
      Quad(-30.0F, -35.0F, 30.0F, 5.0F),    // la cape : ni l'une ni l'autre
      Quad(-5.0F, -70.0F, 5.0F, -62.0F),    // un calque sans partie
  };
  const std::array<ActorPart, 5> parts = {ActorPart::kBody, ActorPart::kHead,
                                          ActorPart::kHeadgearTop, ActorPart::kWeapon,
                                          ActorPart::kGarment};
  const PortraitBoxes boxes = MeasurePortraitBoxes(quads, parts);
  REQUIRE(boxes.has_head);
  REQUIRE(boxes.has_body);
  CHECK(boxes.head.min == glm::vec2(-12.0F, -60.0F));
  CHECK(boxes.head.max == glm::vec2(12.0F, -30.0F));
  // Un quad sans partie compte pour le corps.
  CHECK(boxes.body.min == glm::vec2(-15.0F, -70.0F));
  CHECK(boxes.body.max == glm::vec2(15.0F, 0.0F));
}

TEST_CASE("Portrait : tête seule, la tête remplit la largeur et tombe au centre",
          "[render][portrait]") {
  const VignetteFraming framing = FramePortrait(Character(), Aim(true), kDrawnBox);
  REQUIRE(framing.usable());
  // La largeur du cadre moins quatre pixels, sur les vingt de la tête.
  CHECK(framing.scale == Approx(96.0F / 20.0F));
  // Le centre de la tête, (0, -40), au centre du cadre — repéré depuis la
  // boîte dessinée, un pixel plus loin.
  const glm::vec2 center = framing.origin + glm::vec2(0.0F, -40.0F) * framing.scale;
  CHECK(center.x == Approx(49.0F));
  CHECK(center.y == Approx(49.0F));
}

TEST_CASE("Portrait : une coiffe haute tient aussi en hauteur quand on le demande",
          "[render][portrait]") {
  // Une tête coiffée deux fois plus haute que large.
  PortraitBoxes boxes = Character();
  boxes.head = Box(-10.0F, -70.0F, 10.0F, -30.0F);
  PortraitAim aim = Aim(true);
  // Réglé sur la largeur seule, elle dépasse : quarante pixels à 4,8 font 192.
  CHECK(FramePortrait(boxes, aim, kDrawnBox).scale == Approx(96.0F / 20.0F));
  aim.fit_height = true;
  const VignetteFraming framing = FramePortrait(boxes, aim, kDrawnBox);
  REQUIRE(framing.usable());
  CHECK(framing.scale == Approx(96.0F / 40.0F));
  // Le haut et le bas de la coiffe restent dans le cadre de cent pixels.
  const float top = framing.origin.y + (-70.0F * framing.scale);
  const float bottom = framing.origin.y + (-30.0F * framing.scale);
  CHECK(top >= -kInset);
  CHECK(bottom <= kDrawnBox.y + kInset);
}

TEST_CASE("Portrait : en entier, le point visé est entre la tête et le corps",
          "[render][portrait]") {
  const VignetteFraming framing = FramePortrait(Character(), Aim(false), kDrawnBox);
  REQUIRE(framing.usable());
  // L'union des deux boîtes : trente pixels de large.
  CHECK(framing.scale == Approx(96.0F / 30.0F));
  // La moyenne des centres : (0, -40) et (0, -15) donnent (0, -27.5).
  const glm::vec2 center = framing.origin + glm::vec2(0.0F, -27.5F) * framing.scale;
  CHECK(center.x == Approx(49.0F));
  CHECK(center.y == Approx(49.0F));
}

TEST_CASE("Portrait : le zoom pivote sur le point visé, le décalage le déplace",
          "[render][portrait]") {
  PortraitAim aim = Aim(true);
  const VignetteFraming plain = FramePortrait(Character(), aim, kDrawnBox);
  aim.zoom = 2.0F;
  const VignetteFraming zoomed = FramePortrait(Character(), aim, kDrawnBox);
  CHECK(zoomed.scale == Approx(plain.scale * 2.0F));
  const glm::vec2 head_center(0.0F, -40.0F);
  const glm::vec2 before = plain.origin + head_center * plain.scale;
  const glm::vec2 after = zoomed.origin + head_center * zoomed.scale;
  CHECK(after.x == Approx(before.x));
  CHECK(after.y == Approx(before.y));

  // En tailles du cadre ENTIER : un quart de cent pixels à droite, la moitié
  // vers le haut.
  aim.offset = glm::vec2(0.25F, -0.5F);
  const VignetteFraming moved = FramePortrait(Character(), aim, kDrawnBox);
  const glm::vec2 shifted = moved.origin + head_center * moved.scale;
  CHECK(shifted.x == Approx(before.x + 25.0F));
  CHECK(shifted.y == Approx(before.y - 50.0F));
}

TEST_CASE("Portrait : sans tête, la tête seule cadre le corps", "[render][portrait]") {
  PortraitBoxes boxes = Character();
  boxes.has_head = false;
  const VignetteFraming framing = FramePortrait(boxes, Aim(true), kDrawnBox);
  REQUIRE(framing.usable());
  CHECK(framing.scale == Approx(96.0F / 30.0F));
  const glm::vec2 center = framing.origin + glm::vec2(0.0F, -15.0F) * framing.scale;
  CHECK(center.x == Approx(49.0F));
  CHECK(center.y == Approx(49.0F));
}

TEST_CASE("Portrait : rien à cadrer, rien de dessiné", "[render][portrait]") {
  CHECK_FALSE(FramePortrait(PortraitBoxes{}, Aim(true), kDrawnBox).usable());
  // Une tête d'un pixel de large ne se cadre pas : l'échelle exploserait.
  PortraitBoxes thin = Character();
  thin.head = Box(0.0F, -50.0F, 1.0F, -30.0F);
  CHECK_FALSE(FramePortrait(thin, Aim(true), kDrawnBox).usable());
}

TEST_CASE("Portrait : un cadre étroit et haut, dézoomé, garde un canevas borné",
          "[render][portrait]") {
  // Huit pixels de large sur mille quatre-vingts de haut, au zoom le plus bas :
  // l'échelle ne regarde que la largeur, et la hauteur ramenée en pixels de
  // sprite dépasserait quatre-vingt mille.
  PortraitAim aim = Aim(false);
  aim.zoom = 0.1F;
  const VignetteFraming framing = FramePortrait(Character(), aim, glm::vec2(6.0F, 1078.0F));
  REQUIRE(framing.usable());

  ro::render::ComposedActor composed;
  composed.quads.push_back(Quad(-15.0F, -50.0F, 15.0F, 0.0F));
  const std::array<ro::render::VignetteBox, 2> boxes = {
      ro::render::VignetteBox{.width = 6, .height = 1078},
      // Une hauteur éditée à la main dans le fichier.
      ro::render::VignetteBox{.width = 10, .height = 10'000'000}};
  for (const ro::render::VignetteBox& box : boxes) {
    const ro::formats::Image image = ro::render::DrawVignette(composed, box, framing);
    CHECK(image.width <= static_cast<std::int32_t>(ro::render::kVignetteLargestCanvasPx));
    CHECK(image.height <= static_cast<std::int32_t>(ro::render::kVignetteLargestCanvasPx));
  }
}

// ── LA BOUCLE : UN CADRAGE PAR POSE ─────────────────────────────────────────

namespace {

using ro::render::ActorOutfit;
using ro::render::ActorPiece;
using ro::render::PortraitFramer;
using ro::render::PortraitLoop;

// Un sprite d'une image de deux pixels sur deux.
std::vector<std::uint8_t> SquareBytes() {
  ro::test_support::SprBuildOptions options;
  options.indexed = {{2, 2, {1, 1, 1, 1}}};
  options.palette = {{0, 0, 0}, {255, 0, 0}};
  return ro::test_support::BuildSpr(options);
}

// Huit directions de `frames` images, à `speed` tics ; le calque de l'image k
// est décalé de `step × (k + 1)`.
std::vector<std::uint8_t> MovingBytes(std::size_t frames, float speed, glm::vec2 step) {
  ro::test_support::ActBuildOptions options;
  for (std::size_t direction = 0; direction < ro::render::kDirectionCount; ++direction) {
    ro::test_support::ActActionSpec action;
    for (std::size_t frame = 0; frame < frames; ++frame) {
      ro::test_support::ActFrameSpec frame_spec;
      ro::test_support::ActLayerSpec layer;
      layer.offset_x = step.x * static_cast<float>(frame + 1);
      layer.offset_y = step.y * static_cast<float>(frame + 1);
      frame_spec.layers.push_back(layer);
      frame_spec.anchors.push_back(ro::test_support::ActAnchorSpec{});
      action.frames.push_back(std::move(frame_spec));
    }
    options.actions.push_back(std::move(action));
    options.speeds.push_back(speed);
  }
  return ro::test_support::BuildAct(options);
}

// Un corps, une tête, et une coiffe du haut sur sa propre horloge.
struct HattedDoll {
  ro::formats::SprSprite sprite;
  ro::formats::ActAnimation body;
  ro::formats::ActAnimation head;
  ro::formats::ActAnimation hat;

  [[nodiscard]] ActorOutfit Outfit() const {
    const ActorPiece body_piece{.sprite = &sprite, .animation = &body, .part = ActorPart::kBody};
    const ActorPiece head_piece{.sprite = &sprite,
                                .animation = &head,
                                .part = ActorPart::kHead,
                                .attach = ro::render::AttachTo::kBody,
                                .is_head = true};
    const ActorPiece hat_piece{.sprite = &sprite,
                               .animation = &hat,
                               .part = ActorPart::kHeadgearTop,
                               .attach = ro::render::AttachTo::kHead};
    return ActorOutfit(std::vector<ActorPiece>{body_piece, head_piece, hat_piece});
  }
};

// Le corps compte `body_frames` images qui descendent chacune de trois
// pixels ; la tête n'en a qu'une ; la coiffe en a quatre, trois tics chacune,
// qui glissent de dix pixels vers la droite.
constexpr float kHatStepPx = 10.0F;
constexpr std::size_t kHatFrames = 4;
constexpr float kBodyDropPx = 3.0F;

HattedDoll MakeDoll(std::size_t body_frames) {
  auto sprite = ro::formats::SprSprite::Parse(SquareBytes());
  auto body = ro::formats::ActAnimation::Parse(
      MovingBytes(body_frames, 4.0F, glm::vec2(0.0F, kBodyDropPx)));
  auto head = ro::formats::ActAnimation::Parse(MovingBytes(1, 4.0F, glm::vec2(0.0F)));
  auto hat =
      ro::formats::ActAnimation::Parse(MovingBytes(kHatFrames, 3.0F, glm::vec2(kHatStepPx, 0.0F)));
  REQUIRE(sprite.has_value());
  REQUIRE(body.has_value());
  REQUIRE(head.has_value());
  REQUIRE(hat.has_value());
  return HattedDoll{.sprite = std::move(*sprite),
                    .body = std::move(*body),
                    .head = std::move(*head),
                    .hat = std::move(*hat)};
}

[[nodiscard]] bool Contains(const SpriteBounds& outer, const SpriteBounds& inner) {
  return outer.min.x <= inner.min.x && outer.min.y <= inner.min.y && outer.max.x >= inner.max.x &&
         outer.max.y >= inner.max.y;
}

[[nodiscard]] bool SameFraming(const VignetteFraming& left, const VignetteFraming& right) {
  return left.scale == right.scale && left.origin == right.origin;
}

}  // namespace

TEST_CASE("Portrait : une coiffe animée ne fait plus sauter le cadrage", "[render][portrait]") {
  const HattedDoll doll = MakeDoll(1);
  ActorOutfit outfit = doll.Outfit();
  const PortraitLoop loop{.animation = ro::render::kAnimationIdle, .direction = 0};
  const ro::render::DollClocks clocks =
      ro::render::DollClocksOf(outfit, loop.animation, loop.direction, loop.body_plays);
  const std::vector<ro::render::DollProbe> probes = ro::render::DollEnvelopeProbes(clocks, 0);
  REQUIRE(probes.size() == kHatFrames);

  // IMAGE PAR IMAGE, la boîte de la tête change : la coiffe l'élargit vers la
  // droite. C'est ce qui faisait sauter l'ancien cadrage.
  const PortraitBoxes loop_boxes = ro::render::MeasureLoopPortraitBoxes(outfit, loop);
  REQUIRE(loop_boxes.has_head);
  REQUIRE(loop_boxes.has_body);
  std::vector<VignetteFraming> per_image;
  for (const ro::render::DollProbe& probe : probes) {
    const PortraitBoxes instant = ro::render::MeasurePaintedPortraitBoxes(
        ro::render::ComposeDollProbe(outfit, loop.animation, loop.direction, probe));
    REQUIRE(instant.has_head);
    CHECK(Contains(loop_boxes.head, instant.head));
    CHECK(Contains(loop_boxes.body, instant.body));
    per_image.push_back(FramePortrait(instant, Aim(true), kDrawnBox));
  }
  CHECK_FALSE(SameFraming(per_image.front(), per_image.back()));
  // L'enveloppe va de la tête jusqu'à la coiffe la plus lointaine.
  CHECK(loop_boxes.head.max.x - loop_boxes.head.min.x >=
        kHatStepPx * static_cast<float>(kHatFrames));

  // SUR LA BOUCLE, un seul cadrage pour toutes les images, et une seule mesure.
  PortraitFramer framer;
  const ro::render::ActorLook look;
  const VignetteFraming first = framer.Frame(outfit, look, loop, Aim(true), kDrawnBox);
  REQUIRE(first.usable());
  CHECK(SameFraming(first, FramePortrait(loop_boxes, Aim(true), kDrawnBox)));
  for (const ro::render::DollProbe& probe : probes) {
    // La vignette compose chaque image entre deux cadrages : rien n'y bouge.
    (void)outfit.Composed(loop.animation, loop.direction, probe.at_ms);
    CHECK(SameFraming(framer.Frame(outfit, look, loop, Aim(true), kDrawnBox), first));
  }
  CHECK(framer.measure_count() == 1);
}

TEST_CASE("Portrait : une autre pose se remesure, un autre réglage non", "[render][portrait]") {
  const HattedDoll doll = MakeDoll(3);
  ActorOutfit outfit = doll.Outfit();
  PortraitFramer framer;
  ro::render::ActorLook look;
  PortraitLoop loop{.animation = ro::render::kAnimationIdle, .direction = 0};
  const VignetteFraming head_only = framer.Frame(outfit, look, loop, Aim(true), kDrawnBox);
  REQUIRE(framer.measure_count() == 1);

  // Tête seule, zoom, taille du cadre : la même boucle, une autre règle.
  const VignetteFraming whole = framer.Frame(outfit, look, loop, Aim(false), kDrawnBox);
  CHECK_FALSE(SameFraming(whole, head_only));
  PortraitAim zoomed = Aim(true);
  zoomed.zoom = 2.0F;
  (void)framer.Frame(outfit, look, loop, zoomed, glm::vec2(50.0F, 80.0F));
  CHECK(framer.measure_count() == 1);

  // Une autre direction, une autre animation, un corps qui se met à jouer ou
  // un autre regard, une autre apparence : chacun remesure.
  loop.direction = 2;
  (void)framer.Frame(outfit, look, loop, Aim(true), kDrawnBox);
  CHECK(framer.measure_count() == 2);
  loop.animation = ro::render::kAnimationWalk;
  (void)framer.Frame(outfit, look, loop, Aim(true), kDrawnBox);
  CHECK(framer.measure_count() == 3);
  loop.body_plays = true;
  (void)framer.Frame(outfit, look, loop, Aim(true), kDrawnBox);
  CHECK(framer.measure_count() == 4);
  loop.body_plays = false;
  loop.rest_frame = 1;
  (void)framer.Frame(outfit, look, loop, Aim(true), kDrawnBox);
  CHECK(framer.measure_count() == 5);
  look.robe = 1;
  (void)framer.Frame(outfit, look, loop, Aim(true), kDrawnBox);
  CHECK(framer.measure_count() == 6);
  (void)framer.Frame(outfit, look, loop, Aim(true), kDrawnBox);
  CHECK(framer.measure_count() == 6);
}

TEST_CASE("Portrait : figé, le corps ne compte que l'image qu'il tient", "[render][portrait]") {
  // Trois images de corps qui descendent de trois pixels chacune. Figé, le
  // corps tient une image et sa boîte est la sienne ; animé, elle couvre la
  // descente entière.
  constexpr std::size_t kBodyFrames = 3;
  const HattedDoll doll = MakeDoll(kBodyFrames);
  ActorOutfit outfit = doll.Outfit();
  PortraitLoop loop{.animation = ro::render::kAnimationIdle, .direction = 0};

  const PortraitBoxes still = ro::render::MeasureLoopPortraitBoxes(outfit, loop);
  REQUIRE(still.has_body);
  CHECK(still.body.max.y - still.body.min.y == Approx(2.0F));

  loop.rest_frame = 2;
  const PortraitBoxes held = ro::render::MeasureLoopPortraitBoxes(outfit, loop);
  REQUIRE(held.has_body);
  CHECK(held.body.min.y == Approx(still.body.min.y + 2.0F * kBodyDropPx));

  loop.rest_frame = 0;
  loop.body_plays = true;
  const PortraitBoxes playing = ro::render::MeasureLoopPortraitBoxes(outfit, loop);
  REQUIRE(playing.has_body);
  CHECK(playing.body.max.y - playing.body.min.y ==
        Approx(2.0F + static_cast<float>(kBodyFrames - 1) * kBodyDropPx));
}

TEST_CASE("Portrait : deux coiffes sans boucle commune comptent toutes leurs images",
          "[render][portrait]") {
  // 🔴 HUIT IMAGES DE TROIS TICS ET TREIZE DE SIX : la boucle commune passe
  // quarante instants, et le GIF se replie sur le plus long tour. Échantillonnée
  // à ses pas, la première coiffe ne montrerait que ses images paires — et
  // jamais la dernière, la plus lointaine. La mesure doit les voir toutes.
  constexpr std::size_t kTopFrames = 8;
  constexpr std::size_t kMiddleFrames = 13;
  auto sprite = ro::formats::SprSprite::Parse(SquareBytes());
  auto still = ro::formats::ActAnimation::Parse(MovingBytes(1, 4.0F, glm::vec2(0.0F)));
  auto top =
      ro::formats::ActAnimation::Parse(MovingBytes(kTopFrames, 3.0F, glm::vec2(kHatStepPx, 0.0F)));
  auto middle = ro::formats::ActAnimation::Parse(
      MovingBytes(kMiddleFrames, 6.0F, glm::vec2(-kHatStepPx, 0.0F)));
  REQUIRE(sprite.has_value());
  REQUIRE(still.has_value());
  REQUIRE(top.has_value());
  REQUIRE(middle.has_value());
  const ActorPiece body{.sprite = &*sprite, .animation = &*still, .part = ActorPart::kBody};
  const ActorPiece head{.sprite = &*sprite,
                        .animation = &*still,
                        .part = ActorPart::kHead,
                        .attach = ro::render::AttachTo::kBody,
                        .is_head = true};
  const ActorPiece top_hat{.sprite = &*sprite,
                           .animation = &*top,
                           .part = ActorPart::kHeadgearTop,
                           .attach = ro::render::AttachTo::kHead};
  const ActorPiece middle_hat{.sprite = &*sprite,
                              .animation = &*middle,
                              .part = ActorPart::kHeadgearMiddle,
                              .attach = ro::render::AttachTo::kHead};
  ActorOutfit outfit(std::vector<ActorPiece>{body, head, top_hat, middle_hat});
  const PortraitLoop loop{.animation = ro::render::kAnimationIdle, .direction = 0};
  const ro::render::DollClocks clocks =
      ro::render::DollClocksOf(outfit, loop.animation, loop.direction, false);
  REQUIRE(clocks.attachments.size() == 2);
  // Le plan du GIF se replie bien : il n'a pas les 8 + 13 images distinctes.
  REQUIRE(ro::render::PlanDollTimeline(clocks).sample_ms.size() == kMiddleFrames);

  // Les boîtes de chaque image de chaque coiffe, posée seule à son pas.
  SpriteBounds every;
  bool grown = false;
  for (const auto& [frames, step_ms] :
       {std::pair{kTopFrames, clocks.attachments[0].step_ms()},
        std::pair{kMiddleFrames, clocks.attachments[1].step_ms()}}) {
    for (std::size_t frame = 0; frame < frames; ++frame) {
      const PortraitBoxes boxes = ro::render::MeasurePaintedPortraitBoxes(
          ro::render::ComposeDollProbe(outfit, loop.animation, loop.direction,
                                       {.at_ms = step_ms * static_cast<float>(frame)}));
      REQUIRE(boxes.has_head);
      every.min = grown ? glm::min(every.min, boxes.head.min) : boxes.head.min;
      every.max = grown ? glm::max(every.max, boxes.head.max) : boxes.head.max;
      grown = true;
    }
  }
  const PortraitBoxes loop_boxes = ro::render::MeasureLoopPortraitBoxes(outfit, loop);
  REQUIRE(loop_boxes.has_head);
  CHECK(loop_boxes.head.min == every.min);
  CHECK(loop_boxes.head.max == every.max);
  // De la treizième image de l'une à la huitième de l'autre.
  CHECK(loop_boxes.head.max.x - loop_boxes.head.min.x >=
        kHatStepPx * static_cast<float>(kTopFrames + kMiddleFrames));
}
