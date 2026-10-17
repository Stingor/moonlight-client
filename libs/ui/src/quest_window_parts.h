#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <imgui.h>

#include "ro/ui/quest_window.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/skin_tooltip.h"

#include "quest_paint.h"

// LES PIÈCES QUE LES TROIS FICHIERS DU JOURNAL DES QUÊTES SE PARTAGENT : sa
// palette, ses mesures communes, et les deux volets.
//
// En-tête PRIVÉ. Les mesures sont en PIXELS D'ART ; l'échelle de l'interface
// s'y applique à l'emploi, comme dans les fenêtres voisines.
namespace ro::ui::quest_window {

using quest_paint::Painter;

// ── LA PALETTE ──────────────────────────────────────────────────────────────
//
// LE JOURNAL SE POSE SUR LE PAPIER de l'habillage, celui des bulles et des
// descriptions : ses deux volets s'y étendent. Ses propres couleurs, choisies
// pour du blanc, s'y traduisent par `OnPaper` ; le texte courant, le texte
// secondaire et les trois états prennent ceux du thème.

// Le titre d'une ligne, et celui du détail.
inline constexpr ImU32 kRowTitleText = IM_COL32(0x22, 0x53, 0x95, 0xFF);
inline constexpr ImU32 kDetailTitleText = IM_COL32(0x1D, 0x32, 0x55, 0xFF);
// Le temps qui reste, au repos.
inline constexpr ImU32 kTimeText = IM_COL32(0x4A, 0x5A, 0x78, 0xFF);
// La pastille « Nouveau ».
inline constexpr ImU32 kFreshFill = IM_COL32(0xE0, 0x5A, 0x2B, 0xFF);
inline constexpr ImU32 kFreshText = IM_COL32(0xFF, 0xFF, 0xFF, 0xFF);
// Les barres d'avancée : en cours, remplie.
inline constexpr quest_paint::BarColors kProgressBar{
    .fill = IM_COL32(0x52, 0x7B, 0xCE, 0xFF), .background = IM_COL32(0xDC, 0xE2, 0xEE, 0xFF)};
inline constexpr quest_paint::BarColors kProgressBarDone{.fill = IM_COL32(0x3A, 0xA6, 0x4A, 0xFF),
                                                         .background = kProgressBar.background};
// Le filet qui sépare deux lignes, deux sections.
inline constexpr ImU32 kRuleColor = IM_COL32(0xD6, 0xDB, 0xE6, 0xFF);

[[nodiscard]] inline ImU32 Paper() { return PaintingLook().paper; }
// Une couleur de la palette, posée sur le papier.
[[nodiscard]] inline ImU32 PaperColor(ImU32 color) { return OnPaper(color, Paper()); }
// Un texte de la palette, posé sur le papier et gardé lisible.
[[nodiscard]] inline ImU32 PaperInk(ImU32 color) {
  return ReadableOn(OnPaper(color, Paper()), Paper());
}
// Le texte courant : le résumé d'une ligne, le corps du détail.
[[nodiscard]] inline ImU32 BodyInk() { return PaintingLook().paper_text; }
// Ce qui se dit en second : dates, comptes, textes vides.
[[nodiscard]] inline ImU32 FaintInk() { return ReadableOn(PaintingLook().faint_text, Paper()); }
// Un état : un objectif rempli (réussite), un temps qui presse (avertissement),
// une échéance passée (échec).
[[nodiscard]] inline ImU32 ToneInk(SkinTone tone) {
  return ReadableOn(PaintingLook().Tone(tone), Paper());
}
[[nodiscard]] inline quest_paint::BarColors PaperBar(const quest_paint::BarColors& colors) {
  return {.fill = PaperColor(colors.fill), .background = PaperColor(colors.background)};
}

// ── LES MESURES COMMUNES ────────────────────────────────────────────────────

// Le filtre qui laisse tout passer, celui de l'état par défaut.
inline constexpr std::string_view kAllCategory = "all";
// La colonne de la liste.
inline constexpr float kListWidth = 360.0F;
// Les tailles de texte : le titre d'une ligne, son résumé et ce qui l'entoure,
// la pastille ; le titre du détail et son corps.
inline constexpr float kRowTitleSize = 13.0F;
inline constexpr float kRowTextSize = 12.0F;
inline constexpr float kPillTextSize = 10.0F;
inline constexpr float kDetailTitleSize = 16.0F;
inline constexpr float kDetailBodySize = 13.0F;
// L'icône de catégorie, dans une ligne comme dans un en-tête de groupe.
inline constexpr float kIconWidth = 26.0F;
inline constexpr float kIconHeight = 29.0F;

// Une pastille : la marge autour de son texte, et l'arrondi de ses coins.
inline constexpr float kPillPaddingX = 4.0F;
inline constexpr float kPillPaddingY = 1.0F;
inline constexpr float kPillRounding = 3.0F;

[[nodiscard]] inline float PillWidth(const Painter& paint, std::string_view text, float size) {
  return paint.TextWidth(text, size) + 2.0F * kPillPaddingX;
}
[[nodiscard]] inline float PillHeight(const Painter& paint, float size) {
  return paint.TextHeight(size) + 2.0F * kPillPaddingY;
}

// UNE PASTILLE : un aplat arrondi, son texte dedans. Rend sa largeur.
inline float PaintPill(const Painter& paint, float x, float y, std::string_view text, float size,
                       ImU32 fill, ImU32 ink) {
  const float width = PillWidth(paint, text, size);
  paint.into()->AddRectFilled(paint.At(x, y), paint.At(x + width, y + PillHeight(paint, size)),
                              fill, kPillRounding * paint.scale());
  paint.Text(x + kPillPaddingX, y + kPillPaddingY, text, size, ink);
  return width;
}

// LA PLACE D'UNE IMAGE RAMENÉE DANS UNE BOÎTE, proportions gardées, centrée :
// ses deux coins d'écran.
[[nodiscard]] inline std::pair<ImVec2, ImVec2> FittedCorners(const Painter& paint,
                                                             const SkinImage& image, float x,
                                                             float y, float width, float height) {
  const float ratio = std::min(width / image.width, height / image.height);
  const float shown_width = std::floor(image.width * ratio);
  const float shown_height = std::floor(image.height * ratio);
  const float left = x + std::floor((width - shown_width) * 0.5F);
  const float top = y + std::floor((height - shown_height) * 0.5F);
  return {paint.At(left, top), paint.At(left + shown_width, top + shown_height)};
}

// UNE IMAGE RAMENÉE DANS UNE BOÎTE. Absente, rien.
inline void PaintFitted(const Painter& paint, const SkinImage& image, float x, float y, float width,
                        float height) {
  if (!image.usable() || width <= 0.0F || height <= 0.0F) {
    return;
  }
  const auto [from, to] = FittedCorners(paint, image, x, y, width, height);
  paint.into()->AddImage(image.id, from, to);
}

// La même, peinte sur du blanc, posée sur le papier : voir `PaintOnPaper`.
inline void PaintFittedOnPaper(const Painter& paint, const SkinImage& image, float x, float y,
                               float width, float height) {
  if (!image.usable() || width <= 0.0F || height <= 0.0F) {
    return;
  }
  const auto [from, to] = FittedCorners(paint, image, x, y, width, height);
  PaintOnPaper(paint.into(), image, from, to, Paper());
}

// ── CE QUE LES VOLETS DEMANDENT À LA FENÊTRE ────────────────────────────────
//
// 🔴 UN SURGISSANT S'OUVRE AU NIVEAU DE LA FENÊTRE, jamais dans un volet : son
// identité se hache avec la pile d'ids du moment, et la boîte qui le dessine
// vit au niveau de la fenêtre. Les volets ARMENT, la fenêtre ouvre.
struct PaneRequests {
  // Le menu d'une récompense, que son clic droit arme : celui de la fenêtre.
  ItemLinkMenu& reward_menu;
  // « Tout oublier » cliqué : la confirmation s'ouvre.
  bool forget_all_asked = false;
};

// LA COLONNE DE GAUCHE : filtres, groupes, lignes ou historique.
void DrawListColumn(QuestWindowState& state, const QuestWindowView& view, const QuestArt& art,
                    const SkinArt& skin, float height, QuestWindowAction& action,
                    PaneRequests& requests);

// LE DÉTAIL de la quête choisie, ou le filigrane quand rien ne l'est.
void DrawDetail(const QuestWindowView& view, const QuestArt& art, const SkinArt& skin,
                QuestWindowAction& action, PaneRequests& requests);

// Le texte d'une vue, terminé par un zéro pour les fonctions d'ImGui.
[[nodiscard]] inline std::string Owned(std::string_view text) { return std::string(text); }

}  // namespace ro::ui::quest_window
