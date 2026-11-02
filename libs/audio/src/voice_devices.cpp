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

// Les appareils du genre demandé ; à rendre par `SDL_free`.
[[nodiscard]] SDL_AudioDeviceID* ListDevices(VoiceDeviceKind kind, int& count) {
  return kind == VoiceDeviceKind::kMicrophone ? SDL_GetAudioRecordingDevices(&count)
                                              : SDL_GetAudioPlaybackDevices(&count);
}

[[nodiscard]] SDL_AudioDeviceID FindDevice(VoiceDeviceKind kind, std::string_view name) {
  const SDL_AudioDeviceID fallback = kind == VoiceDeviceKind::kMicrophone
                                         ? SDL_AUDIO_DEVICE_DEFAULT_RECORDING
                                         : SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
  if (name.empty()) return fallback;
  int count = 0;
  SDL_AudioDeviceID* devices = ListDevices(kind, count);
  SDL_AudioDeviceID found = fallback;
  for (int at = 0; devices != nullptr && at < count; ++at) {
    const char* device_name = SDL_GetAudioDeviceName(devices[at]);
    if (device_name != nullptr && name == device_name) {
      found = devices[at];
      break;
    }
  }
  SDL_free(devices);
  if (found == fallback) {
    core::LogWarning("voix : « {} » est absent, l'appareil par défaut le remplace", name);
  }
  return found;
}

}  // namespace

std::vector<std::string> VoiceDevices::DeviceNames(VoiceDeviceKind kind) {
  std::vector<std::string> names;
  if (SDL_WasInit(SDL_INIT_AUDIO) == 0) return names;
  int count = 0;
  SDL_AudioDeviceID* devices = ListDevices(kind, count);
  for (int at = 0; devices != nullptr && at < count; ++at) {
    if (const char* name = SDL_GetAudioDeviceName(devices[at]); name != nullptr) {
      names.emplace_back(name);
    }
  }
  SDL_free(devices);
  return names;
}

core::Result<VoiceDevices> VoiceDevices::Open(std::string_view microphone,
                                              std::string_view speakers) {
  VoiceDevices devices;
  // ⚠ COMPTÉ PAR SDL : la musique a déjà ouvert le son, et chacun le referme
  // pour son compte.
  if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
    return Fail(ErrorCode::kIoFailure, std::string("son de la voix : ") + SDL_GetError());
  }
  devices.subsystem_ = true;

  const SDL_AudioSpec playback{
      .format = SDL_AUDIO_S16, .channels = kPlaybackChannels, .freq = kVoiceRate};
  devices.playback_ = SDL_OpenAudioDeviceStream(FindDevice(VoiceDeviceKind::kSpeakers, speakers),
                                                &playback, nullptr, nullptr);
  if (devices.playback_ == nullptr) {
    return Fail(ErrorCode::kIoFailure, std::string("sortie de la voix : ") + SDL_GetError());
  }
  SDL_ResumeAudioStreamDevice(devices.playback_);

  const SDL_AudioSpec capture{
      .format = SDL_AUDIO_S16, .channels = kCaptureChannels, .freq = kVoiceRate};
  devices.capture_ = SDL_OpenAudioDeviceStream(FindDevice(VoiceDeviceKind::kMicrophone, microphone),
                                               &capture, nullptr, nullptr);
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
