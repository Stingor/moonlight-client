// LE VEILLEUR DE GLISSADE : il se déclenche sur une image qui tient pendant que
// le personnage avance, et sur rien d'autre.

#include "debug/glide_watch.h"

#include <cstddef>
#include <cstdint>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/actor.h"
#include "ro/sim/server_clock.h"

using moonclient::DescribeStep;
using moonclient::GlideEvent;
using moonclient::GlideWatch;
using moonclient::JoinFacts;
using moonclient::ShownStep;
using moonclient::WalkFactsOf;

namespace {

constexpr float kCellsPerFrame = 0.25F;
constexpr std::uint32_t kFrameMs = 16;
constexpr std::size_t kWalkFrames = 8;
constexpr std::size_t kFramesPerWalkImage = 2;
constexpr int kLongWalkFrames = 40;

[[nodiscard]] ShownStep StepAt(int frame, std::size_t image) {
  ShownStep step;
  step.cell_x = static_cast<float>(frame) * kCellsPerFrame;
  step.motion = "kWalk";
  step.held_frame = image;
  step.walking = true;
  step.now_ms = static_cast<std::uint32_t>(frame) * kFrameMs;
  return step;
}

}  // namespace

TEST_CASE("Glissade : une marche normale ne se déclenche jamais", "[app][glide]") {
  GlideWatch watch;
  for (int frame = 0; frame < kLongWalkFrames; ++frame) {
    const std::size_t image = (static_cast<std::size_t>(frame) / kFramesPerWalkImage) % kWalkFrames;
    CHECK(watch.Observe(StepAt(frame, image)) == GlideEvent::kNone);
  }
  CHECK(watch.episodes() == 0);
}

TEST_CASE("Glissade : douze images à un quart de case sous la même image", "[app][glide]") {
  GlideWatch watch;
  int began_at_frame = -1;
  for (int frame = 0; frame < kLongWalkFrames; ++frame) {
    if (watch.Observe(StepAt(frame, 0)) == GlideEvent::kBegan) {
      began_at_frame = frame;
    }
  }
  // Trois cases, à un quart de case par image.
  CHECK(began_at_frame == 12);
  CHECK(watch.episodes() == 1);
  CHECK(watch.gliding());
}

TEST_CASE("Glissade : l'image qui change clôt l'épisode et le chiffre", "[app][glide]") {
  GlideWatch watch;
  for (int frame = 0; frame <= 20; ++frame) {
    static_cast<void>(watch.Observe(StepAt(frame, 0)));
  }
  CHECK(watch.Observe(StepAt(21, 1)) == GlideEvent::kEnded);
  CHECK(watch.ended_cells() == 5.0F);
  CHECK(watch.ended_ms() == 21 * kFrameMs);
  CHECK_FALSE(watch.gliding());
}

TEST_CASE("Glissade : un déplacement sur place ne compte pas", "[app][glide]") {
  GlideWatch watch;
  ShownStep still = StepAt(0, 0);
  still.motion = "kIdle";
  for (int frame = 0; frame < kLongWalkFrames; ++frame) {
    still.now_ms = static_cast<std::uint32_t>(frame) * kFrameMs;
    CHECK(watch.Observe(still) == GlideEvent::kNone);
  }
}

TEST_CASE("Glissade : un téléport repart de zéro", "[app][glide]") {
  GlideWatch watch;
  for (int frame = 0; frame < 8; ++frame) {
    static_cast<void>(watch.Observe(StepAt(frame, 0)));
  }
  CHECK(watch.cells_on_same_image() > 0.0F);
  ShownStep arrived = StepAt(8, 0);
  arrived.cell_x += GlideWatch::kLargestStepCells * 2.0F;
  CHECK(watch.Observe(arrived) == GlideEvent::kNone);
  CHECK(watch.cells_on_same_image() == 0.0F);
}

TEST_CASE("Glissade : le bond de Body Relocation tient son image exprès", "[app][glide]") {
  GlideWatch watch;
  for (int frame = 0; frame < kLongWalkFrames; ++frame) {
    ShownStep step = StepAt(frame, 0);
    step.glide_wanted = true;
    CHECK(watch.Observe(step) == GlideEvent::kNone);
  }
}

TEST_CASE("Glissade : le relevé dit la motion, l'image et l'état de la marche", "[app][glide]") {
  ro::sim::Actor actor;
  actor.body_state = 2;
  actor.speed_ms_per_cell = 120;
  const std::string facts = JoinFacts(WalkFactsOf(actor, ro::sim::ServerClock{}, 500));
  CHECK(facts.find("body_state=2") != std::string::npos);
  CHECK(facts.find("speed=120") != std::string::npos);
  CHECK(facts.find("path=-") != std::string::npos);
  CHECK(DescribeStep(StepAt(0, 3)).find("motion=kWalk rang=0 image=3") != std::string::npos);
}
