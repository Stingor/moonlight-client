#pragma once

// LES PIÈCES COMMUNES DU COMPTEUR DE DÉGÂTS : la fenêtre
// (`damage_meter_window.cpp`) et le volet du palmarès
// (`damage_meter_records.cpp`) les partagent. Interne à `ui`.

#include <algorithm>
#include <cfloat>
#include <cmath>

#include <imgui.h>

#include "ro/ui/damage_meter_window.h"

namespace ro::ui::damage_meter {

[[nodiscard]] inline float Px(float pixels) { return pixels * ImGui::GetStyle().FontScaleMain; }

// Les tailles de texte, en multiples de la police courante. Le gros nombre se
// lit en périphérie, pendant qu'on regarde le combat et pas le compteur.
inline constexpr float kBigTextScale = 1.85F;
// Le « DPS » qui suit le nombre.
inline constexpr float kUnitTextScale = 0.80F;
// Les intitulés des tuiles du bas, et ceux du palmarès.
inline constexpr float kLabelTextScale = 0.72F;

// Gris des textes secondaires. Clair, parce que le fond peut être presque
// transparent : un gris moyen s'y effaçait.
inline constexpr ImU32 kSubColor = IM_COL32(200, 200, 205, 255);
inline constexpr ImU32 kQuietColor = IM_COL32(150, 150, 155, 210);
inline constexpr ImU32 kLabelColor = IM_COL32(165, 165, 172, 235);
inline constexpr ImU32 kTileColor = IM_COL32(255, 255, 255, 11);

// L'ombre portée du texte suit sa TAILLE : un décalage d'un pixel sous un
// nombre de vingt-huit ne se voit plus.
inline constexpr float kShadowPerPixel = 0.055F;
inline constexpr ImU32 kShadowColor = IM_COL32(0, 0, 0, 170);

inline constexpr float kTileGapPixels = 4.0F;
inline constexpr float kTilePadPixels = 3.0F;
inline constexpr float kTileRoundingPixels = 3.0F;

// La couleur du réglage, reprise à une autre opacité — c'est ce qui donne son
// unité de ton au compteur. `times` multiplie l'alpha choisi par le joueur.
[[nodiscard]] inline ImU32 Tint(ImU32 color, float times) {
  ImVec4 rgba = ImGui::ColorConvertU32ToFloat4(color);
  rgba.w = std::clamp(rgba.w * times, 0.0F, 1.0F);
  return ImGui::ColorConvertFloat4ToU32(rgba);
}

// 🔴 MESURER À LA TAILLE DE DESSIN, jamais à celle de la police courante :
// mesurer quinze pixels pour en dessiner vingt-huit décale tous les
// alignements à droite.
[[nodiscard]] inline float TextWidth(ImFont* font, float size, const char* text) {
  return font->CalcTextSizeA(size, FLT_MAX, 0.0F, text).x;
}

// Du texte posé à la main, avec son ombre portée : le compteur flotte au-dessus
// du monde, et sans ombre un chiffre clair disparaît sur un décor clair.
//
// 🔴 POSITION ARRONDIE : un glyphe à coordonnée fractionnaire est flou.
inline void PaintText(ImDrawList* into, ImFont* font, float size, ImVec2 at, ImU32 color,
                      const char* text) {
  const ImVec2 snapped(std::floor(at.x), std::floor(at.y));
  const float shadow = std::max(1.0F, size * kShadowPerPixel);
  into->AddText(font, size, ImVec2(snapped.x + shadow, snapped.y + shadow), kShadowColor, text);
  into->AddText(font, size, snapped, color, text);
}

// La police des chiffres : la grasse si l'appelant en a une.
[[nodiscard]] inline ImFont* NumberFont(const DamageMeterView& view) {
  return view.bold != nullptr ? view.bold : ImGui::GetFont();
}

// La hauteur du volet du palmarès pour tant de rangs, intitulé compris. ⚠
// REMPLI OU NON : voir `kDamageMeterRecordRows`.
[[nodiscard]] float RecordsHeight(std::size_t rows = kDamageMeterRecordRows);
// Combien de rangs tiennent dans cette hauteur, intitulé compris.
[[nodiscard]] std::size_t RecordRowsFitting(float height);

// Le volet du palmarès, à la largeur donnée. Il AVANCE le curseur de sa
// hauteur, comme les autres bandes.
void DrawRecords(const DamageMeterView& view, float width,
                 std::size_t rows = kDamageMeterRecordRows);

// ── LA VUE DU GROUPE — damage_meter_group.cpp ──────────────────────────────
// La barre d'outils : le mode, le segment, et la bascule « Moi | Groupe ».
[[nodiscard]] float ToolbarHeight();
void DrawToolbar(const DamageMeterView& view, float width, DamageMeterAction* asked);
// Les barres du groupe, ou le détail d'un acteur, dans cette place.
void DrawGroup(const DamageMeterView& view, ImVec2 size, DamageMeterAction* asked);

// Ce que le montant d'un mode veut dire, en toutes lettres et en chiffres
// exacts : « 12 345 soignés ».
[[nodiscard]] std::string AmountText(DamageMeterMode mode, std::int64_t amount);

}  // namespace ro::ui::damage_meter
