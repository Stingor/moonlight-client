#pragma once

#include <cstdint>
#include <optional>

#include "ro/audio/audio_engine.h"

namespace moonclient {

// LA MUSIQUE D'UNE CHANSON S'ARRÊTE AVEC LA CHANSON.
//
// Une chanson ou une danse joue sa musique à l'annonce, et le serveur pose une
// unité au sol qui la porte tant qu'elle dure. Interrompue — un coup, la
// fatigue, Adaptation —, la chanson cesse et son unité disparaît, mais la
// musique allait jusqu'au bout de son fichier. Un choix du joueur, à l'écart
// du client d'origine : elle se tait avec son unité.

// La compétence dont l'unité au sol de ce type porte la chanson, ou rien.
[[nodiscard]] std::optional<std::uint16_t> SongSkillOfUnit(std::uint32_t unit_id) noexcept;

// Fait taire la musique que l'annonce de cette compétence a lancée.
void StopSongMusic(ro::audio::AudioEngine& audio, std::uint16_t skill_id);

}  // namespace moonclient
