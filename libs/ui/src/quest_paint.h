#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>

#include <imgui.h>

#include "achievement_paint.h"

// LES BRIQUES DE PEINTURE COMMUNES AU JOURNAL DES QUÊTES ET AU PANNEAU DE SUIVI.
//
// En-tête PRIVÉ. Les mesures sont en PIXELS D'ART, comme celles du peintre des
// succès dont elles reprennent la règle : l'échelle s'applique à chaque
// coordonnée.
namespace ro::ui::quest_paint {

using achievement::Painter;

// « … », ce qu'un texte coupé montre à sa place.
inline constexpr std::string_view kEllipsis = "\xE2\x80\xA6";

// LE TEXTE RAMENÉ À `width` pixels d'art : entier s'il tient, sinon coupé à un
// caractère entier et suivi de « … ». Rien ne tient : vide.
[[nodiscard]] inline std::string FitToWidth(const Painter& paint, std::string_view text, float size,
                                            bool bold, float width) {
  // ⚠ UNE VUE VIDE PEUT N'AVOIR AUCUN OCTET : mesurée, ImGui en chercherait
  // la fin à partir d'un pointeur nul.
  if (text.empty()) {
    return {};
  }
  if (paint.TextWidth(text, size, bold) <= width) {
    return std::string(text);
  }
  const float room = width - paint.TextWidth(kEllipsis, size, bold);
  if (room <= 0.0F) {
    return {};
  }
  // ⚠ COUPÉ À UN CARACTÈRE ENTIER : un octet de tête seul se dessinerait en
  // carré « manquant ».
  std::size_t kept = 0;
  std::size_t next = 0;
  while (next < text.size()) {
    const std::size_t step = achievement::Utf8Length(text[next]);
    const std::size_t candidate = std::min(text.size(), next + step);
    if (paint.TextWidth(text.substr(0, candidate), size, bold) > room) {
      break;
    }
    kept = candidate;
    next = candidate;
  }
  std::string fitted(text.substr(0, kept));
  // Un blanc avant les points de suspension ne se voit pas : il s'efface.
  while (!fitted.empty() && fitted.back() == ' ') {
    fitted.pop_back();
  }
  fitted.append(kEllipsis);
  return fitted;
}

// LE TRIANGLE DE PLIAGE, dans une boîte de `side` pixels d'art : pointé à
// droite replié, vers le bas déplié. `outline` à zéro : sans contour.
//
// ⚠ DESSINÉ ET NON ÉCRIT : les triangles « ▶ ▼ » ne sont pas dans toutes les
// polices qu'un joueur peut choisir, et un losange de remplacement ne dit plus
// dans quel sens on plie.
inline constexpr float kFoldMarkNear = 0.2F;
inline constexpr float kFoldMarkFar = 0.8F;
inline constexpr float kFoldMarkMiddle = 0.5F;
inline constexpr float kFoldMarkOutline = 1.0F;

inline void PaintFoldMark(const Painter& paint, float x, float y, float side, bool folded,
                          ImU32 fill, ImU32 outline) {
  const auto at = [&](float across, float down) {
    return ImVec2(paint.At(x, y).x + across * side * paint.scale(),
                  paint.At(x, y).y + down * side * paint.scale());
  };
  const ImVec2 first = at(kFoldMarkNear, kFoldMarkNear);
  const ImVec2 second = folded ? at(kFoldMarkNear, kFoldMarkFar) : at(kFoldMarkFar, kFoldMarkNear);
  const ImVec2 tip = folded ? at(kFoldMarkFar, kFoldMarkMiddle) : at(kFoldMarkMiddle, kFoldMarkFar);
  paint.into()->AddTriangleFilled(first, second, tip, fill);
  if (outline != 0) {
    paint.into()->AddTriangle(first, second, tip, outline, kFoldMarkOutline * paint.scale());
  }
}

// LES COULEURS D'UNE BARRE D'AVANCÉE.
struct BarColors {
  ImU32 fill = 0;
  ImU32 background = 0;
};

// UNE BARRE FINE : son fond, puis la part remplie. `fraction` est bornée à
// [0, 1] ; négative, rien n'est dessiné.
inline void PaintBar(const Painter& paint, float x, float y, float width, float height,
                     float fraction, const BarColors& colors) {
  if (fraction < 0.0F || width <= 0.0F || height <= 0.0F) {
    return;
  }
  paint.Fill(x, y, width, height, colors.background);
  paint.Fill(x, y, std::floor(width * std::clamp(fraction, 0.0F, 1.0F)), height, colors.fill);
}

}  // namespace ro::ui::quest_paint
