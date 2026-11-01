#pragma once

#include <string>
#include <vector>

namespace ro::ui {

// ── LE TÉMOIN DE LA VOIX ────────────────────────────────────────────────────
//
// Une colonne discrète sur le bord gauche : qui parle à l'instant, soi compris,
// et une ligne quand la voix a quelque chose à dire — une touche à choisir, un
// serveur injoignable. Rien quand personne ne parle et que tout va.
//
// ⚠ L'INTERFACE NE DÉCIDE RIEN : `app` compose les noms et le texte, déjà
// traduits. Le témoin ne prend ni clic ni touche.
struct VoiceOverlayState {
  std::vector<std::string> speaking;
  // Le nom du joueur quand sa voix part, vide sinon.
  std::string self_talking;
  std::string notice;
};

// Où commence la colonne, en fraction de la hauteur de l'écran : sous la
// mini-carte des fenêtres d'usine, au-dessus de la conversation.
inline constexpr float kVoiceOverlayTopRatio = 0.30F;
inline constexpr float kVoiceOverlayLeftPixels = 10.0F;

// ⚠ APRÈS LES FENÊTRES : le témoin passe par-dessus.
void DrawVoiceOverlay(const VoiceOverlayState& state);

}  // namespace ro::ui
