#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "ro/core/byte_reader.h"
#include "ro/formats/gr2_codecs.h"

#include "gr2_image_planes.h"

namespace ro::formats {
namespace gr2 {
namespace {

// LE FILTRE DE SYNTHÈSE LONG, en virgule fixe sur seize bits. Le pixel pair
// part de la valeur basse, corrigée par ses voisines basses et par les hautes
// qui l'encadrent ; le pixel impair part de la moyenne des deux basses qui
// l'encadrent, corrigée par les hautes.
constexpr std::int64_t kEvenCenter = 51674;
constexpr std::int64_t kEvenHighNear = 24733;
constexpr std::int64_t kEvenLowFar = -2667;
constexpr std::int64_t kEvenHighFar = -1563;
constexpr std::int64_t kOddLowNear = 27400;
constexpr std::int64_t kOddHighNear = 7250;
constexpr std::int64_t kOddLowFar = -4230;
constexpr std::int64_t kOddHighFar = -2479;
constexpr std::int64_t kOddCenter = -55882;

// Le filtre long demande au moins six paires ; en deçà d'une largeur de douze
// ou d'une hauteur de dix, on passe au filtre court.
constexpr std::uint32_t kShortestLongRow = 12;
constexpr std::uint32_t kShortestLongColumn = 10;

// L'arrondi du filtre long : au plus proche, les moitiés vers zéro du côté
// positif et loin de zéro du côté négatif.
constexpr std::int32_t kFixedPointOne = 0x10000;
constexpr std::uint32_t kPositiveRounding = 0x7FFF;
constexpr std::uint32_t kNegativeRounding = 0xFFFF8000U;
constexpr std::uint32_t kSignShift = 31;

// Les calculs se font sur trente-deux bits, avec débordement : une somme se
// replie comme elle le ferait dans un registre.
std::int32_t Wrap32(std::int64_t value) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(value));
}

std::int16_t Wrap16(std::int32_t value) {
  return static_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

std::int16_t RoundFixed(std::int64_t sum) {
  const std::int32_t value = Wrap32(sum);
  const std::uint32_t rounding = value < 0 ? kNegativeRounding : kPositiveRounding;
  const std::int32_t biased = Wrap32(std::int64_t{static_cast<std::uint32_t>(value) + rounding});
  return Wrap16(biased / kFixedPointOne);
}

// L'arrondi du filtre court : la moitié, arrondie vers le haut pour une somme
// positive, et d'une unité plus bas pour une somme négative.
std::int16_t HalfUp(std::int32_t sum) {
  const std::int32_t bias = (sum >> kSignShift) ^ 1;
  return Wrap16((sum + bias) / 2);
}

// Une suite de coefficients bas et hauts, prolongée en miroir aux deux bouts :
// la basse autour de son premier échantillon et au-delà de son dernier, la
// haute autour du point qui précède son premier et au-delà de son dernier.
class Halves {
 public:
  Halves(std::span<const std::int16_t> low, std::span<const std::int16_t> high, bool use_high)
      : low_(low), high_(high), use_high_(use_high) {}

  [[nodiscard]] std::int64_t Low(std::int64_t index) const {
    const auto count = static_cast<std::int64_t>(low_.size());
    if (index < 0) {
      index = -index;
    }
    if (index >= count) {
      index = 2 * count - 1 - index;
    }
    return low_[static_cast<std::size_t>(std::clamp<std::int64_t>(index, 0, count - 1))];
  }
  [[nodiscard]] std::int64_t High(std::int64_t index) const {
    if (!use_high_) {
      return 0;
    }
    const auto count = static_cast<std::int64_t>(high_.size());
    if (index < 0) {
      index = -index - 1;
    }
    if (index >= count) {
      index = 2 * count - 2 - index;
    }
    return high_[static_cast<std::size_t>(std::clamp<std::int64_t>(index, 0, count - 1))];
  }

 private:
  std::span<const std::int16_t> low_;
  std::span<const std::int16_t> high_;
  bool use_high_;
};

// Recompose une suite entrelacée — pair, impair — de ses deux moitiés.
void Synthesize(std::span<const std::int16_t> low, std::span<const std::int16_t> high,
                bool use_high, bool long_filter, std::span<std::int16_t> out) {
  const Halves halves(low, high, use_high);
  const auto count = static_cast<std::int64_t>(low.size());
  for (std::int64_t index = 0; index < count; ++index) {
    std::int16_t even = 0;
    std::int16_t odd = 0;
    if (long_filter) {
      even = RoundFixed(kEvenCenter * halves.Low(index) +
                        kEvenHighNear * (halves.High(index - 1) + halves.High(index)) +
                        kEvenLowFar * (halves.Low(index - 1) + halves.Low(index + 1)) +
                        kEvenHighFar * (halves.High(index - 2) + halves.High(index + 1)));
      odd = RoundFixed(kOddHighNear * (halves.High(index - 1) + halves.High(index + 1)) +
                       kOddLowNear * (halves.Low(index) + halves.Low(index + 1)) +
                       kOddLowFar * (halves.Low(index - 1) + halves.Low(index + 2)) +
                       kOddHighFar * (halves.High(index - 2) + halves.High(index + 2)) +
                       kOddCenter * halves.High(index));
    } else if (use_high) {
      const auto twice_low = static_cast<std::int32_t>(2 * halves.Low(index));
      const auto difference = static_cast<std::int32_t>(halves.High(index));
      even = HalfUp(twice_low + difference);
      odd = HalfUp(twice_low - difference);
    } else {
      even = static_cast<std::int16_t>(halves.Low(index));
      odd = even;
    }
    out[static_cast<std::size_t>(2 * index)] = even;
    out[static_cast<std::size_t>(2 * index + 1)] = odd;
  }
}

// Remonte un niveau : une image de `width` sur `height`, dont les rangées se
// posent tous les `step` rangs du plan.
void SynthesizeLevel(Gr2Plane& plane, std::uint32_t step, std::uint32_t width, std::uint32_t height,
                     std::span<const std::uint8_t> row_flags) {
  const std::size_t pitch = std::size_t{step} * plane.width;
  const std::uint32_t half_width = width / 2;
  const std::uint32_t half_height = height / 2;
  std::vector<std::int16_t> rows(std::size_t{width} * height);
  for (std::uint32_t y = 0; y < height; ++y) {
    const std::span<const std::int16_t> source(plane.samples.data() + y * pitch, width);
    const bool use_high = row_flags.empty() || (y < row_flags.size() && row_flags[y] != 0);
    Synthesize(source.first(half_width), source.subspan(half_width, half_width), use_high,
               width >= kShortestLongRow,
               std::span<std::int16_t>(rows.data() + std::size_t{y} * width, width));
  }
  std::vector<std::int16_t> low(half_height);
  std::vector<std::int16_t> high(half_height);
  std::vector<std::int16_t> column(height);
  for (std::uint32_t x = 0; x < width; ++x) {
    for (std::uint32_t pair = 0; pair < half_height; ++pair) {
      low[pair] = rows[std::size_t{2 * pair} * width + x];
      high[pair] = rows[std::size_t{2 * pair + 1} * width + x];
    }
    Synthesize(low, high, true, height >= kShortestLongColumn, column);
    for (std::uint32_t y = 0; y < height; ++y) {
      plane.samples[y * pitch + x] = column[y];
    }
  }
}

}  // namespace

void SynthesizeGr2Plane(Gr2Plane& plane, std::span<const std::uint8_t> row_flags) {
  for (std::uint32_t level = kGr2WaveletLevels; level >= 1; --level) {
    const std::uint32_t finer = level - 1;
    SynthesizeLevel(plane, 1U << finer, plane.width >> finer, plane.height >> finer,
                    finer == 0 ? row_flags : std::span<const std::uint8_t>{});
  }
}

}  // namespace gr2

