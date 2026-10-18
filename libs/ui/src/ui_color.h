#pragma once

// LES PETITS CALCULS DE COULEUR DE L'INTERFACE, en un seul endroit, sur des
// couleurs ImGui `ImU32` (R, G, B, A rangés par les décalages `IM_COL32_*`).
//
// En-tête PRIVÉ de `libs/ui/src`. 🔴 AUCUNE COPIE LOCALE : une copie gardée
// dans un espace anonyme ou dans un sous-espace de famille masque l'aide
// commune SANS ERREUR dans tout l'espace qui l'entoure. Elle ne devient
// ambiguë — donc visible — que déclarée dans `ro::ui` même, avec la même
// signature, et pour les appels faits hors de son espace anonyme.

#include <algorithm>
#include <cmath>

#include <imgui.h>

namespace ro::ui {

// La couleur, son opacité multipliée par `share` borné à [0, 1], ARRONDIE à
// l'entier le plus proche.
[[nodiscard]] inline ImU32 ScaledAlpha(ImU32 color, float share) {
  const auto alpha = static_cast<float>((color >> IM_COL32_A_SHIFT) & 0xFFU);
  const auto kept = static_cast<ImU32>(std::lround(alpha * std::clamp(share, 0.0F, 1.0F)));
  return (color & ~IM_COL32_A_MASK) | (kept << IM_COL32_A_SHIFT);
}

// Une couleur éclaircie de `by` sur chaque canal, plafonné à 255, son opacité
// gardée : c'est ainsi que le jeu marque un survol, plutôt qu'avec une seconde
// teinte à tenir à jour.
[[nodiscard]] constexpr ImU32 Lighter(ImU32 color, int by) {
  const auto clamp = [](int channel) { return channel > 255 ? 255 : channel; };
  const int red = clamp(static_cast<int>((color >> IM_COL32_R_SHIFT) & 0xFF) + by);
  const int green = clamp(static_cast<int>((color >> IM_COL32_G_SHIFT) & 0xFF) + by);
  const int blue = clamp(static_cast<int>((color >> IM_COL32_B_SHIFT) & 0xFF) + by);
  const int alpha = static_cast<int>((color >> IM_COL32_A_SHIFT) & 0xFF);
  return IM_COL32(red, green, blue, alpha);
}

// La même couleur, opaque.
[[nodiscard]] constexpr ImU32 Opaque(ImU32 color) {
  return (color & ~IM_COL32_A_MASK) | IM_COL32(0, 0, 0, 255);
}

// FOND TOUT CE QUI S'EST PEINT DEPUIS `first_vertex` : l'opacité de chaque
// sommet multipliée par `alpha`, TRONQUÉE ; rien ne change à `alpha` ≥ 1.
//
// ⚠ APRÈS COUP, SUR LES SOMMETS : le moteur du fil peint ses couleurs propres —
// celle d'un lien, d'une suite colorée, le masque d'un spoiler, une emote — et
// n'a pas d'opacité à recevoir. Ce qu'il a peint s'efface pourtant d'un seul
// tenant.
inline void FadeSince(ImDrawList* into, int first_vertex, float alpha) {
  if (alpha >= 1.0F) {
    return;
  }
  for (int rank = first_vertex; rank < into->VtxBuffer.Size; ++rank) {
    ImU32& color = into->VtxBuffer[rank].col;
    const auto kept =
        static_cast<ImU32>(static_cast<float>((color >> IM_COL32_A_SHIFT) & 0xFFU) * alpha);
    color = (color & ~IM_COL32_A_MASK) | (kept << IM_COL32_A_SHIFT);
  }
}

}  // namespace ro::ui
