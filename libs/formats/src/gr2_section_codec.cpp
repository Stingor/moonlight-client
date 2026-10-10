#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "ro/core/byte_reader.h"
#include "ro/formats/gr2_arith.h"
#include "ro/formats/gr2_codecs.h"

namespace ro::formats {
namespace {

// Trois plages, chacune ouverte par trois mots de paramètres.
constexpr std::size_t kRegionCount = 3;
constexpr std::size_t kWordsPerRegion = 3;
constexpr std::size_t kParameterBytes = kRegionCount * kWordsPerRegion * sizeof(std::uint32_t);

// Le premier mot : le nombre de littéraux possibles sur neuf bits, puis la
// fenêtre des copies.
constexpr std::uint32_t kNineBits = 0x1FF;
constexpr std::uint32_t kNineBitShift = 9;
// Une distance se code en deux parts : son reste modulo quatre, puis le quotient.
constexpr std::uint32_t kDistanceLowCount = 4;
constexpr std::uint32_t kDistanceLowBits = 2;

// LA LONGUEUR D'UNE COPIE. Le symbole 0 annonce un littéral ; de 1 à 60, une
// copie de symbole + 1 octets ; au-delà, quatre longueurs fixes. Le symbole
// précédent choisit le modèle du suivant.
constexpr std::uint32_t kLengthSymbolCount = 65;
constexpr std::uint32_t kFirstLongLengthSymbol = 61;
constexpr std::array<std::uint32_t, 4> kLongLengths = {128, 192, 256, 512};
// Seize modèles de longueur par octet du troisième mot, le premier octet lu
// étant celui de poids fort ; le soixante-cinquième reprend le dernier.
constexpr std::size_t kLengthModelsPerByte = 16;
constexpr std::size_t kLengthCapacityBytes = 4;
constexpr std::uint32_t kBitsPerByte = 8;
constexpr std::uint32_t kByteMask = 0xFF;
constexpr std::uint32_t kHighestByteShift = 24;

struct Region {
  std::uint32_t literal_count = 0;
  std::uint32_t window = 0;
  std::uint32_t low_count = 0;
  std::vector<Gr2AdaptiveModel> lengths;
  Gr2AdaptiveModel literals{0};
  Gr2AdaptiveModel distance_low{0};
  Gr2AdaptiveModel distance_high{0};
};

Region OpenRegion(std::uint32_t first, std::uint32_t second, std::uint32_t third) {
  Region region;
  region.literal_count = first & kNineBits;
  region.window = first >> kNineBitShift;
  region.low_count = std::min(region.window, kDistanceLowCount);
  region.literals = Gr2AdaptiveModel(second & kNineBits);
  std::uint32_t capacity = 0;
  for (std::size_t byte = 0; byte < kLengthCapacityBytes; ++byte) {
    capacity = (third >> (kHighestByteShift - kBitsPerByte * byte)) & kByteMask;
    for (std::size_t copy = 0; copy < kLengthModelsPerByte; ++copy) {
      region.lengths.emplace_back(capacity);
    }
  }
  region.lengths.emplace_back(capacity);
  region.distance_low = Gr2AdaptiveModel(region.low_count);
  region.distance_high = Gr2AdaptiveModel(second >> kNineBitShift);
  return region;
}

// Un symbole du modèle, ou, s'il est neuf, sa valeur tirée uniformément.
std::uint32_t DecodeSymbol(Gr2AdaptiveModel& model, Gr2RangeDecoder& decoder,
                           std::uint32_t escape_count) {
  const Gr2AdaptiveModel::Decoded decoded = model.Decode(decoder);
  if (!decoded.escaped) {
    return decoded.value;
  }
  const auto value = static_cast<std::uint16_t>(decoder.Uniform(escape_count));
  model.Learn(value);
  return value;
}

}  // namespace

core::Result<std::vector<std::uint8_t>> DecompressGr2Section(
    std::span<const std::uint8_t> packed, const std::array<std::uint32_t, 2>& stops,
    std::uint32_t unpacked_size) {
  if (packed.size() < kParameterBytes) {
    return core::Fail(core::ErrorCode::kUnexpectedEnd, "section Granny : paramètres tronqués");
  }
  core::ByteReader parameters(packed.first(kParameterBytes));
  std::array<std::uint32_t, kRegionCount * kWordsPerRegion> words{};
  for (std::uint32_t& word : words) {
    word = parameters.U32();
  }
  const std::array<std::uint32_t, kRegionCount> ends = {
      std::min(stops[0], unpacked_size), std::min(stops[1], unpacked_size), unpacked_size};
  Gr2RangeDecoder decoder(packed, kParameterBytes);
  std::vector<std::uint8_t> out;
  out.reserve(unpacked_size);
  for (std::size_t index = 0; index < kRegionCount; ++index) {
    Region region = OpenRegion(words[index * kWordsPerRegion], words[index * kWordsPerRegion + 1],
                               words[index * kWordsPerRegion + 2]);
    std::uint32_t produced = 0;
    std::uint32_t previous = 0;
    while (out.size() < ends[index]) {
      const std::uint32_t symbol =
          DecodeSymbol(region.lengths[previous], decoder, kLengthSymbolCount);
      if (symbol >= kLengthSymbolCount) {
        return core::Fail(core::ErrorCode::kCorruptData, "section Granny : longueur hors bornes");
      }
      previous = symbol;
      if (symbol == 0) {
        out.push_back(static_cast<std::uint8_t>(
            DecodeSymbol(region.literals, decoder, region.literal_count)));
        ++produced;
        continue;
      }
      const std::uint32_t length = symbol < kFirstLongLengthSymbol
                                       ? symbol + 1
                                       : kLongLengths[symbol - kFirstLongLengthSymbol];
      const std::uint32_t low = DecodeSymbol(region.distance_low, decoder, region.low_count);
      const std::uint32_t high =
          DecodeSymbol(region.distance_high, decoder, (std::min(region.window, produced) >> 2) + 1);
      produced += length;
      const std::size_t distance = (std::size_t{high} << kDistanceLowBits) + low + 1;
      if (distance > out.size() || out.size() + length > unpacked_size) {
        return core::Fail(
            core::ErrorCode::kCorruptData,
            "section Granny : copie hors de la sortie, à l'octet " + std::to_string(out.size()));
      }
      // Octet par octet : une copie peut relire ce qu'elle vient d'écrire.
      for (std::uint32_t step = 0; step < length; ++step) {
        const std::uint8_t copied = out[out.size() - distance];
        out.push_back(copied);
      }
    }
  }
  out.resize(unpacked_size);
  return out;
}

}  // namespace ro::formats
