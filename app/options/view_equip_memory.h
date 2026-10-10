#pragma once

#include "game/settings_file.h"

namespace moonclient {

// LES DEUX CASES DE « VOIR L'ÉQUIPEMENT », retenues d'une séance à l'autre.
//
// ⚠ AUCUNE LIGNE DE PANNEAU : elles se cochent au pied de la fenêtre, là où
// l'on voit ce qu'elles changent.
struct ViewEquipMemory {
  // Montrer les emplacements vides. Allumé : savoir que la cible n'a ni cape ni
  // accessoire fait partie de ce qu'on vient regarder.
  bool show_empty = true;
  // La colonne de MES pièces. Éteinte : elle élargit la fenêtre, et l'on ne
  // compare pas chaque fois qu'on regarde quelqu'un.
  bool compare = false;

  // ⚠ AVANT LA LECTURE DU FICHIER : la valeur présente ici est le défaut. Les
  // clés sont GRAVÉES : les changer rend à tous les joueurs les réglages
  // d'origine, sans que rien ne le dise.
  void Declare(SettingsFile& into) {
    into.Declare("equipment_seen_slots_empty", &show_empty);
    into.Declare("equipment_seen_compare", &compare);
  }
};

}  // namespace moonclient
