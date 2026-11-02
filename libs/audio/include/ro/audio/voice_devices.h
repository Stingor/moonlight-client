#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"

struct SDL_AudioStream;

namespace ro::audio {

// ── LE MICRO ET LA SORTIE DE LA VOIX ────────────────────────────────────────
//
// Deux flux SDL à 48 kHz, ouverts sur les appareils que le joueur a nommés, ou
// sur ceux du système : le micro en mono, la sortie en stéréo. Ils sont à part
// de ceux de la musique et des bruits, que le mixeur de `AudioEngine` tient :
// la voix arrive déjà mêlée, et n'a besoin que d'être jouée.
//
// 🔴 SÛR D'UN AUTRE FIL : un flux SDL se verrouille lui-même, et la voix lit et
// écrit depuis le sien pendant que la partie dessine.
//
// ⚠ UN MICRO ABSENT N'EMPÊCHE PAS D'ENTENDRE : la sortie s'ouvre seule, et la
// lecture du micro rend alors zéro échantillon.
enum class VoiceDeviceKind : std::uint8_t { kMicrophone, kSpeakers };

class VoiceDevices {
 public:
  // Les noms des appareils que le système connaît, dans son ordre ; vide tant
  // que le son n'est pas ouvert. ⚠ UN NOM EST CE QUI SE GARDE : l'identifiant
  // de SDL change d'une session à l'autre.
  [[nodiscard]] static std::vector<std::string> DeviceNames(VoiceDeviceKind kind);

  // La sortie, ou une erreur si elle ne s'ouvre pas ; le micro, s'il s'ouvre.
  // Un nom vide, ou que le système ne connaît plus (casque débranché), ouvre
  // l'appareil par défaut.
  [[nodiscard]] static core::Result<VoiceDevices> Open(std::string_view microphone,
                                                       std::string_view speakers);

  VoiceDevices(VoiceDevices&&) noexcept;
  VoiceDevices& operator=(VoiceDevices&&) noexcept;
  ~VoiceDevices();

  // Mono, 48 kHz : ce que le micro a entendu depuis la dernière lecture.
  [[nodiscard]] std::size_t ReadCapture(std::span<std::int16_t> into);
  // Stéréo entrelacée, 48 kHz.
  void WritePlayback(std::span<const std::int16_t> stereo);
  // Combien d'échantillons attendent encore d'être joués.
  [[nodiscard]] std::size_t QueuedPlayback() const;
  [[nodiscard]] bool has_microphone() const noexcept { return capture_ != nullptr; }

 private:
  VoiceDevices() = default;
  void Close() noexcept;

  SDL_AudioStream* capture_ = nullptr;
  SDL_AudioStream* playback_ = nullptr;
  bool subsystem_ = false;
};

}  // namespace ro::audio
