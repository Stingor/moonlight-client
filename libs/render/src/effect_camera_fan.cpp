#include "ro/render/effect_camera_fan.h"

#include <algorithm>
#include <cstddef>

#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// LA MARQUE QUI NE DIT PAS UN STYLE MAIS UN ATTACHEMENT, comme au sous-type 147.
constexpr std::uint8_t kMarkThatFollowsTheOwner = 1;

// UN TIC DE LA BRANCHE « MARQUE DEUX », sur une piste déjà sortie de son attente.
void AdvanceTwinStyleTrack(EffectTrack& track) {
  if (track.ticks < kCameraFanTicksOfRise) {
    for (std::size_t lame = 0; lame < kCameraFanBladeCount; ++lame) {
      const std::size_t ou = kCameraFanSlotOfFirstBlade + lame;
      const float ceiling =
          kCameraFanTwinFirstCeiling + kCameraFanTwinCeilingStep * static_cast<float>(lame);
      track.value[ou] = std::min(track.value[ou] + kCameraFanRisePerTick, ceiling);
    }
    return;
  }
  for (std::size_t lame = 0; lame < kCameraFanTwinBladesThatFall; ++lame) {
    const std::size_t ou = kCameraFanSlotOfFirstBlade + lame;
    track.value[ou] = std::max(track.value[ou] - kCameraFanTwinFallPerTick, 0.0F);
  }
  if (track.ticks <= kCameraFanTwinOpeningTick) {
    return;
  }
  // ⚠ LES DEUX DERNIÈRES LAMES SE DÉROBENT ENSEMBLE, et CHACUNE enfle le rayon
  // tant qu'elle vit : il grandit deux fois par tic tant que les deux brillent.
  for (std::size_t lame = kCameraFanTwinBladesThatFall; lame < kCameraFanBladeCount; ++lame) {
    const std::size_t ou = kCameraFanSlotOfFirstBlade + lame;
    track.value[ou] -= kCameraFanTwinLastBladesFall;
    if (track.value[ou] > 0.0F) {
      track.radius_at_anchor *= kCameraFanTwinRadiusGrowth;
    } else {
      track.value[ou] = 0.0F;
    }
  }
}

}  // namespace

bool CameraFanFollowsItsOwner(const std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  return tracks.front().mark[kCameraFanMarkOfStyle] == kMarkThatFollowsTheOwner;
}

void AdvanceCameraFanTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                            std::int16_t billboard_turn_degrees,
                            std::int16_t billboard_tilt_degrees) {
  // ⚠ LES DEUX ANGLES SE RANGENT DANS LES PISTES, ET HORS DE LA BOUCLE : le
  // client les écrit à un déplacement absolu, sur les deux premières, même si
  // elles sont éteintes. La figure les y relit.
  tracks[0].style = billboard_turn_degrees;
  tracks[1].style = billboard_tilt_degrees;

  for (EffectTrack& track : tracks) {
    if (!track.active) {
      continue;
    }
    ++track.ticks;
    // Un compte négatif est une ATTENTE : le semis échelonne ses trois traînées
    // de vingt tics, et la dernière ne paraît qu'après les autres.
    if (track.ticks <= 0) {
      continue;
    }

    const std::uint8_t style = track.mark[kCameraFanMarkOfStyle];
    if (style == kCameraFanTwinStyle) {
      AdvanceTwinStyleTrack(track);
      continue;
    }
    if (track.ticks < kCameraFanTicksOfRise) {
      // ⚠ CHAQUE LAME A SON PLAFOND, et c'est tout le dégradé de l'éventail :
      // la première s'arrête à soixante-dix, la dernière à deux cent cinquante.
      for (std::size_t lame = 0; lame < kCameraFanBladeCount; ++lame) {
        const std::size_t ou = kCameraFanSlotOfFirstBlade + lame;
        const float ceiling =
            kCameraFanFirstCeiling + kCameraFanCeilingStep * static_cast<float>(lame);
        track.value[ou] = std::min(track.value[ou] + kCameraFanRisePerTick, ceiling);
      }
    } else {
      // ⚠ NEUF, PAS DIX : la dixième lame n'est pas de cette chute-là, et c'est
      // elle qui tient l'éventail ouvert jusqu'à son propre seuil.
      for (std::size_t lame = 0; lame < kCameraFanBladesThatFall; ++lame) {
        const std::size_t ou = kCameraFanSlotOfFirstBlade + lame;
        track.value[ou] = std::max(track.value[ou] - kCameraFanFallPerTick, 0.0F);
      }
      const std::int32_t seuil =
          style == kCameraFanQuickStyle ? kCameraFanQuickOpeningTick : kCameraFanLateOpeningTick;
      if (track.ticks > seuil) {
        const std::size_t last = kCameraFanSlotOfFirstBlade + kCameraFanBladeCount - 1;
        track.value[last] -= kCameraFanLastBladeFall;
        // ⚠ LE RAYON N'ENFLE QUE TANT QUE CETTE LAME VIT. Une fois qu'elle est
        // éteinte, l'éventail cesse de s'ouvrir — et le client la borne à zéro
        // au lieu de la laisser filer sous le plancher.
        if (track.value[last] > 0.0F) {
          track.radius_at_anchor *= kCameraFanRadiusGrowth;
        } else {
          track.value[last] = 0.0F;
        }
      }
    }

    // AU DÉBUT, LE RAYON SE RESSERRE : l'éventail se referme avant de s'ouvrir.
    if (track.ticks < kCameraFanTicksOfTightening && style != kCameraFanQuickStyle) {
      track.radius_at_anchor -= kCameraFanTightenPerTick;
    }
  }
}

}  // namespace ro::render
