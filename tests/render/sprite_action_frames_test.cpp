// Les images d'une action, composées sur une toile commune.

#include "ro/render/sprite_action_frames.h"

#include <cstddef>
#include <cstdint>
#include <utility>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/formats/act.h"
#include "ro/formats/spr.h"

#include "support/sprite_builder.h"

using ro::formats::ActAnimation;
using ro::formats::SprSprite;
using ro::render::ActionFrames;
using ro::render::ComposeActionFrames;
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

constexpr std::int32_t kSquareSidePx = 4;
constexpr std::size_t kChannels = 4;
// Huit unités de cadence, soit 192 ms par image.
constexpr float kSpeedUnits = 8.0F;

// Un carré rouge plein de quatre pixels de côté.
SprSprite RedSquare() {
  SprIndexedFrameSpec frame{.width = kSquareSidePx, .height = kSquareSidePx};
  frame.indices.assign(static_cast<std::size_t>(kSquareSidePx * kSquareSidePx), 1);
  SprBuildOptions options;
  options.indexed = {frame};
  options.palette = {SpriteColor{}, SpriteColor{.red = 255}};
  auto sprite = SprSprite::Parse(BuildSpr(options));
  REQUIRE(sprite.has_value());
  return std::move(*sprite);
}

// Deux images : le carré au centre, puis décalé de quatre pixels à droite.
ActAnimation SlidingSquare() {
  ActBuildOptions options;
  options.actions = {ActActionSpec{.frames = {
                                       ActFrameSpec{.layers = {ActLayerSpec{}}},
                                       ActFrameSpec{.layers = {ActLayerSpec{.offset_x = 4.0F}}},
                                   }}};
  options.speeds = {kSpeedUnits};
  auto animation = ActAnimation::Parse(BuildAct(options));
  REQUIRE(animation.has_value());
  return std::move(*animation);
}

// Le rouge du pixel (x, y) d'une image.
[[nodiscard]] std::uint8_t RedAt(const ro::formats::Image& image, std::int32_t x, std::int32_t y) {
  return image.pixels[(static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) +
                       static_cast<std::size_t>(x)) *
                      kChannels];
}

}  // namespace

TEST_CASE("Images d'action : une toile commune, et le dessin bouge dedans", "[render][sprite]") {
  const SprSprite sprite = RedSquare();
  const ActAnimation animation = SlidingSquare();
  const ActionFrames frames = ComposeActionFrames(sprite, animation, 0);
  REQUIRE(frames.frames.size() == 2);
  CHECK(frames.frame_interval_ms ==
        Catch::Approx(kSpeedUnits * ro::formats::kMillisecondsPerSpeedUnit));
  // L'emprise couvre les deux positions : huit pixels de large, quatre de haut.
  for (const ro::formats::Image& frame : frames.frames) {
    CHECK(frame.width == 2 * kSquareSidePx);
    CHECK(frame.height == kSquareSidePx);
  }
  // La première image peint la moitié gauche, la seconde la moitié droite.
  CHECK(RedAt(frames.frames[0], 1, 1) == 255);
  CHECK(RedAt(frames.frames[0], kSquareSidePx + 1, 1) == 0);
  CHECK(RedAt(frames.frames[1], 1, 1) == 0);
  CHECK(RedAt(frames.frames[1], kSquareSidePx + 1, 1) == 255);
}

TEST_CASE("Images d'action : une action absente ne rend rien", "[render][sprite]") {
  const SprSprite sprite = RedSquare();
  const ActAnimation animation = SlidingSquare();
  CHECK(ComposeActionFrames(sprite, animation, 1).frames.empty());
}
