#include "ro/sim/meter_tally.h"

#include <algorithm>
#include <string>

namespace ro::sim {

void MeterTally::Add(const MeterBlow& blow) {
  amount += blow.amount;
  ++events;
  critical += blow.critical ? 1U : 0U;
  biggest = std::max(biggest, blow.amount);
}

void MeterSegment::Add(const MeterBlow& blow, std::string_view who_name) {
  MeterActorTally& actor = modes[Rank(blow.mode)][blow.who];
  // ⚠ LE NOM SE REPREND À CHAQUE FAIT : un membre renommé, ou nommé plus tard
  // par le serveur, garde ainsi le dernier connu.
  if (!who_name.empty()) {
    actor.name.assign(who_name);
  }
  actor.total.Add(blow);
  actor.skills[blow.skill_id].Add(blow);
}

bool MeterSegment::empty() const noexcept {
  return std::all_of(modes.begin(), modes.end(), [](const auto& mode) { return mode.empty(); });
}

}  // namespace ro::sim
