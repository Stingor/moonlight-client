#include "ro/render/doll_animation.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/act.h"
#include "ro/formats/gif.h"
#include "ro/formats/image.h"
#include "ro/formats/spr.h"
#include "ro/render/actor_layers.h"
#include "ro/render/actor_sprite.h"
#include "ro/render/sprite_frame.h"

#include "support/sprite_builder.h"

using ro::render::AlternateClock;
using ro::render::DollClocks;
using ro::render::DollTimeline;
using ro::render::PlanDollTimeline;

namespace {

// Un tic du .act : vingt-quatre millisecondes.
constexpr float kTickMs = ro::formats::kMillisecondsPerSpeedUnit;
// La cadence ordinaire d'un corps : quatre tics par image.
constexpr float kBodyFrameMs = 4.0F * kTickMs;

[[nodiscard]] int Total(const DollTimeline& timeline) {
  return std::accumulate(timeline.delays_cs.begin(), timeline.delays_cs.end(), 0);
}

[[nodiscard]] bool AllPlayable(const DollTimeline& timeline) {
  for (const int delay : timeline.delays_cs) {
    if (delay < ro::formats::kFastestGifDelayCs) {
      return false;
    }
  }
  return true;
}

// Une coiffe qui change d'image tous les `cadence` tics, sur `multiple` images.
[[nodiscard]] AlternateClock Hat(std::int32_t cadence, std::size_t multiple) {
  return AlternateClock{.cadence_units = cadence, .multiple = multiple};
}

}  // namespace

TEST_CASE("Pantin en GIF : rien d'animé, une seule image", "[render][doll_gif]") {
  // Une pose tenue, sans coiffe animée : le corps a beau compter trois images,
  // elles ne défilent pas sur la fiche, et pas davantage dans le GIF.
  const DollClocks still{.body_plays = false, .body_frames = 3, .body_frame_ms = kBodyFrameMs};
  const DollTimeline timeline = PlanDollTimeline(still);
  REQUIRE(timeline.sample_ms.size() == 1);
  CHECK(timeline.sample_ms[0] == 0.0F);
  REQUIRE(timeline.delays_cs.size() == 1);
}

TEST_CASE("Pantin en GIF : le corps seul joue son tour, sans dérive", "[render][doll_gif]") {
  // 🔴 96 MS NE TOMBENT PAS SUR UN CENTIÈME. Arrondir chaque délai donnerait
  // dix partout, soit 80 cs pour un tour de 76,8 : le GIF avancerait d'un
  // trentième de seconde à chaque tour. Cumulés, les délais alternent 10 et 9.
  const DollClocks walking{.body_plays = true, .body_frames = 8, .body_frame_ms = kBodyFrameMs};
  const DollTimeline timeline = PlanDollTimeline(walking);
  REQUIRE(timeline.sample_ms.size() == 8);
  REQUIRE(timeline.delays_cs.size() == 8);
  CHECK(Total(timeline) == 77);
  for (const int delay : timeline.delays_cs) {
    CHECK((delay == 9 || delay == 10));
  }
}

TEST_CASE("Pantin en GIF : corps immobile, la coiffe animée mène", "[render][doll_gif]") {
  // Assis ou au repos, une coiffe qui vit sur sa propre horloge suffit à faire
  // une animation : quatre images de trois tics, soit un tour de 288 ms.
  DollClocks sitting{.body_plays = false, .body_frames = 3, .body_frame_ms = kBodyFrameMs};
  sitting.attachments = {Hat(3, 4)};
  const DollTimeline timeline = PlanDollTimeline(sitting);
  REQUIRE(timeline.sample_ms.size() == 4);
  CHECK(timeline.sample_ms[1] == 72.0F);
  CHECK(Total(timeline) == 29);
  CHECK(AllPlayable(timeline));
}

TEST_CASE("Pantin en GIF : la boucle est le plus petit commun multiple", "[render][doll_gif]") {
  // Un corps de 384 ms et une coiffe de 288 ms ne se retrouvent qu'à 1152 ms :
  // boucler sur le seul corps couperait la coiffe au milieu de son tour.
  DollClocks guard{.body_plays = true, .body_frames = 4, .body_frame_ms = kBodyFrameMs};
  guard.attachments = {Hat(3, 4)};
  const DollTimeline timeline = PlanDollTimeline(guard);
  // 12 pas du corps, 16 de la coiffe, dont 4 communs.
  CHECK(timeline.sample_ms.size() == 24);
  CHECK(Total(timeline) == 115);
  CHECK(AllPlayable(timeline));
  REQUIRE(timeline.delays_cs.size() == timeline.sample_ms.size());
}

