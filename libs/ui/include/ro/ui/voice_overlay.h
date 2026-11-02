#pragma once

#include <span>
#include <string>
#include <vector>

#include <imgui.h>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// ── L'ICÔNE AU-DESSUS DE QUI PARLE ──────────────────────────────────────────
//
// Une par personnage qui parle à l'écran, soi compris : le milieu du haut de
// son corps et de sa tête, en pixels de fenêtre. L'image est celle que le
// joueur a choisie ; `app` la donne, l'interface ne connaît pas de fichier.
struct VoiceMarkView {
  ImVec2 head;
};

// Le côté de l'icône, et l'écart qui la sépare de la tête, en pixels
// d'interface.
inline constexpr float kVoiceMarkPixels = 24.0F;
inline constexpr float kVoiceMarkLiftPixels = 6.0F;

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
  // Les têtes de qui parle, à l'écran, et l'icône à y poser — vide pour aucune.
  std::vector<VoiceMarkView> marks;
  SkinImage mark_icon;
};

// Où commence la colonne, en fraction de la hauteur de l'écran : sous la
// mini-carte des fenêtres d'usine, au-dessus de la conversation.
inline constexpr float kVoiceOverlayTopRatio = 0.30F;
inline constexpr float kVoiceOverlayLeftPixels = 10.0F;

// ⚠ APRÈS LES FENÊTRES : le témoin passe par-dessus.
void DrawVoiceOverlay(const VoiceOverlayState& state);

// ⚠ AVANT LES BULLES, sur le fond : l'icône appartient au monde, et une phrase
// qui monte de la même tête passe par-dessus.
void DrawVoiceMarks(std::span<const VoiceMarkView> marks, const SkinImage& icon);

}  // namespace ro::ui
