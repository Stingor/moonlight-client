#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "ro/formats/palette.h"
#include "ro/render/hue_classes.h"
#include "ro/render/palette_ramps.h"

namespace ro::render {

// LE PROFIL MIXTE : CE QUE LES PIÈCES NE COUVRENT PAS, ET LES RETOUCHES.
//
// Une teinture règle d'abord ses pièces — les rampes du profil 1, ou celles de
// la suite du corps. Ensuite, CHAQUE INDEX qu'aucune pièce ni l'ensemble de peau
// ne couvre reçoit le réglage de sa classe de teinte : c'est « le reste ». En
// dernier viennent les retouches, qui posent une couleur EXACTE sur un index.
//
// 🔴 LE RESTE NE TOUCHE JAMAIS UNE PIÈCE NI LA PEAU : une plume rouge réglée
// comme pièce ne se fait pas repeindre par la classe des rouges. Les deux
// couches se partagent la palette sans se recouvrir.
//
// 🔴 L'INDEX ZÉRO EST LE TRANSPARENT : ni le reste ni une retouche ne le
// touchent. Le magenta pur ne s'écrit jamais, et une couleur d'origine en
// magenta pur reste telle quelle : certains lecteurs le prennent pour un
// transparent.

// Les index qu'une pièce (ou la peau) couvre.
using PaletteCover = std::array<bool, formats::kPaletteEntryCount>;

void Cover(PaletteCover& cover, std::span<const PaletteRamp> ramps);
void Cover(PaletteCover& cover, std::span<const std::uint8_t> indices);

// Le magenta pur, interdit à une retouche.
inline constexpr std::array<std::uint8_t, 3> kKeyMagenta = {255, 0, 255};

// CE QUI RESTE d'un sprite, compté sur l'emploi de ses index.
struct RestMeasure {
  // Les pixels de chaque classe de teinte du reste, et leur couleur moyenne,
  // pondérée par les pixels.
  std::array<int, kHueClassCount> pixels{};
  std::array<std::array<std::uint8_t, 3>, kHueClassCount> mean{};
  // Tous les pixels peints, ceux que les pièces couvrent, et ceux du reste
  // qu'un réglage atteint : tout sauf les noirs de contour, qu'aucun réglage
  // ne fait sortir du noir.
  int total = 0;
  int covered = 0;
  int reachable = 0;

  // La part réglable, en pour-cent arrondi ; -1 sans pixel.
  [[nodiscard]] int ReachablePercent() const noexcept;
};

[[nodiscard]] RestMeasure MeasureRest(const formats::Palette& base,
                                      std::span<const int, formats::kPaletteEntryCount> usage,
                                      const PaletteCover& cover);

// LE RESTE : chaque index non couvert de `painted` reprend la couleur de
// `base` réglée par le réglage de sa classe. Un réglage neutre n'y change rien.
void ApplyRest(formats::Palette& painted, const formats::Palette& base, const PaletteCover& cover,
               const HueClassAdjusts& rest);

// UNE RETOUCHE : la couleur exacte d'un index de la palette du calque.
struct PaletteRetouch {
  std::uint8_t index = 0;
  std::array<std::uint8_t, 3> rgb{};

  [[nodiscard]] friend bool operator==(const PaletteRetouch&, const PaletteRetouch&) = default;
};

// Ni l'index zéro, ni le magenta pur.
[[nodiscard]] bool RetouchAllowed(const PaletteRetouch& retouch) noexcept;

// Les retouches, appliquées telles quelles ; l'octet d'alpha ne bouge pas, et
// une retouche interdite s'ignore.
void ApplyRetouches(formats::Palette& painted, std::span<const PaletteRetouch> retouches);

}  // namespace ro::render
