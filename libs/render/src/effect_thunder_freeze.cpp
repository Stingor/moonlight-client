#include "ro/render/effect_thunder_freeze.h"

#include <algorithm>
#include <optional>

#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_quad_rotation.h"
#include "ro/render/effect_track.h"

#include "effect_draw_below.h"
#include "effect_track_angles.h"

namespace ro::render {
namespace {

// LES DEUX PISTES qui portent les angles de caméra, que la figure relit à des
// emplacements FIXES du nœud, quelle que soit la piste dessinée.
constexpr std::size_t kTrackThatTakesTheTurn = 0;
constexpr std::size_t kTrackThatTakesTheTilt = 1;
// LA PISTE QUI MÈNE : son allumage garde tout, son ancre porte le cœur.
constexpr std::size_t kLeadingTrack = 0;

// LE CARRÉ DU CŒUR : quatre coins à un quart de tour l'un de l'autre, en
// partant de zéro — les tables de cosinus et de sinus lues aux degrés 0, 90,
// 180 et 270.
constexpr std::size_t kCornerCount = 4;
constexpr std::int32_t kQuarterTurnDegrees = 90;

// LE SEIZIÈME PARAMÈTRE DU TRACEUR, à zéro pour les six quads : l'ADDITION.
constexpr std::int16_t kAddedBlend = 0;

// LES QUATRE NOMBRES DE TEXTURE, `0, 1, 0, 1` aux paramètres vingt-deux à
// vingt-cinq pour les six quads : l'image est retournée, un aux deux premiers
// coins et zéro aux deux derniers.
constexpr float kImageVAtFirstCorners = 1.0F;
constexpr float kImageVAtLastCorners = 0.0F;

constexpr std::int16_t kOpaqueAlpha = 255;

// Un angle rangé en flottant, tronqué vers zéro comme le client l'indexe.
[[nodiscard]] std::int32_t TruncatedDegrees(float degrees) {
  return static_cast<std::int32_t>(degrees);
}

// UN PAS DU TREMBLEMENT sur un axe.
//
// ⚠ LA BRANCHE SE CHOISIT AVANT LE PAS : la cible AU-DESSUS fait monter, toute
// autre — égale comprise — fait descendre. Arrivé à la cible sans la passer,
// l'angle ne tire rien ; il ne tire qu'au tic suivant, une fois passé d'un degré.
void StepWanderAxis(float& current, float& target, std::uint32_t& generator) {
  if (target > current) {
    current =
        std::min(current + kThunderFreezeWanderStepDegrees, kThunderFreezeWanderCeilingDegrees);
    if (current > target) {
      const std::int32_t angle = TruncatedDegrees(current);
      target = static_cast<float>(angle == 0 ? 0 : DrawBelow(generator, angle));
    }
    return;
  }
  current = std::max(current - kThunderFreezeWanderStepDegrees, 0.0F);
  // Comparaison stricte de la cible à l'angle : seule une cible encore SOUS
  // l'angle le laisse marcher.
  if (!(target < current)) {
    const std::int32_t angle = TruncatedDegrees(current);
    const std::int32_t drawn = angle == 0 ? 0 : DrawBelow(generator, angle);
    target = static_cast<float>(kThunderFreezeWanderRetargetTop - drawn);
  }
}

// Un angle flottant replié d'un tour quand il l'atteint.
void FoldFullTurn(float& degrees) {
  const auto tour = static_cast<float>(kEffectTrackFullTurnDegrees);
  if (degrees >= tour) {
    degrees -= tour;
  }
}

[[nodiscard]] std::uint8_t AlphaOf(std::int16_t alpha) {
  return static_cast<std::uint8_t>(std::clamp<std::int16_t>(alpha, 0, kOpaqueAlpha));
}

// UN CARRÉ DU CŒUR, tourné vers la caméra et posé sur l'ancre de la première
// piste.
[[nodiscard]] EffectWorldQuad CoreQuad(const EffectTrack& leader, float radius, std::int16_t tour,
                                       std::int16_t tilt, const std::array<std::uint8_t, 3>& tint,
                                       glm::vec3 node_origin) {
  std::array<glm::vec3, kCornerCount> corners;
  for (std::size_t corner = 0; corner < kCornerCount; ++corner) {
    const auto angle = static_cast<std::int32_t>(corner) * kQuarterTurnDegrees;
    corners[corner] = {radius * CosOfDegrees(angle), 0.0F, radius * SinOfDegrees(angle)};
  }
  RotateEffectQuadCorners(corners, tour, tilt);
  EffectParticleQuadRequest request;
  for (std::size_t corner = 0; corner < kCornerCount; ++corner) {
    // ⚠ LE REPÈRE SE RETOURNE ICI, comme pour le carré teinté : les coins se
    // tournent dans celui du client, par les angles de SA caméra, et reviennent
    // au nôtre par `S` entier ; l'ancre y est déjà.
    request.corners[corner] = leader.anchor + EffectPointFromClient(corners[corner]);
  }
  request.absolute = false;
  request.alpha = AlphaOf(leader.alpha);
  request.color = tint;
  request.blend_mode = kAddedBlend;
  request.texture = kThunderFreezeCoreTexture;
  request.v_at_a_and_b = kImageVAtFirstCorners;
  request.v_at_c_and_d = kImageVAtLastCorners;
  return ComposeEffectParticleQuad(request, node_origin);
}

// UN ÉCLAT, debout en travers de son cap, autour de l'ancre du cœur. Rien quand
// sa taille n'est pas positive : c'est ce qui tait une piste dont le compteur
// n'a pas encore atteint zéro.
[[nodiscard]] std::optional<EffectWorldQuad> ShardQuad(const EffectTrack& shard, std::size_t rang,
                                                       glm::vec3 anchor, glm::vec3 node_origin) {
  const float size = shard.value[kThunderFreezeSlotOfShardSize];
  if (!(size > 0.0F)) {
    return std::nullopt;
  }
  const float half = size * kThunderFreezeShardHalf;
  const std::int32_t cap = shard.azimuth_degrees;
  const std::int32_t oppose = cap < kEffectTrackHalfTurnDegrees ? cap + kEffectTrackHalfTurnDegrees
                                                                : cap - kEffectTrackHalfTurnDegrees;
  // Les quatre coins dans le repère du client, dans l'ordre du traceur : le bas
  // au cap, le bas au cap opposé, puis leurs hauts, relevés d'une taille
  // entière.
  const glm::vec3 au_cap{half * CosOfDegrees(cap), half, half * SinOfDegrees(cap)};
  const glm::vec3 a_l_oppose{half * CosOfDegrees(oppose), half, half * SinOfDegrees(oppose)};
  const std::array<glm::vec3, kCornerCount> corners = {
      au_cap, a_l_oppose, glm::vec3{a_l_oppose.x, half - size, a_l_oppose.z},
      glm::vec3{au_cap.x, half - size, au_cap.z}};

  EffectParticleQuadRequest request;
  for (std::size_t corner = 0; corner < kCornerCount; ++corner) {
    // ⚠ UN VRAI DÉCALAGE DU MONDE, pas un panneau : il change de repère en
    // entier, `(x, −y, −z)`.
    request.corners[corner] =
        anchor + glm::vec3{corners[corner].x, -corners[corner].y, -corners[corner].z};
  }
  request.absolute = false;
  request.alpha = AlphaOf(shard.tilt_degrees);
  const std::uint8_t grey = kThunderFreezeShardGreys[rang];
  request.color = {grey, grey, kThunderFreezeShardBlue};
  request.blend_mode = kAddedBlend;
  request.texture = kThunderFreezeShardTexture;
  request.v_at_a_and_b = kImageVAtFirstCorners;
  request.v_at_c_and_d = kImageVAtLastCorners;
  return ComposeEffectParticleQuad(request, node_origin);
}

}  // namespace

void AdvanceThunderFreezeTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                std::int16_t billboard_turn_degrees,
                                std::int16_t billboard_tilt_degrees, std::uint32_t& generator) {
  tracks[kTrackThatTakesTheTurn].style = billboard_turn_degrees;
  tracks[kTrackThatTakesTheTilt].style = billboard_tilt_degrees;

  EffectTrack& leader = tracks[kLeadingTrack];
  if (!leader.active) {
    return;
  }
  ++leader.ticks;
  if (leader.ticks <= kThunderFreezeAlphaRiseTicks) {
    ++leader.alpha;
  }

  for (std::size_t axe = 0; axe < kThunderFreezeWanderAxes; ++axe) {
    const std::size_t case_courante =
        kThunderFreezeSlotOfWander + (kThunderFreezeWanderSlotsPerAxis * axe);
    StepWanderAxis(leader.value[case_courante], leader.value[case_courante + 1], generator);
  }

  // ⚠ SUR LE COMPTEUR D'APRÈS L'INCRÉMENT : le premier tic, qui le porte à un,
  // ne tourne pas.
  if ((leader.ticks & kThunderFreezeOrbitParityMask) == 0) {
    leader.value[kThunderFreezeSlotOfOrbitDegrees] += kThunderFreezeOrbitStepDegrees;
    FoldFullTurn(leader.value[kThunderFreezeSlotOfOrbitDegrees]);
  }

  // LE TREMBLEMENT, un sinus par axe : le premier angle sur l'abscisse, le
  // deuxième sur la profondeur, le troisième sur la hauteur.
  const bool divise = leader.mark[kThunderFreezeMarkOfHalvedWander] != 0;
  const auto jitter = [&](std::size_t axe) {
    const float sinus = SinOfDegrees(TruncatedDegrees(
        leader.value[kThunderFreezeSlotOfWander + (kThunderFreezeWanderSlotsPerAxis * axe)]));
    return divise ? sinus * kThunderFreezeHalvedWanderFactor : sinus;
  };
  const float de_cote = jitter(0);
  const float in_depth = jitter(1);
  const float upward = jitter(2);
  const std::int32_t circle = TruncatedDegrees(leader.value[kThunderFreezeSlotOfOrbitDegrees]);
  const float x_client = (SinOfDegrees(circle) * kThunderFreezeOrbitRadius) + de_cote;
  const float z_client = (CosOfDegrees(circle) * kThunderFreezeOrbitRadius) + in_depth;
  const float y_client = leader.value[kThunderFreezeSlotOfHeight] + upward;
  leader.anchor = {x_client, -y_client, -z_client};

  leader.value[kThunderFreezeSlotOfPulseDegrees] +=
      static_cast<float>(DrawBelow(generator, kThunderFreezePulseDraws) + kThunderFreezePulseBase);
  FoldFullTurn(leader.value[kThunderFreezeSlotOfPulseDegrees]);

  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    EffectTrack& shard = tracks[rang];
    // ⚠ LA PREMIÈRE PISTE A DÉJÀ COMPTÉ son tic plus haut ; les trois autres
    // comptent ici, qu'elles soient allumées ou non — le client ne juge que
    // l'allumage de la première.
    if (rang != kLeadingTrack) {
      ++shard.ticks;
    }
    if (shard.ticks <= 0) {
      continue;
    }
    TurnOnward(shard.degrees, kThunderFreezeShardSpinPerTick);
    float base = shard.lift_key * kThunderFreezeShardGrowth;
    shard.lift_key = base;
    if (base > kThunderFreezeShardFadingSize) {
      shard.tilt_degrees =
          static_cast<std::int16_t>(shard.tilt_degrees - kThunderFreezeShardAlphaFall);
      if (shard.tilt_degrees <= 0) {
        shard.lift_key = kThunderFreezeShardRebirthSize;
        shard.azimuth_degrees =
            static_cast<std::int16_t>(DrawBelow(generator, kThunderFreezeShardHeadingDraws));
        shard.tilt_degrees = kThunderFreezeShardRebirthAlpha;
        base = shard.lift_key;
      }
    }
    const float size = (base * SinOfDegrees(shard.degrees) * kThunderFreezeShardSizeSwing) +
                       (base * kThunderFreezeShardSizeSteady);
    shard.value[kThunderFreezeSlotOfShardSize] = size < 0.0F ? 0.0F : size;
  }
}

