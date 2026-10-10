#include "options/sound_options.h"

namespace moonclient {

namespace {

// ⚠ CES CLÉS SONT CELLES DU FICHIER DU JOUEUR : en changer une fait oublier le
// réglage qu'elle portait.
constexpr const char* kMusicKey = "music";
constexpr const char* kMusicVolumeKey = "volume_music";
// Une clé qui ne porte plus rien : le volume suit toujours l'oreille. Voir
// `ro::audio::VolumeAmplitude`.
constexpr const char* kRetiredPerceptualVolumeKey = "volume_progressive";
constexpr const char* kSpatialSoundKey = "sound_spatialized";
constexpr const char* kSoundKey = "sound_effects";
constexpr const char* kSoundVolumeKey = "volume_sound_effects";
constexpr const char* kSpriteSoundVolumeKey = "volume_sprites";
constexpr const char* kEffectSoundVolumeKey = "volume_effects";
constexpr const char* kAmbienceVolumeKey = "volume_ambience";

}  // namespace

void DeclareSoundOptions(SettingsFile& into, SoundOptions& sound) {
  into.Declare(kMusicKey, &sound.music);
  into.Declare(kMusicVolumeKey, &sound.music_volume);
  into.Retire(kRetiredPerceptualVolumeKey);
  into.Declare(kSpatialSoundKey, &sound.spatial_sound);
  into.Declare(kSoundKey, &sound.sound);
  into.Declare(kSoundVolumeKey, &sound.sound_volume);
  into.Declare(kSpriteSoundVolumeKey, &sound.sprite_sound_volume);
  into.Declare(kEffectSoundVolumeKey, &sound.effect_sound_volume);
  into.Declare(kAmbienceVolumeKey, &sound.ambience_volume);
}

}  // namespace moonclient
