// Les deux affichages du classement PVP. Voir l'en-tête pour les mesures et
// pour ce qu'elles doivent au client vanilla.

#include "ro/render/pvp_hud.h"

#include <array>

namespace ro::render {
namespace {

// Une seule image par action dans les deux fontes.
constexpr std::size_t kOnlyFrame = 0;

// Les tranches d'une durée, de la plus grosse à la plus fine.
constexpr std::uint32_t kMsPerSecond = 1000;
constexpr std::uint32_t kSecondsPerMinute = 60;
constexpr std::uint32_t kMinutesPerHour = 60;
constexpr std::uint32_t kHoursPerDay = 24;

// Combien de glyphes chaque profondeur de tranche demande. ⚠ PAR PAQUETS DE
// TROIS, parce que chaque tranche de tête traîne son séparateur avec elle.
constexpr std::size_t kSecondsOnlyGlyphs = 2;
constexpr std::size_t kWithMinutesGlyphs = 5;
constexpr std::size_t kWithHoursGlyphs = 8;
constexpr std::size_t kWithDaysGlyphs = 11;

// Les chiffres d'un nombre, du plus fort au plus faible. Zéro rend un chiffre :
// « 0 » s'écrit.
[[nodiscard]] std::vector<std::size_t> DigitsOf(std::int32_t value) {
  std::vector<std::size_t> digits;
  do {
    digits.insert(digits.begin(), static_cast<std::size_t>(value % 10));
    value /= 10;
  } while (value > 0);
  return digits;
}

void Append(std::vector<ScreenQuad>& into, std::vector<ScreenQuad> from) {
  into.insert(into.end(), from.begin(), from.end());
}

}  // namespace

std::size_t PvpStopwatchGlyphCount(std::uint32_t elapsed_ms) noexcept {
  const std::uint32_t seconds = elapsed_ms / kMsPerSecond;
  const std::uint32_t minutes = seconds / kSecondsPerMinute;
  const std::uint32_t hours = minutes / kMinutesPerHour;
  const std::uint32_t days = hours / kHoursPerDay;
  if (days > 0) {
    return kWithDaysGlyphs;
  }
  if (hours > 0) {
    return kWithHoursGlyphs;
  }
  if (minutes > 0) {
    return kWithMinutesGlyphs;
  }
  return kSecondsOnlyGlyphs;
}

PvpHudPlacement DefaultPvpRankPlacement(glm::vec2 viewport_px) {
  // LE COIN BAS-DROIT DE L'ÉCRAN, à l'échelle un : les décalages relevés se
  // comptent alors depuis le bord, exactement comme chez le client d'origine.
  return PvpHudPlacement{.corner_px = viewport_px, .scale = 1.0F};
}

PvpHudPlacement DefaultPvpStopwatchPlacement(glm::vec2 viewport_px) {
  // LE MILIEU DU BORD HAUT, descendu de ce que le client descend le sien.
  return PvpHudPlacement{
      .corner_px = glm::vec2{viewport_px.x / 2.0F, kPvpStopwatchFromTopPx},
      .scale = 1.0F,
  };
}

std::vector<ScreenQuad> ComposePvpRank(const formats::SprSprite& sprite,
                                       const formats::ActAnimation& animation, std::int32_t rank,
                                       std::int32_t total, const PvpHudPlacement& placement) {
  // ⚠ SIGNÉE, COMME CELLE DU CLIENT D'ORIGINE : moins un ne dessine rien, et
  // c'est ce que vaut le rang d'un joueur caché une fois lu en entier signé.
  if (rank <= 0 || total <= 0) {
    return {};
  }
  std::vector<ScreenQuad> quads;
  const glm::vec2 corner = placement.corner_px;
  const float ui_scale = placement.scale;
  const float pitch = kPvpRankDigitPitchPx * ui_scale;

  // LE RANG, de ses unités vers la gauche : le chiffre le plus faible est le
  // plus à droite, et chaque dizaine recule d'un pas.
  const std::vector<std::size_t> rank_digits = DigitsOf(rank);
  for (std::size_t index = 0; index < rank_digits.size(); ++index) {
    const auto from_units = static_cast<float>(rank_digits.size() - 1 - index);
    const glm::vec2 at{corner.x - kPvpRankUnitsFromRightPx * ui_scale - from_units * pitch,
                       corner.y - kPvpRankUnitsFromBottomPx * ui_scale};
    Append(quads, ComposeScreenSprite(sprite, animation, rank_digits[index], kOnlyFrame, at));
  }

  // LA BARRE, entre les deux et plus bas : c'est elle qui donne l'escalier.
  Append(quads, ComposeScreenSprite(sprite, animation, kPvpSeparatorAction, kOnlyFrame,
                                    glm::vec2{corner.x - kPvpRankBarFromRightPx * ui_scale,
                                              corner.y - kPvpRankBarFromBottomPx * ui_scale}));

  // LE TOTAL, de sa gauche vers la droite : son premier chiffre est déjà d'un
  // pas à droite de la barre.
  const std::vector<std::size_t> total_digits = DigitsOf(total);
  for (std::size_t index = 0; index < total_digits.size(); ++index) {
    const auto step = static_cast<float>(index + 1);
    const glm::vec2 at{corner.x - kPvpRankBarFromRightPx * ui_scale + step * pitch,
                       corner.y - kPvpRankTotalFromBottomPx * ui_scale};
    Append(quads, ComposeScreenSprite(sprite, animation, total_digits[index], kOnlyFrame, at));
  }
  return quads;
}

std::vector<ScreenQuad> ComposePvpStopwatch(const formats::SprSprite& sprite,
                                            const formats::ActAnimation& animation,
                                            std::uint32_t elapsed_ms,
                                            const PvpHudPlacement& placement) {
  const std::size_t glyphs = PvpStopwatchGlyphCount(elapsed_ms);

  const std::uint32_t seconds = elapsed_ms / kMsPerSecond;
  const std::uint32_t minutes = seconds / kSecondsPerMinute;
  const std::uint32_t hours = minutes / kMinutesPerHour;
  const std::uint32_t days = hours / kHoursPerDay;
  // Les quatre tranches, de la plus grosse à la plus fine, chacune bornée à ce
  // que la suivante ne reprenne pas.
  const std::array<std::uint32_t, 4> slices = {
      days, hours % kHoursPerDay, minutes % kMinutesPerHour, seconds % kSecondsPerMinute};
  // ⚠ ON COMMENCE PAR LA FIN DU TABLEAU, parce que ce sont les tranches de TÊTE
  // qui tombent : à cinq glyphes, on écrit les minutes et les secondes, donc
  // les deux dernières.
  const std::size_t shown_slices = (glyphs + 1) / 3;
  const std::size_t first_slice = slices.size() - shown_slices;

  const float ui_scale = placement.scale;
  // ⚠ CENTRÉ SUR LE POINT DONNÉ, et non sur l'écran : c'est ce qui rend la pièce
  // déplaçable sans lui faire perdre son centrage sur elle-même. Le nombre de
  // glyphes change avec la durée, et un chronomètre calé à gauche s'allongerait
  // vers la droite en passant la minute.
  const float left =
      placement.corner_px.x - static_cast<float>(glyphs) * kPvpStopwatchHalfPitchPx * ui_scale;
  std::vector<ScreenQuad> quads;
  std::size_t slice = first_slice;
  bool high_digit = true;
  for (std::size_t rank = 0; rank < glyphs; ++rank) {
    const glm::vec2 at{left + static_cast<float>(rank) * kPvpStopwatchGlyphPitchPx * ui_scale,
                       placement.corner_px.y};
    if (PvpStopwatchSeparatorAt(rank)) {
      Append(quads, ComposeScreenSprite(sprite, animation, kPvpSeparatorAction, kOnlyFrame, at));
      continue;
    }
    const std::uint32_t value = slices[slice];
    const std::size_t digit = high_digit ? value / 10 : value % 10;
    Append(quads, ComposeScreenSprite(sprite, animation, digit, kOnlyFrame, at));
    if (high_digit) {
      high_digit = false;
    } else {
      high_digit = true;
      ++slice;
    }
  }
  return quads;
}

}  // namespace ro::render
