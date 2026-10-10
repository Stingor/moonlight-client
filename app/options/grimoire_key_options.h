#pragma once

#include "game/settings_file.h"

namespace moonclient {

// LE MODE CLAVIER DU GRIMOIRE, sur le disque. Voir `character/grimoire_keys.h`.
//
// ⚠ SANS RANGÉE DANS LE PANNEAU, comme « Lisser les icônes » : la case vit dans
// l'en-tête du grimoire, là où l'on s'en sert. Même aller-retour : la valeur
// descend dans l'état de la fenêtre avant le dessin, et remonte après.
class GrimoireKeyOptions {
 public:
  [[nodiscard]] bool keyboard_layout() const noexcept { return keyboard_layout_; }

  // Rend vrai quand la case a changé : le fichier est alors à réécrire.
  [[nodiscard]] bool NoteKeyboardLayout(bool seen) noexcept {
    if (keyboard_layout_ == seen) {
      return false;
    }
    keyboard_layout_ = seen;
    return true;
  }

  // ⚠ AVANT LA LECTURE DU FICHIER : la valeur présente ici est le défaut.
  void Declare(SettingsFile& into) { into.Declare("grimoire_mode_keyboard", &keyboard_layout_); }

 private:
  // ⚠ ÉTEINT PAR DÉFAUT : allumé, une touche tenue sur le grimoire ne lance
  // plus le sort de sa case.
  bool keyboard_layout_ = false;
};

}  // namespace moonclient
