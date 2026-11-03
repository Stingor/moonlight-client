#include "game/survivors_presence_labels.h"

#include <algorithm>
#include <array>
#include <cstdio>

#include "ro/core/i18n.h"

namespace moonclient {
namespace {

constexpr std::uint32_t kSecondsPerMinute = 60;
constexpr std::int64_t kMsPerSecond = 1000;
// « 999:59 » et son zéro final suffisent : une partie dure une demi-heure.
constexpr std::size_t kClockChars = 16;
constexpr std::size_t kLabelChars = 160;

}  // namespace

std::uint32_t PresenceElapsedS(const SurvivorsPresenceSeen& seen, std::int64_t now_ms) noexcept {
  if (now_ms <= seen.entered_at_ms) return 0;
  return static_cast<std::uint32_t>((now_ms - seen.entered_at_ms) / kMsPerSecond);
}

std::string PresenceLabel(const SurvivorsPresenceSeen& seen, std::string_view arena_name,
                          std::int64_t now_ms) {
  const std::uint32_t elapsed_s = PresenceElapsedS(seen, now_ms);
  std::array<char, kClockChars> clock{};
  std::snprintf(clock.data(), clock.size(), "%02u:%02u", elapsed_s / kSecondsPerMinute,
                elapsed_s % kSecondsPerMinute);
  const std::string name =
      arena_name.empty() ? std::string(ro::core::Tr("Arène inconnue")) : std::string(arena_name);
  std::array<char, kLabelChars> line{};
  if (seen.state == ro::net::SurvivorsPresenceState::kPaused) {
    std::snprintf(line.data(), line.size(), ro::core::Tr("En pause dans l'arène : %s, %s"),
                  name.c_str(), clock.data());
  } else {
    std::snprintf(line.data(), line.size(), ro::core::Tr("En arène : %s, %s"), name.c_str(),
                  clock.data());
  }
  return std::string(line.data());
}

void SurvivorsPresenceLabels::Compose(std::span<const SurvivorsPresenceSeen> seen,
                                      const PresenceArenaNamer& arena_name, std::int64_t now_ms) {
  lines_.clear();
  lines_.reserve(seen.size());
  for (const SurvivorsPresenceSeen& one : seen) {
    const std::string name = arena_name ? arena_name(one.arena_id) : std::string();
    lines_.emplace_back(one.account_id, PresenceLabel(one, name, now_ms));
  }
}

std::string_view SurvivorsPresenceLabels::Of(std::uint32_t account_id) const noexcept {
  const auto at = std::ranges::lower_bound(lines_, account_id, {},
                                           &std::pair<std::uint32_t, std::string>::first);
  return at != lines_.end() && at->first == account_id ? std::string_view(at->second)
                                                       : std::string_view();
}

}  // namespace moonclient
