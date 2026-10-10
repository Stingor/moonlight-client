// Voir l'en-tête.

#include "ro/sim/companion_statuses.h"

namespace ro::sim {
namespace {

// La dernière trace d'un skill du compagnon : incantation, emploi, case visée.
[[nodiscard]] std::optional<std::uint32_t> LatestSkillTrace(const Actor& self) noexcept {
  std::optional<std::uint32_t> latest;
  const auto note = [&latest](std::uint32_t seen_ms) {
    if (!latest.has_value() ||
        static_cast<std::int64_t>(seen_ms) - static_cast<std::int64_t>(*latest) > 0) {
      latest = seen_ms;
    }
  };
  if (self.cast.has_value() && self.cast->skill_id != 0) {
    note(self.cast->started_ms);
  }
  if (self.skill_use.has_value() && self.skill_use->skill_id != 0) {
    note(self.skill_use->started_ms);
  }
  if (self.ground_skill.has_value() && self.ground_skill->skill_id != 0) {
    note(self.ground_skill->started_ms);
  }
  return latest;
}

}  // namespace

bool CompanionStatusWatch::ShouldAsk(bool wanted, const Actor* self, std::uint32_t now_ms) {
  if (!wanted || self == nullptr) {
    return false;
  }
  const std::optional<std::uint32_t> trace_ms = LatestSkillTrace(*self);
  const bool fresh_trace =
      trace_ms.has_value() && trace_ms != trace_asked_ms_ &&
      (!asked_ms_.has_value() ||
       static_cast<std::int64_t>(*trace_ms) - static_cast<std::int64_t>(*asked_ms_) >= 0);
  const bool due = !asked_ms_.has_value() || now_ms - *asked_ms_ >= kCompanionStatusAskMs;
  if (!due && !fresh_trace) {
    return false;
  }
  asked_ms_ = now_ms;
  trace_asked_ms_ = trace_ms;
  return true;
}

void CompanionStatusWatch::Receive(bool valid, std::span<const StatusEffectId> statuses,
                                   std::uint32_t now_ms) {
  if (!valid) {
    return;
  }
  statuses_.assign(statuses.begin(), statuses.end());
  received_ms_ = now_ms;
}

void CompanionStatusWatch::Forget() noexcept {
  statuses_.clear();
  received_ms_.reset();
  asked_ms_.reset();
  trace_asked_ms_.reset();
}

}  // namespace ro::sim
