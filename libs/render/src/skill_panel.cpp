#include "ro/render/skill_panel.h"

#include <algorithm>
#include <cstdint>

#include "ro/render/screen_text.h"

namespace ro::render {
namespace {

// L'épaisseur du cadre, en pixels.
constexpr float kBorderPixels = 1.0F;

ScreenQuad SolidQuad(glm::vec2 min, glm::vec2 max, const std::array<std::uint8_t, 4>& color) {
  ScreenQuad quad;
  quad.min = min;
  quad.max = max;
  quad.tint = color;
  return quad;
}

// Le haut de la zone des lignes : sous le bandeau de titre.
float RowsTop(const SkillPanelLayout& layout) { return layout.origin.y + kSkillPanelTitlePixels; }

void Append(std::vector<ScreenQuad>& into, std::vector<ScreenQuad> what) {
  into.insert(into.end(), what.begin(), what.end());
}

}  // namespace

SkillPanelLayout SkillPanelPlacement(float viewport_width, float viewport_height,
                                     std::size_t row_count) {
  SkillPanelLayout layout;
  // Ce qui reste à la liste une fois le bandeau et les deux marges retirés.
  const float free_height =
      viewport_height - 2.0F * kSkillPanelMarginPixels - kSkillPanelTitlePixels;
  const auto fits = free_height <= 0.0F
                        ? std::size_t{0}
                        : static_cast<std::size_t>(free_height / kSkillPanelRowPixels);
  layout.rows_shown = std::min(row_count, fits);
  layout.origin = glm::vec2{viewport_width - kSkillPanelMarginPixels - kSkillPanelWidthPixels,
                            kSkillPanelMarginPixels};
  layout.size = glm::vec2{
      kSkillPanelWidthPixels,
      kSkillPanelTitlePixels + static_cast<float>(layout.rows_shown) * kSkillPanelRowPixels};
  return layout;
}

bool SkillPanelHolds(const SkillPanelLayout& layout, glm::vec2 point_px) {
  return point_px.x >= layout.origin.x && point_px.x < layout.origin.x + layout.size.x &&
         point_px.y >= layout.origin.y && point_px.y < layout.origin.y + layout.size.y;
}

std::optional<std::size_t> SkillPanelRowAt(const SkillPanelLayout& layout, glm::vec2 point_px) {
  if (!SkillPanelHolds(layout, point_px)) {
    return std::nullopt;
  }
  const float from_top = point_px.y - RowsTop(layout);
  if (from_top < 0.0F) {
    // Le bandeau de titre : il tient le clic sans désigner de ligne.
    return std::nullopt;
  }
  const auto row = static_cast<std::size_t>(from_top / kSkillPanelRowPixels);
  if (row >= layout.rows_shown) {
    return std::nullopt;
  }
  return row;
}

std::vector<ScreenQuad> ComposeSkillPanel(const FontAtlas& font, const SkillPanelLayout& layout,
                                          std::string_view title,
                                          std::span<const SkillPanelRow> rows,
                                          std::size_t first_row) {
  std::vector<ScreenQuad> quads;
  const glm::vec2 corner = layout.origin + layout.size;

  // Le cadre est un rectangle plein sous les autres : c'est trois quads de
  // moins qu'une bordure faite de quatre bandes, et le résultat est le même.
  quads.push_back(SolidQuad(layout.origin - glm::vec2{kBorderPixels, kBorderPixels},
                            corner + glm::vec2{kBorderPixels, kBorderPixels},
                            kSkillPanelBorderColor));
  quads.push_back(SolidQuad(layout.origin, corner, kSkillPanelBackColor));
  quads.push_back(SolidQuad(layout.origin,
                            glm::vec2{corner.x, layout.origin.y + kSkillPanelTitlePixels},
                            kSkillPanelTitleColor));

  // La ligne de base d'une ligne : le texte s'écrit sur elle, et la police
  // monte au-dessus. Centrer sur la hauteur de ligne plutôt que de coller au
  // haut évite qu'un accent touche la ligne précédente.
  const float text_left = layout.origin.x + kSkillPanelPaddingPixels;
  const float title_baseline = layout.origin.y + (kSkillPanelTitlePixels + font.ascent()) * 0.5F;
  Append(quads,
         ComposeLabel(font, title, glm::vec2{text_left, title_baseline}, kSkillPanelTextColor));

  for (std::size_t shown = 0; shown < layout.rows_shown; ++shown) {
    const std::size_t index = first_row + shown;
    if (index >= rows.size()) {
      break;
    }
    const SkillPanelRow& row = rows[index];
    const float top = RowsTop(layout) + static_cast<float>(shown) * kSkillPanelRowPixels;
    if (row.armed) {
      quads.push_back(SolidQuad(glm::vec2{layout.origin.x, top},
                                glm::vec2{corner.x, top + kSkillPanelRowPixels},
                                kSkillPanelArmedColor));
    }
    const float baseline = top + (kSkillPanelRowPixels + font.ascent()) * 0.5F;
    const std::array<std::uint8_t, 4>& color =
        row.passive ? kSkillPanelPassiveColor : kSkillPanelTextColor;
    Append(quads, ComposeLabel(font, row.label, glm::vec2{text_left, baseline}, color));
    if (!row.detail.empty()) {
      // Le détail se cale à DROITE : les rangs et les coûts s'alignent alors
      // les uns sous les autres, quelle que soit la longueur des noms.
      const float detail_left = corner.x - kSkillPanelPaddingPixels - MeasureText(font, row.detail);
      Append(quads, ComposeLabel(font, row.detail, glm::vec2{detail_left, baseline}, color));
    }
  }
  return quads;
}

std::size_t SkillPanelScrolled(const SkillPanelLayout& layout, std::size_t row_count,
                               std::size_t first_row, float wheel_steps) {
  if (row_count <= layout.rows_shown) {
    return 0;
  }
  const std::size_t last_start = row_count - layout.rows_shown;
  // Le compte se fait SIGNÉ : remonter au-delà du premier rang déborderait par
  // le bas d'un compteur non signé et sauterait à la fin de la liste.
  const auto from = static_cast<std::int64_t>(first_row);
  const auto steps = static_cast<std::int64_t>(wheel_steps);
  const std::int64_t moved =
      std::clamp<std::int64_t>(from - steps, 0, static_cast<std::int64_t>(last_start));
  return static_cast<std::size_t>(moved);
}

}  // namespace ro::render
