#include "ro/audio/sound_onset.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string_view>

namespace ro::audio {
namespace {

// L'en-tête RIFF : « RIFF », la taille, « WAVE ». Les blocs suivent.
constexpr std::size_t kRiffHeaderBytes = 12;
constexpr std::size_t kRiffFormOffset = 8;
// Un bloc : quatre octets de nom, quatre de taille, puis le contenu, complété
// à une longueur paire.
constexpr std::size_t kChunkHeaderBytes = 8;
constexpr std::size_t kChunkNameBytes = 4;
// Dans le bloc « fmt » : l'étiquette du codage, les voies, puis, à l'octet
// quatorze, les bits par échantillon.
constexpr std::size_t kFormatTagOffset = 0;
constexpr std::size_t kChannelsOffset = 2;
constexpr std::size_t kBitsOffset = 14;
constexpr std::size_t kFormatMinBytes = 16;
constexpr std::uint16_t kPcmFormatTag = 1;

constexpr std::uint16_t kEightBits = 8;
constexpr std::uint16_t kSixteenBits = 16;
// Le PCM 8 bits est non signé, centré sur 128 ; le 16 bits est signé.
constexpr float kEightBitCenter = 128.0F;
constexpr float kEightBitScale = 128.0F;
constexpr float kSixteenBitScale = 32768.0F;
constexpr unsigned kBitsPerByte = 8;

std::uint32_t ReadLittle(std::span<const std::uint8_t> bytes, std::size_t offset,
                         std::size_t width) {
  std::uint32_t value = 0;
  for (std::size_t index = 0; index < width; ++index) {
    value |= static_cast<std::uint32_t>(bytes[offset + index]) << (kBitsPerByte * index);
  }
  return value;
}

bool NameIs(std::span<const std::uint8_t> bytes, std::size_t offset, std::string_view name) {
  return std::equal(
      name.begin(), name.end(), bytes.begin() + static_cast<std::ptrdiff_t>(offset),
      [](char letter, std::uint8_t byte) { return static_cast<std::uint8_t>(letter) == byte; });
}

}  // namespace

bool SameOnset(std::uint64_t first_ms, std::uint64_t now_ms) {
  return now_ms >= first_ms && now_ms - first_ms <= kOnsetMergeWindowMs;
}

MergedOnset MergeOnset(float held_gain, float held_pan, float added_gain, float added_pan) {
  const float held_energy = held_gain * held_gain;
  const float added_energy = added_gain * added_gain;
  const float energy = held_energy + added_energy;
  if (!(energy > 0.0F)) {
    return {.gain = 0.0F, .pan = added_pan};
  }
  return {.gain = std::sqrt(energy),
          .pan = (held_energy * held_pan + added_energy * added_pan) / energy};
}

float OpeningStep(std::span<const std::uint8_t> wav_bytes) {
  if (wav_bytes.size() < kRiffHeaderBytes || !NameIs(wav_bytes, 0, "RIFF") ||
      !NameIs(wav_bytes, kRiffFormOffset, "WAVE")) {
    return 0.0F;
  }
  std::uint16_t channels = 0;
  std::uint16_t bits = 0;
  bool pcm = false;
  std::size_t chunk = kRiffHeaderBytes;
  while (chunk + kChunkHeaderBytes <= wav_bytes.size()) {
    const std::size_t body = chunk + kChunkHeaderBytes;
    const std::size_t size = ReadLittle(wav_bytes, chunk + kChunkNameBytes, sizeof(std::uint32_t));
    const std::size_t available = std::min(size, wav_bytes.size() - body);
    if (NameIs(wav_bytes, chunk, "fmt ") && available >= kFormatMinBytes) {
      pcm = ReadLittle(wav_bytes, body + kFormatTagOffset, sizeof(std::uint16_t)) == kPcmFormatTag;
      channels = static_cast<std::uint16_t>(
          ReadLittle(wav_bytes, body + kChannelsOffset, sizeof(std::uint16_t)));
      bits = static_cast<std::uint16_t>(
          ReadLittle(wav_bytes, body + kBitsOffset, sizeof(std::uint16_t)));
    } else if (NameIs(wav_bytes, chunk, "data")) {
      if (!pcm || channels == 0 || (bits != kEightBits && bits != kSixteenBits)) {
        return 0.0F;
      }
      const std::size_t sample_bytes = bits / kBitsPerByte;
      const std::size_t frame_bytes = sample_bytes * channels;
      if (available < frame_bytes) {
        return 0.0F;
      }
      float step = 0.0F;
      for (std::size_t lane = 0; lane < channels; ++lane) {
        const std::size_t at = body + lane * sample_bytes;
        const float sample =
            bits == kEightBits
                ? (static_cast<float>(wav_bytes[at]) - kEightBitCenter) / kEightBitScale
                : static_cast<float>(
                      static_cast<std::int16_t>(ReadLittle(wav_bytes, at, sizeof(std::uint16_t)))) /
                      kSixteenBitScale;
        step = std::max(step, std::abs(sample));
      }
      return step;
    }
    // Un bloc de longueur impaire est complété d'un octet.
    chunk = body + size + (size & 1U);
  }
  return 0.0F;
}

}  // namespace ro::audio
