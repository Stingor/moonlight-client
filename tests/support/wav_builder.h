#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace ro::test_support {

// Construit un fichier WAV PCM EN MÉMOIRE, comme ceux de l'archive : aucun
// bruit du jeu n'est versionné, les essais fabriquent le leur.
//
// `samples` est une voie par trame en 16 bits signés, ou en 8 bits non signés
// centrés sur 128 quand `bits` vaut 8 — chaque valeur est alors tronquée à son
// octet de poids faible.

inline void AppendLittle(std::vector<std::uint8_t>& out, std::uint32_t value, std::size_t width) {
  constexpr unsigned kBitsPerByte = 8;
  constexpr std::uint32_t kByteMask = 0xFF;
  for (std::size_t index = 0; index < width; ++index) {
    out.push_back(static_cast<std::uint8_t>((value >> (kBitsPerByte * index)) & kByteMask));
  }
}

inline void AppendTag(std::vector<std::uint8_t>& out, std::string_view tag) {
  for (const char letter : tag) {
    out.push_back(static_cast<std::uint8_t>(letter));
  }
}

inline std::vector<std::uint8_t> BuildWav(std::span<const std::int16_t> samples, int rate_hz,
                                          std::uint16_t channels = 1, std::uint16_t bits = 16,
                                          std::uint16_t format_tag = 1) {
  constexpr std::uint32_t kFormatBytes = 16;
  constexpr unsigned kBitsPerByte = 8;
  constexpr std::uint16_t kEightBits = 8;
  const std::uint32_t sample_bytes = bits / kBitsPerByte;
  const auto data_bytes = static_cast<std::uint32_t>(samples.size() * sample_bytes);
  // « WAVE », puis l'en-tête et le contenu de « fmt » et de « data ».
  constexpr std::uint32_t kFormTagBytes = 4;
  constexpr std::uint32_t kChunkHeaderBytes = 8;
  const std::uint32_t riff_bytes =
      kFormTagBytes + kChunkHeaderBytes + kFormatBytes + kChunkHeaderBytes + data_bytes;

  std::vector<std::uint8_t> out;
  AppendTag(out, "RIFF");
  AppendLittle(out, riff_bytes, sizeof(std::uint32_t));
  AppendTag(out, "WAVE");
  AppendTag(out, "fmt ");
  AppendLittle(out, kFormatBytes, sizeof(std::uint32_t));
  AppendLittle(out, format_tag, sizeof(std::uint16_t));
  AppendLittle(out, channels, sizeof(std::uint16_t));
  AppendLittle(out, static_cast<std::uint32_t>(rate_hz), sizeof(std::uint32_t));
  AppendLittle(out, static_cast<std::uint32_t>(rate_hz) * channels * sample_bytes,
               sizeof(std::uint32_t));
  AppendLittle(out, channels * sample_bytes, sizeof(std::uint16_t));
  AppendLittle(out, bits, sizeof(std::uint16_t));
  AppendTag(out, "data");
  AppendLittle(out, data_bytes, sizeof(std::uint32_t));
  for (const std::int16_t sample : samples) {
    AppendLittle(out, static_cast<std::uint16_t>(sample), bits == kEightBits ? 1 : 2);
  }
  return out;
}

}  // namespace ro::test_support
