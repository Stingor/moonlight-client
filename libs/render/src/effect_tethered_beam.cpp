#include "ro/render/effect_tethered_beam.h"

#include <algorithm>
#include <cstddef>

#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// LES TEINTES, en dur dans le dessinateur : l'âme puis la gaine, par variante.
constexpr std::array<std::uint8_t, 3> kYellowCore = {255, 255, 0};
constexpr std::array<std::uint8_t, 3> kYellowSheath = {0, 0, 255};
constexpr std::array<std::uint8_t, 3> kTealCore = {0, 100, 150};
constexpr std::array<std::uint8_t, 3> kTealSheath = {255, 50, 0};
constexpr std::array<std::uint8_t, 3> kPink = {255, 89, 182};
constexpr std::array<std::uint8_t, 3> kGrey = {50, 50, 50};

// CE QUE LE MOUVEMENT DU CAS 38 POSE.

// LA VARIANTE QUI ENFLE PUIS S'EFFACE : jusqu'à quel tic elle monte, à partir
// duquel elle tombe, et de combien.
constexpr std::int32_t kTealRiseUntilTick = 20;
constexpr std::int32_t kTealFallAfterTick = 40;
constexpr std::int16_t kTealRise = 6;
constexpr std::int16_t kTealFall = 3;
// LA VARIANTE QUI RESPIRE : jusqu'à quel tic son alpha monte, de combien, et
// de combien son rayon enfle par tic.
constexpr std::int32_t kPinkRiseUntilTick = 80;
constexpr std::int16_t kPinkRise = 1;
constexpr float kPinkRadiusGrowth = 0.005F;
// LE SCINTILLEMENT DES DEUX AUTRES : la longueur de son cycle, sa moitié, et
// son pas.
constexpr std::int32_t kFlickerCycleTicks = 10;
constexpr std::int32_t kFlickerHalfCycle = 5;
constexpr std::int16_t kFlickerStep = 2;

// UN POINT DE L'ANNEAU autour d'un centre, dans NOTRE repère.
//
// ⚠ NOTRE REPÈRE MONTE, CELUI DU CLIENT DESCEND : la hauteur de l'échantillon
// change de signe, et les deux bouts portent déjà la leur.
[[nodiscard]] glm::vec3 RingPoint(const glm::vec3& centre, std::size_t face, float radius,
                                  std::int32_t cap_du_plan) {
  const std::int32_t angle = kTetheredBeamStepDegrees * static_cast<std::int32_t>(face);
  const float radial = radius * CosOfDegrees(angle);
  return {centre.x + radial * CosOfDegrees(cap_du_plan), centre.y - radius * SinOfDegrees(angle),
          centre.z + radial * SinOfDegrees(cap_du_plan)};
}

[[nodiscard]] glm::vec3 NearEndPoint(const EffectTrack& effect_track) {
  return {effect_track.tail[kTetheredBeamTailOfNearEnd],
          effect_track.tail[kTetheredBeamTailOfNearEnd + 1],
          effect_track.tail[kTetheredBeamTailOfNearEnd + 2]};
}

[[nodiscard]] glm::vec3 TargetEndPoint(const EffectTrack& effect_track) {
  return {effect_track.tail[kTetheredBeamTailOfFarEnd],
          effect_track.tail[kTetheredBeamTailOfFarEnd + 1],
          effect_track.tail[kTetheredBeamTailOfFarEnd + 2]};
}

}  // namespace

std::array<std::uint8_t, 3> EffectTetheredBeamColor(float variant, std::size_t pass) {
  const bool gaine = pass == 1;
  if (variant == kTetheredBeamTealVariant) {
    return gaine ? kTealSheath : kTealCore;
  }
  if (variant == kTetheredBeamPinkVariant) {
    return kPink;
  }
  if (variant == kTetheredBeamGreyVariant) {
    return kGrey;
  }
  // ⚠ L'AIGUILLAGE DU CLIENT RETOMBE ICI POUR TOUTE AUTRE VALEUR, et c'est la
  // variante zéro : jaune au cœur, bleu autour. Il a bien un cas par défaut,
  // au contraire de la bourrasque.
  return gaine ? kYellowSheath : kYellowCore;
}

std::int16_t EffectTetheredBeamBlendMode(float variant) {
  return variant == kTetheredBeamGreyVariant ? kTetheredBeamOrdinaryBlend : kTetheredBeamAddedBlend;
}

std::uint8_t EffectTetheredBeamAlpha(std::int16_t alpha, float variant, std::size_t pass) {
  if (variant == kTetheredBeamGreyVariant) {
    return static_cast<std::uint8_t>(alpha / 2);
  }
  // ⚠ TRONQUÉ, PAS PLAFONNÉ : le client empile ses quatre canaux par décalages
  // de huit bits, et l'alpha n'y laisse que son octet bas.
  return pass == 1 ? static_cast<std::uint8_t>(alpha + alpha) : static_cast<std::uint8_t>(alpha);
}

glm::vec3 EffectTetheredBeamFarEnd(const EffectTrack& track) {
  const glm::vec3 target = TargetEndPoint(track);
  if (track.ticks >= kTetheredBeamGrowthTicks) {
    return target;
  }
  const glm::vec3 near_point = NearEndPoint(track);
  const float part = SinOfDegrees(kTetheredBeamGrowthStepDegrees * track.ticks);
  return near_point + (target - near_point) * part;
}

