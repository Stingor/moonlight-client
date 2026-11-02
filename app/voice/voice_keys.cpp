// Les touches de la voix. Voir l'en-tête.

#include "voice/voice_keys.h"

#include <cstddef>

namespace moonclient {

VoiceKeys ReadVoiceKeys(const Hotkeys& bindings) {
  VoiceKeys keys;
  for (std::size_t rank = 0; rank < kVoiceTalkKeyCount; ++rank) {
    const Binding bound = bindings.Of(kVoiceTalkActions[rank]);
    if (bound.bound()) {
      keys.codes[rank] = bound.key;
      keys.labels[rank] = LabelOfBinding(bound);
    }
  }
  return keys;
}

bool IsVoiceTalkKey(const Hotkeys& bindings, std::uint32_t key) {
  if (key == 0) return false;
  for (const GameAction action : kVoiceTalkActions) {
    const Binding bound = bindings.Of(action);
    if (bound.bound() && bound.key == key) return true;
  }
  return false;
}

int VoiceKeyCaptureRank(VoiceTalkKey key) {
  return static_cast<int>(ActionRef(kVoiceTalkActions[static_cast<std::size_t>(key)]).rank());
}

std::optional<VoiceTalkKey> VoiceKeyCapturing(int capture_rank) {
  for (std::size_t rank = 0; rank < kVoiceTalkKeyCount; ++rank) {
    const auto key = static_cast<VoiceTalkKey>(rank);
    if (VoiceKeyCaptureRank(key) == capture_rank) return key;
  }
  return std::nullopt;
}

}  // namespace moonclient