namespace {

// Les dimensions se codent arrondies au multiple de seize.
constexpr std::uint32_t kDimensionAlignment = 16;
constexpr std::uint32_t kColorPlanes = 3;
constexpr std::uint32_t kColorAndAlphaPlanes = 4;
constexpr std::size_t kRgbaBytes = 4;
constexpr std::size_t kRgbBytes = 3;
constexpr std::uint8_t kOpaque = 255;
constexpr std::int32_t kChannelMax = 255;
// Le vert se déduit de la luminance et du quart des deux chrominances.
constexpr std::int32_t kChromaDivisor = -4;
// Le flux s'ouvre sur la taille de l'espace de travail du décodeur, qu'on
// n'emploie pas.
constexpr std::size_t kScratchSizeBytes = 4;
// LE PLUS GRAND CÔTÉ ADMIS. Les textures des modèles du jeu en sont loin ; au
// format, un côté va jusqu'à 32 767, et une texture de quelques octets en
// annonçant deux demandait une dizaine de gigaoctets — sa texture RGBA et ses
// plans — avant le premier contrôle de son flux. Refusée, la texture laisse
// le maillage sans peinture, rien de plus.
constexpr std::int32_t kLargestTextureSide = 4096;

std::uint32_t AlignDimension(std::int32_t value) {
  return (static_cast<std::uint32_t>(value) + kDimensionAlignment - 1) & ~(kDimensionAlignment - 1);
}

std::uint8_t Channel(std::int32_t value) {
  return static_cast<std::uint8_t>(std::clamp(value, 0, kChannelMax));
}

}  // namespace

