#pragma once

#include <cstdint>
#include <vector>

#include "ro/ui/options_window.h"

#include "afk/afk_settings.h"
#include "game/settings_file.h"
#include "options/game_option_ids.h"
#include "options/option_targets.h"

namespace moonclient {

// LA PAGE DE L'ÉCRAN DE VEILLE : ce qu'elle garde, ce qu'elle montre, ce
// qu'elle écrit.
//
// ⚠ UNE CLASSE À PART, ET C'EST LE PLAFOND QUI L'A VOULU. `GameOptions` touche
// ses huit cents lignes, et cette section demandait une douzaine de membres à
// elle seule. Le découpage n'est pas arbitraire pour autant : c'est le même
// qu'`OptionTargets` — ce n'est pas la classe des réglages, c'est UN objet
// qu'elle tient, avec ses lignes et ses listes.
//
// 🔴 LES LIBELLÉS ET LEUR ORDRE SONT GRAVÉS. Les infobulles disent ce que fait
// CE client là où il a son propre repère — la plongée du jeu, qui est à
// cinquante degrés ; le plafond de recul, que notre moteur ne borne pas par un
// brouillard. Chacun de ces points est dit à sa ligne.
class AfkOptions {
 public:
  [[nodiscard]] const AfkSettings& settings() const noexcept { return settings_; }

  // Déclare les vingt-trois clés au fichier. ⚠ LA VALEUR PRÉSENTE DANS
  // `AfkSettings` EST LE DÉFAUT : il n'y en a pas d'autre source.
  void Declare(SettingsFile& into);

  // Refait les lignes. ⚠ À CHAQUE IMAGE, panneau ouvert : un changement de
  // langue se voit alors à l'image suivante, sans rien de figé.
  void Compose();

  [[nodiscard]] std::span<const ro::ui::OptionRowView> rows() const noexcept { return rows_; }

  // Écrit ce que la ligne a changé. ⚠ ELLE NE POSE RIEN DANS LE MOTEUR : la
  // veille relit ces réglages à chaque image. Seul « Essayer maintenant » a
  // quelque chose à demander, et il le fait par `OptionTargets`.
  void Change(GameOption which, const ro::ui::OptionsWindowAction& asked, const OptionTargets& to);

 private:
  AfkSettings settings_;
  std::vector<ro::ui::OptionRowView> rows_;
  // Les deux listes déroulantes. ⚠ REFAITES À CHAQUE IMAGE, comme les noms de
  // langue : le pointeur d'un texte traduit meurt au changement de langue.
  std::vector<const char*> wake_names_;
  std::vector<const char*> anchor_names_;
};

}  // namespace moonclient
