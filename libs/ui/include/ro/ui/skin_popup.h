#pragma once

#include <imgui.h>

namespace ro::ui {

// ── LE MENU SURGISSANT DU JEU : lisible, OÙ QU'IL S'OUVRE ────────────────────
//
// DANS UNE FENÊTRE HABILLÉE, il prend l'habillage du joueur — fond, liseré,
// sélection, texte et texte grisé —, qu'il soit sombre ou clair : c'est la
// couleur « Menus et listes ». Hors d'une fenêtre habillée — un HUD, la
// conversation —, il prend le corps et le texte du jeu, comme suit.
//
// 🔴 UN SURGISSANT HÉRITE DES COULEURS DE L'ENDROIT OÙ IL S'OUVRE. La
// conversation pose un texte BLANC pour son fond sombre ; un menu ouvert sur un
// lien du fil prenait ce blanc sur son propre fond clair, et ses entrées
// disparaissaient. Le défaut revenait à chaque nouveau menu, parce que chaque
// appelant devait penser à rétablir le texte lui-même.
//
// Ces deux-là le rétablissent pour tous : le texte de l'habillage et son
// texte grisé, posés DANS le surgissant — ce qu'il contient ne dépend plus
// de ce qui l'entoure. `scripts/check_hygiene.py` refuse un `BeginPopup` nu.
//
// 🔴 LE TEXTE SOMBRE EXIGE UN FOND CLAIR, et ils posent donc la PAIRE. Le
// fond clair n'existe que dans une fenêtre habillée : ouvert ailleurs — un HUD
// sombre comme le compteur de dégâts, ou hors de toute fenêtre —, le
// surgissant gardait le fond sombre d'ImGui sous le texte de l'habillage.
// Quand l'endroit impose un fond sombre, ils posent donc aussi le fond, le
// liseré, les filets et la sélection de l'habillage ; un fond déjà clair, posé
// exprès par l'appelant, est respecté.
//
// ⚠ `EndSkinPopup` NE S'APPELLE QUE SI L'OUVERTURE A RENDU VRAI, comme
// `EndPopup`.
[[nodiscard]] bool BeginSkinPopup(const char* id, ImGuiWindowFlags flags = 0);
// Le menu d'un clic droit sur l'élément qu'on vient de poser. Un second clic
// sur le même élément, menu ouvert, le referme au lieu de le rouvrir.
[[nodiscard]] bool BeginSkinPopupContextItem(
    const char* id = nullptr, ImGuiPopupFlags flags = ImGuiPopupFlags_MouseButtonRight);
void EndSkinPopup();

// Vrai quand ce fond de surgissant est trop sombre pour le texte de
// l'habillage : le surgissant prend alors le corps de l'habillage.
[[nodiscard]] bool SkinPopupNeedsBody(ImU32 ambient);

}  // namespace ro::ui
