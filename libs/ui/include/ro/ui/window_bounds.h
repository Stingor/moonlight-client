#pragma once

#include <imgui.h>

namespace ro::ui {

// FENÊTRES : RESTER DANS L'ÉCRAN, ET N'EN PRENDRE QU'UNE PART.
//
// ImGui laisse SCIEMMENT une fenêtre sortir presque entièrement de l'écran : son
// bornage interne ne garantit que `style.DisplayWindowPadding` de fenêtre
// visible — dix-neuf pixels. Un joueur qui glisse un panneau vers un bord se
// retrouve avec un moignon de barre de titre, et la fenêtre est perdue : rien à
// attraper pour la ramener, et la position part dans les réglages pour la
// session suivante.
//
// On impose donc l'invariant « une fenêtre reste ENTIÈREMENT dans l'écran ».

// La part d'écran qu'une fenêtre ne dépasse pas. Assez pour qu'une fenêtre
// pleine reste utilisable, assez peu pour qu'il reste toujours de quoi
// l'attraper et voir le jeu derrière.
inline constexpr float kMaxScreenFraction = 0.90F;

// Corrige la position d'un rectangle pour qu'il tienne en entier dans l'écran.
// Rend la position inchangée si elle est déjà bonne, ou si l'écran est de taille
// nulle — fenêtre du jeu réduite : borner dans un écran nul collerait tout en
// (0,0), et cette position-là serait mémorisée.
//
// Une fenêtre plus grande que l'écran sur un axe s'aligne sur le bord haut ou
// gauche : c'est le coin dont on a besoin — la barre de titre, les premiers
// éléments — et c'est ce qu'ImGui fait lui-même pour la taille.
[[nodiscard]] ImVec2 ClampWindowPosToScreen(ImVec2 window_pos, ImVec2 window_size);

// LA POIGNÉE DOIT BUTER, ET PAS APRÈS COUP.
//
// ⚠ À POSER AVANT LE `Begin` DE LA FENÊTRE, et le moment fait tout : la borne
// entre alors dans le calcul du glisser — `CalcResizePosSizeFromAnyCorner`
// consulte la contrainte à l'image même —, donc la poignée s'arrête net.
//
// ⚠ LA PASSE DE `WindowPlacer` NE PEUT PAS FAIRE CELA, et ce n'est pas un
// doublon. Elle tourne après `NewFrame`, donc AVANT les `Begin` : pendant un
// redimensionnement elle arrive systématiquement une image trop tôt, corrige la
// taille de l'image précédente, et le glisser la réécrit aussitôt depuis la
// position de la souris. Elle est le FILET — tailles démesurées relues des
// réglages, fenêtres ouvertes sans passer par nos aides —, ceci est le mécanisme.
//
// Ce qui est borné : la part d'écran ci-dessus, ET le bord de l'écran lui-même
// tant que le joueur tient une poignée. Le nom sert à retrouver la fenêtre, qui
// peut n'exister encore nulle part : c'est sa première image, elle n'a alors ni
// position ni poignée, et seule la part d'écran la borne.
void ConstrainNextWindowToScreen(const char* name);

// Les fenêtres dont ImGui décide seul : il les borne déjà au viewport, et les
// replace de toute façon à chaque `Begin`.
inline constexpr ImGuiWindowFlags kWindowOwnedByImGui =
    ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_Popup | ImGuiWindowFlags_Tooltip |
    ImGuiWindowFlags_ChildMenu;

// Taille dictée par le contenu ou par l'appelant : ce n'est pas le joueur qui
// l'a tirée, ce n'est pas à nous de la couper.
inline constexpr ImGuiWindowFlags kWindowSizeNotOurs =
    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize;

}  // namespace ro::ui
