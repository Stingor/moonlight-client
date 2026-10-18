// LES SOURCES DE SON DE LA CARTE — l'eau, le vent, les animaux, la foule.
//
// Sorties d'`actor_sounds.cpp` parce qu'elles ne doivent rien aux acteurs : ce
// sont des objets posés par la carte, que le client fait vivre comme des
// acteurs sans corps. Elles partagent seulement le cache des bruits et
// l'auditeur. Le calcul de chaque source vit dans `ro/audio/ambient_sound.h`.

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "ro/audio/sound_placement.h"
#include "ro/vfs/encoding.h"

#include "world/actor_sounds.h"

namespace moonclient {

void ActorSounds::RaiseMapAmbience(std::span<const ro::formats::RswSound> sounds,
                                   const ro::render::MapEffectFrame& frame, std::uint32_t now_ms) {
  ambience_.clear();
  ambience_frame_ = frame;
  raised_ambience_.assign(sounds.begin(), sounds.end());
  for (const ro::formats::RswSound& sound : sounds) {
    // UNE SOURCE SANS FICHIER NE SONNE JAMAIS : le client la crée, puis vérifie
    // à chaque image que son nom n'est pas vide. Ne pas la créer revient au même.
    if (sound.wave_name.empty()) {
      continue;
    }
    const glm::vec3 place = {sound.position[0], sound.position[1], sound.position[2]};
    ro::audio::AmbientSoundSource source;
    source.native_ground = {place.x, place.z};
    source.width = sound.width;
    source.height = sound.height;
    source.range = sound.range;
    source.cycle_seconds = sound.cycle_seconds;
    source.volume = sound.volume;
    // ⚠ LE NOM DU FICHIER EST EN CP949, comme tout nom qu'un format du client
    // cite : `Known` le veut en UTF-8.
    auto utf8 = ro::vfs::Cp949ToUtf8(sound.wave_name);
    ambience_.push_back(MapAmbientSource{utf8 ? std::move(*utf8) : sound.wave_name,
                                         ro::audio::AmbientSoundEmitter(source, now_ms)});
  }
}

void ActorSounds::ReraiseMapAmbience(std::span<const ro::formats::RswSound> sounds,
                                     std::uint32_t now_ms) {
  std::vector<MapAmbientSource> earlier = std::move(ambience_);
  RaiseMapAmbience(sounds, ambience_frame_, now_ms);
  // ⚠ PAR RANG, ET NON PAR NOM : une source qu'on glisse change de place, pas
  // de rang. Une source masquée décale les suivantes, qui reprennent alors une
  // cadence voisine — au pire un bruit avancé, jamais un bruit par image.
  const std::size_t kept = std::min(earlier.size(), ambience_.size());
  for (std::size_t rank = 0; rank < kept; ++rank) {
    ambience_[rank].emitter.KeepPaceOf(earlier[rank].emitter);
  }
}

void ActorSounds::LeaveMapAmbience(ro::audio::AudioEngine& audio) {
  ambience_.clear();
  // ⚠ LA FAMILLE ENTIÈRE, et c'est juste : seules les sources de la carte
  // jouent dans l'ambiance.
  audio.StopSounds(ro::audio::SoundChannel::kAmbience);
}

void ActorSounds::HearMapAmbience(const SoundStage& scene) {
  if (ambience_.empty() || scene.audio == nullptr || !scene.audio->is_open()) {
    return;
  }
  const Listener listener = ListenerOf(scene);
  ambience_listener_ = listener.position;
  // L'AUDITEUR PASSE DANS LE REPÈRE DU CLIENT, où la source fait son calcul ;
  // le point qu'elle rend en revient.
  const glm::vec3 ear = {listener.position[0], listener.position[1], listener.position[2]};
  const glm::vec3 native_ear = ambience_frame_.ToNative(ear);
  for (MapAmbientSource& source : ambience_) {
    const std::optional<ro::audio::AmbientSoundCue> cue =
        source.emitter.Tick({native_ear.x, native_ear.z}, scene.now_ms);
    if (!cue.has_value()) {
      continue;
    }
    // Le client passe zéro pour l'axe vertical : seul le sol compte.
    const glm::vec3 point = ambience_frame_.ToWorld(
        glm::vec3{cue->native_ground[0], native_ear.y, cue->native_ground[1]});
    const ro::audio::SoundPlacement placement =
        ro::audio::PlaceSound(listener.position, listener.right, {point.x, point.y, point.z},
                              cue->hearing_range, cue->near_distance);
    if (!(placement.gain > 0.0F) || !Known(*scene.audio, source.name)) {
      continue;
    }
    scene.audio->PlaySound(source.name, placement.gain * cue->volume, placement.pan,
                           ro::audio::SoundChannel::kAmbience);
  }
}

}  // namespace moonclient
