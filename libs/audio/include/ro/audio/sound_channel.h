#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace ro::audio {

// LES FAMILLES DE BRUITS, que le joueur règle chacune sous le volume général.
//
// ⚠ LE CLIENT D'ORIGINE N'EN A AUCUNE : tous ses bruits suivent un seul
// volume. Ce découpage est un AJOUT, et tous les curseurs à fond rendent
// exactement sa conduite.
//
// 🔴 LA FAMILLE SE DÉCIDE PAR LE CHEMIN QUI JOUE LE BRUIT, jamais par le
// fichier : un même `.wav` peut servir au cri d'un monstre et à un effet, et
// chacun sonne alors au volume de la sienne.
enum class SoundChannel : std::uint8_t {
  // Ce que les sprites font entendre : les sons de leur animation — cri d'un
  // monstre, élan d'une arme, pas — et l'impact d'un coup sur sa cible.
  kSprites,
  // Les compétences et les effets : leur annonce, leur coup, les unités posées
  // au sol, les bruits de leurs nœuds.
  kEffects,
  // Les sources de son posées par la carte : l'eau, le vent, les animaux, la
  // foule.
  kAmbience,
  // Ce que l'interface fait entendre d'elle-même, hors de toute entité et de
  // toute carte : l'alarme d'un chuchotement, l'arrivée d'un membre au salon,
  // l'alerte d'un MVP. ⚠ JAMAIS PLACÉ DANS LE MONDE : son site d'appel le joue
  // au centre, sans atténuation par la distance.
  kInterface,
};

inline constexpr std::size_t kSoundChannelCount = 4;

// Le volume de chaque famille, de zéro à un, à fond par défaut.
using SoundChannelVolumes = std::array<float, kSoundChannelCount>;
inline constexpr SoundChannelVolumes kFullSoundChannelVolumes = {1.0F, 1.0F, 1.0F, 1.0F};

[[nodiscard]] constexpr std::size_t IndexOf(SoundChannel channel) noexcept {
  return static_cast<std::size_t>(channel);
}

// UN CURSEUR DEVIENT UNE AMPLITUDE, et il suit l'oreille : un cube.
//
// 🔴 LE CUBE, ET CE N'EST PAS UN GOÛT. Un curseur rendu tel quel étale vingt
// décibels sur ses neuf derniers dixièmes et vingt autres sur son premier :
// tout le réglage utile se tasse en bas, et au-delà de dix pour cent on
// n'entend plus la différence. Le cube étale SOIXANTE décibels sur toute la
// course — la moitié du curseur vaut −18 dB, le dixième −60 —, ce qui est la
// courbe usuelle d'un atténuateur.
//
// ⚠ IL NE S'APPLIQUE QU'AU RÉGLAGE, jamais à l'atténuation par la distance :
// celle-ci est une amplitude PHYSIQUE, déjà juste, et la recourber éteindrait
// tout ce qui n'est pas sous le nez du joueur.
[[nodiscard]] float VolumeAmplitude(float ratio);

// Le gain d'un bruit : ce que le site d'appel demande, multiplié par le volume
// général PUIS par celui de sa famille, chacun passé par la même courbe. Le
// général reste donc un maître : à zéro, plus rien ne sonne.
[[nodiscard]] float MixedSoundGain(float asked, float master_volume, float channel_volume);

}  // namespace ro::audio
