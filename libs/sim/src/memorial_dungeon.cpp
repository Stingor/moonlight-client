// L'instance du joueur. Voir l'en-tête.

#include "ro/sim/memorial_dungeon.h"

#include <algorithm>
#include <utility>

namespace ro::sim {

void MemorialDungeon::Arrive(std::string name, MemorialPhase phase) {
  if (phase != phase_ || name != name_) {
    ++arrival_serial_;
  }
  // UNE AUTRE INSTANCE : le détail de la précédente ne la décrit pas.
  if (name != name_) {
    details_.reset();
  }
  phase_ = phase;
  name_ = std::move(name);
}

void MemorialDungeon::TellCreated(std::string name, int wait_position) {
  Arrive(std::move(name), MemorialPhase::kWaiting);
  wait_position_ = wait_position;
  keep_limit_s_ = 0;
  idle_limit_s_ = 0;
}

void MemorialDungeon::TellWaitPosition(int wait_position) {
  if (phase_ == MemorialPhase::kNone) {
    return;
  }
  wait_position_ = wait_position;
}

void MemorialDungeon::TellReservationCancelled() {
  if (phase_ == MemorialPhase::kNone) {
    return;
  }
  Reset();
  endings_.push_back(MemorialEnding::kReservationCancelled);
}

void MemorialDungeon::TellOpen(std::string name, std::uint32_t keep_limit_s,
                               std::uint32_t idle_limit_s) {
  Arrive(std::move(name), MemorialPhase::kOpen);
  wait_position_ = 0;
  keep_limit_s_ = keep_limit_s;
  idle_limit_s_ = idle_limit_s;
}

void MemorialDungeon::TellIdleLimit(std::uint32_t idle_limit_s) {
  if (phase_ == MemorialPhase::kNone) {
    return;
  }
  idle_limit_s_ = idle_limit_s;
}

void MemorialDungeon::TellEnded(MemorialEnding ending) {
  Reset();
  endings_.push_back(ending);
}

void MemorialDungeon::TellDetails(MemorialDetails details, std::uint32_t server_now_s,
                                  std::int64_t now_unix_s) {
  details.server_ahead_s = static_cast<std::int64_t>(server_now_s) - now_unix_s;
  details_ = std::move(details);
}

void MemorialDungeon::Reset() {
  phase_ = MemorialPhase::kNone;
  name_.clear();
  wait_position_ = 0;
  keep_limit_s_ = 0;
  idle_limit_s_ = 0;
  details_.reset();
  destroy_asked_at_s_.reset();
}

std::optional<std::int64_t> MemorialDungeon::Left(std::uint32_t limit_s,
                                                  std::int64_t now_unix_s) const noexcept {
  if (phase_ != MemorialPhase::kOpen || limit_s == 0) {
    return std::nullopt;
  }
  const std::int64_t server_now_s =
      now_unix_s + (details_.has_value() ? details_->server_ahead_s : 0);
  return std::max<std::int64_t>(0, static_cast<std::int64_t>(limit_s) - server_now_s);
}

std::optional<std::int64_t> MemorialDungeon::SecondsLeft(std::int64_t now_unix_s) const noexcept {
  return Left(keep_limit_s_, now_unix_s);
}

std::optional<std::int64_t> MemorialDungeon::IdleSecondsLeft(
    std::int64_t now_unix_s) const noexcept {
  return Left(idle_limit_s_, now_unix_s);
}

std::optional<std::int64_t> MemorialDungeon::LocalEnd() const noexcept {
  if (phase_ != MemorialPhase::kOpen || keep_limit_s_ == 0) {
    return std::nullopt;
  }
  return static_cast<std::int64_t>(keep_limit_s_) -
         (details_.has_value() ? details_->server_ahead_s : 0);
}

bool MemorialDungeon::DestroyPending(std::int64_t now_unix_s) const noexcept {
  return destroy_asked_at_s_.has_value() &&
         now_unix_s - *destroy_asked_at_s_ < kMemorialDestroyAnswerS;
}

std::vector<MemorialEnding> MemorialDungeon::TakeEndings() { return std::exchange(endings_, {}); }

}  // namespace ro::sim