std::vector<EffectWorldQuad> ComposeTetheredBeamQuads(const EffectTrack& track,
                                                      const EffectOwnerTint& owner) {
  std::vector<EffectWorldQuad> quads;
  if (!track.active || track.alpha <= 0) {
    return quads;
  }
  const glm::vec3 near_point = NearEndPoint(track);
  const glm::vec3 target = TargetEndPoint(track);
  // ⚠ LE CAP VA DE LA CIBLE VERS LA SOURCE, et le plan de l'anneau est à un
  // quart de tour de là. L'inverser coucherait le tube en travers de sa route.
  // Ce cap est mesuré dans NOTRE repère, où les angles tournent à l'opposé de
  // ceux du client : son quart de tour s'y RETRANCHE, sans quoi la coupe
  // serait le miroir de la sienne.
  const std::int32_t cap_du_plan =
      WithinFullTurn(HeadingOfVector(near_point.x - target.x, near_point.z - target.z) -
                     kEffectTrackFullTurnDegrees / 4);
  const glm::vec3 far_point = EffectTetheredBeamFarEnd(track);
  const float variant_value = track.value[kTetheredBeamSlotOfVariant];

  quads.reserve(kTetheredBeamPasses * kTetheredBeamSides);
  for (std::size_t passe = 0; passe < kTetheredBeamPasses; ++passe) {
    // ⚠ LA GAINE EST AU DOUBLE DU RAYON, comme pour le trait gainé.
    const float radius = passe == 1 ? track.lift_key + track.lift_key : track.lift_key;
    for (std::size_t face = 1; face <= kTetheredBeamSides; ++face) {
      EffectParticleQuadRequest request;
      // LES QUATRE COINS, dans l'ordre du client : le bord précédent au bout
      // proche, le bord courant au bout proche, puis les deux au bout lointain.
      request.corners = {RingPoint(near_point, face - 1, radius, cap_du_plan),
                         RingPoint(near_point, face, radius, cap_du_plan),
                         RingPoint(far_point, face, radius, cap_du_plan),
                         RingPoint(far_point, face - 1, radius, cap_du_plan)};
      // ⚠ ABSOLUS : les deux bouts portent déjà leur position du monde.
      request.absolute = true;
      request.alpha = EffectTetheredBeamAlpha(track.alpha, variant_value, passe);
      request.color = EffectTetheredBeamColor(variant_value, passe);
      request.blend_mode = EffectTetheredBeamBlendMode(variant_value);
      // ⚠ L'IMAGE EST RETOURNÉE : le client donne un aux deux premiers coins et
      // zéro aux deux derniers, là où le défaut fait l'inverse.
      request.v_at_a_and_b = 1.0F;
      request.v_at_c_and_d = 0.0F;
      quads.push_back(ComposeEffectParticleQuad(request, glm::vec3{0.0F, 0.0F, 0.0F}, owner));
    }
  }
  return quads;
}

void AdvanceTetheredBeamTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                               glm::vec3 owner_position,
                               const std::optional<glm::vec3>& target_position) {
  EffectTrack& effect_track = tracks.front();
  if (!effect_track.active) {
    return;
  }
  ++effect_track.ticks;

  // LE BOUT PROCHE SUIT SON PORTEUR, élevé de huit au-dessus de lui.
  effect_track.tail[kTetheredBeamTailOfNearEnd] = owner_position.x;
  effect_track.tail[kTetheredBeamTailOfNearEnd + 1] = owner_position.y + kTetheredBeamLift;
  effect_track.tail[kTetheredBeamTailOfNearEnd + 2] = owner_position.z;

  const float variant_value = effect_track.value[kTetheredBeamSlotOfVariant];
  if (variant_value == kTetheredBeamTealVariant) {
    if (effect_track.ticks > kTealFallAfterTick) {
      effect_track.alpha =
          std::max<std::int16_t>(static_cast<std::int16_t>(effect_track.alpha - kTealFall), 0);
    } else if (effect_track.ticks < kTealRiseUntilTick) {
      effect_track.alpha = static_cast<std::int16_t>(effect_track.alpha + kTealRise);
    }
    // ⚠ ENTRE LE VINGTIÈME ET LE QUARANTIÈME TIC, RIEN : l'alpha tient son
    // plateau. Combler l'intervalle ferait monter le trait sans s'arrêter.
  } else if (variant_value == kTetheredBeamPinkVariant) {
    if (effect_track.ticks <= kPinkRiseUntilTick) {
      effect_track.alpha = static_cast<std::int16_t>(effect_track.alpha + kPinkRise);
    }
    // ⚠ ET LE RAYON ENFLE AU SINUS DU COMPTEUR, seule variante dans ce cas.
    effect_track.lift_key += kPinkRadiusGrowth * SinOfDegrees(WithinFullTurn(effect_track.ticks));
  } else {
    // ⚠ IL SCINTILLE : cinq tics de plus deux, cinq tics de moins deux. Le
    // lisser donnerait un trait mat là où le client en fait battre un.
    effect_track.alpha =
        static_cast<std::int16_t>(effect_track.ticks % kFlickerCycleTicks < kFlickerHalfCycle
                                      ? effect_track.alpha + kFlickerStep
                                      : effect_track.alpha - kFlickerStep);
  }

  if (!target_position.has_value()) {
    // ⚠ PAS DE CIBLE, PAS DE TRAIT.
    effect_track.active = false;
    effect_track.alpha = 0;
    return;
  }
  effect_track.tail[kTetheredBeamTailOfFarEnd] = target_position->x;
  effect_track.tail[kTetheredBeamTailOfFarEnd + 1] = target_position->y + kTetheredBeamLift;
  effect_track.tail[kTetheredBeamTailOfFarEnd + 2] = target_position->z;
}

}  // namespace ro::render
