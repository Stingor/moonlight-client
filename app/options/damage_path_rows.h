#pragma once

#include <vector>

#include "ro/ui/options_window.h"
#include "ro/ui/path_canvas.h"

#include "options/damage_page_names.h"
#include "world/hit_popup_look.h"

namespace moonclient {

// ── LA TRAJECTOIRE, DANS L'ONGLET « ALLURE » DES DÉGÂTS ─────────────────────
//
// Le type qu'on règle, le cadre où ses nombres sortent et où l'on tire ses
// ancres, puis ses valeurs exactes. Tout vaut pour le type MONTRÉ —
// `HitPopupLook::path_shown` — et les ancres portent les identifiants des
// curseurs qu'elles remplacent : une ancre tirée et un curseur poussé passent
// par le même aiguillage — voir `damage_path_anchors.h`.

// LE VOLET DE DROITE : le choix du type, et le cadre où ses nombres sortent —
// il y prend toute la hauteur. `anchored` y pose les ancres de la trajectoire :
// l'onglet de l'allure les tire, celui des couleurs ne fait que regarder.
// `sample` est le cadre composé par la partie, lu AU MOMENT DE PEINDRE.
void PathPreviewRows(const HitPopupLook& look, const DamagePageNames& names,
                     const ro::ui::PathSample* sample, bool anchored,
                     std::vector<ro::ui::OptionRowView>& side);

// EN TÊTE DE L'ONGLET DE L'ALLURE, LE TYPE MONTRÉ : ce que disent les ancres, sa
// vie — sauf pour les mots, qui gardent celle du client —, sa dérive de côté,
// sa dispersion, et le geste qui le rend au client. ⚠ PAS DE CURSEUR POUR LE
// CHEMIN : le départ, l'arrivée, les poignées et les rayons se tirent à la
// souris, dans le volet.
void PathTypeRows(const HitPopupLook& look, std::vector<ro::ui::OptionRowView>& rows);

}  // namespace moonclient
