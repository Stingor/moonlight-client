#include "ro/formats/image.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

#include "ro/core/ascii.h"
#include "ro/core/ink.h"
#include "ro/formats/bmp.h"
#include "ro/formats/jpeg.h"
#include "ro/formats/png.h"
#include "ro/formats/tga.h"

namespace ro::formats {

void ApplyMagentaColorKey(Image& image) {
  for (std::size_t offset = 0; offset + 3 < image.pixels.size(); offset += 4) {
    if (image.pixels[offset] == 255 && image.pixels[offset + 1] == 0 &&
        image.pixels[offset + 2] == 255) {
      image.pixels[offset + 3] = 0;
    }
  }
}

void ApplyTextureColorKey(Image& image) {
  // Cinq bits par canal : on garde ceux de poids fort.
  constexpr int kDroppedBits = 3;
  constexpr int kFullChannel = 0x1F;
  for (std::size_t offset = 0; offset + 3 < image.pixels.size(); offset += 4) {
    if ((image.pixels[offset] >> kDroppedBits) == kFullChannel &&
        (image.pixels[offset + 1] >> kDroppedBits) == 0 &&
        (image.pixels[offset + 2] >> kDroppedBits) == kFullChannel) {
      image.pixels[offset] = 0;
      image.pixels[offset + 1] = 0;
      image.pixels[offset + 2] = 0;
      image.pixels[offset + 3] = 0;
    }
  }
}

namespace {

// Seize bits par texel : cinq par canal sans alpha, quatre avec, alpha compris.
constexpr int kChannelBits = 8;
constexpr int kDroppedBitsWithoutAlpha = 3;
constexpr int kDroppedBitsWithAlpha = 4;
constexpr int kChannelMax = 0xFF;
// Sans canal alpha, la texture n'en garde qu'un bit : le bit de poids fort.
constexpr int kOneBitAlphaThreshold = 0x80;

[[nodiscard]] bool HasAlphaChannel(const Image& image) {
  return image.file == ImageFile::kTga || image.file == ImageFile::kPng;
}

[[nodiscard]] int DroppedBitsOf(const Image& image) {
  return HasAlphaChannel(image) ? kDroppedBitsWithAlpha : kDroppedBitsWithoutAlpha;
}

// Les clés d'une texture de seize bits : le magenta, l'alpha nul, et le noir
// quand on le demande.
void ApplySixteenBitColorKey(Image& image, bool black_is_clear) {
  const bool with_alpha = HasAlphaChannel(image);
  const int dropped_bits = DroppedBitsOf(image);
  const int full_channel = kChannelMax >> dropped_bits;
  for (std::size_t offset = 0; offset + 3 < image.pixels.size(); offset += 4) {
    const int red = image.pixels[offset] >> dropped_bits;
    const int green = image.pixels[offset + 1] >> dropped_bits;
    const int blue = image.pixels[offset + 2] >> dropped_bits;
    const bool magenta = red == full_channel && green == 0 && blue == full_channel;
    const bool black = black_is_clear && red == 0 && green == 0 && blue == 0;
    const bool clear = with_alpha && (image.pixels[offset + 3] >> dropped_bits) == 0;
    if (magenta || black || clear) {
      image.pixels[offset + 3] = 0;
    }
  }
}

// Une valeur de huit bits ramenée à la texture puis relue : ses bits de poids
// fort, recopiés dans ceux de poids faible — 31 rend 255, 1 rend 8 à cinq bits.
[[nodiscard]] std::uint8_t ThroughTextureDepth(std::uint8_t value, int dropped_bits) {
  const int kept_bits = kChannelBits - dropped_bits;
  const int kept = value >> dropped_bits;
  return static_cast<std::uint8_t>((kept << dropped_bits) | (kept >> (kept_bits - dropped_bits)));
}

// ── LE DÉTOURAGE D'UNE IMAGE SUR PAPIER ─────────────────────────────────────
//
// La BLANCHEUR MANQUANTE d'un pixel — 255 moins son canal le plus bas — dit ce
// qu'il est : presque rien pour le papier, un peu pour le liseré, beaucoup pour
// l'objet.
constexpr int kPaperGap = 24;
constexpr int kFringeGap = 96;
// Un gris d'ombre : assez clair, et presque sans couleur.
constexpr int kShadowGap = 110;
constexpr int kShadowSpread = 40;
constexpr std::size_t kPixelBytes = 4;
constexpr std::size_t kAlphaByte = 3;
constexpr std::size_t kColorBytes = 3;

[[nodiscard]] int WhitenessGap(const std::uint8_t* pixel) {
  return kChannelMax - std::min({pixel[0], pixel[1], pixel[2]});
}

[[nodiscard]] bool ShadowGrey(const std::uint8_t* pixel) {
  const int spread =
      std::max({pixel[0], pixel[1], pixel[2]}) - std::min({pixel[0], pixel[1], pixel[2]});
  return WhitenessGap(pixel) <= kShadowGap && spread <= kShadowSpread;
}

// RETIRE LE BLANC D'UN PIXEL qu'une couche d'opacité `alpha` (sur 255) a posé
// sur du papier : la couleur nette, qui sur du blanc redonne exactement celle-ci.
void Unblend(std::uint8_t* pixel, int alpha) {
  if (alpha <= 0) {
    pixel[kAlphaByte] = 0;
    return;
  }
  for (std::size_t channel = 0; channel < kColorBytes; ++channel) {
    const int clean = (kChannelMax * (pixel[channel] - kChannelMax + alpha) + alpha / 2) / alpha;
    pixel[channel] = static_cast<std::uint8_t>(std::clamp(clean, 0, kChannelMax));
  }
  pixel[kAlphaByte] = static_cast<std::uint8_t>(alpha);
}

}  // namespace

void ApplyEffectFileColorKey(Image& image) {
  ApplySixteenBitColorKey(image, /*black_is_clear=*/true);
}

void ApplyEffectTextureColorKey(Image& image) {
  ApplySixteenBitColorKey(image, /*black_is_clear=*/false);
}

void QuantizeToEffectTextureDepth(Image& image) {
  const bool with_alpha = HasAlphaChannel(image);
  const int dropped_bits = DroppedBitsOf(image);
  for (std::size_t offset = 0; offset + 3 < image.pixels.size(); offset += 4) {
    for (std::size_t channel = 0; channel < 3; ++channel) {
      image.pixels[offset + channel] =
          ThroughTextureDepth(image.pixels[offset + channel], dropped_bits);
    }
    std::uint8_t& alpha = image.pixels[offset + 3];
    alpha = with_alpha
                ? ThroughTextureDepth(alpha, dropped_bits)
                : static_cast<std::uint8_t>(alpha >= kOneBitAlphaThreshold ? kChannelMax : 0);
  }
}

void MattePaperBackground(Image& image) {
  if (image.width <= 0 || image.height <= 0) {
    return;
  }
  const auto width = static_cast<std::size_t>(image.width);
  const auto height = static_cast<std::size_t>(image.height);
  const std::size_t count = width * height;
  if (image.pixels.size() < count * kPixelBytes) {
    return;
  }
  std::uint8_t* const pixels = image.pixels.data();
  const auto pixel_at = [pixels](std::size_t index) { return pixels + index * kPixelBytes; };

  // LE FOND : presque blanc — ou déjà transparent —, en partant des bords.
  std::vector<std::uint8_t> background(count, 0);
  std::vector<std::size_t> pending;
  const auto spread_paper = [&](std::size_t index) {
    const std::uint8_t* pixel = pixel_at(index);
    if (background[index] == 0 && (pixel[kAlphaByte] == 0 || WhitenessGap(pixel) <= kPaperGap)) {
      background[index] = 1;
      pending.push_back(index);
    }
  };
  for (std::size_t x = 0; x < width; ++x) {
    spread_paper(x);
    spread_paper((height - 1) * width + x);
  }
  for (std::size_t y = 0; y < height; ++y) {
    spread_paper(y * width);
    spread_paper(y * width + width - 1);
  }
  const auto for_each_side = [width, height](std::size_t index, auto&& visit) {
    const std::size_t x = index % width;
    const std::size_t y = index / width;
    if (x > 0) {
      visit(index - 1);
    }
    if (x + 1 < width) {
      visit(index + 1);
    }
    if (y > 0) {
      visit(index - width);
    }
    if (y + 1 < height) {
      visit(index + width);
    }
  };
  while (!pending.empty()) {
    const std::size_t index = pending.back();
    pending.pop_back();
    for_each_side(index, spread_paper);
  }

  // LES OMBRES ISOLÉES : un amas de gris d'ombre que seul le fond entoure.
  std::vector<std::uint8_t> shadow(count, 0);
  std::vector<std::uint8_t> visited(count, 0);
  std::vector<std::size_t> component;
  for (std::size_t start = 0; start < count; ++start) {
    if (visited[start] != 0 || background[start] != 0 || !ShadowGrey(pixel_at(start))) {
      continue;
    }
    component.clear();
    visited[start] = 1;
    pending.push_back(start);
    bool isolated = true;
    while (!pending.empty()) {
      const std::size_t index = pending.back();
      pending.pop_back();
      component.push_back(index);
      for_each_side(index, [&](std::size_t next) {
        if (background[next] != 0) {
          return;
        }
        if (!ShadowGrey(pixel_at(next))) {
          // Il touche l'objet : c'en est une partie claire, pas une ombre.
          isolated = false;
          return;
        }
        if (visited[next] == 0) {
          visited[next] = 1;
          pending.push_back(next);
        }
      });
    }
    if (isolated) {
      for (const std::size_t index : component) {
        shadow[index] = 1;
      }
    }
  }

  // CHAQUE PIXEL PREND SA PART : le fond tombe, l'ombre et le liseré perdent
  // leur blanc, l'objet reste tel.
  const auto touches_paper = [&](std::size_t index) {
    const std::size_t x = index % width;
    const std::size_t y = index / width;
    for (std::size_t ny = y == 0 ? 0 : y - 1; ny <= std::min(y + 1, height - 1); ++ny) {
      for (std::size_t nx = x == 0 ? 0 : x - 1; nx <= std::min(x + 1, width - 1); ++nx) {
        if (background[ny * width + nx] != 0) {
          return true;
        }
      }
    }
    return false;
  };
  for (std::size_t index = 0; index < count; ++index) {
    std::uint8_t* const pixel = pixel_at(index);
    if (background[index] != 0) {
      pixel[kAlphaByte] = 0;
    } else if (shadow[index] != 0) {
      Unblend(pixel, WhitenessGap(pixel));
    } else if (const int gap = WhitenessGap(pixel); gap < kFringeGap && touches_paper(index)) {
      Unblend(pixel, gap * kChannelMax / kFringeGap);
    }
  }
}

void RemapForDarkPaper(Image& image) {
  // ⚠ UNE IMAGE D'INTERFACE A PEU DE COULEURS : chacune ne se convertit qu'une
  // fois.
  std::unordered_map<std::uint32_t, core::Rgba8> turned;
  for (std::size_t offset = 0; offset + kAlphaByte < image.pixels.size(); offset += kPixelBytes) {
    std::uint8_t* const pixel = image.pixels.data() + offset;
    const core::Rgba8 color{
        .red = pixel[0], .green = pixel[1], .blue = pixel[2], .alpha = pixel[kAlphaByte]};
    const std::uint32_t key = static_cast<std::uint32_t>(color.red) |
                              (static_cast<std::uint32_t>(color.green) << kChannelBits) |
                              (static_cast<std::uint32_t>(color.blue) << (2 * kChannelBits)) |
                              (static_cast<std::uint32_t>(color.alpha) << (3 * kChannelBits));
    const auto [known, fresh] = turned.try_emplace(key);
    if (fresh) {
      known->second = core::InkForDarkPaper(color);
    }
    pixel[0] = known->second.red;
    pixel[1] = known->second.green;
    pixel[2] = known->second.blue;
    pixel[kAlphaByte] = known->second.alpha;
  }
}

void ReduceToInkShape(Image& image) {
  // L'ENCRE : la blancheur manquante la plus répandue hors du papier. Sur une
  // marque d'une seule encre, c'est son corps ; le liseré, moins nombreux, la
  // borde à des valeurs plus faibles.
  std::array<std::size_t, kChannelMax + 1> gaps_seen = {};
  for (std::size_t offset = 0; offset + kAlphaByte < image.pixels.size(); offset += kPixelBytes) {
    const std::uint8_t* const pixel = image.pixels.data() + offset;
    if (const int gap = WhitenessGap(pixel); pixel[kAlphaByte] != 0 && gap > kPaperGap) {
      ++gaps_seen[static_cast<std::size_t>(gap)];
    }
  }
  const auto most_seen = std::ranges::max_element(gaps_seen);
  const int ink_gap = *most_seen == 0 ? 0 : static_cast<int>(most_seen - gaps_seen.begin());
  for (std::size_t offset = 0; offset + kAlphaByte < image.pixels.size(); offset += kPixelBytes) {
    std::uint8_t* const pixel = image.pixels.data() + offset;
    // ⚠ PLUS FONCÉ QUE L'ENCRE, C'EST ENCORE DE L'ENCRE : plein, sans plus.
    const int coverage =
        ink_gap == 0 ? 0 : std::min(kChannelMax, WhitenessGap(pixel) * kChannelMax / ink_gap);
    pixel[0] = kChannelMax;
    pixel[1] = kChannelMax;
    pixel[2] = kChannelMax;
    pixel[kAlphaByte] =
        static_cast<std::uint8_t>((coverage * pixel[kAlphaByte] + kChannelMax / 2) / kChannelMax);
  }
}

core::Result<Image> DecodeImage(std::string_view file_name, std::span<const std::uint8_t> data) {
  const std::size_t dot = file_name.rfind('.');
  if (dot == std::string_view::npos) {
    return core::Fail(core::ErrorCode::kUnsupportedVersion,
                      "fichier image sans extension : " + std::string(file_name));
  }
  // Les noms cités par les fichiers RO mélangent les casses librement.
  const std::string extension = core::AsciiLower(file_name.substr(dot));

  const auto from = [](core::Result<Image> decoded, ImageFile file) {
    if (decoded) {
      decoded->file = file;
    }
    return decoded;
  };
  if (extension == ".bmp") {
    return from(DecodeBmp(data), ImageFile::kBmp);
  }
  if (extension == ".tga") {
    return from(DecodeTga(data), ImageFile::kTga);
  }
  // ⚠ LE JPEG N'EST PAS UN FORMAT DE CE CLIENT : il n'y sert qu'à des
  // illustrations, qui se regardent et ne se découpent pas. Voir `jpeg.h`, et
  // la clé magenta qu'il ne faut SURTOUT pas leur appliquer.
  if (extension == ".jpg" || extension == ".jpeg") {
    return from(DecodeJpeg(data), ImageFile::kJpeg);
  }
  if (extension == ".png") {
    return from(DecodePng(data), ImageFile::kPng);
  }
  return core::Fail(core::ErrorCode::kUnsupportedVersion,
                    "format d'image " + extension + " non géré");
}

}  // namespace ro::formats
