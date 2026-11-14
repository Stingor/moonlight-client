#pragma once

// UNE LISTE DE DESSIN SANS CONTEXTE IMGUI. Ce qu'on mesure, ce sont les SOMMETS
// produits, et ils ne demandent ni fenêtre ni GPU.
//
// ⚠ ELLE PEINT SANS FONDU : ses drapeaux sont nuls, donc un remplissage n'étale
// pas le demi-pixel qu'ImGui pose d'ordinaire au-delà du contour. Un sommet lu
// ici est un sommet du contour lui-même.

#include <imgui.h>
#include <imgui_internal.h>

namespace ro::test {

struct LoneDrawList {
  // La tolérance d'arc d'ImGui par défaut, en pixels.
  static constexpr float kCircleMaxError = 1.60F;
  // L'étendue du découpage : bien plus grande que tout ce qu'un essai dessine.
  static constexpr float kSheetPixels = 4096.0F;

  ImDrawListSharedData shared;
  ImDrawList into;

  LoneDrawList() : into(&shared) {
    // 🔴 SANS CETTE LIGNE, LE BANC DIVISE PAR ZÉRO. Le constructeur de
    // `ImDrawListSharedData` efface tout, y compris la table qui dit en combien
    // de segments tailler un arc : c'est `ImGui::CreateContext` qui la remplit
    // d'ordinaire, et un banc sans contexte ne l'a pas. Tout arrondi la lit.
    shared.SetCircleTessellationMaxError(kCircleMaxError);
    shared.ClipRectFullscreen = ImVec4(0.0F, 0.0F, kSheetPixels, kSheetPixels);
    into._ResetForNewFrame();
    into.PushClipRect(ImVec2(0.0F, 0.0F), ImVec2(kSheetPixels, kSheetPixels), false);
    into.PushTextureID(ImTextureRef());
  }
};

}  // namespace ro::test
