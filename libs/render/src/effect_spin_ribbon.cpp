#include "ro/render/effect_spin_ribbon.h"

#include <algorithm>
#include <cstddef>

#include "ro/render/effect_node.h"
#include "ro/render/effect_scene.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_trail_palette.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// Combien de bandes de texture le ruban compte : une de plus que ses pas, comme
// la jupe qui le précède.
constexpr std::int32_t kBands = kSpinRibbonSteps + 1;

// La coordonnée de texture au travers du ruban : un bord porte le haut de
// l'image, l'autre le bas.
constexpr float kTextureAtFirstEdge = 1.0F;
constexpr float kTextureAtSecondEdge = 0.0F;

// CE QUE LES DEUX MOUVEMENTS POSENT, relevé sur les cas 77 et 91.

// LA VIE DU SOUS-TYPE 91 : jusqu'à quel tic l'alpha monte, à partir duquel il
// retombe, et de combien.
constexpr std::int32_t kRiseUntilTick = 25;
constexpr std::int32_t kFallAfterTick = 70;
constexpr std::int16_t kAlphaStep = 2;
// De combien son rayon enfle par tic, et de combien son inclinaison avance au
// moins.
constexpr float kRadiusGrowth = 1.01F;
constexpr std::int16_t kTiltStep = 1;
// Sur combien de valeurs le tirage de l'inclinaison se fait : une fois sur
// deux, elle avance d'un degré de plus.
constexpr std::int32_t kTiltDrawSpread = 2;

// LES DEUX BORDS d'un échantillon, dans NOTRE repère.
//
// Le centre court sur un cercle de rayon `radius_at_anchor` INCLINÉ, et les
// deux bords s'en écartent de la demi-largeur, perpendiculairement à
// l'inclinaison. Ils sont donc symétriques — c'est ce qui sépare ce ruban de la
// jupe, dont un seul bord bouge.
//
// ⚠ NOTRE REPÈRE MONTE, CELUI DU CLIENT DESCEND : toutes les ordonnées changent
// de signe, y compris celle que la piste porte. ET LA PROFONDEUR AVEC ELLES :
// le passage d'un repère à l'autre est un demi-tour autour de X, qui retourne
// les deux axes. N'en retourner qu'un ferait du ruban le MIROIR du sien, qui
// tournerait à l'envers vu d'en haut.
struct RibbonEdges {
  glm::vec3 first{0.0F, 0.0F, 0.0F};
  glm::vec3 second{0.0F, 0.0F, 0.0F};
};

[[nodiscard]] RibbonEdges EdgesAt(const EffectTrack& effect_track, std::int32_t angle) {
  const float layer = CosOfDegrees(effect_track.tilt_degrees);
  const float dressee = SinOfDegrees(effect_track.tilt_degrees);
  const float le_long = CosOfDegrees(angle);
  const float half = effect_track.lift_key;
  const glm::vec3 centre{effect_track.radius_at_anchor * layer * le_long,
                         effect_track.value[kSpinRibbonSlotOfHeight] -
                             effect_track.radius_at_anchor * dressee * le_long,
                         -effect_track.radius_at_anchor * SinOfDegrees(angle)};
  const glm::vec3 offset{-half * dressee, -half * layer, 0.0F};
  return {centre + offset, centre - offset};
}

}  // namespace