std::vector<EffectWorldQuad> ComposeThunderFreezeQuads(
    const std::array<EffectTrack, kEffectTracksPerNode>& tracks, glm::vec3 node_origin) {
  std::vector<EffectWorldQuad> quads;
  const EffectTrack& leader = tracks[kLeadingTrack];
  if (!leader.active || leader.alpha <= 0) {
    return quads;
  }
  const std::int16_t tour = tracks[kTrackThatTakesTheTurn].style;
  const std::int16_t tilt = tracks[kTrackThatTakesTheTilt].style;
  const float radius = leader.radius_at_anchor;
  const float breath =
      radius * SinOfDegrees(TruncatedDegrees(leader.value[kThunderFreezeSlotOfPulseDegrees])) *
      kThunderFreezeCoreBreath;
  quads.push_back(
      CoreQuad(leader, breath + radius, tour, tilt, kThunderFreezeOuterCoreColor, node_origin));
  quads.push_back(CoreQuad(leader, breath + (radius * kThunderFreezeInnerCoreShare), tour, tilt,
                           kThunderFreezeInnerCoreColor, node_origin));
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    if (const std::optional<EffectWorldQuad> shard =
            ShardQuad(tracks[rang], rang, leader.anchor, node_origin);
        shard.has_value()) {
      quads.push_back(*shard);
    }
  }
  return quads;
}

}  // namespace ro::render
