#include "ro/render/palette_ramps.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ro::render {
namespace {

// Le nombre de secteurs du cercle des teintes, tel que la conversion HSV le
// découpe : six, et les deux sens de conversion s'y accordent.
constexpr double kHueSectors = 6.0;
constexpr double kDegreesPerTurn = 360.0;
constexpr double kHalfTurnDegrees = 180.0;
// Ce qu'une composante vaut au plus, et le facteur d'un pourcentage.
constexpr double kChannelMax = 255.0;
constexpr double kPercent = 100.0;
// Le poids que le score donne à une rampe GRISE, et celui qu'ajoute une
// saturation pleine. Une pièce franche l'emporte donc sur un aplat terne
// jusqu'à 1,99 fois plus grand — assez pour remonter les rangs neuf mesurés,
// pas assez pour faire passer une paillette devant une tunique.
constexpr std::int64_t kScoreNeutralWeight = 256;

struct Hsv {
  double h = 0.0;  // 0..1
  double s = 0.0;
  double v = 0.0;
};

// Transcription de `colorsys.rgb_to_hsv` — même découpage en secteurs, parce
// que la référence Python et le jeu doivent trouver les mêmes rampes sur les
// mêmes octets.
[[nodiscard]] Hsv RgbToHsv(int red, int green, int blue) {
  const double r = static_cast<double>(red) / kChannelMax;
  const double g = static_cast<double>(green) / kChannelMax;
  const double b = static_cast<double>(blue) / kChannelMax;
  const double high = std::max({r, g, b});
  const double low = std::min({r, g, b});
  Hsv out;
  out.v = high;
  if (high == low) {
    return out;  // gris pur : ni teinte ni saturation
  }
  const double range = high - low;
  out.s = range / high;
  const double rc = (high - r) / range;
  const double gc = (high - g) / range;
  const double bc = (high - b) / range;
  double hue = 0.0;
  if (r == high) {
    hue = bc - gc;
  } else if (g == high) {
    hue = 2.0 + rc - bc;
  } else {
    hue = 4.0 + gc - rc;
  }
  hue = std::fmod(hue / kHueSectors, 1.0);
  if (hue < 0.0) {
    hue += 1.0;
  }
  out.h = hue;
  return out;
}

// L'arrondi du C++, et NON celui de la bibliothèque standard de Python.
//
// 🔴 `round()` de Python est BANCAIRE — `round(0,5)` rend zéro — là où
// `int(x + 0,5)` monte toujours. La référence s'aligne sur cette règle-ci ;
// prendre l'autre ferait diverger d'un pas la moitié des entrées d'une
// palette, donc les couleurs de deux joueurs.
[[nodiscard]] std::uint8_t ToByte(double value) {
  const int scaled = static_cast<int>(value * kChannelMax + 0.5);
  return static_cast<std::uint8_t>(std::clamp(scaled, 0, 255));
}

// Transcription de `colorsys.hsv_to_rgb`.
void HsvToRgb(double hue, double saturation, double value, std::uint8_t* out) {
  double r = value;
  double g = value;
  double b = value;
  if (saturation != 0.0) {
    const double sectors = hue * kHueSectors;
    int sector = static_cast<int>(sectors);
    const double fraction = sectors - static_cast<double>(sector);
    const double p = value * (1.0 - saturation);
    const double q = value * (1.0 - saturation * fraction);
    const double t = value * (1.0 - saturation * (1.0 - fraction));
    sector %= static_cast<int>(kHueSectors);
    switch (sector) {
      case 0:
        r = value, g = t, b = p;
        break;
      case 1:
        r = q, g = value, b = p;
        break;
      case 2:
        r = p, g = value, b = t;
        break;
      case 3:
        r = p, g = q, b = value;
        break;
      case 4:
        r = t, g = p, b = value;
        break;
      default:
        r = value, g = p, b = q;
        break;
    }
  }
  out[0] = ToByte(r);
  out[1] = ToByte(g);
  out[2] = ToByte(b);
}

[[nodiscard]] Hsv EntryHsv(const formats::Palette& palette, std::size_t index) {
  const auto& entry = palette.entries[index];
  return RgbToHsv(entry[0], entry[1], entry[2]);
}

// La somme des trois composantes : ce qui décide qu'une entrée est NOIRE.
[[nodiscard]] int Luma(const formats::Palette& palette, std::size_t index) {
  const auto& entry = palette.entries[index];
  return static_cast<int>(entry[0]) + entry[1] + entry[2];
}

// L'entrée que l'outil du .pal n'a jamais écrite : les QUATRE octets nuls.
// C'est le séparateur naturel entre deux pièces du costume.
[[nodiscard]] bool Undefined(const formats::Palette& palette, std::size_t index) {
  return Luma(palette, index) == 0 && palette.entries[index][3] == 0;
}

// Écart CIRCULAIRE entre deux teintes, en degrés (0..180). Sans le repliement,
// le rouge à 359 et le rouge à 1 passeraient pour opposés.
[[nodiscard]] double HueGap(double left, double right) {
  const double gap = std::fmod(std::abs(left - right), kDegreesPerTurn);
  return gap > kHalfTurnDegrees ? kDegreesPerTurn - gap : gap;
}

// La teinte de l'index le plus SATURÉ de la rampe — et non la moyenne, qui sur
// des gris donnerait un angle arbitraire. L'index le plus saturé, lui, porte la
// couleur que le joueur croit voir.
[[nodiscard]] std::optional<int> DominantHue(const formats::Palette& palette,
                                             std::span<const int> usage, std::size_t begin,
                                             std::size_t end) {
  double best_hue = 0.0;
  double best_saturation = -1.0;
  bool found = false;
  for (std::size_t index = begin; index < end; ++index) {
    if (usage[index] == 0) {
      continue;
    }
    const Hsv color = EntryHsv(palette, index);
    if (color.s > best_saturation) {
      best_saturation = color.s;
      best_hue = color.h * kDegreesPerTurn;
      found = true;
    }
  }
  if (!found || best_saturation < kRampNeutralSaturation) {
    return std::nullopt;
  }
  // Arrondi entier, comme la référence : sans cela les deux implémentations
  // rendraient des teintes différentes de la même rampe, et comparer leurs
  // sorties ne prouverait plus rien.
  const int degrees = static_cast<int>(best_hue + 0.5);
  return degrees >= 360 ? degrees - 360 : degrees;
}

// L'index représentatif : le milieu des index PEINTS ET NON NOIRS.
[[nodiscard]] std::uint8_t RepresentativeIndex(const formats::Palette& palette,
                                               std::span<const int> usage, std::size_t begin,
                                               std::size_t end) {
  const auto paintable = [&](std::size_t index) {
    return usage[index] != 0 && !(palette.entries[index][0] == 0 &&
                                  palette.entries[index][1] == 0 && palette.entries[index][2] == 0);
  };
  std::size_t first = 0;
  std::size_t last = 0;
  std::size_t count = 0;
  for (std::size_t index = begin; index < end; ++index) {
    if (!paintable(index)) {
      continue;
    }
    if (count == 0) {
      first = index;
    }
    last = index;
    ++count;
  }
  if (count == 0) {
    return static_cast<std::uint8_t>(std::min<std::size_t>(begin + (end - begin) / 2, 255));
  }
  // Le milieu des index RETENUS, en les reparcourant : ils ne sont pas
  // forcément contigus, et la moyenne des bornes retomberait sur un trou.
  const std::size_t target = count / 2;
  std::size_t seen = 0;
  for (std::size_t index = first; index <= last; ++index) {
    if (!paintable(index)) {
      continue;
    }
    if (seen == target) {
      return static_cast<std::uint8_t>(index);
    }
    ++seen;
  }
  return static_cast<std::uint8_t>(last);
}

// CE QUI DÉCIDE DU RANG D'UNE RAMPE : sa surface, pondérée par sa franchise.
//
// 🔴 TOUT EN ENTIERS, et c'est la raison d'être de cette fonction. Le
// classement choisit QUELLES pièces reçoivent un curseur, donc ce que les
// rangs d'une recette désignent. Un flottant dans la clé départagerait les ex
// æquo autrement d'une implémentation à l'autre.
//
// La saturation d'une entrée vaut `(haut − bas) × 255 / haut`, la définition
// HSV ramenée aux entiers, et elle est moyennée en pondérant par les PIXELS :
// une couleur vive employée sur trois pixels ne colore pas tout le verdict.
[[nodiscard]] std::int64_t RampScore(const formats::Palette& palette, std::span<const int> usage,
                                     std::size_t begin, std::size_t end, int pixels) {
  std::int64_t weighted_saturation = 0;
  std::int64_t counted_pixels = 0;
  for (std::size_t index = begin; index < end; ++index) {
    const int used = usage[index];
    if (used == 0) {
      continue;
    }
    const auto& entry = palette.entries[index];
    const int high = std::max({entry[0], entry[1], entry[2]});
    const int low = std::min({entry[0], entry[1], entry[2]});
    const int saturation = high != 0 ? ((high - low) * 255) / high : 0;
    weighted_saturation += static_cast<std::int64_t>(saturation) * used;
    counted_pixels += used;
  }
  const std::int64_t mean = counted_pixels != 0 ? weighted_saturation / counted_pixels : 0;
  return static_cast<std::int64_t>(pixels) * (kScoreNeutralWeight + mean);
}

}  // namespace

