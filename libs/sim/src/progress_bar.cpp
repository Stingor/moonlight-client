#include "ro/sim/progress_bar.h"

#include <optional>

#include "ro/sim/world.h"

namespace ro::sim {
namespace {

SkillGauge GaugeOf(std::uint32_t seconds, std::uint32_t now_ms) {
  // ⚠ LE PRODUIT SE FAIT SUR TRENTE-DEUX BITS, débordement compris, comme
  // chez le client : une durée absurde ne vaut pas mieux qu'une autre.
  return SkillGauge{.started_ms = now_ms, .duration_ms = seconds * kMsPerProgressSecond};
}

}  // namespace

void ProgressReport::GaugeEnded() noexcept {
  if (owed_) {
    owed_ = false;
    due_ = true;
  }
}

bool ProgressReport::TakeDue() noexcept {
  const bool due = due_;
  due_ = false;
  return due;
}

bool StartOwnProgress(World& world, ProgressReport& report, std::uint32_t seconds,
                      std::uint32_t now_ms) {
  if (!world.SetGauge(world.player_id(), GaugeOf(seconds, now_ms))) {
    return false;
  }
  report.Owe();
  return true;
}

bool StartActorProgress(World& world, std::uint32_t actor_id, std::uint32_t seconds,
                        std::uint32_t now_ms) {
  const Actor* actor = world.Find(actor_id);
  if (actor == nullptr || actor_id == world.player_id() || actor->kind == ActorKind::kItem) {
    return false;
  }
  return world.SetGauge(actor_id, GaugeOf(seconds, now_ms));
}

void CancelOwnProgress(World& world, ProgressReport& report) {
  static_cast<void>(world.SetGauge(world.player_id(), std::nullopt));
  report.GaugeEnded();
}

void NoteOwnProgressEnd(const World& world, ProgressReport& report, std::uint32_t now_ms) {
  if (!report.owed()) {
    return;
  }
  const Actor* self = world.player();
  if (self == nullptr || !self->gauge.has_value() ||
      now_ms - self->gauge->started_ms >= self->gauge->duration_ms) {
    report.GaugeEnded();
  }
}

}  // namespace ro::sim
