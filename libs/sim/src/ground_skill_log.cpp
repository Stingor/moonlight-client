#include "ro/sim/ground_skill_log.h"

#include <algorithm>
#include <cstddef>

namespace ro::sim {
namespace {

// Ce que le journal garde au moins : bien plus qu'une image n'en apporte.
constexpr std::size_t kKeptSkills = 64;

}  // namespace

void UnseenGroundSkillLog::Record(std::uint32_t caster_id, const GroundSkillUse& use) {
  // ⚠ LA PURGE SE FAIT PAR MOITIÉ, pas entrée par entrée : effacer en tête d'un
  // vecteur à chaque ajout recopierait tout le reste à chaque fois.
  if (skills_.size() >= kKeptSkills * 2) {
    skills_.erase(skills_.begin(),
                  skills_.begin() + static_cast<std::ptrdiff_t>(skills_.size() - kKeptSkills));
  }
  skills_.push_back(UnseenGroundSkill{.serial = next_serial_, .caster_id = caster_id, .use = use});
  ++next_serial_;
}

std::span<const UnseenGroundSkill> UnseenGroundSkillLog::Since(std::uint64_t serial) const {
  const auto first = std::ranges::upper_bound(
      skills_, serial, {}, [](const UnseenGroundSkill& kept) { return kept.serial; });
  return {first, skills_.end()};
}

std::span<const UnseenGroundSkill> UnseenGroundSkillCursor::Take(const UnseenGroundSkillLog& log) {
  if (seen_serial_ > log.last_serial()) {
    seen_serial_ = 0;
  }
  const std::span<const UnseenGroundSkill> fresh = log.Since(seen_serial_);
  seen_serial_ = log.last_serial();
  return fresh;
}

}  // namespace ro::sim
