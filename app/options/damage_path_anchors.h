#pragma once

#include <vector>

#include <imgui.h>

#include "ro/ui/options_window.h"
#include "ro/ui/path_canvas.h"

#include "options/game_option_ids.h"
#include "world/hit_popup_look.h"

namespace moonclient {

// ── LES ANCRES DES TRAJECTOIRES DE DÉGÂTS ───────────────────────────────────
//
// Ce que le cadre de l'onglet « Allure » montre à tirer, et ce qu'une
// ancre tirée — ou un curseur poussé — change. Tout vaut pour le type MONTRÉ,
// `HitPopupLook::path_shown`, et les ancres portent les identifiants des
// curseurs qu'elles doublent : les deux gestes passent par le même aiguillage.
//
// ⚠ SANS RANGÉE NI PAGE : des fonctions pures, que les essais appellent seules.

// Les ancres d'une trajectoire, pour le cadre : les deux poignées de la courbe,
// les deux rayons de la dispersion quand elle est allumée, puis l'arrivée et
// enfin le départ. ⚠ DANS CET ORDRE : sans arrivée, toutes se confondent au
// départ, et c'est la dernière posée qu'on saisit.
//
// 🔴 SUR LE CHEMIN QUE LES NOMBRES SUIVENT : la trajectoire s'ajoute à celui de
// l'allure, et une ancre posée sans lui tombait loin des nombres — une cloche
// qui dérive à gauche annulait une arrivée tirée à droite. `allure` dit de
// combien l'allure a déplacé le nombre là où chaque ancre agit.
struct AllureShift {
  // Au tiers puis aux deux tiers de la vie, où chaque poignée s'accroche.
  ImVec2 one{0.0F, 0.0F};
  ImVec2 two{0.0F, 0.0F};
  // À l'extinction, où il arrive.
  ImVec2 end{0.0F, 0.0F};
};
//
// `client` est le chemin que le client donne au type : c'est là que « Remettre
// comme le client » ramène chaque ancre.
[[nodiscard]] std::vector<ro::ui::PathAnchor> PathAnchorsOf(const NumberPath& path,
                                                            const AllureShift& allure = {},
                                                            const NumberPath& client = {});

// De combien cette ancre est décalée par l'allure. ⚠ À RETIRER DU POINT OÙ ON
// LA LÂCHE avant `MoveAnchor`, qui ne connaît que la trajectoire.
[[nodiscard]] ImVec2 AllureShiftOf(GameOption anchor, const AllureShift& allure) noexcept;

// Et le chemin dessiné.
[[nodiscard]] ro::ui::PathTrace PathTraceOf(const NumberPath& path) noexcept;

// Ce qu'une ancre lâchée en `point` change de la trajectoire. Faux pour ce qui
// n'est pas une ancre.
bool MoveAnchor(NumberPath& path, GameOption anchor, ImVec2 point) noexcept;

// CE QUE PORTE `what` DANS LA TRAJECTOIRE DU TYPE MONTRÉ — une ancre, ou toute
// la trajectoire sous `kDamagePathCopy` —, copié vers le type de rang
// `target`, ou vers tous les autres au rang `kDamagePathCount`. Faux pour une
// cible ou une ancre inconnue.
bool CopyPathPart(HitPopupLook& look, GameOption what, int target);

// Ce qu'une rangée — ou une ancre — de l'onglet change. Faux pour une autre
// ligne.
bool ChangeDamagePath(HitPopupLook& look, GameOption which,
                      const ro::ui::OptionsWindowAction& asked);

}  // namespace moonclient