core::Result<std::vector<std::uint8_t>> DecodeGr2WaveletTexture(std::span<const std::uint8_t> data,
                                                                std::int32_t width,
                                                                std::int32_t height,
                                                                bool has_alpha) {
  if (width <= 0 || height <= 0 || width > kLargestTextureSide || height > kLargestTextureSide) {
    return core::Fail(core::ErrorCode::kCorruptData, "texture Granny : dimensions invalides");
  }
  const std::size_t pixel_count =
      static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
  if (pixel_count <= static_cast<std::size_t>(kGr2LargestRawTexturePixels)) {
    std::vector<std::uint8_t> rgba(pixel_count * kRgbaBytes);
    const std::size_t stride = has_alpha ? kRgbaBytes : kRgbBytes;
    if (data.size() < pixel_count * stride) {
      return core::Fail(core::ErrorCode::kUnexpectedEnd, "texture Granny : pixels tronqués");
    }
    for (std::size_t pixel = 0; pixel < pixel_count; ++pixel) {
      for (std::size_t channel = 0; channel < kRgbBytes; ++channel) {
        rgba[pixel * kRgbaBytes + channel] = data[pixel * stride + channel];
      }
      rgba[pixel * kRgbaBytes + kRgbBytes] = has_alpha ? data[pixel * stride + kRgbBytes] : kOpaque;
    }
    return rgba;
  }
  if (data.size() < kScratchSizeBytes) {
    return core::Fail(core::ErrorCode::kUnexpectedEnd, "texture Granny : flux vide");
  }
  const std::uint32_t plane_count = has_alpha ? kColorAndAlphaPlanes : kColorPlanes;
  std::array<gr2::Gr2Plane, kColorAndAlphaPlanes> planes;
  std::vector<std::uint8_t> row_flags;
  std::size_t offset = kScratchSizeBytes;
  for (std::uint32_t index = 0; index < plane_count; ++index) {
    gr2::Gr2Plane& plane = planes[index];
    plane.width = AlignDimension(width);
    plane.height = AlignDimension(height);
    auto consumed = gr2::DecodeGr2PlaneBands(data.subspan(offset), plane, row_flags);
    if (!consumed) {
      return std::unexpected(consumed.error());
    }
    gr2::SynthesizeGr2Plane(plane, row_flags);
    offset += *consumed;
  }
  // ⚠ ALLOUÉE APRÈS LES PLANS : un flux qui ne les porte pas échoue avant.
  std::vector<std::uint8_t> rgba(pixel_count * kRgbaBytes);
  const std::size_t pitch = planes[0].width;
  for (std::size_t y = 0; y < static_cast<std::size_t>(height); ++y) {
    for (std::size_t x = 0; x < static_cast<std::size_t>(width); ++x) {
      const std::size_t sample = y * pitch + x;
      const std::int32_t luma = planes[0].samples[sample];
      const std::int32_t red_chroma = planes[1].samples[sample];
      const std::int32_t blue_chroma = planes[2].samples[sample];
      const std::int32_t green = (red_chroma + blue_chroma) / kChromaDivisor + luma;
      std::uint8_t* pixel = rgba.data() + (y * static_cast<std::size_t>(width) + x) * kRgbaBytes;
      pixel[0] = Channel(green + red_chroma);
      pixel[1] = Channel(green);
      pixel[2] = Channel(green + blue_chroma);
      pixel[kRgbBytes] = has_alpha ? Channel(planes[kColorPlanes].samples[sample]) : kOpaque;
    }
  }
  return rgba;
}

}  // namespace ro::formats
