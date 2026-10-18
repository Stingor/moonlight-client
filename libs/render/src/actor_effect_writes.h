#pragma once

#include <cstdint>

#include "ro/render/actor_effect_table.h"

// LA RECHERCHE D'UNE ÉCRITURE dans ce qu'un effet change sur l'acteur.
//
// En-tête PRIVÉ : il vit à côté des sources qui l'emploient et ne sort pas de
// `libs/render/src`. Rien de public n'en dépend.
//
// 🔴 AUCUNE COPIE LOCALE SOUS CE NOM. Une copie homonyme dans un espace anonyme
// masquerait celle-ci SANS ERREUR. Une recherche qui ne fait PAS la même chose
// — qui garde la DERNIÈRE écriture du champ, ou en filtre d'autres — porte un
// autre nom.

namespace ro::render {

// La PREMIÈRE écriture que cet effet pousse sous ce champ, ou nullptr.
[[nodiscard]] inline const ActorFieldWrite* WriteOn(const ActorEffectChange& change,
                                                    std::uint16_t field) {
  for (const ActorFieldWrite& write : change.writes) {
    if (write.field == field) {
      return &write;
    }
  }
  return nullptr;
}

}  // namespace ro::render
