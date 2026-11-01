#include "ro/audio/voice_devices.h"

#include <string>
#include <utility>

#include <SDL3/SDL.h>

#include "ro/core/log.h"

namespace ro::audio {
namespace {

using core::ErrorCode;
using core::Fail;

// Le débit de la voix sur le fil : SDL convertit vers celui de l'appareil.
constexpr int kVoiceRate = 48000;
constexpr int kCaptureChannels = 1;
constexpr int kPlaybackChannels = 2;
constexpr std::size_t kSampleBytes = sizeof(std::int16_t);

}  // namespace

core::Result<VoiceDevices> VoiceDevices::Open() {
  VoiceDevices devices;
  // ⚠ COMPTÉ PAR SDL : la musique a déjà ouvert le son, et chacun le referme
  // pour son compte.
  if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
    return Fail(ErrorCode::kIoFailure, std::string("son de la voix : ") + SDL_GetError());
  }
  devices.subsystem_ = true;

  const SDL_AudioSpec playback{
      .format = SDL_AUDIO_S16, .channels = kPlaybackChannels, .freq = kVoiceRate};
  devices.playback_ =
      SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &playback, nullptr, nullptr);
  if (devices.playback_ == nullptr) {
    return Fail(ErrorCode::kIoFailure, std::string("sortie de la voix : ") + SDL_GetError());
  }
  SDL_ResumeAudioStreamDevice(devices.playback_);

  const SDL_AudioSpec capture{
      .format = SDL_AUDIO_S16, .channels = kCaptureChannels, .freq = kVoiceRate};
  devices.capture_ =
      SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_RECORDING, &capture, nullptr, nullptr);
  if (devices.capture_ == nullptr) {
    core::LogWarning("voix : pas de micro ({})", SDL_GetError());
  } else {
    SDL_ResumeAudioStreamDevice(devices.capture_);
  }
  return devices;
}

VoiceDevices::VoiceDevices(VoiceDevices&& other) noexcept
    : capture_(std::exchange(other.capture_, nullptr)),
      playback_(std::exchange(other.playback_, nullptr)),
      subsystem_(std::exchange(other.subsystem_, false)) {}

VoiceDevices& VoiceDevices::operator=(VoiceDevices&& other) noexcept {
  if (this != &other) {
    Close();
    capture_ = std::exchange(other.capture_, nullptr);
    playback_ = std::exchange(other.playback_, nullptr);
    subsystem_ = std::exchange(other.subsystem_, false);
  }
  return *this;
}

VoiceDevices::~VoiceDevices() { Close(); }

void VoiceDevices::Close() noexcept {
  if (capture_ != nullptr) {
    SDL_DestroyAudioStream(capture_);
    capture_ = nullptr;
  }
  if (playback_ != nullptr) {
    SDL_DestroyAudioStream(playback_);
    playback_ = nullptr;
  }
  if (subsystem_) {
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    subsystem_ = false;
  }
}

std::size_t VoiceDevices::ReadCapture(std::span<std::int16_t> into) {
  if (capture_ == nullptr || into.empty()) return 0;
  const int read =
      SDL_GetAudioStreamData(capture_, into.data(), static_cast<int>(into.size_bytes()));
  return read > 0 ? static_cast<std::size_t>(read) / kSampleBytes : 0;
}

void VoiceDevices::WritePlayback(std::span<const std::int16_t> stereo) {
  if (playback_ == nullptr || stereo.empty()) return;
  SDL_PutAudioStreamData(playback_, stereo.data(), static_cast<int>(stereo.size_bytes()));
}

std::size_t VoiceDevices::QueuedPlayback() const {
  if (playback_ == nullptr) return 0;
  const int queued = SDL_GetAudioStreamQueued(playback_);
  return queued > 0 ? static_cast<std::size_t>(queued) / kSampleBytes : 0;
}

}  // namespace ro::audio
