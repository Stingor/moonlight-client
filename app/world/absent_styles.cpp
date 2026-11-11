#include "world/absent_styles.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "ro/net/absent_styles.h"

namespace moonclient {
namespace {

// Un membre qu'aucun panneau n'a voulu depuis tant de temps n'est plus affiché.
constexpr std::uint32_t kWantedLingerMs = 2000;
// Une réponse vaut tant de temps : au-delà, on redemande ce membre s'il est
// encore affiché.
constexpr std::uint32_t kAnswerFreshMs = 60000;
// Une demande restée sans réponse se refait après ce délai.
constexpr std::uint32_t kAnswerTimeoutMs = 5000;

[[nodiscard]] bool Before(std::uint32_t then_ms, std::uint32_t now_ms, std::uint32_t span_ms) {
  return now_ms - then_ms < span_ms;
}

}  // namespace

void AbsentStyles::Want(std::uint32_t char_id, std::uint32_t now_ms) {
  if (char_id != 0) {
    wanted_at_ms_[char_id] = now_ms;
  }
}

std::vector<std::uint32_t> AbsentStyles::Due(const ro::sim::World& world,
                                             std::uint32_t now_ms) const {
  const int told = world.session().style_absent_ids_most;
  const std::size_t most =
      told > 0 ? std::min(static_cast<std::size_t>(told), ro::net::kAbsentStyleIdsMost)
               : ro::net::kAbsentStyleIdsMost;
  std::vector<std::uint32_t> due;
  for (const auto& [char_id, wanted_ms] : wanted_at_ms_) {
    if (due.size() >= most) {
      break;
    }
    if (!Before(wanted_ms, now_ms, kWantedLingerMs)) {
      continue;
    }
    const ro::sim::AbsentStyle* known = world.styles().FindAbsent(char_id);
    if (known != nullptr && Before(known->heard_at_ms, now_ms, kAnswerFreshMs)) {
      continue;
    }
    if (const auto asked = asked_at_ms_.find(char_id);
        asked != asked_at_ms_.end() && Before(asked->second, now_ms, kAnswerTimeoutMs)) {
      continue;
    }
    due.push_back(char_id);
  }
  return due;
}

void AbsentStyles::HearRefusals(const ro::sim::World& world) {
  const ro::sim::AbsentRefusal& refusal = world.styles().absent_refusal();
  if (refusal.sequence == seen_refusal_) {
    return;
  }
  seen_refusal_ = refusal.sequence;
  holding_ = true;
  hold_until_ms_ = refusal.heard_at_ms + refusal.wait_ms;
  // Ce qui était demandé ne viendra pas : on le redemandera après l'attente.
  asked_at_ms_.clear();
}

bool AbsentStyles::MaySend(const ro::sim::World& world, std::uint32_t now_ms) const {
  const std::uint32_t cadence_ms = world.session().style_absent_cadence_ms;
  if (cadence_ms == 0) {
    return false;
  }
  if (sent_once_ && Before(last_sent_ms_, now_ms, cadence_ms)) {
    return false;
  }
  return !holding_ || static_cast<std::int32_t>(hold_until_ms_ - now_ms) <= 0;
}

void AbsentStyles::NoteSent(const std::vector<std::uint32_t>& char_ids, std::uint32_t now_ms) {
  for (const std::uint32_t char_id : char_ids) {
    asked_at_ms_[char_id] = now_ms;
  }
  sent_once_ = true;
  last_sent_ms_ = now_ms;
  holding_ = false;
}

void AbsentStyles::Pump(ro::net::Connection& link, const ro::sim::World& world,
                        std::uint32_t now_ms) {
  HearRefusals(world);
  // Ce qu'on ne veut plus ne se garde pas.
  std::erase_if(wanted_at_ms_, [now_ms](const auto& wanted) {
    return !Before(wanted.second, now_ms, kWantedLingerMs);
  });
  std::erase_if(asked_at_ms_,
                [this](const auto& asked) { return !wanted_at_ms_.contains(asked.first); });
  if (!link.is_open() || !MaySend(world, now_ms)) {
    return;
  }
  const std::vector<std::uint32_t> due = Due(world, now_ms);
  if (due.empty()) {
    return;
  }
  if (ro::net::RequestAbsentStyles(link, due)) {
    NoteSent(due, now_ms);
  }
}

}  // namespace moonclient
