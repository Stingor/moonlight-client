#include "ro/render/effect_ground_box.h"

#include <algorithm>
#include <cstddef>

#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// CE QUE LE MOUVEMENT POSE, relevé au cas 64 du répartiteur.

// De combien l'angle de respiration avance par tic.
constexpr std::int16_t kBreathStepDegrees = 1;
// LES DEUX PARTS DE LA HAUTEUR : celle qui ne bouge pas, et celle que le sinus
// fait respirer. Leur somme fait un, et leur différence le creux.
constexpr float kBreathFloorShare = 0.65F;
constexpr float kBreathWaveShare = 0.35F;
// Sur combien de tics la boîte se déplie en entrant.
constexpr std::int32_t kUnfoldTicks = 90;
// De combien l'alpha tombe par tic une fois la durée passée.
constexpr std::int16_t kAlphaFall = 2;
// L'ONDE DU BLEU : un par tic, entre zéro et deux cents.
constexpr float kBlueStep = 1.0F;
constexpr float kBlueCeiling = 200.0F;
constexpr float kBlueRising = 0.0F;
constexpr float kBlueFalling = 1.0F;

// L'ALPHA DOUBLÉ des panneaux en diagonale, et son plafond.
constexpr std::int16_t kDiagonalAlphaCeiling = 255;

// LES QUATRE COINS DE LA BOÎTE, tournés de l'azimut, dans l'ordre où le client
// fait le tour.
[[nodiscard]] std::array<glm::vec2, kGroundBoxWalls> BoxCorners(const EffectTrack& track) {
  const float half_x = track.value[kGroundBoxSlotOfHalfX];
  const float half_z = track.value[kGroundBoxSlotOfHalfZ];
  const float cosinus = CosOfDegrees(track.azimuth_degrees);
  const float sinus = SinOfDegrees(track.azimuth_degrees);
  const auto rotated = [&](float before, float cote) {
    return glm::vec2{before * cosinus - cote * sinus, before * sinus + cote * cosinus};
  };
  return {rotated(half_x, half_z), rotated(half_x, -half_z), rotated(-half_x, -half_z),
          rotated(-half_x, half_z)};
}

}  // namespace

std::array<std::uint8_t, 3> EffectGroundBoxColor(const EffectTrack& track) {
  const float variant_value = track.value[kGroundBoxSlotOfVariant];
  if (variant_value == kGroundBoxPaleVariant) {
    return kGroundBoxPale;
  }
  if (variant_value == kGroundBoxFullVariant) {
    return kGroundBoxFull;
  }
  if (variant_value == kGroundBoxPulsingVariant) {
    // ⚠ LE BLEU EST LA SOMME D'UN TIRAGE FIXE ET D'UNE ONDE, tronquée comme le
    // client la tronque. Les deux ensemble ne passent pas deux cent
    // quarante-neuf : aucun repli à craindre.
    const float blue =
        track.value[kGroundBoxSlotOfBlueFloor] + track.value[kGroundBoxSlotOfBlueWave];
    return {kGroundBoxPulsingRedGreen[0], kGroundBoxPulsingRedGreen[1],
            static_cast<std::uint8_t>(static_cast<std::int32_t>(blue))};
  }
  // ⚠ LE CINQUIÈME CAS DU CLIENT LIT DES VALEURS NON INITIALISÉES : il ne pose
  // ni teinte ni mélange. Aucun des quatre sites n'y mène ; nous rendons la
  // variante nue plutôt qu'un hasard de pile.
  return kGroundBoxPlain;
}

std::int16_t EffectGroundBoxBlendMode(float variant) {
  return variant == kGroundBoxPlainVariant ? kGroundBoxAddedBlend : kGroundBoxOrdinaryBlend;
}

std::vector<EffectWorldQuad> ComposeGroundBoxQuads(const EffectTrack& track, glm::vec3 node_origin,
                                                   const EffectOwnerTint& owner) {
  std::vector<EffectWorldQuad> quads;
  if (!track.active || track.alpha <= 0) {
    return quads;
  }
  const std::array<glm::vec2, kGroundBoxWalls> corners = BoxCorners(track);
  const float height = track.value[kGroundBoxSlotOfHeight];
  const std::array<std::uint8_t, 3> tint = EffectGroundBoxColor(track);
  const std::int16_t blend = EffectGroundBoxBlendMode(track.value[kGroundBoxSlotOfVariant]);

  quads.reserve(kGroundBoxWalls + kGroundBoxDiagonals);
  for (std::size_t wall = 0; wall < kGroundBoxWalls; ++wall) {
    quads.push_back(ComposeEffectWallQuad(corners[wall], corners[(wall + 1) % kGroundBoxWalls],
                                          height, track.alpha, tint, blend, node_origin, owner));
  }
  if (track.value[kGroundBoxSlotOfVariant] != kGroundBoxFullVariant) {
    return quads;
  }
  // ⚠ LES DEUX DIAGONALES SE PEIGNENT À L'ALPHA DOUBLÉ, borné : c'est ce qui
  // rend le mur de brume opaque en son milieu.
  const auto double_alpha =
      static_cast<std::int16_t>(std::min<std::int32_t>(2 * track.alpha, kDiagonalAlphaCeiling));
  for (std::size_t diagonale = 0; diagonale < kGroundBoxDiagonals; ++diagonale) {
    quads.push_back(ComposeEffectWallQuad(corners[diagonale],
                                          corners[diagonale + kGroundBoxDiagonals], height,
                                          double_alpha, tint, blend, node_origin, owner));
  }
  return quads;
}

void AdvanceGroundBoxTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  for (EffectTrack& effect_track : tracks) {
    if (!effect_track.active) {
      continue;
    }
    ++effect_track.ticks;
    if (effect_track.ticks <= 0) {
      continue;
    }
    effect_track.degrees = static_cast<std::int16_t>(effect_track.degrees + kBreathStepDegrees);
    if (effect_track.degrees >= kEffectTrackFullTurnDegrees) {
      effect_track.degrees =
          static_cast<std::int16_t>(effect_track.degrees - kEffectTrackFullTurnDegrees);
    }
    if (effect_track.ticks > effect_track.style) {
      effect_track.alpha =
          std::max<std::int16_t>(static_cast<std::int16_t>(effect_track.alpha - kAlphaFall), 0);
    }
    // LA HAUTEUR QUI RESPIRE, et qui ne tombe jamais à zéro.
    float height = effect_track.lift_key *
                   (kBreathFloorShare + kBreathWaveShare * SinOfDegrees(effect_track.degrees));
    if (effect_track.ticks < kUnfoldTicks) {
      height *= SinOfDegrees(effect_track.ticks);
    }
    effect_track.value[kGroundBoxSlotOfHeight] = height;

    if (effect_track.value[kGroundBoxSlotOfVariant] != kGroundBoxPulsingVariant) {
      continue;
    }
    // L'ONDE TRIANGULAIRE DU BLEU : elle monte, bute, redescend, bute.
    float& wave = effect_track.value[kGroundBoxSlotOfBlueWave];
    float& descend = effect_track.value[kGroundBoxSlotOfBlueFalling];
    if (descend == kBlueRising) {
      wave += kBlueStep;
      if (wave > kBlueCeiling) {
        descend = kBlueFalling;
        wave = kBlueCeiling;
      }
    } else if (descend == kBlueFalling) {
      wave -= kBlueStep;
      if (wave < 0.0F) {
        descend = kBlueRising;
        wave = 0.0F;
      }
    }
  }
}

}  // namespace ro::render
