#include "ro/sim/stat_raise_run.h"

namespace ro::sim {

bool StatRaiseRun::Start(const StatPlan& plan, const StatRaiseSight& seen) {
  if (running_) {
    return false;
  }
  queue_.clear();
  for (std::size_t rank = 0; rank < kBaseStatCount; ++rank) {
    const StatPlanRow& row = plan.rows[rank];
    for (std::int64_t step = row.current; step < row.after; ++step) {
      queue_.push_back(static_cast<std::uint8_t>(rank));
    }
  }
  if (queue_.empty()) {
    return false;
  }
  done_ = 0;
  running_ = true;
  stop_ = StatRunStop::kNone;
  expected_base_ = seen.base;
  expected_points_ = seen.status_point;
  asked_ = false;
  waiting_ = false;
  return true;
}

std::optional<std::uint8_t> StatRaiseRun::Tick(const StatRaiseSight& seen) {
  if (!running_) {
    return std::nullopt;
  }
  // Une montée demandée dont l'appelant n'a jamais dit le sort : on ne sait
  // pas si elle est partie, et on ne dépense pas davantage sur un doute.
  if (asked_) {
    Halt(StatRunStop::kNotSent);
    return std::nullopt;
  }
  if (waiting_) {
    if (seen.refusals != refusals_at_send_) {
      Halt(StatRunStop::kRefused);
      return std::nullopt;
    }
    // ⚠ LA RÉPONSE PEUT ARRIVER EN DEUX TRAMES : au-delà de 255, l'accusé porte
    // une valeur bornée que la trame suivante corrige. On attend donc que le
    // monde montre EXACTEMENT ce que la montée devait produire.
    const bool settled = seen.base == base_after_ && seen.status_point == points_after_;
    if (seen.awaited || !settled) {
      if (seen.now_ms - sent_ms_ >= kStatRaiseReplyTimeoutMs) {
        if (seen.awaited) {
          Halt(StatRunStop::kNoReply);
        } else {
          Halt(seen.status_point != points_after_ ? StatRunStop::kPointsChanged
                                                  : StatRunStop::kStatsChanged);
        }
      }
      return std::nullopt;
    }
    waiting_ = false;
    ++done_;
    expected_base_ = base_after_;
    expected_points_ = points_after_;
  }
  if (seen.base != expected_base_) {
    Halt(StatRunStop::kStatsChanged);
    return std::nullopt;
  }
  if (seen.status_point != expected_points_) {
    Halt(StatRunStop::kPointsChanged);
    return std::nullopt;
  }
  if (done_ >= queue_.size()) {
    Halt(StatRunStop::kFinished);
    return std::nullopt;
  }
  // Une montée du joueur est en vol : elle changera les valeurs, et c'est
  // l'image suivante qui arrêtera la file.
  if (seen.awaited) {
    return std::nullopt;
  }
  const std::uint8_t rank = queue_[done_];
  const std::int64_t cost = seen.next_cost[rank];
  if (cost <= 0 || cost > seen.status_point) {
    Halt(StatRunStop::kShort);
    return std::nullopt;
  }
  base_after_ = seen.base;
  ++base_after_[rank];
  points_after_ = seen.status_point - cost;
  refusals_at_send_ = seen.refusals;
  asked_ = true;
  return rank;
}

void StatRaiseRun::Sent(bool emitted, std::uint32_t now_ms) {
  if (!running_ || !asked_) {
    return;
  }
  asked_ = false;
  if (!emitted) {
    Halt(StatRunStop::kNotSent);
    return;
  }
  waiting_ = true;
  sent_ms_ = now_ms;
}

void StatRaiseRun::Cancel() {
  if (running_) {
    Halt(StatRunStop::kCancelled);
  }
}

void StatRaiseRun::Halt(StatRunStop why) {
  running_ = false;
  asked_ = false;
  waiting_ = false;
  stop_ = why;
}

}  // namespace ro::sim
