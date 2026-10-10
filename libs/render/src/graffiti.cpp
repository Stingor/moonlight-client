#include "ro/render/graffiti.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>

namespace ro::render {
namespace {

// LA PLANCHE : des glyphes carrés, huit par rangée.
constexpr std::int32_t kGlyphSide = 32;
constexpr std::int32_t kGlyphsPerRow = 8;

// LA LARGEUR ÉTROITE, celle de « ? », de « ! » et des signes que la planche
// n'a pas.
constexpr std::int32_t kNarrowWidth = 16;

// LE RANG DES GLYPHES : les majuscules d'abord, puis les minuscules, puis les
// chiffres six rangs après « 0 » de l'ASCII.
constexpr std::int32_t kFirstLowercaseGlyph = 26;
constexpr std::int32_t kFirstDigitGlyph = 54;
constexpr std::int32_t kQuestionGlyph = 53;
constexpr std::int32_t kExclamationGlyph = 54;

// LA MISE EN PAGE : la hauteur d'une ligne, la largeur où l'on passe à la
// suivante, et la première ligne, d'autant plus basse que le message est court.
constexpr std::int32_t kLineHeight = 36;
constexpr std::int32_t kLineWidth = 352;
constexpr std::int32_t kTopLineReference = 144;
constexpr std::int32_t kCharactersPerLineStep = 24;

constexpr std::size_t kChannels = 4;

// UN SIGNE : son glyphe s'il en a un, et sa largeur.
struct Sign {
  std::optional<std::int32_t> glyph;
  std::int32_t width = 0;
};

[[nodiscard]] Sign SignOf(char character) {
  if (character >= 'a' && character <= 'z') {
    return {kFirstLowercaseGlyph + (character - 'a'), kGlyphSide};
  }
  if (character >= 'A' && character <= 'Z') {
    return {character - 'A', kGlyphSide};
  }
  if (character >= '0' && character <= '9') {
    return {kFirstDigitGlyph + (character - '0'), kGlyphSide};
  }
  if (character == '?') {
    return {kQuestionGlyph, kNarrowWidth};
  }
  if (character == '!') {
    return {kExclamationGlyph, kNarrowWidth};
  }
  return {std::nullopt, kNarrowWidth};
}

// COPIE UN GLYPHE dans la toile, étiré à la largeur voulue, transparence
// comprise. Ce qui déborde de la toile est perdu.
void Stamp(const formats::Image& alphabet, std::int32_t glyph, std::int32_t x, std::int32_t y,
           std::int32_t width, formats::Image& canvas) {
  const std::int32_t source_x = kGlyphSide * (glyph % kGlyphsPerRow);
  const std::int32_t source_y = kGlyphSide * (glyph / kGlyphsPerRow);
  for (std::int32_t row = 0; row < kGlyphSide; ++row) {
    const std::int32_t from_y = source_y + row;
    const std::int32_t to_y = y + row;
    if (from_y >= alphabet.height || to_y < 0 || to_y >= canvas.height) {
      continue;
    }
    for (std::int32_t column = 0; column < width; ++column) {
      const std::int32_t from_x = source_x + column * kGlyphSide / width;
      const std::int32_t to_x = x + column;
      if (from_x >= alphabet.width || to_x < 0 || to_x >= canvas.width) {
        continue;
      }
      const std::size_t from =
          (static_cast<std::size_t>(from_y) * static_cast<std::size_t>(alphabet.width) +
           static_cast<std::size_t>(from_x)) *
          kChannels;
      const std::size_t to =
          (static_cast<std::size_t>(to_y) * static_cast<std::size_t>(canvas.width) +
           static_cast<std::size_t>(to_x)) *
          kChannels;
      for (std::size_t channel = 0; channel < kChannels; ++channel) {
        canvas.pixels[to + channel] = alphabet.pixels[from + channel];
      }
    }
  }
}

}  // namespace

formats::Image ComposeGraffitiCanvas(const formats::Image& alphabet, std::string_view message) {
  formats::Image canvas;
  canvas.width = kGraffitiCanvasSide;
  canvas.height = kGraffitiCanvasSide;
  canvas.pixels.assign(static_cast<std::size_t>(kGraffitiCanvasSide) *
                           static_cast<std::size_t>(kGraffitiCanvasSide) * kChannels,
                       0);
  const auto length = static_cast<std::int32_t>(message.size());
  std::int32_t x = 0;
  std::int32_t y = kLineHeight * ((kTopLineReference - length) / kCharactersPerLineStep);
  // ⚠ LA LARGEUR DU SIGNE PRÉCÉDENT, que l'espace reprend. Le client la laisse
  // sans valeur avant le premier signe : une espace en tête n'avance ici de rien.
  std::int32_t previous_width = 0;
  for (std::size_t index = 0; index < message.size(); ++index) {
    const char character = message[index];
    std::int32_t width = 0;
    std::optional<std::int32_t> glyph;
    if (character == ' ') {
      // LE MOT QUI SUIT TIENT-IL DANS LA LIGNE ? Chacune de ses lettres compte
      // trente-deux pixels, quelle que soit sa largeur réelle.
      std::int32_t probe = x;
      for (std::size_t next = index + 1; next < message.size() && message[next] != ' '; ++next) {
        probe += kGlyphSide;
      }
      if (probe >= kLineWidth) {
        y += kLineHeight;
        x = 0;
      }
      width = previous_width;
    } else {
      const Sign sign = SignOf(character);
      glyph = sign.glyph;
      width = sign.width;
      previous_width = width;
    }
    if (x + width >= kLineWidth) {
      y += kLineHeight;
      x = 0;
    }
    if (glyph.has_value() && !alphabet.empty()) {
      Stamp(alphabet, *glyph, x, y, width, canvas);
    }
    x += width;
  }
  return canvas;
}

std::vector<EffectGroundCellPaint> ComposeGraffitiCells(float body_degrees) {
  // ⚠ LE CLIENT PASSE EN RADIANS PAR SON PROPRE PI, à six décimales.
  constexpr float kClientPi = 3.141592F;
  constexpr float kDegreesPerHalfTurn = 180.0F;
  const float radians = body_degrees / kDegreesPerHalfTurn * kClientPi;
  const float sine = std::sin(radians);
  const float cosine = std::cos(radians);
  const auto side = static_cast<float>(kGraffitiGridSide);
  const auto half = static_cast<float>(kGraffitiGridHalf);
  std::vector<EffectGroundCellPaint> cells;
  cells.reserve(static_cast<std::size_t>(kGraffitiGridSide * kGraffitiGridSide));
  for (std::int32_t row = 0; row < kGraffitiGridSide; ++row) {
    for (std::int32_t column = 0; column < kGraffitiGridSide; ++column) {
      EffectGroundCellPaint cell;
      cell.cell_offset_x = column - kGraffitiGridHalf;
      cell.cell_offset_y = row - kGraffitiGridHalf;
      for (std::size_t corner = 0; corner < cell.texture_uv.size(); ++corner) {
        // L'ÉCART DU COIN au centre de la grille, en cases, tourné puis ramené
        // dans la toile. L'abscisse de la toile court à rebours de la grille.
        const float dx = static_cast<float>(cell.cell_offset_x) + (corner % 2 == 1 ? 1.0F : 0.0F);
        const float dy = static_cast<float>(cell.cell_offset_y) + (corner >= 2 ? 1.0F : 0.0F);
        cell.texture_uv[corner] = {1.0F - (dx * cosine + dy * sine + half) / side,
                                   (dy * cosine - dx * sine + half) / side};
      }
      cells.push_back(cell);
    }
  }
  return cells;
}

std::int32_t FlagGraffitiSide(std::int32_t level) { return 2 * std::max(level, 1) + 1; }

std::vector<EffectGroundCellPaint> ComposeFlagGraffitiCells(std::int32_t level) {
  const std::int32_t side = FlagGraffitiSide(level);
  const std::int32_t half = side / 2;
  const auto extent = static_cast<float>(side);
  std::vector<EffectGroundCellPaint> cells;
  cells.reserve(static_cast<std::size_t>(side * side));
  for (std::int32_t row = 0; row < side; ++row) {
    for (std::int32_t column = 0; column < side; ++column) {
      EffectGroundCellPaint cell;
      cell.cell_offset_x = column - half;
      cell.cell_offset_y = row - half;
      for (std::size_t corner = 0; corner < cell.texture_uv.size(); ++corner) {
        // LE HAUT DE L'IMAGE AU NORD : l'ordonnée de cellule croît vers le
        // nord, celle de l'image vers le bas.
        const float x = static_cast<float>(column) + (corner % 2 == 1 ? 1.0F : 0.0F);
        const float y = static_cast<float>(row) + (corner >= 2 ? 1.0F : 0.0F);
        cell.texture_uv[corner] = {x / extent, 1.0F - y / extent};
      }
      cells.push_back(cell);
    }
  }
  return cells;
}

}  // namespace ro::render