TEST_CASE("Pantin en GIF : au-delà de quarante images, le tour du corps suffit",
          "[render][doll_gif]") {
  // 768 ms de marche et 288 ms de coiffe : 48 instants sur 2304 ms. Trop pour
  // un GIF qu'on colle dans un salon ; on garde le tour du corps.
  DollClocks walking{.body_plays = true, .body_frames = 8, .body_frame_ms = kBodyFrameMs};
  walking.attachments = {Hat(3, 4)};
  const DollTimeline timeline = PlanDollTimeline(walking);
  CHECK(timeline.sample_ms.size() == 8);
  CHECK(Total(timeline) == 77);
}

TEST_CASE("Pantin en GIF : deux instants trop proches n'en font qu'un", "[render][doll_gif]") {
  // Douze millisecondes par image : en deçà de deux centièmes, les navigateurs
  // n'obéissent plus. Aucun délai ne descend sous ce seuil, et le tour garde
  // sa durée.
  const DollClocks hasty{.body_plays = true, .body_frames = 10, .body_frame_ms = 12.0F};
  const DollTimeline timeline = PlanDollTimeline(hasty);
  // Dix instants de 0 à 108 ms, soit 0, 1, 2, 4, 5, 6, 7, 8, 10 et 11 cs :
  // restent 0, 2, 4, 6, 8 et 10.
  CHECK(timeline.sample_ms.size() == 6);
  CHECK(AllPlayable(timeline));
  CHECK(Total(timeline) == 12);
}

TEST_CASE("Pantin en GIF : une cadence aberrante ne compte pas", "[render][doll_gif]") {
  // 🔴 UN .ACT ABÎMÉ NE DOIT NI GELER LA BOUCLE NI RENDRE UN PLAN VIDE. Infinie,
  // la cadence donnerait une boucle sans fin ; indéfinie, une comparaison
  // toujours fausse ; sous la milliseconde, aucun instant ; énorme, un
  // débordement des centièmes. Chacune laisse la pièce sur son image.
  const float aberrant[] = {std::numeric_limits<float>::infinity(),
                            std::numeric_limits<float>::quiet_NaN(),
                            0.1F,
                            0.0F,
                            -96.0F,
                            1.0e30F};
  for (const float frame_ms : aberrant) {
    const DollClocks broken{.body_plays = true, .body_frames = 8, .body_frame_ms = frame_ms};
    const DollTimeline timeline = PlanDollTimeline(broken);
    REQUIRE(timeline.sample_ms.size() == 1);
    REQUIRE(timeline.delays_cs.size() == 1);
  }
  // Une coiffe aberrante n'empêche pas le corps de jouer.
  DollClocks walking{.body_plays = true, .body_frames = 8, .body_frame_ms = kBodyFrameMs};
  walking.attachments = {Hat(std::numeric_limits<std::int32_t>::max(), 4)};
  const DollTimeline timeline = PlanDollTimeline(walking);
  CHECK(timeline.sample_ms.size() == 8);
  CHECK(Total(timeline) == 77);
}

// ── LES IMAGES ──────────────────────────────────────────────────────────────

