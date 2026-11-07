#include "debug/glide_watch.h"

#include <cmath>
#include <format>
#include <utility>

#include "ro/core/log.h"
#include "ro/sim/actor.h"
#include "ro/sim/world.h"

namespace moonclient {
namespace {

[[nodiscard]] bool SameImage(const ShownStep& left, const ShownStep& right) {
  return left.motion == right.motion && left.animation == right.animation &&
         left.held_frame == right.held_frame;
}

template <typename Range, typename Read>
[[nodiscard]] std::string ListOf(const Range& range, Read read) {
  std::string listed;
  for (const auto& element : range) {
    listed += listed.empty() ? "" : ",";
    listed += std::format("{}", read(element));
  }
  return listed.empty() ? "-" : listed;
}

template <typename Value>
[[nodiscard]] std::string OptionalOf(const std::optional<Value>& value) {
  return value.has_value() ? std::format("{}", *value) : "-";
}

[[nodiscard]] std::string Cells(float cells) { return std::format("{:.2f}", cells); }

}  // namespace

GlideEvent GlideWatch::Observe(const ShownStep& step) {
  if (!last_.has_value()) {
    last_ = step;
    since_ms_ = step.now_ms;
    return GlideEvent::kNone;
  }
  const float step_cells = std::hypot(step.cell_x - last_->cell_x, step.cell_y - last_->cell_y);
  const bool continues =
      SameImage(step, *last_) && !step.glide_wanted && step_cells <= kLargestStepCells;
  GlideEvent event = GlideEvent::kNone;
  if (continues) {
    cells_ += step_cells;
    if (!gliding_ && cells_ >= kGlideCells) {
      gliding_ = true;
      ++episodes_;
      event = GlideEvent::kBegan;
    }
  } else {
    if (gliding_) {
      gliding_ = false;
      ended_cells_ = cells_;
      ended_ms_ = step.now_ms - since_ms_;
      event = GlideEvent::kEnded;
    }
    cells_ = 0.0F;
    since_ms_ = step.now_ms;
  }
  last_ = step;
  return event;
}

std::vector<WalkFact> WalkFactsOf(const ro::sim::Actor& actor, const ro::sim::ServerClock& clock,
                                  std::uint32_t now_ms) {
  std::vector<WalkFact> facts;
  const auto add = [&facts](std::string_view key, std::string value) {
    facts.push_back(WalkFact{key, std::move(value)});
  };
  add("now_ms", std::format("{}", now_ms));
  add("posture", std::format("{}", static_cast<int>(actor.posture)));
  add("speed", std::format("{}", actor.speed_ms_per_cell));
  add("glide", std::format("{}", actor.glide_ms_per_cell));
  if (actor.movement.has_value()) {
    add("path", std::format("{},{}>{},{}", actor.movement->from.x, actor.movement->from.y,
                            actor.movement->to.x, actor.movement->to.y));
    add("walk_started_ms", std::format("{}", actor.movement_started_ms));
    add("walk_age_ms",
        std::format("{}", static_cast<std::int32_t>(now_ms - actor.movement_started_ms)));
  } else {
    add("path", "-");
  }
  add("clock_offset_ms", std::format("{}", clock.OffsetMs()));
  add("round_trip_ms", std::format("{}", clock.round_trip_ms()));
  add("travelled_before", Cells(actor.travelled_before_cells));
  add("travelled_correction", Cells(actor.travelled_correction_cells));
  add("absorb",
      std::format("{},{}", Cells(actor.walk_absorb_dx_cells), Cells(actor.walk_absorb_dy_cells)));
  add("body_state", std::format("{}", actor.body_state));
  add("body_state_since_ms", std::format("{}", actor.body_state_since_ms));
  add("health_state", std::format("{}", actor.health_state));
  add("option", std::format("{}", actor.option));
  add("blade_stop_since_ms", OptionalOf(actor.blade_stop_since_ms));
  add("blade_stop_caster", actor.blade_stop_caster ? "1" : "0");
  add("cast_skill", actor.cast.has_value() ? std::format("{}", actor.cast->skill_id) : "-");
  add("cast_started_ms", actor.cast.has_value() ? std::format("{}", actor.cast->started_ms) : "-");
  add("skill_use",
      actor.skill_use.has_value() ? std::format("{}", actor.skill_use->skill_id) : "-");
  add("skill_use_started_ms",
      actor.skill_use.has_value() ? std::format("{}", actor.skill_use->started_ms) : "-");
  add("ground_skill",
      actor.ground_skill.has_value() ? std::format("{}", actor.ground_skill->skill_id) : "-");
  add("attack_skill", actor.attack.has_value() ? std::format("{}", actor.attack->skill_id) : "-");
  add("attack_started_ms",
      actor.attack.has_value() ? std::format("{}", actor.attack->started_ms) : "-");
  add("arrival_pose_since_ms", OptionalOf(actor.arrival_pose_since_ms));
  add("stopped_ms", OptionalOf(actor.stopped_ms));
  add("states", ListOf(actor.status_effects, [](auto state) { return state; }));
  add("effects", ListOf(actor.effects, [](const auto& effect) { return effect.id; }));
  return facts;
}

std::string DescribeStep(const ShownStep& step) {
  return std::format("motion={} rang={} image={} marche={} case={:.2f},{:.2f} distance={:.2f}",
                     step.motion, step.animation, OptionalOf(step.held_frame), step.walking ? 1 : 0,
                     step.cell_x, step.cell_y, step.travelled_cells);
}

std::string JoinFacts(const std::vector<WalkFact>& facts) {
  std::string joined;
  for (const WalkFact& fact : facts) {
    joined += joined.empty() ? "" : " ";
    joined += std::format("{}={}", fact.key, fact.value);
  }
  return joined;
}

void NoteShownStep(GlideWatch& watch, const ro::sim::World& world,
                   const std::optional<ShownStep>& step) {
  const ro::sim::Actor* player = world.player();
  if (!step.has_value() || player == nullptr) {
    return;
  }
  const GlideEvent event = watch.Observe(*step);
  if (watch.episodes() > GlideWatch::kMostLoggedEpisodes) {
    return;
  }
  if (event == GlideEvent::kBegan) {
    ro::core::LogWarning("glissade {} : {:.1f} cases sous la même image ; {} ; {}",
                         watch.episodes(), watch.cells_on_same_image(), DescribeStep(*step),
                         JoinFacts(WalkFactsOf(*player, world.server_clock(), step->now_ms)));
  } else if (event == GlideEvent::kEnded) {
    ro::core::LogInfo("glissade {} finie : {:.1f} cases en {} ms ; {}", watch.episodes(),
                      watch.ended_cells(), watch.ended_ms(), DescribeStep(*step));
  }
}

}  // namespace moonclient
