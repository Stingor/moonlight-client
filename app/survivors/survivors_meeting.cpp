#include "survivors/survivors_meeting.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>

#include "survivors/survivors_book.h"

namespace moonclient {
namespace {

// Assez de morceaux pour que la recherche à cheval sur leurs bords ne les
// régénère pas sans cesse.
constexpr std::int32_t kSearchCacheSideChunks = 4;

[[nodiscard]] constexpr std::int64_t Squared(std::int64_t value) noexcept { return value * value; }

}  // namespace

const SurvivorsMeetingRule* MeetingOfClass(std::string_view class_key) noexcept {
  const auto found =
      std::ranges::find(kShippedMeetings, class_key, &SurvivorsMeetingRule::class_key);
  return found != kShippedMeetings.end() ? &*found : nullptr;
}

const ro::sim::ClassRule* ClassAwaitingMeeting(const ro::sim::SurvivorsRules& rules,
                                               const ro::sim::ArenaRule& arena,
                                               const SurvivorsRunBoard& board,
                                               const SurvivorsSave& save) {
  if (board.kind != SurvivorsBoardKind::kFree || board.coop) return nullptr;
  const auto awaiting = std::ranges::find_if(rules.classes, [&](const ro::sim::ClassRule& rule) {
    const SurvivorsMeetingRule* meeting = MeetingOfClass(rule.key);
    return meeting != nullptr && std::string_view(arena.key) == meeting->arena_key &&
           ClassUnlockFeat(rule) == meeting->feat_key && !ClassUnlocked(rules, rule, save);
  });
  return awaiting != rules.classes.end() ? &*awaiting : nullptr;
}

std::optional<ro::sim::FieldPoint> MeetingPlaceOf(const ro::sim::SurvivorsGround& ground,
                                                  ro::sim::FieldPoint start,
                                                  const SurvivorsMeetingRule& meeting) {
  if (ground.rule() == nullptr) return std::nullopt;
  const ro::sim::SurvivorsGround asked = ro::sim::SurvivorsGround::Generated(
      *ground.rule(), ground.run_seed(), kSearchCacheSideChunks);
  const std::int32_t wanted_x = start.x / ro::sim::kSubunitsPerCell + meeting.east_cells;
  const std::int32_t wanted_y = start.y / ro::sim::kSubunitsPerCell + meeting.north_cells;
  std::optional<ro::sim::FieldPoint> nearest;
  std::int64_t nearest_squared = 0;
  // Par anneaux croissants : un anneau plus loin que la meilleure case trouvée
  // ne peut plus en donner de plus proche.
  for (std::int32_t ring = 0; ring <= kMeetingSearchCells; ++ring) {
    if (nearest.has_value() && Squared(ring) > nearest_squared) break;
    for (std::int32_t east = -ring; east <= ring; ++east) {
      for (std::int32_t north = -ring; north <= ring; ++north) {
        if (std::max(std::abs(east), std::abs(north)) != ring) continue;
        if (!asked.Passable(wanted_x + east, wanted_y + north)) continue;
        const std::int64_t squared = Squared(east) + Squared(north);
        if (nearest.has_value() && squared >= nearest_squared) continue;
        nearest = ro::sim::CellCenter(wanted_x + east, wanted_y + north);
        nearest_squared = squared;
      }
    }
  }
  return nearest;
}

void SurvivorsMeeting::Place(ro::sim::FieldPoint at, std::uint64_t leaves_at_run_ms,
                             std::uint32_t zeny_wanted) noexcept {
  at_ = at;
  met_ms_.reset();
  left_ms_.reset();
  leaves_at_run_ms_ = leaves_at_run_ms;
  zeny_wanted_ = zeny_wanted;
}

bool SurvivorsMeeting::Follow(ro::sim::FieldPoint hero, std::uint64_t run_ms, std::uint64_t now_ms,
                              std::uint32_t hero_zeny) noexcept {
  if (!at_.has_value() || met_ms_.has_value() || left_ms_.has_value()) return false;
  if (leaves_at_run_ms_ != 0 && run_ms >= leaves_at_run_ms_) {
    left_ms_ = now_ms;
    return false;
  }
  if (ro::sim::DistanceSquared(hero, *at_) > Squared(ro::sim::Cells(kMeetingReachCells))) {
    return false;
  }
  // Sans la somme, il attend : le héros peut revenir avec.
  if (hero_zeny < zeny_wanted_) return false;
  met_ms_ = now_ms;
  return true;
}

std::optional<ro::sim::FieldPoint> SurvivorsMeeting::waiting_at() const noexcept {
  return met_ms_.has_value() || left_ms_.has_value() ? std::nullopt : at_;
}

std::optional<SurvivorsMeetingBody> SurvivorsMeeting::BodyAt(std::uint64_t now_ms) const noexcept {
  if (!at_.has_value()) return std::nullopt;
  const std::optional<std::uint64_t> gone_ms = met_ms_.has_value() ? met_ms_ : left_ms_;
  if (!gone_ms.has_value()) return SurvivorsMeetingBody{.at = *at_};
  const std::uint64_t elapsed_ms = now_ms > *gone_ms ? now_ms - *gone_ms : 0;
  if (elapsed_ms >= kMeetingFarewellMs) return std::nullopt;
  return SurvivorsMeetingBody{
      .at = *at_,
      .opacity = 1.0F - static_cast<float>(elapsed_ms) / static_cast<float>(kMeetingFarewellMs)};
}

bool SurvivorsMeeting::TellingAt(std::uint64_t now_ms) const noexcept {
  return met_ms_.has_value() && now_ms < *met_ms_ + kMeetingNoticeMs;
}

}  // namespace moonclient
