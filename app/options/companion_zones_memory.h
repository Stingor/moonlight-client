#pragma once

#include "game/settings_file.h"

namespace moonclient {

// LA CASE « MONTRER LES ZONES AU SOL » de la fenêtre « IA du compagnon »,
// retenue d'une séance à l'autre.
//
// ⚠ AUCUNE LIGNE DE PANNEAU : elle se coche dans la fenêtre, là où l'on voit
// les zones qu'elle garde.
struct CompanionZonesMemory {
  // Les zones restent au sol fenêtre fermée. Éteinte : elles ne servent qu'à
  // régler, et un cercle permanent sous les pieds se remarque.
  bool kept_shown = false;

  // ⚠ AVANT LA LECTURE DU FICHIER : la valeur présente ici est le défaut. La
  // clé est GRAVÉE : la changer rendrait à chacun le réglage d'origine.
  void Declare(SettingsFile& into) { into.Declare("companion_ai_ground_zones", &kept_shown); }

  // LA CASE CLIQUÉE DANS LA FENÊTRE la bascule. Rend vrai quand elle a changé :
  // le fichier est à écrire.
  bool Follow(bool toggled) noexcept {
    if (toggled) {
      kept_shown = !kept_shown;
    }
    return toggled;
  }
};

}  // namespace moonclient
