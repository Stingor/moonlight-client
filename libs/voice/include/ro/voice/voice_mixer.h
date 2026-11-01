#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "ro/voice/opus_codec.h"
#include "ro/voice/speaker_queue.h"
#include "ro/voice/voice_wire.h"

namespace ro::voice {

// ── CE QU'ON ENTEND : les locuteurs décodés, posés et mêlés ─────────────────
//
// Une sortie stéréo à 48 kHz, une trame de 20 ms à la fois. Chaque locuteur a
// sa file et son décodeur ; le serveur a déjà pesé la distance dans son volume,
// et le mixeur n'ajoute que le côté d'où vient la voix et les réglages du
// joueur.

inline constexpr std::size_t kVoiceOutputChannels = 2;
inline constexpr std::size_t kVoiceOutputFrameSamples = kVoiceFrameSamples * kVoiceOutputChannels;
// La distance, en cases, à laquelle une voix est toute d'un côté : celle où le
// serveur cesse de la faire entendre.
inline constexpr float kVoicePanReachCells = 14.0F;
// Combien de temps une voix compte encore pour « parle » après son dernier
// paquet : assez pour qu'un nom ne clignote pas entre deux mots.
inline constexpr std::uint32_t kVoiceSpeakingHoldMs = 300;
// Un locuteur muet depuis plus longtemps est oublié, décodeur compris.
inline constexpr std::uint32_t kVoiceForgetSpeakerMs = 10'000;

struct MixSettings {
  float output_gain = 1.0F;  // de 0 à 1, le curseur du joueur
  bool spatial = true;       // faux : chaque voix au centre
  // LA CASE DU JOUEUR, que le serveur dit ; sans elle, chaque voix au centre.
  std::optional<std::int32_t> listener_x;
  // Ceux que le joueur ne veut pas entendre.
  std::set<std::uint32_t> muted;
};

struct SpeakingVoice {
  std::uint32_t speaker_id = 0;
  std::string name;
};

class VoiceMixer {
 public:
  void Hear(const HeardVoice& heard, std::uint32_t now_ms);
  // Une trame stéréo entrelacée de `kVoiceOutputFrameSamples` échantillons.
  void MixFrame(std::span<std::int16_t> stereo, const MixSettings& settings, std::uint32_t now_ms);
  // Qui parle à l'instant, dans l'ordre où ils ont commencé.
  [[nodiscard]] std::vector<SpeakingVoice> speaking(std::uint32_t now_ms) const;

 private:
  struct Speaker {
    std::optional<OpusVoiceDecoder> decoder;
    SpeakerQueue queue;
    std::string name;
    float volume = 0.0F;
    std::int32_t x = 0;
    std::uint32_t last_heard_ms = 0;
    std::uint32_t began_ms = 0;
  };

  // Gauche et droite d'une voix venue de la case `x`.
  [[nodiscard]] static std::pair<float, float> SidesOf(const Speaker& speaker,
                                                       const MixSettings& settings);

  std::map<std::uint32_t, Speaker> speakers_;
  std::vector<std::int16_t> decoded_ = std::vector<std::int16_t>(kVoiceFrameSamples);
  std::vector<float> sum_ = std::vector<float>(kVoiceOutputFrameSamples);
};

}  // namespace ro::voice