std::vector<EffectWorldQuad> ComposeSpinRibbonQuads(const EffectTrack& track, std::uint8_t variant,
                                                    glm::vec3 anchor) {
  std::vector<EffectWorldQuad> quads;
  if (!track.active || track.degrees < 0) {
    return quads;
  }
  const std::int32_t step = track.degrees / kSpinRibbonSteps;
  if (step <= 0) {
    return quads;
  }
  const EffectTrailVariant tint = EffectTrailVariantFor(variant);
  const std::int16_t alpha = tint.forced_alpha != 0 ? tint.forced_alpha : track.alpha;
  if (alpha <= 0) {
    return quads;
  }

  RibbonEdges previous;
  bool has_previous = false;
  std::int32_t band = 0;
  for (std::int32_t travelled = 0; travelled <= track.degrees; travelled += step) {
    // ⚠ LE DERNIER ÉCHANTILLON REPREND L'AZIMUT DE DÉPART quand le tour est
    // entier : accumuler le pas y laisserait une fente large de l'arrondi.
    const bool referme = track.degrees == kEffectTrackFullTurnDegrees && band == kSpinRibbonSteps;
    const std::int32_t angle =
        referme ? track.azimuth_degrees : WithinFullTurn(track.azimuth_degrees + travelled);
    const RibbonEdges edges = EdgesAt(track, angle);

    if (has_previous) {
      const float u_before = static_cast<float>(band - 1) / static_cast<float>(kBands);
      const float u_after = static_cast<float>(band) / static_cast<float>(kBands);
      EffectWorldQuad quad;
      quad.corners = {anchor + previous.first, anchor + edges.first, anchor + edges.second,
                      anchor + previous.second};
      quad.texture_uv = {
          glm::vec2{u_before, kTextureAtFirstEdge}, glm::vec2{u_after, kTextureAtFirstEdge},
          glm::vec2{u_after, kTextureAtSecondEdge}, glm::vec2{u_before, kTextureAtSecondEdge}};
      quad.tint = {tint.color[0], tint.color[1], tint.color[2], static_cast<std::uint8_t>(alpha)};
      quad.blend_source = kEffectNodeBlendSource;
      quad.blend_destination = tint.blend_destination;
      quads.push_back(quad);
    }

    previous = edges;
    has_previous = true;
    // ⚠ L'INDEX DE BANDE SE BLOQUE AU VINGTIÈME, et le client le fait
    // explicitement : un balayage qui n'est pas multiple de vingt donne des
    // échantillons de plus, qui repeignent tous la dernière bande.
    band = std::min(band + 1, kSpinRibbonSteps);
  }
  return quads;
}

void AdvanceSpinRibbonTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  for (EffectTrack& effect_track : tracks) {
    if (!effect_track.active) {
      continue;
    }
    ++effect_track.ticks;
    // LA VITESSE EST PAR PISTE : le client l'ajoute en flottant puis tronque.
    const std::int32_t rotated =
        static_cast<std::int32_t>(static_cast<float>(effect_track.azimuth_degrees) +
                                  effect_track.value[kSpinRibbonSlotOfSpinSpeed]);
    effect_track.azimuth_degrees = static_cast<std::int16_t>(
        rotated >= kEffectTrackFullTurnDegrees ? rotated - kEffectTrackFullTurnDegrees : rotated);
  }
}

void AdvanceRisingSpinRibbonTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                   std::uint32_t& generator) {
  for (EffectTrack& effect_track : tracks) {
    if (!effect_track.active) {
      continue;
    }
    ++effect_track.ticks;
    // ⚠ LE RETARD EST PAR PISTE, et il se tire au semis : tant que le compteur
    // n'est pas positif, la piste ne bouge NI NE TIRE.
    if (effect_track.ticks <= 0) {
      continue;
    }
    if (effect_track.ticks <= kRiseUntilTick) {
      effect_track.alpha = static_cast<std::int16_t>(effect_track.alpha + kAlphaStep);
    } else if (effect_track.ticks > kFallAfterTick) {
      effect_track.alpha = static_cast<std::int16_t>(effect_track.alpha - kAlphaStep);
    }
    // ⚠ ET RIEN ENTRE LES DEUX : l'alpha tient son plateau du vingt-sixième au
    // soixante-dixième tic.
    effect_track.alpha = std::max<std::int16_t>(effect_track.alpha, 0);
    effect_track.radius_at_anchor *= kRadiusGrowth;

    // ⚠ L'INCLINAISON AVANCE D'UN DEGRÉ, PUIS D'UN DE PLUS UNE FOIS SUR DEUX.
    // Le client incrémente, tire, et n'incrémente une seconde fois que sur le
    // zéro. Tirer d'abord donnerait la même moyenne et un autre grain.
    std::int32_t tilt = effect_track.tilt_degrees + kTiltStep;
    if (NextEffectNodeDraw(generator) % static_cast<std::uint32_t>(kTiltDrawSpread) == 0U) {
      ++tilt;
    }
    effect_track.tilt_degrees = static_cast<std::int16_t>(
        tilt >= kEffectTrackFullTurnDegrees ? tilt - kEffectTrackFullTurnDegrees : tilt);

    const std::int32_t rotated =
        static_cast<std::int32_t>(static_cast<float>(effect_track.azimuth_degrees) +
                                  effect_track.value[kSpinRibbonSlotOfSpinSpeed]);
    effect_track.azimuth_degrees = static_cast<std::int16_t>(
        rotated >= kEffectTrackFullTurnDegrees ? rotated - kEffectTrackFullTurnDegrees : rotated);
  }
}

}  // namespace ro::render
