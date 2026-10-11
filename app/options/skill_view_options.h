#pragma once

#include "game/settings_file.h"

namespace moonclient {

// LA GRILLE OU LA LISTE des deux arbres de compétences, sur le disque : le
// grimoire et l'onglet de la guilde.
//
// ⚠ SANS RANGÉE DANS LE PANNEAU, comme le mode clavier du grimoire : la bascule
// vit dans l'en-tête de chaque arbre. Même aller-retour : la valeur descend dans
// l'état de la fenêtre avant le dessin, et remonte après.
class SkillViewOptions {
 public:
  [[nodiscard]] bool grimoire_grid() const noexcept { return grimoire_grid_; }
  [[nodiscard]] bool guild_grid() const noexcept { return guild_grid_; }

  // Rend vrai quand la vue a changé : le fichier est alors à réécrire.
  [[nodiscard]] bool NoteGrimoireGrid(bool seen) noexcept { return Note(grimoire_grid_, seen); }
  [[nodiscard]] bool NoteGuildGrid(bool seen) noexcept { return Note(guild_grid_, seen); }

  // ⚠ AVANT LA LECTURE DU FICHIER : les valeurs présentes ici sont les défauts.
  void Declare(SettingsFile& into) {
    into.Declare("grimoire_view_grid", &grimoire_grid_);
    into.Declare("guild_skills_view_grid", &guild_grid_);
  }

 private:
  [[nodiscard]] static bool Note(bool& kept, bool seen) noexcept {
    if (kept == seen) {
      return false;
    }
    kept = seen;
    return true;
  }

  // ⚠ LA GRILLE PAR DÉFAUT, comme l'état des fenêtres : la vue du client.
  bool grimoire_grid_ = true;
  bool guild_grid_ = true;
};

}  // namespace moonclient
