#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "ro/core/byte_writer.h"
#include "ro/formats/gr2_arith.h"

#include "support/gr2_builder.h"

namespace ro::test_support {
namespace {

constexpr std::uint32_t kCodeMask = 0x7FFFFFFFU;
constexpr std::uint32_t kTopBit = 0x40000000U;
constexpr std::uint32_t kSecondBit = 0x20000000U;
constexpr std::uint32_t kTopBitShift = 30;
// De quoi couvrir les 31 bits que le décodeur lit d'avance.
constexpr std::size_t kTrailingBytes = 8;

// Les paramètres d'une plage : 256 littéraux, une fenêtre de 64 Kio, et 65
// symboles de longueur pour chacun des modèles.
constexpr std::uint32_t kLiteralCount = 256;
constexpr std::uint32_t kWindow = 1U << 16;
constexpr std::uint32_t kNineBitShift = 9;
constexpr std::uint32_t kLengthSymbols = 65;
constexpr std::uint32_t kLengthCapacities = 0x41414141U;
constexpr std::uint32_t kShortestCopy = 2;
constexpr std::uint32_t kLongestCopy = 61;
constexpr std::size_t kSearchedDistance = 4096;
constexpr std::uint32_t kDistanceLowCount = 4;

}  // namespace

void Gr2RangeEncoder::Emit(std::uint32_t bit) {
  if (bit_count_ % 8 == 0) {
    bytes_.push_back(0);
  }
  bytes_.back() = static_cast<std::uint8_t>(bytes_.back() | (bit << (bit_count_ % 8)));
  ++bit_count_;
}

void Gr2RangeEncoder::EmitWithPending(std::uint32_t bit) {
  Emit(bit);
  for (; pending_ > 0; --pending_) {
    Emit(bit ^ 1U);
  }
}

void Gr2RangeEncoder::Encode(const formats::Gr2Interval& interval) {
  const std::uint64_t span = std::uint64_t{high_} - low_ + 1;
  std::uint32_t high = static_cast<std::uint32_t>(
      low_ + (std::uint64_t{interval.cumulative} + interval.frequency) * span / interval.total - 1);
  std::uint32_t low =
      static_cast<std::uint32_t>(low_ + std::uint64_t{interval.cumulative} * span / interval.total);
  while (((high ^ low) & kTopBit) == 0) {
    EmitWithPending((low >> kTopBitShift) & 1U);
    low = (low << 1) & kCodeMask;
    high = ((high << 1) | 1U) & kCodeMask;
  }
  while ((low & kSecondBit) != 0 && (high & kSecondBit) == 0) {
    ++pending_;
    low = (low & (kSecondBit - 1)) << 1;
    high = ((high << 1) | kTopBit | 1U) & kCodeMask;
  }
  low_ = low;
  high_ = high;
}

void Gr2RangeEncoder::Symbol(formats::Gr2AdaptiveModel& model, std::uint16_t value,
                             std::uint32_t escape_count) {
  const formats::Gr2AdaptiveModel::Encoded encoded = model.Encode(value);
  Encode(encoded.interval);
  if (encoded.escaped) {
    Uniform(value, escape_count);
  }
}

std::vector<std::uint8_t> Gr2RangeEncoder::Finish() {
  // Deux bits désignent un quart entier de l'intervalle : quoi que le
  // décodeur lise ensuite, son code y reste.
  ++pending_;
  EmitWithPending((low_ & kSecondBit) != 0 ? 1U : 0U);
  bytes_.resize(bytes_.size() + kTrailingBytes, 0);
  return bytes_;
}

std::vector<std::uint8_t> CompressGr2Section(std::span<const std::uint8_t> data,
                                             const std::array<std::uint32_t, 2>& stops) {
  std::vector<std::uint8_t> packed;
  core::ByteWriter out(packed);
  const std::uint32_t first = kLiteralCount | (kWindow << kNineBitShift);
  const std::uint32_t second = kLiteralCount | (((kWindow >> 2) + 1) << kNineBitShift);
  for (std::size_t region = 0; region < 3; ++region) {
    out.U32(first);
    out.U32(second);
    out.U32(kLengthCapacities);
  }
  Gr2RangeEncoder encoder;
  const std::array<std::size_t, 3> ends = {std::min<std::size_t>(stops[0], data.size()),
                                           std::min<std::size_t>(stops[1], data.size()),
                                           data.size()};
  std::size_t position = 0;
  for (const std::size_t end : ends) {
    std::vector<formats::Gr2AdaptiveModel> lengths(kLengthSymbols,
                                                   formats::Gr2AdaptiveModel(kLengthSymbols));
    formats::Gr2AdaptiveModel literals(kLiteralCount);
    formats::Gr2AdaptiveModel low_model(kDistanceLowCount);
    formats::Gr2AdaptiveModel high_model((kWindow >> 2) + 1);
    const std::size_t start = position;
    std::uint16_t previous = 0;
    while (position < end) {
      const std::size_t produced = position - start;
      std::size_t best_length = 0;
      std::size_t best_distance = 0;
      const std::size_t reach = std::min({produced, kSearchedDistance, std::size_t{kWindow}});
      for (std::size_t distance = 1; distance <= reach; ++distance) {
        std::size_t length = 0;
        while (length < kLongestCopy && position + length < data.size() &&
               data[position + length] == data[position + length - distance]) {
          ++length;
        }
        if (length > best_length) {
          best_length = length;
          best_distance = distance;
        }
      }
      if (best_length >= kShortestCopy) {
        const auto symbol = static_cast<std::uint16_t>(best_length - 1);
        encoder.Symbol(lengths[previous], symbol, kLengthSymbols);
        previous = symbol;
        const std::size_t offset = best_distance - 1;
        encoder.Symbol(low_model, static_cast<std::uint16_t>(offset & 3U), kDistanceLowCount);
        encoder.Symbol(
            high_model, static_cast<std::uint16_t>(offset >> 2),
            static_cast<std::uint32_t>((std::min<std::size_t>(kWindow, produced) >> 2) + 1));
        position += best_length;
      } else {
        encoder.Symbol(lengths[previous], 0, kLengthSymbols);
        previous = 0;
        encoder.Symbol(literals, data[position], kLiteralCount);
        ++position;
      }
    }
  }
  out.Bytes(encoder.Finish());
  return packed;
}

std::vector<std::uint8_t> EncodeFlatGr2Texture(std::uint32_t height,
                                               std::span<const std::int16_t> coarse) {
  constexpr std::uint32_t kLevels = 4;
  constexpr std::uint32_t kBandsPerLevel = 3;
  constexpr std::uint32_t kWordBits = 16;
  const std::uint32_t padded_height = (height + 15) & ~15U;
  std::vector<std::uint8_t> stream;
  core::ByteWriter out(stream);
  out.U32(0);  // l'espace de travail du décodeur d'origine
  for (const std::int16_t value : coarse) {
    // Le flux en clair : la bande grossière unie, puis douze bandes de
    // détails unies à zéro.
    std::vector<std::uint8_t> raw;
    std::size_t bits = 0;
    const auto put = [&](std::uint32_t word, std::uint32_t count) {
      for (std::uint32_t index = 0; index < count; ++index, ++bits) {
        if (bits % 8 == 0) {
          raw.push_back(0);
        }
        raw.back() = static_cast<std::uint8_t>(raw.back() | (((word >> index) & 1U) << (bits % 8)));
      }
    };
    put(1, 1);
    put(static_cast<std::uint16_t>(value), kWordBits);
    for (std::uint32_t band = 0; band < kLevels * kBandsPerLevel; ++band) {
      put(1, kWordBits);
      put(1, 1);
      put(0, kWordBits);
    }
    // Le flux arithmétique : le seuil des drapeaux à zéro, si bien que chaque
    // rangée garde sa moitié haute.
    Gr2RangeEncoder coded;
    coded.Uniform(0, padded_height + 1);
    for (std::uint32_t row = 0; row < padded_height; ++row) {
      coded.Encode({0, padded_height, padded_height});
    }
    const std::vector<std::uint8_t> coded_bytes = coded.Finish();
    out.U32(static_cast<std::uint32_t>(coded_bytes.size()));
    out.U32(static_cast<std::uint32_t>(raw.size()));
    out.Bytes(coded_bytes);
    out.Bytes(raw);
  }
  return stream;
}

}  // namespace ro::test_support
