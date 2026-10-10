// Les réglages de l'IA sous les règles du serveur. Voir l'en-tête.

#include "ro/sim/companion_server_rules.h"

#include <algorithm>
#include <array>

namespace ro::sim {
namespace {

// Les distances que le rayon de vue borne.
constexpr std::array<int CompanionAiSettings::*, 5> kViewBoundCells = {
    &CompanionAiSettings::follow_distance_cells, &CompanionAiSettings::leash_still_cells,
    &CompanionAiSettings::leash_walking_cells, &CompanionAiSettings::engage_still_cells,
    &CompanionAiSettings::engage_walking_cells};

}  // namespace

std::optional<CompanionAiSettings> SettingsUnderServerRules(const CompanionAiSettings& settings,
                                                            const SessionSettings& session) {
  const int view_cells = session.view_cells;
  const bool beyond_view =
      view_cells > 0 && std::ranges::any_of(kViewBoundCells, [&](int CompanionAiSettings::* field) {
        return settings.*field > view_cells;
      });
  const bool factory_hold = settings.arrival_attack_hold_ms == kServerCompanionAttackHoldMs &&
                            session.homunculus_teleport_hold_ms != kServerCompanionAttackHoldMs;
  if (!beyond_view && !factory_hold) {
    return std::nullopt;
  }
  CompanionAiSettings applied = settings;
  if (beyond_view) {
    for (int CompanionAiSettings::* field : kViewBoundCells) {
      applied.*field = std::min(applied.*field, view_cells);
    }
  }
  if (factory_hold) {
    applied.arrival_attack_hold_ms = session.homunculus_teleport_hold_ms;
  }
  return applied;
}

}  // namespace ro::sim
