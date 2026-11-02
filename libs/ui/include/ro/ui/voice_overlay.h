#pragma once

#include <array>
#include <cstddef>
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

// ── LA BARRE DE LA VOIX ─────────────────────────────────────────────────────
//
// Une petite fenêtre qu'on place où l'on veut : une pastille par canal, qu'un
// clic écoute ou n'écoute plus et qu'un clic droit fait canal par défaut, le
// micro, la sourdine, et dessous qui parle.
//
// ⚠ ELLE NE CHANGE RIEN ELLE-MÊME : elle rend ce qu'on a cliqué, et `app` le
// pose comme un réglage de la page Son.
inline constexpr std::size_t kVoiceBarChannelCount = 3;

struct VoiceBarChannel {
  std::string name;  // déjà traduit
  std::string tip;   // la bulle, déjà traduite
  bool listened = false;
  bool is_default = false;
  bool talking = false;  // sa propre voix y part
};

struct VoiceBarView {
  bool shown = false;
  std::array<VoiceBarChannel, kVoiceBarChannelCount> channels;
  bool self_muted = false;
  bool deafened = false;
  std::string mute_tip;
  std::string deafen_tip;
  SkinImage mic_icon;
  SkinImage speaker_icon;
  // ── SON APPARENCE, réglée dans la page Son ──
  float scale = 1.0F;
  float opacity = 1.0F;
  bool vertical = false;  // les boutons en colonne
  bool locked = false;    // elle ne se déplace plus
  bool speakers = true;   // qui parle, sous les boutons
  // ESTOMPÉE quand personne ne parle et que la souris est ailleurs : elle se
  // fait oublier sans disparaître.
  bool fade_idle = false;
};

// Ce qu'on a cliqué sur la barre, à cette image. Moins un : rien.
struct VoiceBarAnswer {
  int toggle_listen = -1;
  int make_default = -1;
  bool toggle_mute = false;
  bool toggle_deafen = false;
};

// ── LE TÉMOIN DE LA VOIX ────────────────────────────────────────────────────
//
// La barre quand elle est montrée ; sinon une colonne discrète sur le bord
// gauche : qui parle à l'instant, soi compris, et une ligne quand la voix a
// quelque chose à dire — une touche à choisir, un serveur injoignable. Rien
// quand personne ne parle et que tout va.
//
// ⚠ L'INTERFACE NE DÉCIDE RIEN : `app` compose les noms et le texte, déjà
// traduits, et lit `bar_answer` après le dessin.
struct VoiceOverlayState {
  std::vector<std::string> speaking;
  // Le nom du joueur quand sa voix part, vide sinon.
  std::string self_talking;
  std::string notice;
  // Les têtes de qui parle, à l'écran, et l'icône à y poser — vide pour aucune.
  std::vector<VoiceMarkView> marks;
  SkinImage mark_icon;
  VoiceBarView bar;
  VoiceBarAnswer bar_answer;
};

// Où commence la colonne, en fraction de la hauteur de l'écran : sous la
// mini-carte des fenêtres d'usine, au-dessus de la conversation.
inline constexpr float kVoiceOverlayTopRatio = 0.30F;
inline constexpr float kVoiceOverlayLeftPixels = 10.0F;

// La barre, au premier lancement : sous la mini-carte, comme la colonne.
inline constexpr const char* kVoiceBarWindowId = "##voice_bar";

// ⚠ APRÈS LES FENÊTRES : le témoin passe par-dessus. Pose `bar_answer`.
void DrawVoiceOverlay(VoiceOverlayState& state);

// ⚠ AVANT LES BULLES, sur le fond : l'icône appartient au monde, et une phrase
// qui monte de la même tête passe par-dessus.
void DrawVoiceMarks(std::span<const VoiceMarkView> marks, const SkinImage& icon);

}  // namespace ro::ui