std::array<int, formats::kPaletteEntryCount> CountIndexUsage(const formats::SprSprite& sprite) {
  std::array<int, formats::kPaletteEntryCount> usage{};
  for (const formats::SprIndexedFrame& frame : sprite.indexed_frames()) {
    for (const std::uint8_t index : frame.indices) {
      ++usage[index];
    }
  }
  // ⚠ L'INDEX ZÉRO EST LE TRANSPARENT. Le laisser compter donnerait à la
  // première rampe la surface de tout le vide autour du personnage.
  usage[0] = 0;
  return usage;
}

formats::Palette MergeServerPalette(const formats::Palette& sprite,
                                    const formats::Palette* server) {
  if (server == nullptr) {
    return sprite;
  }
  // On part du SERVEUR — c'est lui qui porte la couleur que le joueur voit —
  // puis on ne rapatrie du sprite que les trous.
  formats::Palette merged = *server;
  std::size_t index = 1;  // l'index zéro est le transparent : il ne se peint pas
  while (index < formats::kPaletteEntryCount) {
    if (Undefined(*server, index)) {
      merged.entries[index] = sprite.entries[index];
      ++index;
      continue;
    }
    // Une plage définie va d'ici jusqu'au prochain séparateur.
    std::size_t end = index;
    int brightest = 0;
    while (end < formats::kPaletteEntryCount && !Undefined(*server, end)) {
      brightest = std::max(brightest, Luma(*server, end));
      ++end;
    }
    if (brightest <= kPaletteBlackSum) {
      for (std::size_t each = index; each < end; ++each) {
        merged.entries[each] = sprite.entries[each];
      }
    }
    index = end;
  }
  return merged;
}

