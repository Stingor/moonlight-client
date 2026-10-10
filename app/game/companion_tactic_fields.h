#pragma once

// LES CHAMPS AVANCÉS D'UNE TACTIQUE, entre la ligne de l'IA et la fenêtre
// « Avancé… ». Les listes de la fenêtre suivent l'ordre des énumérations de
// l'IA, et ses bornes sont celles de l'IA : le fichier source le vérifie à la
// compilation.

#include "ro/sim/companion_players.h"
#include "ro/sim/companion_tactics.h"
#include "ro/ui/companion_tactic_window.h"

namespace moonclient {

[[nodiscard]] ro::ui::TacticAdvancedFields FieldsOf(const ro::sim::CompanionTactic& tactic);
// La ligne avec ces champs ; sa clé, son rang et son déclencheur restent.
[[nodiscard]] ro::sim::CompanionTactic WithFields(ro::sim::CompanionTactic tactic,
                                                  const ro::ui::TacticAdvancedFields& fields);

// LES CHAMPS D'UNE CONDUITE ENVERS UN JOUEUR : ceux qui valent pour lui ; les
// autres gardent leur valeur neutre et ne reviennent pas.
[[nodiscard]] ro::ui::TacticAdvancedFields ConductFieldsOf(
    const ro::sim::CompanionPlayerConduct& conduct);
// La conduite avec ces champs ; sa clé, son rang et son déclencheur restent.
[[nodiscard]] ro::sim::CompanionPlayerConduct WithConductFields(
    ro::sim::CompanionPlayerConduct conduct, const ro::ui::TacticAdvancedFields& fields);

}  // namespace moonclient
