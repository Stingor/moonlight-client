#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include "input/hotkeys.h"
#include "options/voice_options.h"

namespace moonclient {

// ── LES TOUCHES DE LA VOIX ──────────────────────────────────────────────────
//
// « Parler », sur le canal par défaut, puis celle de chaque canal : des
// raccourcis comme les autres, lus tenus sur l'état du clavier, et qu'on pose
// aussi depuis la page Son.

// Leurs actions, au rang de `VoiceTalkKey`.
inline constexpr std::array<GameAction, kVoiceTalkKeyCount> kVoiceTalkActions = {
    GameAction::kVoiceTalk, GameAction::kVoiceTalkNearby, GameAction::kVoiceTalkParty,
    GameAction::kVoiceTalkGuild};

// Les touches posées, au rang de `VoiceTalkKey` : leur code, nul quand il n'y
// en a pas, et leur nom pour la page Son, vide de même.
struct VoiceKeys {
  std::array<std::uint32_t, kVoiceTalkKeyCount> codes{};
  std::array<std::string, kVoiceTalkKeyCount> labels;
};

[[nodiscard]] VoiceKeys ReadVoiceKeys(const Hotkeys& bindings);

// Vrai quand cette touche est l'une de celles de la voix.
[[nodiscard]] bool IsVoiceTalkKey(const Hotkeys& bindings, std::uint32_t key);

// LA CAPTURE DES RACCOURCIS désigne une action par son rang : celui d'une
// touche de la voix, et la touche qu'un rang désigne, s'il en désigne une.
[[nodiscard]] int VoiceKeyCaptureRank(VoiceTalkKey key);
[[nodiscard]] std::optional<VoiceTalkKey> VoiceKeyCapturing(int capture_rank);

}  // namespace moonclient
