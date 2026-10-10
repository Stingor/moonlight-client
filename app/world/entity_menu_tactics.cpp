// LES VERBES DES SOUS-MENUS « TACTIQUE » ET « CONDUITE » D'UN COMPAGNON, et
// leurs libellés : voir `world/entity_menu.h`.

#include <optional>
#include <string>

#include "ro/core/i18n.h"

#include "world/entity_menu.h"

namespace moonclient {
namespace {

using ro::core::Tr;

// Les deux compagnons qui ont une IA, et donc un bloc de verbes chacun.
constexpr int kAiCompanionCount = kTacticVerbCompanions;

}  // namespace

int TacticVerb(ro::sim::AiCompanion companion, int choice) noexcept {
  const int companion_rank = companion == ro::sim::AiCompanion::kHomunculus ? 0 : 1;
  return kTacticVerbBase + (companion_rank * kTacticVerbsPerCompanion) + choice;
}

std::optional<TacticVerbParts> TacticVerbOf(int verb) noexcept {
  const int offset = verb - kTacticVerbBase;
  if (offset < 0 || offset >= kAiCompanionCount * kTacticVerbsPerCompanion) {
    return std::nullopt;
  }
  return TacticVerbParts{.companion = offset < kTacticVerbsPerCompanion
                                          ? ro::sim::AiCompanion::kHomunculus
                                          : ro::sim::AiCompanion::kMercenary,
                         .choice = offset % kTacticVerbsPerCompanion};
}

int ConductVerb(ro::sim::AiCompanion companion, int choice) noexcept {
  return TacticVerb(companion, choice) - kTacticVerbBase + kConductVerbBase;
}

std::optional<TacticVerbParts> ConductVerbOf(int verb) noexcept {
  if (verb < kConductVerbBase) {
    return std::nullopt;
  }
  return TacticVerbOf(verb - kConductVerbBase + kTacticVerbBase);
}

void RefreshConductChecks(ro::ui::EntityMenuView& view, ro::sim::AiCompanion companion,
                          const std::optional<ro::sim::CompanionPlayerConduct>& line) {
  for (ro::ui::EntityMenuItem& item : view.items) {
    const std::optional<TacticVerbParts> parts = ConductVerbOf(item.verb);
    if (!parts.has_value() || parts->companion != companion) {
      continue;
    }
    const int rank = parts->choice - static_cast<int>(TacticMenuChoice::kRankFirst);
    const int trigger = parts->choice - static_cast<int>(TacticMenuChoice::kTriggerFirst);
    if (rank >= 0 && rank < static_cast<int>(ro::sim::kTacticRankCount)) {
      item.checked = line.has_value() && static_cast<int>(line->rank) == rank;
    } else if (trigger >= 0 && trigger < static_cast<int>(ro::sim::kTacticTriggerCount)) {
      item.checked = line.has_value() && static_cast<int>(line->trigger) == trigger;
    } else if (parts->choice == static_cast<int>(TacticMenuChoice::kRemove)) {
      item.disabled = !line.has_value();
      item.tip = line.has_value() ? std::string()
                                  : std::string(Tr("Aucune conduite n'est posée pour ce joueur."));
    }
  }
}

const char* TacticRankLabel(ro::sim::TacticRank rank) {
  switch (rank) {
    case ro::sim::TacticRank::kAbsolute:
      return Tr("Priorité absolue");
    case ro::sim::TacticRank::kHigh:
      return Tr("Haut");
    case ro::sim::TacticRank::kMedium:
      return Tr("Moyen");
    case ro::sim::TacticRank::kLow:
      return Tr("Bas");
    case ro::sim::TacticRank::kLast:
      return Tr("En dernier");
    case ro::sim::TacticRank::kHold:
      return Tr("Tenir");
    case ro::sim::TacticRank::kIgnore:
      return Tr("Ignorer");
  }
  return Tr("Moyen");
}

const char* TacticTriggerLabel(ro::sim::TacticTrigger trigger) {
  switch (trigger) {
    case ro::sim::TacticTrigger::kAlways:
      return Tr("Toujours");
    case ro::sim::TacticTrigger::kIfAttacks:
      return Tr("S'il attaque");
    case ro::sim::TacticTrigger::kIfAttacksCompanion:
      return Tr("S'il attaque le compagnon");
  }
  return Tr("Toujours");
}

}  // namespace moonclient
