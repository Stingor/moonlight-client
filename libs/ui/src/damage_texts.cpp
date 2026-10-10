#include "ro/ui/damage_texts.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>

namespace ro::ui {
namespace {

// La taille à laquelle on MESURE le zéro d'une famille : assez grande pour que
// le rapport de sa hauteur à la taille de police ne dépende plus de l'arrondi
// au pixel.
constexpr float kMeasureFontPixels = 64.0F;

// En deçà, un chiffre ne se lit plus : on ne le grave pas. Au-delà, la police
// ne grossit plus — un nombre de la taille de l'écran n'est pas une lecture, et
// chaque taille gravée coûte une place dans l'atlas.
constexpr float kSmallestDigitPixels = 2.0F;
constexpr float kLargestFontPixels = 256.0F;

// Un cerne plus épais que cela s'ouvre entre ses huit copies : un second anneau,
// à mi-rayon, en bouche les brèches.
constexpr float kThinOutlinePixels = 2.0F;

// Les huit directions d'un cerne, à rayon un.
constexpr float kDiagonal = std::numbers::sqrt2_v<float> / 2.0F;
constexpr std::size_t kOutlineCopies = 8;
constexpr std::array<ImVec2, kOutlineCopies> kOutlineDirections = {
    ImVec2(1.0F, 0.0F),  ImVec2(kDiagonal, kDiagonal),
    ImVec2(0.0F, 1.0F),  ImVec2(-kDiagonal, kDiagonal),
    ImVec2(-1.0F, 0.0F), ImVec2(-kDiagonal, -kDiagonal),
    ImVec2(0.0F, -1.0F), ImVec2(kDiagonal, -kDiagonal)};

// Où écrire chaque glyphe pour que son dessin se centre sur sa place.
struct PlacedGlyphs {
  std::array<ImVec2, kDamageTextCapacity> at{};
  std::size_t count = 0;
  float font_size = 0.0F;
};

[[nodiscard]] bool PlaceGlyphs(ImFont* font, const DamageTextView& text, PlacedGlyphs& placed) {
  // LA TAILLE DE POLICE QUI DONNE AU ZÉRO LA HAUTEUR DU CHIFFRE DU SPRITE. Le
  // rapport se mesure une fois pour toutes à une grande taille : il ne dépend
  // que du dessin de la famille.
  ImFontBaked* reference = font->GetFontBaked(kMeasureFontPixels);
  const ImFontGlyph* measured =
      reference != nullptr ? reference->FindGlyphNoFallback('0') : nullptr;
  if (measured == nullptr || measured->Y1 <= measured->Y0) {
    return false;
  }
  const float digit_per_font = (measured->Y1 - measured->Y0) / reference->Size;
  placed.font_size = std::min(text.digit_height / digit_per_font, kLargestFontPixels);
  ImFontBaked* baked = font->GetFontBaked(placed.font_size);
  const ImFontGlyph* zero = baked != nullptr ? baked->FindGlyphNoFallback('0') : nullptr;
  if (zero == nullptr) {
    return false;
  }
  // ImGui grave à la taille ARRONDIE et étire le reste : les mesures du glyphe
  // gravé se ramènent à la taille demandée.
  const float to_size = placed.font_size / baked->Size;
  // ⚠ LE MILIEU DU ZÉRO POUR TOUS, et non celui de chaque glyphe : un « K »
  // centré sur lui-même flotterait au-dessus de la ligne des chiffres.
  const float top = text.middle.y - (zero->Y0 + zero->Y1) * 0.5F * to_size;

  while (placed.count < text.glyphs.size() && text.glyphs[placed.count] != '\0') {
    ++placed.count;
  }
  if (placed.count == 0) {
    return false;
  }
  const auto half_span = static_cast<float>(placed.count - 1) * 0.5F;
  for (std::size_t rank = 0; rank < placed.count; ++rank) {
    const ImFontGlyph* glyph = baked->FindGlyph(static_cast<ImWchar>(text.glyphs[rank]));
    const float slot_x = text.middle.x + text.pitch * (static_cast<float>(rank) - half_span);
    const float middle_x = glyph != nullptr ? (glyph->X0 + glyph->X1) * 0.5F * to_size : 0.0F;
    placed.at[rank] = ImVec2(slot_x - middle_x, top);
  }
  return true;
}

void PaintOutline(ImDrawList* into, ImFont* font, const DamageTextView& text,
                  const PlacedGlyphs& placed, ImU32 color, float radius) {
  for (std::size_t rank = 0; rank < placed.count; ++rank) {
    const char* glyph = &text.glyphs[rank];
    for (const ImVec2& toward : kOutlineDirections) {
      const ImVec2 at(placed.at[rank].x + toward.x * radius, placed.at[rank].y + toward.y * radius);
      into->AddText(font, placed.font_size, at, color, glyph, glyph + 1);
    }
  }
}

// L'OPACITÉ D'UNE COPIE DU CERNE, pour que `copies` copies superposées rendent
// celle du nombre : 1 − (1 − a)^(1/n).
//
// 🔴 CHAQUE COPIE À L'OPACITÉ DU NOMBRE, huit copies noires se cumulaient sur
// un remplissage posé une fois : un nombre qui pâlit devenait une ombre, et
// quarante nombres superposés une traînée noire.
[[nodiscard]] ImU32 OutlineCopyAlpha(ImU32 alpha, std::size_t copies) {
  constexpr float kOpaque = 255.0F;
  const float covered = static_cast<float>(alpha) / kOpaque;
  const float each = 1.0F - std::pow(1.0F - covered, 1.0F / static_cast<float>(copies));
  return static_cast<ImU32>(std::lround(std::clamp(each, 0.0F, 1.0F) * kOpaque));
}

void PaintDamageText(ImDrawList* into, ImFont* font, const DamageTextView& text) {
  const ImU32 alpha = (text.color & IM_COL32_A_MASK) >> IM_COL32_A_SHIFT;
  if (alpha == 0 || text.digit_height < kSmallestDigitPixels) {
    return;
  }
  PlacedGlyphs placed;
  if (!PlaceGlyphs(font, text, placed)) {
    return;
  }
  // LE CERNE D'ABORD, POUR TOUS LES GLYPHES : posé glyphe par glyphe avec son
  // remplissage, celui du suivant mordrait sur le précédent quand l'écart est
  // serré. Il pâlit avec le nombre.
  if (text.outline > 0.0F) {
    const bool thick = text.outline > kThinOutlinePixels;
    const ImU32 outline =
        (kDamageTextOutlineColor & ~IM_COL32_A_MASK) |
        (OutlineCopyAlpha(alpha, thick ? 2U * kOutlineCopies : kOutlineCopies) << IM_COL32_A_SHIFT);
    PaintOutline(into, font, text, placed, outline, text.outline);
    if (thick) {
      PaintOutline(into, font, text, placed, outline, text.outline * 0.5F);
    }
  }
  for (std::size_t rank = 0; rank < placed.count; ++rank) {
    const char* glyph = &text.glyphs[rank];
    into->AddText(font, placed.font_size, placed.at[rank], text.color, glyph, glyph + 1);
  }
}

// La police dessine-t-elle un zéro ? C'est lui qui donne la taille : sans lui,
// rien ne s'écrirait.
[[nodiscard]] bool HasDigits(ImFont* font) {
  if (font == nullptr) {
    return false;
  }
  ImFontBaked* measured = font->GetFontBaked(kMeasureFontPixels);
  return measured != nullptr && measured->FindGlyphNoFallback('0') != nullptr;
}

}  // namespace

void DrawDamageTexts(std::span<const DamageTextView> texts, ImFont* font) {
  PaintDamageTexts(*ImGui::GetBackgroundDrawList(), texts, font);
}

void PaintDamageTexts(ImDrawList& into, std::span<const DamageTextView> texts, ImFont* font) {
  if (texts.empty()) {
    return;
  }
  ImFont* const written = HasDigits(font) ? font : ImGui::GetFont();
  if (written == nullptr) {
    return;
  }
  for (const DamageTextView& text : texts) {
    PaintDamageText(&into, written, text);
  }
}

}  // namespace ro::ui
