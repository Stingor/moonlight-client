#include <algorithm>

#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// COMMENT LE MOUVEMENT DE L'ÉVENTAIL LIT SA PISTE.
//
// ⚠ DIX CASES D'AFFILÉE PORTENT DIX ALPHAS, un par LAME de l'éventail. Ce n'est
// pas une valeur répétée : chacune a son propre plafond, et c'est l'écart entre
// ces plafonds qui donne à l'éventail son dégradé. Les lire comme une seule
// valeur donnerait dix lames de même éclat, c'est-à-dire une tache.
constexpr std::size_t kBladeCount = 10;
constexpr std::size_t kSlotOfFirstBlade = 1;
constexpr std::size_t kMarkOfStyle = 4;

// L'ALPHA D'UNE LAME MONTE de dix par tic jusqu'à SON plafond. Le premier est à
// soixante-dix, et chaque lame suivante monte vingt plus haut que la précédente.
//
// ⚠ LA MONTÉE S'ARRÊTE AVANT QUE LES TROIS DERNIÈRES ATTEIGNENT LEUR PLAFOND :
// dix-neuf pas de dix font cent quatre-vingt-dix, et les plafonds de deux cent
// dix, deux cent trente et deux cent cinquante restent hors d'atteinte. Le
// dégradé court donc sur les sept premières lames et s'aplatit sur les trois
// dernières. Les plafonds hauts sont transcrits tels quels — ce sont ceux du
// client, et c'est la FENÊTRE de montée qui les rend inatteignables, pas eux.
constexpr float kBladeRisePerTick = 10.0F;
constexpr float kFirstBladeCeiling = 70.0F;
constexpr float kCeilingStepPerBlade = 20.0F;
constexpr std::int32_t kTicksOfRise = 20;

// ⚠ PASSÉ LA MONTÉE, SEULES LES NEUF PREMIÈRES LAMES RETOMBENT. La dixième reste
// où la montée l'a laissée, et deux styles sur trois seulement la rappellent
// (voir plus bas). Lui appliquer cette chute-ci l'éteindrait avant le
// quatre-vingt-dixième tic, et le bouquet ne s'ouvrirait jamais.
constexpr std::size_t kBladesThatFall = 9;
constexpr float kBladeFallPerTick = 4.0F;

// LES DEUX STYLES QUI RESPIRENT À LA FIN. Passé leur quatre-vingt-dixième tic, la
// DERNIÈRE lame s'efface de cinq par tic et le rayon enfle d'un centième par tic
// tant qu'elle vit. C'est ce qui fait s'ouvrir l'éventail avant qu'il ne
// disparaisse — et c'est la SEULE chose qui efface cette lame : pour le
// troisième style, elle brille jusqu'à ce que le nœud meure.
constexpr std::uint8_t kFirstStyleThatOpens = 6;
constexpr std::uint8_t kLastStyleThatOpens = 7;
constexpr std::int32_t kTicksBeforeOpening = 90;
constexpr float kLastBladeFallPerTick = 5.0F;
constexpr float kRadiusGrowthWhileOpening = 1.01F;

// LA MARQUE QUI NE DIT PAS UN STYLE MAIS UN ATTACHEMENT : le nœud relit la
// position de son porteur à chaque tic, au lieu de garder celle de sa naissance.
constexpr std::uint8_t kMarkThatFollowsTheOwner = 1;

// AU DÉBUT, LE RAYON SE RESSERRE. Cinquante tics, deux dixièmes par tic : le
// bouquet se referme avant de s'ouvrir, et le retirer le fige.
constexpr std::int32_t kTicksOfTightening = 50;
constexpr float kRadiusTightenPerTick = 0.2F;

}  // namespace

bool FanTracksFollowTheirOwner(const std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  // ⚠ LA PREMIÈRE PISTE DÉCIDE POUR LE NŒUD, et le client la lit à un
  // déplacement ABSOLU que sa boucle ne décale pas. Interroger chaque piste
  // ferait suivre le porteur à certaines et pas aux autres, ce qui déchirerait
  // l'éventail.
  return tracks.front().mark[kMarkOfStyle] == kMarkThatFollowsTheOwner;
}

void AdvanceFanBladeTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                           std::int16_t billboard_turn_degrees,
                           std::int16_t billboard_tilt_degrees) {
  tracks[0].style = billboard_turn_degrees;
  tracks[1].style = billboard_tilt_degrees;

  for (EffectTrack& track : tracks) {
    if (!track.active) {
      continue;
    }
    ++track.ticks;
    if (track.ticks <= 0) {
      continue;
    }

    if (track.ticks < kTicksOfRise) {
      // ⚠ CHAQUE LAME A SON PLAFOND, et c'est tout le dégradé. La première
      // s'arrête à soixante-dix, la dernière à deux cent cinquante.
      for (std::size_t lame = 0; lame < kBladeCount; ++lame) {
        const std::size_t ou = kSlotOfFirstBlade + lame;
        const float ceiling = kFirstBladeCeiling + kCeilingStepPerBlade * static_cast<float>(lame);
        track.value[ou] = std::min(track.value[ou] + kBladeRisePerTick, ceiling);
      }
    } else {
      // ⚠ NEUF, PAS DIX : la dernière lame n'est pas de cette chute-ci.
      for (std::size_t lame = 0; lame < kBladesThatFall; ++lame) {
        const std::size_t ou = kSlotOfFirstBlade + lame;
        track.value[ou] = std::max(track.value[ou] - kBladeFallPerTick, 0.0F);
      }
      const std::uint8_t style = track.mark[kMarkOfStyle];
      if (style >= kFirstStyleThatOpens && style <= kLastStyleThatOpens &&
          track.ticks > kTicksBeforeOpening) {
        const std::size_t last = kSlotOfFirstBlade + kBladeCount - 1;
        track.value[last] -= kLastBladeFallPerTick;
        if (track.value[last] > 0.0F) {
          track.radius_at_anchor *= kRadiusGrowthWhileOpening;
        } else {
          track.value[last] = 0.0F;
        }
      }
    }

    if (track.ticks < kTicksOfTightening) {
      track.radius_at_anchor -= kRadiusTightenPerTick;
    }
  }
}

}  // namespace ro::render
