#include "ro/render/screen_text.h"

#include <cstddef>

#include "ro/core/text.h"

namespace ro::render {
namespace {

using core::NextCodePoint;

}  // namespace

float MeasureText(const FontAtlas& atlas, std::string_view utf8) {
  float width = 0.0F;
  std::size_t at = 0;
  while (at < utf8.size()) {
    const char32_t code = NextCodePoint(utf8, at);
    if (const FontAtlas::Glyph* glyph = atlas.GlyphFor(code); glyph != nullptr) {
      width += glyph->advance;
    }
  }
  return width;
}

std::vector<ScreenQuad> ComposeText(const FontAtlas& atlas, std::string_view utf8,
                                    glm::vec2 baseline_px,
                                    const std::array<std::uint8_t, 4>& color) {
  std::vector<ScreenQuad> quads;
  const formats::Image& image = atlas.image();
  if (image.width <= 0 || image.height <= 0) {
    return quads;
  }
  const auto atlas_width = static_cast<float>(image.width);
  const auto atlas_height = static_cast<float>(image.height);

  glm::vec2 pen = baseline_px;
  std::size_t at = 0;
  while (at < utf8.size()) {
    const char32_t code = NextCodePoint(utf8, at);
    const FontAtlas::Glyph* glyph = atlas.GlyphFor(code);
    if (glyph == nullptr) {
      continue;
    }
    const glm::vec2 size = glyph->max - glyph->min;
    // Un blanc n'a pas d'image, mais il fait avancer l'écriture.
    if (size.x > 0.0F && size.y > 0.0F) {
      ScreenQuad quad;
      // ARRONDI AU PIXEL. Un glyphe posé à une abscisse fractionnaire est
      // rééchantillonné, et le texte devient flou — d'autant plus que la
      // position vient d'une projection, donc d'un zoom. La police est gravée
      // pour être vue pixel pour pixel : la poser ailleurs qu'à un pixel entier
      // gâche la gravure.
      quad.min = glm::round(pen + glyph->bearing);
      quad.max = quad.min + size;
      quad.atlas = &image;
      quad.uv_min = glm::vec2{glyph->min.x / atlas_width, glyph->min.y / atlas_height};
      quad.uv_max = glm::vec2{glyph->max.x / atlas_width, glyph->max.y / atlas_height};
      quad.tint = color;
      quads.push_back(quad);
    }
    pen.x += glyph->advance;
  }
  return quads;
}

namespace {

// La ligne et son ombre, celle-ci pouvant être absente.
[[nodiscard]] std::vector<ScreenQuad> ComposeShadowedLine(
    const FontAtlas& atlas, std::string_view utf8, glm::vec2 baseline_px,
    const std::array<std::uint8_t, 4>& color, const std::array<std::uint8_t, 4>& shadow_color) {
  // Le point de départ est arrondi : poser une ligne sur un demi-pixel la
  // décale tout entière, et l'arrondi de chaque glyphe ne rattraperait pas le
  // flou.
  const glm::vec2 left = glm::round(baseline_px);
  std::vector<ScreenQuad> quads;
  // L'ombre d'abord : elle passe SOUS le texte, et il suffit qu'elle soit
  // décalée d'un pixel vers le bas et la droite pour détacher une ligne du sol.
  // ⚠ RIEN DU TOUT quand elle est transparente : voir l'en-tête.
  if (shadow_color[3] != 0) {
    quads = ComposeText(atlas, utf8, left + glm::vec2{kTextShadowPixels, kTextShadowPixels},
                        shadow_color);
  }
  for (ScreenQuad& quad : ComposeText(atlas, utf8, left, color)) {
    quads.push_back(quad);
  }
  return quads;
}

}  // namespace

std::vector<ScreenQuad> ComposeLabel(const FontAtlas& atlas, std::string_view utf8,
                                     glm::vec2 baseline_px,
                                     const std::array<std::uint8_t, 4>& color) {
  return ComposeShadowedLine(atlas, utf8, baseline_px, color, kNameShadowColor);
}

std::vector<ScreenQuad> ComposeLabel(const FontAtlas& atlas, std::string_view utf8,
                                     glm::vec2 baseline_px,
                                     const std::array<std::uint8_t, 4>& color,
                                     const std::array<std::uint8_t, 4>& shadow_color) {
  return ComposeShadowedLine(atlas, utf8, baseline_px, color, shadow_color);
}

std::vector<ScreenQuad> ComposeCenteredLabel(const FontAtlas& atlas, std::string_view utf8,
                                             glm::vec2 center_baseline_px,
                                             const std::array<std::uint8_t, 4>& color) {
  return ComposeCenteredLabel(atlas, utf8, center_baseline_px, color, kNameShadowColor);
}

std::vector<ScreenQuad> ComposeCenteredLabel(const FontAtlas& atlas, std::string_view utf8,
                                             glm::vec2 center_baseline_px,
                                             const std::array<std::uint8_t, 4>& color,
                                             const std::array<std::uint8_t, 4>& shadow_color) {
  const float width = MeasureText(atlas, utf8);
  return ComposeShadowedLine(atlas, utf8,
                             glm::vec2{center_baseline_px.x - width * 0.5F, center_baseline_px.y},
                             color, shadow_color);
}

std::vector<std::string_view> WrapTextLines(const FontAtlas& atlas, std::string_view utf8,
                                            float most_width_px) {
  std::vector<std::string_view> lines;
  if (utf8.empty()) {
    return lines;
  }
  if (most_width_px <= 0.0F || MeasureText(atlas, utf8) <= most_width_px) {
    lines.push_back(utf8);
    return lines;
  }
  std::size_t start = 0;
  while (start < utf8.size()) {
    // Le plus long morceau qui tienne, en s'arrêtant à chaque espace. `taken`
    // garde le dernier qui passait ; zéro veut dire qu'aucun n'a tenu.
    std::size_t taken = 0;
    std::size_t at = utf8.find(' ', start);
    while (at != std::string_view::npos) {
      if (MeasureText(atlas, utf8.substr(start, at - start)) > most_width_px) {
        break;
      }
      taken = at - start;
      at = utf8.find(' ', at + 1);
    }
    if (at == std::string_view::npos && MeasureText(atlas, utf8.substr(start)) <= most_width_px) {
      taken = utf8.size() - start;
    }
    if (taken == 0) {
      // 🔴 AUCUN ESPACE N'A SUFFI : on coupe DANS le mot. Un mot plus
      // large que la bulle existe, et le laisser passer annulerait la borne.
      // Au moins un octet, sinon la boucle ne finirait jamais.
      taken = 1;
      while (start + taken < utf8.size() &&
             MeasureText(atlas, utf8.substr(start, taken + 1)) <= most_width_px) {
        ++taken;
      }
    }
    lines.push_back(utf8.substr(start, taken));
    start += taken;
    // L'espace de coupe ne commence pas la ligne suivante.
    while (start < utf8.size() && utf8[start] == ' ') {
      ++start;
    }
  }
  return lines;
}

std::vector<ScreenQuad> ComposeCenteredParagraph(const FontAtlas& atlas, std::string_view utf8,
                                                 glm::vec2 center_baseline_px, float most_width_px,
                                                 const std::array<std::uint8_t, 4>& color) {
  const std::vector<std::string_view> lines = WrapTextLines(atlas, utf8, most_width_px);
  std::vector<ScreenQuad> quads;
  for (std::size_t rank = 0; rank < lines.size(); ++rank) {
    // La DERNIÈRE ligne tombe sur la ligne de base donnée ; les précédentes
    // s'empilent au-dessus.
    const float above = static_cast<float>(lines.size() - 1 - rank) * atlas.line_height();
    for (const ScreenQuad& quad : ComposeCenteredLabel(
             atlas, lines[rank], glm::vec2{center_baseline_px.x, center_baseline_px.y - above},
             color)) {
      quads.push_back(quad);
    }
  }
  return quads;
}

}  // namespace ro::render
