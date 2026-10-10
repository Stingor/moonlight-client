#pragma once

#include "game/settings_file.h"

namespace moonclient {

// LES RÉGLAGES DU SON, et LEURS DÉFAUTS : l'initialisateur écrit ici est la
// seule source, le fichier de réglages le lit à la déclaration.
//
// ⚠ LE SON PART AUDIBLE, comme dans le jeu. Il partait coupé tant que le choix
// ne se retenait pas d'une partie à l'autre — ce qui n'est plus le cas.
struct SoundOptions {
  bool music = true;
  float music_volume = 1.0F;
  bool sound = true;
  // LE VOLUME GÉNÉRAL DES BRUITAGES : un maître, que les trois familles
  // ci-dessous multiplient. À zéro, plus rien ne sonne.
  float sound_volume = 1.0F;
  // ── UN AJOUT AU NATIF : UN VOLUME PAR FAMILLE ─────────────────────────────
  //
  // Le client d'origine n'a qu'un volume pour tous ses bruits ; tout à fond
  // rend sa conduite. Voir `ro::audio::SoundChannel`.
  float sprite_sound_volume = 1.0F;
  float effect_sound_volume = 1.0F;
  float ambience_volume = 1.0F;
  // ── UN AJOUT AU NATIF, ET IL SE DÉBRANCHE ─────────────────────────────────
  //
  // 🔴 LE CLIENT D'ORIGINE NE L'A PAS : son « Mode Sonore » propose « Aucun »,
  // « 2D » et « 3D », et le 2D ne connaît que la distance. Le couper rend
  // exactement sa conduite. Voir `ro::audio::AudioEngine`.
  bool spatial_sound = true;
};

// Déclare les réglages du son au fichier, dans l'ordre où il les écrit.
void DeclareSoundOptions(SettingsFile& into, SoundOptions& sound);

}  // namespace moonclient
