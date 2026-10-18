#include "ro/render/effect_tinted_quad_seed.h"

#include <algorithm>
#include <span>
#include <string>
#include <string_view>

#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

#include "effect_draw_below.h"

namespace ro::render {
namespace {

// LES CASES QUE LE BÂTISSEUR REMPLIT.
constexpr std::size_t kMarkOfTexture = 0;
constexpr std::size_t kMarkOfDrawing = 2;
constexpr std::size_t kMarkOfSubDrawing = 3;
constexpr std::size_t kSlotOfSpinDegrees = 8;
constexpr std::size_t kSlotOfNudgeKey = 9;
constexpr std::size_t kTailSlotOfHeight = 4;

// CE QUE LES DIX STYLES ONT EN COMMUN, et que la table n'a donc pas à répéter.
// Chacun est écrit en dur dans la queue partagée du bâtisseur.
//
// ⚠ L'ALPHA NAÎT PLEIN. Celui de l'arc naît nul et attend son mouvement ; ce
// carré-ci se montre dès le premier tic. Deux bâtisseurs de la même famille,
// deux réponses — et supposer l'une pour l'autre laisse l'écran vide ou montre
// tout d'un coup.
constexpr std::int16_t kStartingAlpha = 200;
constexpr std::int16_t kStartingDegrees = 45;
// ⚠ DE COMBIEN LA PISTE SE TIENT AU-DESSUS du point de l'effet. Le client
// l'écrit en RETRANCHANT six, dans un repère qui DESCEND : la soustraction
// élève. Recopier le signe au lieu du sens enfoncerait le carré sous le sol.
constexpr float kLiftAboveEffect = 6.0F;

// L'UNIQUE PISTE VIVE : le bâtisseur allume la première et éteint les trois
// autres, comme celui de l'arc.
constexpr std::size_t kLivingTrack = 0;

}  // namespace

const EffectTintedQuadStyleSeed* EffectTintedQuadStyleSeedFor(std::uint8_t style) {
  const std::span<const EffectTintedQuadStyleSeed> table = EffectTintedQuadStyleSeedTable();
  const auto found = std::ranges::find(table, style, &EffectTintedQuadStyleSeed::style);
  return found == table.end() ? nullptr : &*found;
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectTintedQuadTracks(
    const EffectTintedQuadStyleSeed& seed, glm::vec3 anchor, std::uint32_t& generator) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  EffectTrack& living = tracks[kLivingTrack];
  living.active = true;

  // ⚠ LE REPÈRE DU CLIENT DESCEND, LE NÔTRE MONTE : le sens se rétablit ici, et
  // c'est une ADDITION. Voir `kLiftAboveEffect`.
  const float height = anchor.y + kLiftAboveEffect;
  living.anchor = {anchor.x, height, anchor.z};
  living.tail[kTailSlotOfHeight] = height;

  living.alpha = kStartingAlpha;
  living.degrees = kStartingDegrees;
  living.ticks = 0;
  living.radius_at_anchor = seed.radius_at_anchor;
  living.value[kSlotOfSpinDegrees] = 0.0F;
  living.value[kSlotOfNudgeKey] = seed.nudge_key;
  living.mark[kMarkOfTexture] = 0;
  living.mark[kMarkOfDrawing] = seed.draw_mark;
  living.mark[kMarkOfSubDrawing] = seed.sub_mark;

  // L'azimut se tire pour tous les styles : deux nœuds semés à la suite ne se
  // superposent donc jamais exactement.
  living.azimuth_degrees =
      static_cast<std::int16_t>(DrawBelow(generator, kEffectTrackFullTurnDegrees));

  // ⚠ ET UN STYLE OUVRE LES TROIS AUTRES PISTES, SANS RIEN Y METTRE. Elles
  // naissent au point zéro, de rayon nul, l'alpha à zéro — invisibles. Leur
  // donner l'état de la première inventerait trois carrés que le client ne
  // montre pas ; ne pas les ouvrir du tout priverait leur mouvement des pistes
  // qu'il remplit.
  if (seed.lights_every_track) {
    for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
      if (rang == kLivingTrack) {
        continue;
      }
      tracks[rang].active = true;
      tracks[rang].alpha = 0;
    }
  }
  return tracks;
}

PooledEffectNode SowEffectTintedQuadNode(const EffectTintedQuadNodePattern& pattern,
                                         glm::vec3 anchor, std::int32_t effect_life_ticks,
                                         std::uint32_t& generator) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kTintedQuads;
  pooled.ground_position = anchor;
  pooled.node.life_ticks = effect_life_ticks;
  const EffectTintedQuadStyleSeed* style_seed = EffectTintedQuadStyleSeedFor(pattern.style);

  // ⚠ LE TIRAGE DE TEXTURE PASSE AVANT CELUI DE L'AZIMUT, comme chez le client :
  // son `switch` de texture est la première chose qui suive l'allocation, et
  // l'azimut se tire bien plus bas. Les intervertir donnerait à ces trois
  // styles-là un angle de départ qui n'est pas le leur.
  std::string_view texture = pattern.texture;
  if (style_seed != nullptr) {
    const std::span<const std::string_view> siennes = EffectTintedQuadStyleTextures(*style_seed);
    if (!siennes.empty()) {
      texture = siennes[NextEffectNodeDraw(generator) % siennes.size()];
    }
  }
  pooled.resource_paths = {std::string(texture)};

  if (style_seed != nullptr) {
    pooled.tracks = SeedEffectTintedQuadTracks(*style_seed, anchor, generator);
  }
  return pooled;
}

}  // namespace ro::render
