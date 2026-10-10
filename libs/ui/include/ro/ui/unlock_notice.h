#pragma once

#include <span>

namespace ro::ui {

// L'AVIS DE POSE : ce que l'écran rappelle tant qu'un élément d'interface est
// déverrouillé — barres de raccourcis, jauges, barre d'états, cadres du HUD.
//
// ⚠ IL DIT LES GESTES, ET IL OFFRE LA SORTIE. Un élément déverrouillé se saisit
// au lieu de se cliquer : sans l'avis, on croit le jeu cassé ; sans la case,
// il faudrait fouiller les options élément par élément pour le refermer.
//
// `app` décide quand l'avis paraît, et ce que la case verrouille : `ui` ne
// connaît aucun réglage.
struct UnlockNoticeAnswer {
  // La case vient d'être cochée : tout verrouiller.
  bool lock_all = false;
  // L'autre case : ne plus montrer l'avis. Elle se rallume dans les options,
  // section Interface.
  bool hide_notice = false;
};

// Une petite fenêtre flottante, sans titre, en haut de l'écran. Elle se
// déplace d'un glisser sur son fond, comme ce qu'elle décrit.
//
// `unlocked` nomme, déjà traduits, les éléments déverrouillés : l'avis les
// cite sous son titre, pour qu'on sache ce qu'on a ouvert.
[[nodiscard]] UnlockNoticeAnswer DrawUnlockNotice(std::span<const char* const> unlocked);

}  // namespace ro::ui