std::vector<PaletteRamp> DetectRamps(const formats::Palette& palette, std::span<const int> usage) {
  if (usage.size() < formats::kPaletteEntryCount) {
    return {};
  }
  // La rampe et sa clé de tri. ⚠ LE SCORE RESTE LOCAL : il ne sert qu'à
  // classer, et le publier inviterait à le transmettre — or ce qui voyage est
  // le RANG, chaque client recalculant le sien sur son propre sprite.
  struct Scored {
    PaletteRamp ramp;
    std::int64_t score = 0;
  };
  std::vector<Scored> found;

  bool open = false;
  std::size_t begin = 0;
  // Négative tant que la rampe n'a pas de teinte : elle a pu démarrer en gris.
  double reference_hue = -1.0;
  double previous_value = 0.0;

  const auto close = [&](std::size_t end) {
    if (!open) {
      return;
    }
    const std::size_t length = end - begin;
    if (length < static_cast<std::size_t>(kRampMinLength)) {
      return;
    }
    int pixels = 0;
    for (std::size_t index = begin; index < end; ++index) {
      pixels += usage[index];
    }
    if (pixels < kRampMinPixels) {
      return;
    }
    Scored scored;
    scored.ramp.start = static_cast<std::uint8_t>(begin);
    scored.ramp.length = static_cast<std::uint8_t>(length);
    scored.ramp.pixels = pixels;
    scored.ramp.hue = DominantHue(palette, usage, begin, end);
    scored.ramp.ref = RepresentativeIndex(palette, usage, begin, end);
    scored.score = RampScore(palette, usage, begin, end, pixels);
    found.push_back(scored);
  };

  for (std::size_t index = 1; index < formats::kPaletteEntryCount; ++index) {
    if (usage[index] == 0) {  // un index jamais peint coupe toujours
      close(index);
      open = false;
      reference_hue = -1.0;
      continue;
    }
    const Hsv color = EntryHsv(palette, index);
    const double hue_degrees = color.h * kDegreesPerTurn;

    if (!open) {
      open = true;
      begin = index;
      previous_value = color.v;
      reference_hue = color.s >= kRampNeutralSaturation ? hue_degrees : -1.0;
      continue;
    }

    // Deux ruptures possibles : un dégradé RO va du clair au foncé dans une
    // teinte stable.
    bool cut = false;
    if (color.s >= kRampNeutralSaturation) {
      if (reference_hue < 0.0) {
        reference_hue = hue_degrees;  // la rampe démarrait sur du gris
      } else if (HueGap(hue_degrees, reference_hue) > kRampHueTolerance) {
        cut = true;  // une autre pièce du costume
      }
    }
    if (color.v > previous_value + kRampValueRebound) {
      cut = true;  // retour au clair : nouvelle rampe
    }
    if (cut) {
      close(index);
      begin = index;
      reference_hue = color.s >= kRampNeutralSaturation ? hue_degrees : -1.0;
    }
    previous_value = color.v;
  }
  close(formats::kPaletteEntryCount);

  // Les plus VISIBLES d'abord : c'est l'ordre des curseurs, et le plafond doit
  // couper les rampes anecdotiques, pas la tunique.
  // ⚠ TRI STABLE SUR UNE CLÉ ENTIÈRE : voir l'en-tête. Deux ex æquo doivent se
  // départager par leur ordre d'apparition, ici comme dans la référence.
  std::stable_sort(found.begin(), found.end(), [](const Scored& left, const Scored& right) {
    return left.score > right.score;
  });

  std::vector<PaletteRamp> ramps;
  ramps.reserve(std::min(found.size(), kMaxRamps));
  for (std::size_t rank = 0; rank < found.size() && rank < kMaxRamps; ++rank) {
    ramps.push_back(found[rank].ramp);
  }
  return ramps;
}

