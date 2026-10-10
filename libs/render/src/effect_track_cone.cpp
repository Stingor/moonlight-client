#include <algorithm>
#include <cmath>
#include <numbers>

#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

[[nodiscard]] float SinOfDegrees(std::int32_t degrees) {
  return std::sin(static_cast<float>(degrees) * std::numbers::pi_v<float> / 180.0F);
}

// CE QUE LE CÔNE POSE EN DUR.

// ⚠ LA DERNIÈRE PISTE N'EST PAS COMME LES TROIS AUTRES, et c'est vrai de tout ce
// mouvement : elle ne tourne pas, monte moins haut, s'éteint plus lentement.
// C'est elle le halo ; la traiter comme ses sœurs le noierait dedans.
constexpr std::size_t kLastTrack = kEffectTracksPerNode - 1;

// LES TROIS PREMIÈRES TOURNENT, et chacune plus vite que la précédente : trois
// degrés par tic pour la première, quatre pour la deuxième, cinq pour la
// troisième. C'est ce décalage qui ouvre le cône ; un pas commun les garderait
// alignées.
constexpr std::int16_t kTurnOfFirstTrack = 3;

// LA MONTÉE NE DURE QUE VINGT TICS, et le plafond n'est pas le même pour la
// dernière piste.
constexpr std::int32_t kTicksOfRise = 20;
constexpr std::int16_t kAlphaRisePerTick = 10;
constexpr std::int16_t kAlphaCeiling = 180;
constexpr std::int16_t kAlphaRiseOfLastTrack = 8;
constexpr std::int16_t kAlphaCeilingOfLastTrack = 60;

// ⚠ UN STYLE EMPÊCHE LA MONTÉE. La piste reste à l'alpha que son semis lui a
// donné ; l'oublier ferait remonter à cent quatre-vingts une piste que le client
// laisse où elle est.
constexpr std::int16_t kStyleThatNeverRises = 1;

// LA SORTIE COMMENCE QUARANTE TICS AVANT LA FIN, et la dernière piste s'efface
// plus lentement — deux par tic contre cinq.
constexpr std::int32_t kTicksOfFadeBeforeDeath = 40;
constexpr std::int16_t kAlphaFallPerTick = 5;
constexpr std::int16_t kAlphaFallOfLastTrack = 2;

// LE JUMEAU QUI ONDULE : sa dernière piste monte jusqu'à cent, s'efface de
// trois, et ses segments figés battent entre cinquante-cinq et cent pour cent
// de leur arche, au rythme d'un sinus du temps.
constexpr std::int16_t kAlphaCeilingOfWavingLastTrack = 100;
constexpr std::int16_t kAlphaFallOfWavingLastTrack = 3;
constexpr float kWaveFloorShare = 0.55F;
constexpr float kWaveSwingShare = 0.45F;

// ⚠ L'ARCHE COURT SUR VINGT ET UN SEGMENTS, NEUF DEGRÉS CHACUN. Cent
// quatre-vingt-neuf degrés en tout : le sinus y fait une arche entière, nulle
// aux deux bouts et pleine au milieu. C'est elle la forme du cône.
constexpr std::int32_t kDegreesPerSlot = 9;

// ⚠ ET LE TEMPS EST UN SINUS AUSSI, sur les quatre-vingt-dix premiers tics : le
// segment monte de zéro à son plafond en un quart de tour. Passé quatre-vingt-dix
// tics, il ne bouge plus par ce chemin-là.
constexpr std::int32_t kLastTickOfGrowth = 90;

// ⚠ LA TEINTE TROIS N'ANIME QUE LA DERNIÈRE PISTE. Le client saute les trois
// premières dès l'entrée de sa boucle ; les animer quand même montrerait trois
// couronnes que cet effet-là ne montre pas.
constexpr float kTrailVariantThatKeepsOnlyTheLast = 3.0F;

}  // namespace

void AdvanceParticleConeSpawnTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                    std::int32_t life_ticks, float trail_variant,
                                    bool waves_last_track) {
  const std::size_t first = trail_variant == kTrailVariantThatKeepsOnlyTheLast ? kLastTrack : 0;
  for (std::size_t rang = first; rang < tracks.size(); ++rang) {
    EffectTrack& track = tracks[rang];
    if (!track.active) {
      continue;
    }
    ++track.ticks;

    if (rang < kLastTrack) {
      // ⚠ CHAQUE PISTE TOURNE À SA VITESSE. Voir `kTurnOfFirstTrack`.
      track.azimuth_degrees = static_cast<std::int16_t>(track.azimuth_degrees + kTurnOfFirstTrack +
                                                        static_cast<std::int16_t>(rang));
      if (track.azimuth_degrees >= kEffectTrackFullTurnDegrees) {
        track.azimuth_degrees =
            static_cast<std::int16_t>(track.azimuth_degrees - kEffectTrackFullTurnDegrees);
      }
    }

    const bool waves = waves_last_track && rang == kLastTrack;
    if (track.ticks < life_ticks - kTicksOfFadeBeforeDeath) {
      if (track.ticks < kTicksOfRise && (waves_last_track || track.style != kStyleThatNeverRises)) {
        const std::int16_t rise = rang == kLastTrack ? kAlphaRiseOfLastTrack : kAlphaRisePerTick;
        const std::int16_t last_ceiling =
            waves ? kAlphaCeilingOfWavingLastTrack : kAlphaCeilingOfLastTrack;
        const std::int16_t ceiling = rang == kLastTrack ? last_ceiling : kAlphaCeiling;
        track.alpha = std::min(static_cast<std::int16_t>(track.alpha + rise), ceiling);
      }
    } else {
      const std::int16_t last_fall = waves ? kAlphaFallOfWavingLastTrack : kAlphaFallOfLastTrack;
      const std::int16_t chute = rang == kLastTrack ? last_fall : kAlphaFallPerTick;
      track.alpha =
          std::max(static_cast<std::int16_t>(track.alpha - chute), static_cast<std::int16_t>(0));
    }

    // ⚠ UN SEGMENT QUI A ATTEINT SON PLAFOND Y RESTE, et sa marque le dit. Le
    // client pose un octet par segment ; sans lui, le sinus du temps ferait
    // redescendre le cône après son quart de tour, et l'anneau respirerait au
    // lieu de s'ouvrir une fois.
    for (std::size_t segment = 0; segment < kEffectTrackSlots; ++segment) {
      const float ceiling =
          track.lift_key * SinOfDegrees(static_cast<std::int32_t>(segment) * kDegreesPerSlot);
      // LE JUMEAU : un segment figé bat autour de son arche, sans fin ; un
      // segment qui pousse encore se fige passé le quart de tour.
      if (waves && track.mark[segment] != 0) {
        track.value[segment] =
            ceiling * kWaveFloorShare +
            ceiling * kWaveSwingShare * SinOfDegrees(track.ticks % kEffectTrackFullTurnDegrees);
        continue;
      }
      if (track.mark[segment] != 0) {
        continue;
      }
      if (track.ticks <= kLastTickOfGrowth) {
        track.value[segment] = ceiling * SinOfDegrees(track.ticks);
      } else if (waves) {
        track.mark[segment] = 1;
      }
      if (track.value[segment] > ceiling) {
        track.value[segment] = ceiling;
        track.mark[segment] = 1;
      }
      track.value[segment] = std::max(track.value[segment], 0.0F);
    }
  }
}

}  // namespace ro::render
