#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <span>

#include <imgui.h>

struct ImGuiWindow;

namespace ro::ui {

// ── L'AIMANT ENTRE FENÊTRES ─────────────────────────────────────────────────
//
// La fenêtre qu'on traîne colle, bord à bord, aux fenêtres VISIBLES qu'elle
// approche et aux bords de l'écran ; une fois accolée, elle aligne ses bords
// de même côté sur sa voisine. Comme la grille, il passe par le seul point de
// passage de toute fenêtre traînée — `SnapMovingWindow` — et ne connaît aucune
// fenêtre en particulier.
//
// ⚠ UN BORD NE COLLE QU'À UNE FENÊTRE QU'IL LONGE : les deux doivent se
// chevaucher sur l'autre axe, à la portée près. Sans cette garde, le bord
// gauche d'une fenêtre en haut de l'écran sauterait sur le bord droit d'une
// autre posée tout en bas.

// La portée de l'aimant, en pixels de l'art d'origine : elle suit l'échelle de
// l'interface, comme les fenêtres qu'elle rapproche.
inline constexpr float kWindowMagnetReachPixels = 8.0F;

// L'écart sous lequel deux bords « se touchent » déjà. ⚠ NON NUL : une place
// recalculée par `bord - largeur + largeur` peut manquer l'égalité d'un ulp.
inline constexpr float kWindowMagnetTouchSlackPixels = 0.5F;

// Le repère du bord qui accroche : un cœur fin et clair sur un halo plus large.
inline constexpr ImU32 kWindowMagnetLineColor = IM_COL32(140, 225, 255, 235);
inline constexpr ImU32 kWindowMagnetGlowColor = IM_COL32(140, 225, 255, 70);
inline constexpr float kWindowMagnetLinePixels = 1.5F;
inline constexpr float kWindowMagnetGlowPixels = 5.0F;

// Le plus grand nombre de fenêtres qu'un aimant passé avant celui-ci peut faire
// suivre la fenêtre traînée. Huit : les barres de raccourcis.
inline constexpr std::size_t kMostMagnetRiders = 8;

// Un rectangle d'écran, en pixels, bords compris.
struct MagnetBox {
  float left = 0.0F;
  float top = 0.0F;
  float right = 0.0F;
  float bottom = 0.0F;
};

// Ce qu'un aimant passé AVANT celui-ci a déjà décidé pour la fenêtre traînée.
struct EarlierMagnet {
  // Un axe déjà tenu n'est plus touché par l'aimant entre fenêtres : l'aimant
  // des barres de raccourcis aligne aussi des bords qui ne se touchent pas.
  bool held_across = false;
  bool held_down = false;
  // ⚠ LES FENÊTRES QUI SUIVENT LA FENÊTRE TRAÎNÉE ne sont pas des voisines :
  // posées sur sa place de l'image d'avant, elles la retiendraient sur place.
  // Zéro : aucune.
  std::array<ImGuiID, kMostMagnetRiders> riders{};

  [[nodiscard]] bool Rides(ImGuiID window_id) const;
};

struct WindowMagnetAsk {
  // Le coin haut-gauche où la souris pose la fenêtre, et sa taille.
  ImVec2 at{0.0F, 0.0F};
  ImVec2 size{0.0F, 0.0F};
  std::span<const MagnetBox> neighbours;
  ImVec2 screen{0.0F, 0.0F};
  float reach_px = kWindowMagnetReachPixels;
  bool held_across = false;
  bool held_down = false;
};

// Un trait d'écran, d'un bout à l'autre.
struct MagnetLine {
  ImVec2 from{0.0F, 0.0F};
  ImVec2 to{0.0F, 0.0F};
};

struct WindowMagnetCatch {
  ImVec2 at{0.0F, 0.0F};
  // Le bord qui accroche sur chaque axe, à peindre ; vide quand l'axe est libre.
  std::optional<MagnetLine> across;
  std::optional<MagnetLine> down;
};

// La portée de l'aimant à cette échelle d'interface.
[[nodiscard]] float WindowMagnetReachPixels(float interface_scale);

// LA PLACE AIMANTÉE d'une fenêtre posée en `ask.at`. Pure.
//
// Sur chaque axe libre, le bord le plus proche dans la portée gagne : bord de
// l'écran, ou bord opposé d'une voisine que la fenêtre longe. Un axe resté
// libre s'aligne ensuite sur une voisine contre laquelle la fenêtre est
// accolée sur l'AUTRE axe — hauts ensemble pour deux fenêtres côte à côte,
// gauches ensemble pour deux fenêtres l'une sur l'autre.
[[nodiscard]] WindowMagnetCatch MagnetizedWindow(const WindowMagnetAsk& ask);

// La place aimantée de la fenêtre qu'ImGui traîne, parmi les fenêtres de
// premier niveau visibles et déplaçables, sans `moving` ni ce qui la suit.
[[nodiscard]] WindowMagnetCatch MagnetizedMovingWindow(const ImGuiWindow& moving,
                                                       const EarlierMagnet& earlier);

// Peint les bords qui accrochent, au premier plan.
void DrawWindowMagnetCatch(const WindowMagnetCatch& caught);

}  // namespace ro::ui
