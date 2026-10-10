#pragma once

#include <cstdint>

#include "ro/render/skill_suite_table.h"

namespace moonclient {

class EffectsInPlay;

// VRAI SI L'ÉTAPE JOUE au regard de sa condition sur un effet déjà porté — voir
// `ro::render::SkillSuiteStep::condition_effect_id`. Sans effets en jeu, aucun
// acteur ne porte rien.
//
// ⚠ À DEMANDER AVANT DE JOUER LA SUITE : le client pose ses questions d'abord,
// et une étape qui vient de semer l'effet ne doit pas changer la réponse des
// suivantes.
[[nodiscard]] bool SkillSuiteConditionHolds(const EffectsInPlay* effects,
                                            const ro::render::SkillSuiteStep& step,
                                            std::uint32_t caster_id, std::uint32_t target_id);

}  // namespace moonclient
