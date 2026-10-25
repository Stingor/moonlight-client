#pragma once

// LES PIÈCES COMMUNES DU CONTRÔLE DES RÈGLES SURVIVORS : le refus, les bornes
// des mesures, et les contrôles du contenu d'une arène complète que
// CheckSurvivorsRules enchaîne après ceux du noyau.
//
// En-tête PRIVÉ de `libs/sim/src`.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>

#include "ro/core/error.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_rules.h"

namespace ro::sim::survivors {

// LES BORNES DES MESURES. Une mesure en sous-unités (portée, rayon, aimant,
// poussée, cadre) ne dépasse pas le côté de la plus grande arène : la somme de
// trois d'entre elles tient en 32 bits, et son carré en 64. Une vitesse ne fait
// pas traverser plus que cette arène en un pas.
inline constexpr std::int32_t kMostMeasureUnits = Cells(kMostArenaSideCells);
inline constexpr std::int64_t kMostSpeedUnitsPerSecond =
    std::int64_t{kMostMeasureUnits} * kStepsPerSecond;

[[nodiscard]] inline bool MeasureOutOfBounds(std::int64_t units) noexcept {
  return units < 0 || units > kMostMeasureUnits;
}

[[nodiscard]] inline bool CellsOutOfBounds(std::int64_t cells) noexcept {
  return cells < 0 || cells > kMostArenaSideCells;
}

// Une majoration en pour cent du catalogue : dans [0, kMostStatBonus].
[[nodiscard]] inline bool PercentOutOfBounds(std::int64_t pct) noexcept {
  return pct < 0 || pct > kMostStatBonus;
}

[[nodiscard]] inline std::unexpected<core::Error> Refuse(std::string detail) {
  return core::Fail(core::ErrorCode::kInvalidArgument, "règles survivors : " + std::move(detail));
}

[[nodiscard]] inline bool Contains(std::span<const std::uint16_t> ids, std::uint16_t id) {
  return std::find(ids.begin(), ids.end(), id) != ids.end();
}

// Le premier Id porté deux fois par `table`, ou rien.
template <typename Rule, typename IdOf>
[[nodiscard]] std::optional<std::uint16_t> RepeatedId(std::span<const Rule> table, IdOf id_of) {
  for (std::size_t later = 1; later < table.size(); ++later) {
    for (std::size_t earlier = 0; earlier < later; ++earlier) {
      if (id_of(table[earlier]) == id_of(table[later])) return id_of(table[later]);
    }
  }
  return std::nullopt;
}

// Un archétype qui vise un monstre à portée du héros : sans portée, il ne
// trouve jamais personne.
[[nodiscard]] constexpr bool ArchetypeAims(SkillArchetype archetype) noexcept {
  return archetype == SkillArchetype::kTargetStrike || archetype == SkillArchetype::kHoming ||
         archetype == SkillArchetype::kClusterBlast || archetype == SkillArchetype::kLinear;
}

// survivors_rules_check_content.cpp : les contrôles du contenu d'une arène
// complète, chacun supposant ceux du noyau passés.
[[nodiscard]] core::Result<void> CheckSkillContent(const SurvivorsRules& rules);
[[nodiscard]] core::Result<void> CheckFoeBehaviours(const SurvivorsRules& rules);
[[nodiscard]] core::Result<void> CheckBossScripts(const SurvivorsRules& rules);
[[nodiscard]] core::Result<void> CheckEventsAndElites(const SurvivorsRules& rules);
[[nodiscard]] core::Result<void> CheckEvolutions(const SurvivorsRules& rules);
[[nodiscard]] core::Result<void> CheckLoot(const SurvivorsRules& rules);
[[nodiscard]] core::Result<void> CheckContentTuning(const SurvivorsRules& rules);
// Ce que la partie interprète de la méta : cartes des monstres, cote de
// chute, table de la Cursed Water, relève.
[[nodiscard]] core::Result<void> CheckMetaRules(const SurvivorsRules& rules);

}  // namespace ro::sim::survivors