std::optional<std::size_t> RampAt(std::span<const PaletteRamp> ramps, std::uint8_t index) {
  for (std::size_t rank = 0; rank < ramps.size(); ++rank) {
    const PaletteRamp& ramp = ramps[rank];
    if (index >= ramp.start && index < ramp.start + ramp.length) {
      return rank;
    }
  }
  return std::nullopt;
}

std::array<std::uint8_t, 3> RampColor(const formats::Palette& palette, const PaletteRamp& ramp) {
  const auto& entry = palette.entries[ramp.ref];
  return {entry[0], entry[1], entry[2]};
}

int RampValueCeiling(const formats::Palette& palette, const PaletteRamp& ramp) {
  const auto& entry = palette.entries[ramp.ref];
  const int value = std::max({entry[0], entry[1], entry[2]});
  // ⚠ LE DOUBLE, PLAFONNÉ : c'est exactement ce que rend une luminosité de
  // plus cent, dont la borne fait toute cette limite. Les deux doivent bouger
  // ensemble, sinon la fenêtre annonce une clarté que le réglage n'atteint pas.
  return std::min(value * 2, 255);
}

RampAdjust AdjustToReach(const formats::Palette& palette, const PaletteRamp& ramp,
                         std::array<std::uint8_t, 3> target) {
  RampAdjust adjust;
  const Hsv from = EntryHsv(palette, ramp.ref);
  const Hsv to = RgbToHsv(target[0], target[1], target[2]);

  // 🔴 TOUJOURS ABSOLU : voir l'en-tête. Une pièce terne ne peut pas devenir
  // vive par un facteur.
  adjust.absolute = true;
  int degrees = static_cast<int>(to.h * kDegreesPerTurn + 0.5);
  degrees = std::max(degrees, 0);
  if (degrees >= 360) {
    degrees -= 360;
  }
  adjust.hue = static_cast<std::int16_t>(degrees);

  const int saturation = static_cast<int>(to.s * kPercent + 0.5);
  adjust.saturation = static_cast<std::int8_t>(std::clamp(saturation, 0, 100));

  // La luminosité reste RELATIVE : le rapport entre la clarté demandée et
  // celle d'origine, pour que le dégradé garde son modelé.
  // ⚠ UNE BASE NOIRE N'A PAS DE RAPPORT : diviser par elle donnerait un
  // réglage infini, et la garde vaut mieux qu'une valeur inventée.
  constexpr double kDarkest = 0.001;
  if (from.v > kDarkest) {
    const double percent = (to.v / from.v - 1.0) * kPercent;
    const double bounded = std::clamp(percent, -kPercent, kPercent);
    // ⚠ ARRONDI VERS L'EXTÉRIEUR, et non par troncature : sans cela un
    // éclaircissement d'un demi pour-cent disparaît, et deux clics successifs
    // sur la même couleur ne rendent pas le même réglage.
    adjust.value = static_cast<std::int8_t>(bounded < 0 ? bounded - 0.5 : bounded + 0.5);
  }
  return adjust;
}

