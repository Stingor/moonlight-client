// Le score du champ de bataille et la marque des ennemis. Voir l'en-tête.

#include "ro/render/battle_score_hud.h"

#include <algorithm>
#include <array>

namespace ro::render {
namespace {

// Une seule image par action dans la fonte : l'action porte le chiffre.
constexpr std::size_t kOnlyFrame = 0;
constexpr std::int32_t kDecimalBase = 10;
constexpr std::uint8_t kOpaque = 255;

// LE SURLIGNAGE DU CAMP DU JOUEUR : un cartouche translucide qui déborde un
// peu de la rangée, derrière elle.
constexpr float kHighlightMarginPx = 4.0F;
constexpr float kHighlightCornerRadiusPx = 6.0F;
constexpr std::array<std::uint8_t, 4> kHighlightTint = {255, 214, 90, 72};

struct ScoreRow {
  std::int32_t score = 0;
  const formats::Image* emblem = nullptr;
  float emblem_top_px = 0.0F;
  float emblem_bottom_px = 0.0F;
  float digits_top_px = 0.0F;
};

void Append(std::vector<ScreenQuad>& into, const std::vector<ScreenQuad>& from) {
  into.insert(into.end(), from.begin(), from.end());
}

// L'emblème puis les chiffres d'une rangée : les unités à leur colonne, les
// dizaines un pas à leur gauche, et pas de zéro de tête.
[[nodiscard]] std::vector<ScreenQuad> ComposeRow(const formats::SprSprite& digits_sprite,
                                                 const formats::ActAnimation& digits,
                                                 const ScoreRow& row, float viewport_w,
                                                 float units_x) {
  std::vector<ScreenQuad> quads;
  if (!row.emblem->empty()) {
    quads.push_back(
        ScreenQuad{.min = {viewport_w - kBattleEmblemLeftFromRightPx, row.emblem_top_px},
                   .max = {viewport_w - kBattleEmblemRightFromRightPx, row.emblem_bottom_px},
                   .atlas = row.emblem,
                   .tint = {kOpaque, kOpaque, kOpaque, kBattleEmblemAlpha}});
  }
  const auto units = static_cast<std::size_t>(row.score % kDecimalBase);
  Append(quads, ComposeScreenSprite(digits_sprite, digits, units, kOnlyFrame,
                                    glm::vec2{units_x, row.digits_top_px}));
  if (row.score >= kDecimalBase) {
    const auto tens = static_cast<std::size_t>(row.score / kDecimalBase);
    Append(quads, ComposeScreenSprite(digits_sprite, digits, tens, kOnlyFrame,
                                      glm::vec2{units_x - kBattleDigitPitchPx, row.digits_top_px}));
  }
  return quads;
}

// Le cartouche qui couvre une rangée, ou rien si elle n'a rien dessiné.
[[nodiscard]] std::vector<ScreenQuad> ComposeHighlight(const std::vector<ScreenQuad>& row_quads) {
  if (row_quads.empty()) return {};
  glm::vec2 low = row_quads.front().min;
  glm::vec2 high = row_quads.front().max;
  for (const ScreenQuad& quad : row_quads) {
    low = glm::min(low, quad.min);
    high = glm::max(high, quad.max);
  }
  const glm::vec2 margin{kHighlightMarginPx, kHighlightMarginPx};
  return ComposeRoundedRect(low - margin, high + margin, kHighlightCornerRadiusPx, kHighlightTint);
}

}  // namespace

std::vector<ScreenQuad> ComposeBattleScore(const formats::SprSprite& digits_sprite,
                                           const formats::ActAnimation& digits,
                                           const BattlefieldArt& art,
                                           const BattleScoreFigures& figures,
                                           glm::vec2 viewport_px) {
  if (figures.lion < 0 || figures.eagle < 0) return {};
  const ScoreRow lion{.score = std::min(figures.lion, kBattleScoreMost),
                      .emblem = &art.lion,
                      .emblem_top_px = kLionEmblemTopPx,
                      .emblem_bottom_px = kLionEmblemBottomPx,
                      .digits_top_px = kLionDigitsTopPx};
  const ScoreRow eagle{.score = std::min(figures.eagle, kBattleScoreMost),
                       .emblem = &art.eagle,
                       .emblem_top_px = kEagleEmblemTopPx,
                       .emblem_bottom_px = kEagleEmblemBottomPx,
                       .digits_top_px = kEagleDigitsTopPx};
  // UNE SEULE COLONNE D'UNITÉS POUR LES DEUX RANGÉES : dès qu'un score a deux
  // chiffres, les deux unités se décalent ensemble et restent alignées.
  const bool wide = lion.score >= kDecimalBase || eagle.score >= kDecimalBase;
  const float units_x =
      viewport_px.x - (wide ? kBattleUnitsWideFromRightPx : kBattleUnitsFromRightPx);

  std::vector<ScreenQuad> lion_quads =
      ComposeRow(digits_sprite, digits, lion, viewport_px.x, units_x);
  std::vector<ScreenQuad> eagle_quads =
      ComposeRow(digits_sprite, digits, eagle, viewport_px.x, units_x);
  std::vector<ScreenQuad> quads;
  // Le cartouche passe EN PREMIER : il se peint sous la rangée qu'il désigne.
  if (figures.highlight == BattleScoreHighlight::kLion) Append(quads, ComposeHighlight(lion_quads));
  if (figures.highlight == BattleScoreHighlight::kEagle) {
    Append(quads, ComposeHighlight(eagle_quads));
  }
  Append(quads, lion_quads);
  Append(quads, eagle_quads);
  return quads;
}

ScreenQuad ComposeEnemyMarker(const formats::Image& marker, glm::vec2 feet_px,
                              glm::vec2 viewport_px, float height_scale) {
  if (marker.empty()) return ScreenQuad{};
  // LE DÉCALAGE SUIT L'ÉCRAN : une fraction de sa largeur et de sa hauteur,
  // puis la hauteur du sprite. L'image, elle, garde sa taille de fichier.
  const glm::vec2 shift{kEnemyMarkerShiftXPx / kEnemyMarkerReferenceWidthPx * viewport_px.x,
                        kEnemyMarkerShiftYPx / kEnemyMarkerReferenceHeightPx * viewport_px.y};
  const glm::vec2 top_left = feet_px + shift * height_scale;
  const glm::vec2 size{static_cast<float>(marker.width), static_cast<float>(marker.height)};
  return ScreenQuad{.min = top_left, .max = top_left + size, .atlas = &marker};
}

}  // namespace ro::render
