#pragma once

#include <string>
#include <string_view>

#include "survivors/survivors_sightings.h"

namespace moonclient {

// ── LA SECTION `codex` DU CARNET, en texte YAML ─────────────────────────────
//
//   codex:
//     skills: [fire_bolt, cold_bolt]
//     foes: ["1063", "1002"]
//     events: [swarm_1011, night]
//
// Une liste de chaînes par catégorie, dans l'ordre de la première rencontre ;
// toutes facultatives, et une section absente est un Codex vierge. yaml-cpp
// reste privé au carnet : la section passe en texte.

// La valeur de la section (une table), lue. Un doublon du fichier se lit une
// fois ; une catégorie inconnue, une catégorie connue d'une autre forme et une
// entrée qui n'est pas une chaîne se gardent (`SurvivorsCodexSeen`). Un texte
// qui ne se lit pas, ou qui n'est pas une table, donne un Codex vierge.
[[nodiscard]] SurvivorsCodexSeen ReadSurvivorsCodex(std::string_view yaml);

// La valeur de la section, à écrire sous `codex` : les catégories de ce
// client dans leur ordre, puis les étrangères. Une catégorie vide ne s'écrit
// pas, sauf la forme étrangère qu'elle garde.
[[nodiscard]] std::string WriteSurvivorsCodex(const SurvivorsCodexSeen& seen);

}  // namespace moonclient
