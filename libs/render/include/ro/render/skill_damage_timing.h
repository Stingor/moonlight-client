#pragma once

#include <array>
#include <cstdint>

namespace ro::render {

// L'INSTANT DES DÉGÂTS D'UNE COMPÉTENCE : ce que certaines ajoutent à l'instant
// ordinaire où le chiffre paraît, où la cible bronche et où le coup sonne.
//
// Le client range cet instant dans le message de dégâts qu'il se diffère à
// lui-même ; pour une poignée de compétences, il l'y repousse d'une durée
// écrite en dur. Tetra Vortex, dont chaque élément arrive une demi-seconde
// après le précédent, en est le cas le plus visible.
struct SkillTimingEntry {
  std::uint16_t skill_id = 0;
  std::uint16_t ms = 0;
};

inline constexpr std::array<SkillTimingEntry, 8> kSkillDamageDelays = {{
    {.skill_id = 77, .ms = 500},
    {.skill_id = 415, .ms = 300},
    {.skill_id = 417, .ms = 300},
    {.skill_id = 2219, .ms = 500},
    {.skill_id = 2220, .ms = 1000},
    {.skill_id = 2221, .ms = 1500},
    {.skill_id = 2592, .ms = 300},
    {.skill_id = 2593, .ms = 300},
}};

// Et le pas entre deux coups d'un coup multiple, qu'une compétence allonge.
inline constexpr std::array<SkillTimingEntry, 1> kSkillHitStepExtras = {{
    {.skill_id = 56, .ms = 80},
}};

// Ce que la compétence ajoute à l'instant de ses dégâts, en millisecondes.
[[nodiscard]] constexpr std::uint32_t SkillDamageDelayMs(std::uint16_t skill_id) {
  for (const SkillTimingEntry& entry : kSkillDamageDelays) {
    if (entry.skill_id == skill_id) {
      return entry.ms;
    }
  }
  return 0;
}

// Ce que la compétence ajoute au pas entre deux coups, en millisecondes.
[[nodiscard]] constexpr std::uint32_t SkillHitStepExtraMs(std::uint16_t skill_id) {
  for (const SkillTimingEntry& entry : kSkillHitStepExtras) {
    if (entry.skill_id == skill_id) {
      return entry.ms;
    }
  }
  return 0;
}

}  // namespace ro::render
