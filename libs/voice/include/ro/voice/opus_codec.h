#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include "ro/core/error.h"

// Les états d'Opus, déclarés comme `opus.h` les déclare : son en-tête ne sort
// pas de cette couche.
struct OpusEncoder;
struct OpusDecoder;

namespace ro::voice {

// LA VOIX SUR LE FIL : mono, 48 kHz, une trame de 20 ms par paquet. Le serveur
// refuse toute autre durée que 10 ou 20 ms.
inline constexpr int kVoiceSampleRate = 48000;
inline constexpr std::size_t kVoiceFrameSamples = 960;
inline constexpr std::uint32_t kVoiceFrameMs = 20;

// Le débit visé : la parole y est claire, et un paquet tient loin sous la
// taille d'un datagramme sûr.
inline constexpr int kVoiceBitrate = 24000;
// La perte que l'encodeur prépare : ses paquets portent assez de la trame
// précédente pour la reconstruire quand celle-ci se perd.
inline constexpr int kVoiceExpectedLossPercent = 10;

class OpusVoiceEncoder {
 public:
  [[nodiscard]] static core::Result<OpusVoiceEncoder> Create();

  // Une trame de `kVoiceFrameSamples` échantillons, encodée.
  [[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(std::span<const std::int16_t> frame);

 private:
  struct Release {
    void operator()(OpusEncoder* state) const noexcept;
  };
  std::unique_ptr<OpusEncoder, Release> state_;
};

class OpusVoiceDecoder {
 public:
  [[nodiscard]] static core::Result<OpusVoiceDecoder> Create();

  // Un paquet décodé dans `frame`, de `kVoiceFrameSamples` échantillons.
  [[nodiscard]] core::Result<void> Decode(std::span<const std::uint8_t> packet,
                                          std::span<std::int16_t> frame);
  // UNE TRAME PERDUE, reconstruite. `next` est le paquet qui la suit, s'il est
  // déjà là : il porte de quoi la refaire fidèlement. Sinon, le décodeur
  // prolonge ce qu'il entendait.
  [[nodiscard]] core::Result<void> Conceal(std::span<const std::uint8_t> next,
                                           std::span<std::int16_t> frame);

 private:
  struct Release {
    void operator()(OpusDecoder* state) const noexcept;
  };
  std::unique_ptr<OpusDecoder, Release> state_;
};

}  // namespace ro::voice