namespace {

using ro::render::ActorOutfit;
using ro::render::ActorPart;
using ro::render::ActorPiece;
using ro::render::kDirectionCount;
using ro::test_support::ActActionSpec;
using ro::test_support::ActAnchorSpec;
using ro::test_support::ActBuildOptions;
using ro::test_support::ActFrameSpec;
using ro::test_support::ActLayerSpec;
using ro::test_support::BuildAct;
using ro::test_support::BuildSpr;
using ro::test_support::SprBuildOptions;

// Un sprite d'une image rouge de deux pixels sur deux.
std::vector<std::uint8_t> RedSquareBytes() {
  SprBuildOptions options;
  options.indexed = {{2, 2, {1, 1, 1, 1}}};
  options.palette = {{0, 0, 0}, {255, 0, 0}};
  return BuildSpr(options);
}

// Huit actions de `frames` images, quatre tics chacune ; à chaque image, le
// carré DESCEND d'un pixel.
std::vector<std::uint8_t> FallingBytes(std::size_t frames) {
  ActBuildOptions options;
  for (std::size_t direction = 0; direction < kDirectionCount; ++direction) {
    ActActionSpec action;
    for (std::size_t frame = 0; frame < frames; ++frame) {
      ActFrameSpec frame_spec;
      ActLayerSpec layer;
      layer.frame_index = 0;
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

[[nodiscard]] bool OpaqueRedAt(const ro::formats::Image& image, int x, int y) {
  const std::size_t at = (static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) +
                          static_cast<std::size_t>(x)) *
                         4U;
  return image.pixels[at] == 255 && image.pixels[at + 1] == 0 && image.pixels[at + 3] == 255;
}

[[nodiscard]] bool ClearAt(const ro::formats::Image& image, int x, int y) {
  const std::size_t at = (static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) +
                          static_cast<std::size_t>(x)) *
                         4U;
  return image.pixels[at + 3] == 0;
}

}  // namespace

TEST_CASE("Pantin en GIF : un seul cadrage, l'échelle entière et la marge", "[render][doll_gif]") {
  auto sprite = ro::formats::SprSprite::Parse(RedSquareBytes());
  REQUIRE(sprite.has_value());
  constexpr std::size_t kFrames = 3;
  auto animation = ro::formats::ActAnimation::Parse(FallingBytes(kFrames));
  REQUIRE(animation.has_value());
  ActorPiece body;
  body.sprite = &*sprite;
  body.animation = &*animation;
  body.part = ActorPart::kBody;
  ActorOutfit outfit(std::vector<ActorPiece>{body});

  const ro::render::DollShot shot{.animation = ro::render::kAnimationIdle,
                                  .direction = 0,
                                  .body_plays = true,
                                  .scale = 2,
                                  .margin_px = ro::render::kDollGifMarginPx};
  const DollTimeline timeline =
      PlanDollTimeline(ro::render::DollClocksOf(outfit, shot.animation, shot.direction, true));
  REQUIRE(timeline.sample_ms.size() == kFrames);

  const auto poses = ro::render::ComposeDollPoses(outfit, shot, timeline);
  REQUIRE(poses.has_value());
  const auto frames = ro::render::DrawDollFrames(*poses, shot);
  REQUIRE(frames.has_value());
  REQUIRE(frames->size() == kFrames);
  // L'enveloppe COMMUNE : deux pixels de large, quatre de haut (le carré
  // descend de deux), doublés, plus quatorze de chaque côté.
  constexpr int kMargin = ro::render::kDollGifMarginPx;
  for (const ro::formats::Image& frame : *frames) {
    CHECK(frame.width == 2 * 2 + 2 * kMargin);
    CHECK(frame.height == 4 * 2 + 2 * kMargin);
  }
  // Première image : le carré en haut de l'enveloppe ; la troisième l'a
  // descendu de deux pixels de sprite, soit quatre du GIF.
  CHECK(OpaqueRedAt((*frames)[0], kMargin, kMargin));
  CHECK(OpaqueRedAt((*frames)[0], kMargin + 3, kMargin + 3));
  CHECK(ClearAt((*frames)[2], kMargin, kMargin));
  CHECK(OpaqueRedAt((*frames)[2], kMargin, kMargin + 4));
  // La marge reste vide.
  CHECK(ClearAt((*frames)[0], kMargin - 1, kMargin));
}

TEST_CASE("Pantin en GIF : une tenue vide ne fait pas d'image", "[render][doll_gif]") {
  ActorOutfit nobody(std::vector<ActorPiece>{});
  const ro::render::DollShot shot{};
  const DollTimeline timeline{.sample_ms = {0.0F}, .delays_cs = {0}};
  CHECK_FALSE(ro::render::ComposeDollPoses(nobody, shot, timeline).has_value());
  CHECK_FALSE(ro::render::DrawDollFrames(ro::render::DollPoses{}, shot).has_value());
}

TEST_CASE("Pantin en GIF : au-delà du plafond de pixels, rien ne s'alloue", "[render][doll_gif]") {
  // 🔴 LES IMAGES SE FONT SUR LE FIL DE LA BOUCLE : une enveloppe de 512 de
  // côté, à l'échelle trois, sur quarante images, ferait 1,5 Go. Le refus doit
  // tomber AVANT de dessiner — il se juge sur des poses vides, que le dessin
  // ne saurait de toute façon pas peindre.
  ro::render::DollPoses huge;
  huge.width = ro::render::kLargestDollSpritePx;
  huge.height = ro::render::kLargestDollSpritePx;
  huge.poses.resize(ro::render::kMostDollGifFrames);
  const ro::render::DollShot shot{.scale = ro::render::kLargestDollGifScale};
  CHECK(ro::render::DollGifPixels(huge, shot) > ro::render::kMostDollGifPixels);
  CHECK_FALSE(ro::render::DrawDollFrames(huge, shot).has_value());
  // Un personnage ordinaire — deux cents de côté — passe à l'échelle trois.
  ro::render::DollPoses ordinary = huge;
  ordinary.width = 200;
  ordinary.height = 200;
  CHECK(ro::render::DollGifPixels(ordinary, shot) <= ro::render::kMostDollGifPixels);
}

namespace {

// Huit actions de `frames` images, à `speed` tics ; le calque de l'image k est
// décalé de `base_x + step_x × k`.
std::vector<std::uint8_t> SlidingBytes(std::size_t frames, float speed, float base_x,
                                       float step_x) {
  ActBuildOptions options;
  for (std::size_t direction = 0; direction < kDirectionCount; ++direction) {
    ActActionSpec action;
    for (std::size_t frame = 0; frame < frames; ++frame) {
      ActFrameSpec frame_spec;
      ActLayerSpec layer;
      layer.frame_index = 0;
      layer.offset_x = base_x + step_x * static_cast<float>(frame);
      frame_spec.layers.push_back(layer);
      frame_spec.anchors.push_back(ActAnchorSpec{});
      action.frames.push_back(std::move(frame_spec));
    }
    options.actions.push_back(std::move(action));
    options.speeds.push_back(speed);
  }
  return BuildAct(options);
}

}  // namespace

TEST_CASE("Pantin en GIF : une coiffe à horloge propre vit sur un corps immobile",
          "[render][doll_gif]") {
  // De bout en bout : la garde-robe dit l'horloge de la coiffe, le plan en
  // tire ses instants, et chaque image montre la coiffe à SA place.
  auto sprite = ro::formats::SprSprite::Parse(RedSquareBytes());
  REQUIRE(sprite.has_value());
  // Le corps et la tête : une image, aucun décalage.
  auto still = ro::formats::ActAnimation::Parse(SlidingBytes(1, 4.0F, 0.0F, 0.0F));
  REQUIRE(still.has_value());
  // La coiffe : quatre images pour une de tête, trois tics chacune, et le
  // carré qui glisse de dix pixels à chaque image.
  constexpr float kHatStepPx = 10.0F;
  auto hat = ro::formats::ActAnimation::Parse(SlidingBytes(4, 3.0F, kHatStepPx, kHatStepPx));
  REQUIRE(hat.has_value());

  ActorPiece body{.sprite = &*sprite, .animation = &*still, .part = ActorPart::kBody};
  ActorPiece head{.sprite = &*sprite,
                  .animation = &*still,
                  .part = ActorPart::kHead,
                  .attach = ro::render::AttachTo::kBody,
                  .is_head = true};
  ActorPiece top{.sprite = &*sprite,
                 .animation = &*hat,
                 .part = ActorPart::kHeadgearTop,
                 .attach = ro::render::AttachTo::kHead};
  ActorOutfit outfit(std::vector<ActorPiece>{body, head, top});

  const std::vector<AlternateClock> clocks = outfit.OwnClocks(ro::render::kAnimationIdle);
  REQUIRE(clocks.size() == 1);
  CHECK(clocks[0].cadence_units == 3);
  CHECK(clocks[0].multiple == 4);

  const ro::render::DollShot shot{.animation = ro::render::kAnimationIdle,
                                  .direction = 0,
                                  .body_plays = false,
                                  .scale = 1,
                                  .margin_px = 0};
  const DollTimeline timeline =
      PlanDollTimeline(ro::render::DollClocksOf(outfit, shot.animation, shot.direction, false));
  REQUIRE(timeline.sample_ms.size() == 4);
  const auto poses = ro::render::ComposeDollPoses(outfit, shot, timeline);
  REQUIRE(poses.has_value());
  const auto frames = ro::render::DrawDollFrames(*poses, shot);
  REQUIRE(frames.has_value());
  REQUIRE(frames->size() == 4);
  // Le corps occupe les colonnes 0 et 1 ; la coiffe de l'image k commence à la
  // colonne 10 × (k + 1), et n'est nulle part ailleurs.
  for (std::size_t image = 0; image < frames->size(); ++image) {
    const ro::formats::Image& frame = (*frames)[image];
    const int column = static_cast<int>(kHatStepPx) * (static_cast<int>(image) + 1);
    INFO("image " << image);
    CHECK(OpaqueRedAt(frame, 0, 0));
    CHECK(OpaqueRedAt(frame, column, 0));
    for (int other = 1; other <= 4; ++other) {
      if (other != static_cast<int>(image) + 1) {
        CHECK(ClearAt(frame, static_cast<int>(kHatStepPx) * other, 0));
      }
    }
  }
}
