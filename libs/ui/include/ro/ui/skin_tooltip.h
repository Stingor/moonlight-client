#pragma once

#include <imgui.h>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// ── LA BULLE DU JEU, SANS AVOIR SON HABILLAGE EN MAIN ───────────────────────
//
// `BeginSkinnedTooltip` demande l'habillage, et la plupart des bulles naissent
// loin de lui : au fond d'une cellule, d'une ligne de table, d'un bouton. Le
// faire descendre jusque-là toucherait des dizaines de signatures pour un seul
// pointeur — que la fenêtre habillée en cours de dessin tient DÉJÀ.
//
// 🔴 ELLE PREND DONC L'HABILLAGE DE LA FENÊTRE (OU DE LA MODALE) EN COURS. Hors
// de toute fenêtre habillée — une barre posée sur le monde, la conversation —,
// elle garde le même corps, blanc à texte noir, sans le cadre : jamais le fond
// sombre d'ImGui, sur lequel le texte du jeu, écrit pour le clair, disparaît.
//
// ⚠ `EndSkinTooltip` NE S'APPELLE QUE SI L'OUVERTURE A RENDU VRAI.
//
// `widest` borne la largeur ; zéro prend celle des bulles du jeu.
[[nodiscard]] bool BeginSkinTooltip(float widest = 0.0F);

// LA MARGE ET L'INTERLIGNE DE LA BULLE SANS CADRE, en pixels d'interface.
//
// 🔴 POSÉS PAR ELLE, jamais hérités : une barre de raccourcis pose une marge
// nulle pour ses cases, et la bulle ouverte au milieu collait son texte à ses
// bords. La bulle habillée n'en a pas besoin : l'épaisseur de son cadre fait
// sa marge. L'interligne est celui d'ImGui.
inline constexpr float kBareTooltipPaddingPixels = 6.0F;
inline constexpr float kBareTooltipLineGapPixels = 4.0F;
void EndSkinTooltip();

// UNE BULLE DE TEXTE, formatée comme `printf`, repliée à la largeur des bulles
// du jeu. ⚠ ELLE NE TESTE PAS LE SURVOL : c'est à l'appelant, comme pour
// l'infobulle d'ImGui qu'elle remplace.
void SkinTooltipFormat(const char* format, ...) IM_FMTARGS(1);

// ── LE SIGNE D'AVERTISSEMENT ────────────────────────────────────────────────
//
// 🔴 PEINT, ET NON ÉCRIT : le glyphe « ⚠ » n'est dans aucune des plages que
// l'atlas charge, et il s'afficherait en point d'interrogation. Un triangle
// ambre, un point d'exclamation sombre.
inline constexpr ImU32 kSkinWarningFill = IM_COL32(240, 170, 30, 255);
inline constexpr ImU32 kSkinWarningInk = IM_COL32(40, 30, 10, 255);

// Peint le signe dans le carré de côté `side` posé à `top_left`, en pixels
// d'écran. Le côté d'une ligne de texte l'aligne sur elle.
void PaintWarningMark(ImDrawList& into, ImVec2 top_left, float side);

// UNE BULLE QUI MET EN GARDE : le texte, une ligne vide, puis le signe et
// `warning` enroulé à sa droite — à la largeur des bulles du jeu. Sans
// `warning`, c'est la bulle de texte seule ; sans texte, le signe et
// l'avertissement seuls.
//
// ⚠ ELLE NE TESTE PAS LE SURVOL, comme `SkinTooltipFormat`. Et les deux textes
// sont posés TELS QUELS, sans gabarit : un pourcent dans une traduction n'y
// devient pas une conversion.
void SkinTooltipWarned(const char* text, const char* warning);

// L'habillage de la fenêtre ou de la modale habillée en cours de dessin, sinon
// celui qu'un cadre non habillé prête, ou nul. Tenu par `skinned_window.cpp`.
[[nodiscard]] const SkinArt* PaintingSkinArt() noexcept;

// UN CADRE QUI N'EST PAS UNE FENÊTRE HABILLÉE PRÊTE SON HABILLAGE à ses menus
// et à ses bulles — la conversation : son corps reste le sien, translucide sur
// le monde, mais ce qui surgit d'elle suit le thème du joueur, sombre compris.
// Nul retire le prêt.
//
// ⚠ LE TEMPS DU CADRE SEULEMENT : prêté à l'ouverture, retiré à la fermeture,
// sans quoi les surgissants d'un HUD dessiné ensuite en hériteraient.
void LendSkinArt(const SkinArt* art) noexcept;

// Le prêt, retiré à la sortie de la portée. ⚠ COMME POUR UNE FENÊTRE, rien
// n'est prêté d'un habillage qui n'habille pas les fenêtres : ses surgissants
// garderaient sinon un cadre fait de pièces absentes.
class LentSkinArt {
 public:
  explicit LentSkinArt(const SkinArt& art) noexcept;
  ~LentSkinArt() { LendSkinArt(nullptr); }
  LentSkinArt(const LentSkinArt&) = delete;
  LentSkinArt& operator=(const LentSkinArt&) = delete;
  LentSkinArt(LentSkinArt&&) = delete;
  LentSkinArt& operator=(LentSkinArt&&) = delete;
};

// L'HABILLAGE RÉGLÉ DE LA FENÊTRE EN COURS DE DESSIN, ou celui d'origine hors
// d'une fenêtre habillée. Pour ce qui peint une couleur de l'habillage sans
// recevoir l'art — une couleur d'état au fond d'une liste.
[[nodiscard]] const SkinLook& PaintingLook() noexcept;

}  // namespace ro::ui
