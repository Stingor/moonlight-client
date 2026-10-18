#include "ro/render/effect_wandering_motes.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "ro/render/effect_quad_burst.h"
#include "ro/render/effect_quad_rotation.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_track_spread.h"

#include "effect_draw_below.h"
#include "effect_track_angles.h"

namespace ro::render {
namespace {

// DE COMBIEN UNE LUCIOLE AVANCE PAR TIC, et le pas réduit du style solitaire.
constexpr float kWanderStep = 0.05F;
constexpr float kLoneStep = 0.03F;

// À QUELLE DISTANCE DU NŒUD une luciole est jugée ÉCHAPPÉE.
constexpr std::int16_t kEscapeDistance = 30;

// OÙ UNE LUCIOLE ÉCHAPPÉE SE REPLACE : de moins quinze à plus quinze.
constexpr std::int32_t kRespawnSpread = 31;
constexpr std::int32_t kRespawnFloor = -15;

// DE COMBIEN L'ÉCLAT TOMBE quand la luciole est échappée, et sous quel seuil
// elle se replace.
constexpr std::int16_t kEscapeGlowFall = 2;
constexpr std::int16_t kRespawnGlowFloor = 1;

// LE SCINTILLEMENT : une chance sur tant, le seuil d'éclat qui l'autorise, sa
// durée sombre, sa fin, et ses deux pas.
constexpr std::int32_t kTwinkleChance = 125;
constexpr std::int32_t kLoneTwinkleChance = 20;
constexpr std::int16_t kTwinkleGlowThreshold = 100;
constexpr std::int16_t kTwinkleDimUntil = 90;
constexpr std::int16_t kTwinkleEnd = 120;
constexpr std::int16_t kTwinkleDimStep = 6;
constexpr std::int16_t kTwinkleRiseStep = 4;

// AU BOUT DE COMBIEN DE TICS la luciole solitaire repart de son nœud.
constexpr std::int32_t kLoneResetTicks = 3000;

// LES QUATRE QUADRANTS D'ERRANCE, un par luciole — les mêmes signes que la
// ronde du sous-type 27.
constexpr std::array<std::array<float, 2>, kWanderingMoteParticles> kWanderSigns = {{
    {1.0F, 1.0F},
    {-1.0F, -1.0F},
    {1.0F, -1.0F},
    {-1.0F, 1.0F},
}};

// ⚠ LES DEUX PREMIÈRES LUCIOLES FUIENT À TRENTE, LES DEUX AUTRES AU-DELÀ DE
// TRENTE. Le client écrit quatre comparaisons voisines et deux d'entre elles
// emploient l'inégalité stricte ; c'est très probablement une recopie, mais
// c'est ce qu'il fait et c'est ce qui décide à la limite exacte.
constexpr std::array<bool, kWanderingMoteParticles> kEscapeIsStrict = {false, false, true, true};

// LA DISTANCE QUE LE CLIENT MESURE : l'abscisse, ou la profondeur quand
// l'abscisse est trop courte. Les deux sont tronquées en entier de seize bits
// avant d'être comparées.
[[nodiscard]] std::int16_t MeasuredOffset(const glm::vec3& shift) {
  const auto de_cote = static_cast<std::int16_t>(
      std::abs(static_cast<std::int16_t>(static_cast<std::int32_t>(shift.x))));
  if (de_cote >= kEscapeDistance) {
    return de_cote;
  }
  return static_cast<std::int16_t>(
      std::abs(static_cast<std::int16_t>(static_cast<std::int32_t>(shift.z))));
}

// LE SCINTILLEMENT D'UNE LUCIOLE : une fois déclenché, il l'assombrit pendant
// quatre-vingt-dix tics puis la rallume, et s'éteint de lui-même.
void Scintiller(EffectTrack& effect_track, std::size_t firefly, std::int32_t chance,
                std::uint32_t& rng_state) {
  std::uint8_t& scintille = effect_track.mark[kWanderingMoteMarkOfFirstTwinkle + firefly];
  std::uint8_t& shard = effect_track.mark[kWanderingMoteMarkOfFirstGlow + firefly];
  if (DrawBelow(rng_state, chance) == 0 &&
      static_cast<std::int8_t>(shard) > kTwinkleGlowThreshold && scintille == 0) {
    scintille = 1;
  }
  if (scintille < 1) {
    return;
  }
  ++scintille;
  if (scintille <= kTwinkleDimUntil) {
    const auto assombri =
        static_cast<std::int16_t>(static_cast<std::int8_t>(shard) - kTwinkleDimStep);
    shard = static_cast<std::uint8_t>(assombri < 0 ? 0 : assombri);
    return;
  }
  if (scintille >= kTwinkleEnd) {
    scintille = 0;
    return;
  }
  const auto rallume =
      static_cast<std::int16_t>(static_cast<std::int8_t>(shard) + kTwinkleRiseStep);
  shard =
      static_cast<std::uint8_t>(rallume > kWanderingMoteGlowCap ? kWanderingMoteGlowCap : rallume);
}

}  // namespace

std::array<std::uint8_t, 3> EffectWanderingMoteColor(std::uint8_t mark) {
  if (mark == kWanderingMotePinkColor) {
    return kWanderingMotePink;
  }
  if (mark == kWanderingMoteYellowColor) {
    return kWanderingMoteYellow;
  }
  if (mark == kWanderingMoteWhiteColor) {
    return kWanderingMoteWhite;
  }
  // ⚠ LE CAS PAR DÉFAUT EXISTE et n'est atteint par AUCUN des trois sites : leur
  // bâtisseur n'écrit que huit ou dix. Le transcrire coûte une ligne et évite de
  // croire, plus tard, qu'une teinte manque.
  return kWanderingMoteGreen;
}

std::uint8_t EffectWanderingMoteAlpha(std::int8_t glow) {
  // ⚠ UN ÉCLAT NÉGATIF NE PEINT RIEN : le client teste le bit de signe de
  // l'octet ÉTENDU, pas de l'octet nu.
  if (glow < 0) {
    return 0;
  }
  return static_cast<std::uint8_t>(kWanderingMoteGlowToAlpha * glow);
}

std::vector<EffectWorldQuad> ComposeWanderingMoteQuads(const EffectTrack& track,
                                                       std::int16_t turn_degrees,
                                                       std::int16_t tilt_degrees, glm::vec3 anchor,
                                                       const EffectOwnerTint& owner) {
  std::vector<EffectWorldQuad> quads;
  if (!track.active) {
    return quads;
  }

  std::array<glm::vec3, 4> quad;
  for (std::size_t corner = 0; corner < quad.size(); ++corner) {
    const std::int32_t angle = kQuadBurstFirstCornerDegrees +
                               kQuadBurstDegreesPerCorner * static_cast<std::int32_t>(corner);
    quad[corner] = {track.radius_at_anchor * CosOfDegrees(angle), 0.0F,
                    track.radius_at_anchor * SinOfDegrees(angle)};
  }
  RotateEffectQuadCorners(quad, turn_degrees, tilt_degrees);

  const std::array<std::uint8_t, 3> tint =
      EffectWanderingMoteColor(track.mark[kWanderingMoteMarkOfColor]);
  quads.reserve(kWanderingMoteParticles);
  for (std::size_t firefly = 0; firefly < kWanderingMoteParticles; ++firefly) {
    const glm::vec3 shift = EffectFourParticleOffsetOf(track, firefly);
    EffectParticleQuadRequest request;
    for (std::size_t corner = 0; corner < quad.size(); ++corner) {
      // ⚠ LE COIN ET SON DÉCALAGE SONT DANS LE REPÈRE DU CLIENT, tournés par
      // les angles de SA caméra : ils reviennent au nôtre ENSEMBLE, par `S`
      // entier, au dernier moment.
      request.corners[corner] = EffectPointFromClient(quad[corner] + shift);
    }
    request.alpha = EffectWanderingMoteAlpha(
        static_cast<std::int8_t>(track.mark[kWanderingMoteMarkOfFirstGlow + firefly]));
    request.color = tint;
    request.v_at_a_and_b = 1.0F;
    request.v_at_c_and_d = 0.0F;
    quads.push_back(ComposeEffectParticleQuad(request, anchor, owner));
  }
  return quads;
}

void AdvanceWanderingMoteTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                std::int16_t camera_turn_degrees, std::int16_t camera_tilt_degrees,
                                std::uint32_t& generator) {
  StoreEffectBillboardAngles(tracks, camera_turn_degrees, camera_tilt_degrees);

  for (EffectTrack& effect_track : tracks) {
    // ⚠ LE COMPTEUR AVANCE AVANT LA GARDE, y compris sur une piste éteinte.
    ++effect_track.ticks;
    if (!effect_track.active) {
      continue;
    }
    const bool solitaire = effect_track.mark[kWanderingMoteMarkOfStyle] == kWanderingMoteLoneStyle;
    const float step = solitaire ? kLoneStep : kWanderStep;

    // L'ÉCLAT MONTE D'UN PAR TIC, jusqu'à son plafond. ⚠ LE STYLE SOLITAIRE NE
    // LÈVE QUE LE PREMIER : ses trois autres lucioles restent noires.
    const std::size_t levees = solitaire ? 1 : kWanderingMoteParticles;
    for (std::size_t firefly = 0; firefly < levees; ++firefly) {
      std::uint8_t& shard = effect_track.mark[kWanderingMoteMarkOfFirstGlow + firefly];
      if (static_cast<std::int8_t>(shard) < kWanderingMoteGlowCap) {
        shard = static_cast<std::uint8_t>(static_cast<std::int8_t>(shard) + kWanderingMoteGlowRise);
      }
    }

    ChaseEffectTrackAngles(effect_track, kWanderingMoteAnglePairs, generator);

    for (std::size_t firefly = 0; firefly < kWanderingMoteParticles; ++firefly) {
      glm::vec3 shift = EffectFourParticleOffsetOf(effect_track, firefly);
      const float wave_x =
          step * SinOfDegrees(static_cast<std::int32_t>(effect_track.value[4 * firefly]));
      const float wave_z =
          step * SinOfDegrees(static_cast<std::int32_t>(effect_track.value[4 * firefly + 2]));
      bool echappee = false;
      if (solitaire) {
        // ⚠ LA LUCIOLE SOLITAIRE MONTE au lieu d'errer, et c'est son COMPTEUR
        // qui la ramène — pas sa distance.
        shift.y += wave_x;
        echappee = effect_track.ticks > kLoneResetTicks;
      } else {
        shift.x += kWanderSigns[firefly][0] * wave_x;
        shift.z += kWanderSigns[firefly][1] * wave_z;
      }
      const std::int16_t offset = MeasuredOffset(shift);
      echappee = echappee ||
                 (kEscapeIsStrict[firefly] ? offset > kEscapeDistance : offset >= kEscapeDistance);
      if (echappee) {
        std::uint8_t& shard = effect_track.mark[kWanderingMoteMarkOfFirstGlow + firefly];
        shard = static_cast<std::uint8_t>(static_cast<std::int8_t>(shard) - kEscapeGlowFall);
        if (static_cast<std::int8_t>(shard) <= kRespawnGlowFloor) {
          if (solitaire) {
            // ⚠ ELLE REPART DE SON NŒUD, compteur remis à zéro.
            effect_track.ticks = 0;
            shift = {0.0F, 0.0F, 0.0F};
          } else {
            shift.x = static_cast<float>(DrawBelow(generator, kRespawnSpread) + kRespawnFloor);
            shift.z = static_cast<float>(DrawBelow(generator, kRespawnSpread) + kRespawnFloor);
          }
        }
      } else {
        Scintiller(effect_track, firefly, solitaire ? kLoneTwinkleChance : kTwinkleChance,
                   generator);
      }
      SetEffectFourParticleOffset(effect_track, firefly, shift);
      // ⚠ LE STYLE SOLITAIRE S'ARRÊTE À LA PREMIÈRE : le client SORT de sa
      // boucle, il ne passe pas à la suivante.
      if (solitaire) {
        break;
      }
    }
  }
}

}  // namespace ro::render