formats::Palette ApplyRecipe(const formats::Palette& base, std::span<const PaletteRamp> ramps,
                             const PaletteRecipe& recipe) {
  formats::Palette painted = base;
  for (std::size_t rank = 0; rank < ramps.size() && rank < kMaxRamps; ++rank) {
    const RampAdjust& adjust = recipe.ramps[rank];
    // ⚠ UN RÉGLAGE NEUTRE EST SAUTÉ. L'aller-retour RGB→HSV→RGB se trouve être
    // l'identité, mais la parité avec le reste doit tenir par construction et
    // non par une propriété numérique heureuse.
    if (adjust.neutral()) {
      continue;
    }
    const PaletteRamp& ramp = ramps[rank];
    const std::size_t end = std::min<std::size_t>(
        static_cast<std::size_t>(ramp.start) + ramp.length, formats::kPaletteEntryCount);
    for (std::size_t index = ramp.start; index < end; ++index) {
      const Hsv color = EntryHsv(base, index);
      double hue = 0.0;
      double saturation = 0.0;
      if (adjust.absolute) {
        hue = std::fmod(static_cast<double>(adjust.hue), kDegreesPerTurn);
        saturation = static_cast<double>(adjust.saturation) / kPercent;
      } else {
        hue =
            std::fmod(color.h * kDegreesPerTurn + static_cast<double>(adjust.hue), kDegreesPerTurn);
        saturation = color.s * (1.0 + static_cast<double>(adjust.saturation) / kPercent);
      }
      // ⚠ LE MODULO DU C++ GARDE LE SIGNE DU DIVIDENDE, celui de Python celui
      // du diviseur : une teinte pivotée vers le bas sortirait négative, et la
      // conversion la placerait dans un tout autre secteur.
      if (hue < 0.0) {
        hue += kDegreesPerTurn;
      }
      const double value = color.v * (1.0 + static_cast<double>(adjust.value) / kPercent);
      HsvToRgb(hue / kDegreesPerTurn, std::clamp(saturation, 0.0, 1.0), std::clamp(value, 0.0, 1.0),
               painted.entries[index].data());
    }
  }
  return painted;
}

}  // namespace ro::render
