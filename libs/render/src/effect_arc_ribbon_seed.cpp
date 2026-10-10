#include "ro/render/effect_arc_ribbon_seed.h"

#include <algorithm>
#include <string>

#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// LES CASES QUE LE BÂTISSEUR REMPLIT. Chacune est lue par un autre : la figure prend les marques 0
// et 2 et les cases 8 et 10, le mouvement prend la marque 4.
constexpr std::size_t kMarkOfTexture = 0;
constexpr std::size_t kMarkOfDrawing = 2;
constexpr std::size_t kMarkOfStyle = 4;
constexpr std::size_t kSlotOfSpinDegrees = 8;
constexpr std::size_t kSlotOfShape = 10;
// La case de queue où il range la hauteur elle-même.
constexpr std::size_t kTailSlotOfHeight = 4;

// Les pixels du sprite en unités du monde, et le relèvement du client.
constexpr float kBodyPixelsToUnits = 0.29F;
constexpr float kBodyLiftRaise = 4.0F;

// L'UNIQUE PISTE VIVE. Le bâtisseur allume la première et éteint les trois
// autres, à chaque style.
constexpr std::size_t kLivingTrack = 0;

// Un tirage entier dans [0, bound), comme le modulo du client.
[[nodiscard]] std::int32_t DrawBelow(std::uint32_t& generator, std::int32_t bound) {
  return static_cast<std::int32_t>(NextEffectNodeDraw(generator) %
                                   static_cast<std::uint32_t>(bound));
}

}  // namespace

const EffectArcRibbonStyleSeed* EffectArcRibbonStyleSeedFor(std::uint8_t style) {
  const std::span<const EffectArcRibbonStyleSeed> table = EffectArcRibbonStyleSeedTable();
  const auto found = std::ranges::find(table, style, &EffectArcRibbonStyleSeed::style);
  return found == table.end() ? nullptr : &*found;
}

float ArcRibbonLiftFromBody(std::optional<float> body_first_layer_y) {
  // ⚠ LE REPÈRE DU CLIENT DESCEND : son `ordonnée × 0,29 − 4` s'AJOUTE à la
  // hauteur de l'effet, et ce qui monte chez nous en est l'opposé.
  const float units = body_first_layer_y.value_or(0.0F) * kBodyPixelsToUnits;
  if (units == 0.0F) {
    return kArcRibbonLiftWithoutBody;
  }
  return kBodyLiftRaise - units;
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectArcRibbonTracks(
    const EffectArcRibbonStyleSeed& seed, glm::vec3 anchor, std::uint32_t& generator,
    float instance_scale, float body_lift) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  EffectTrack& living = tracks[kLivingTrack];
  living.active = true;

  // ⚠ LE REPÈRE DU CLIENT DESCEND, LE NÔTRE MONTE, et c'est ici que le sens se
  // rétablit. Le client retranche trente à la hauteur de son effet, ce qui chez
  // lui ÉLÈVE de trente ; chez nous il faut donc AJOUTER. La soustraction
  // symétrique enfoncerait l'arc de soixante sous sa place.
  const float lift = seed.lift_from_actor_height ? body_lift : seed.lift_above_effect;
  const float height = anchor.y + lift;
  living.anchor = {anchor.x, height, anchor.z};
  // Le client garde la hauteur DANS LA QUEUE, et le creux dans une case ; son
  // dessinateur y relit l'un et l'autre. Les poser tous deux garde la piste
  // lisible par qui la lit comme lui.
  living.tail[kTailSlotOfHeight] = height;
  living.value[kArcRibbonSlotOfHeightGap] = lift;

  // 🔴 L'ÉCHELLE DE L'INSTANCE PASSE AVANT LE RAYON DU STYLE, et la garde du
  // client porte sur elle seule : `si (échelle > 0) rayon = échelle × facteur ;
  // sinon rayon = constante`. Un style dont le facteur est nul ne la regarde
  // jamais.
  living.radius_at_anchor = instance_scale > 0.0F && seed.radius_per_instance_scale > 0.0F
                                ? instance_scale * seed.radius_per_instance_scale
                                : seed.radius_at_anchor;
  living.value[kSlotOfShape] = seed.shape_key;
  living.mark[kMarkOfTexture] = 0;
  living.mark[kMarkOfDrawing] = seed.draw_mark;
  living.mark[kMarkOfStyle] = seed.style_mark;

  // ⚠ L'ALPHA NAÎT NUL et le mouvement le monte : le bâtisseur le pose à zéro
  // pour tous les styles. Le naître plein montrerait la figure d'un coup.
  living.alpha = 0;
  living.ticks = 0;
  living.value[kSlotOfSpinDegrees] = 0.0F;

  // ⚠ L'AZIMUT SE TIRE TOUJOURS, l'angle de départ seulement pour neuf styles.
  // Les tirer dans cet ordre est ce qui rend deux nœuds semés à la suite
  // différents, et une graine rejouée identique.
  living.degrees =
      seed.degrees_drawn
          ? static_cast<std::int16_t>(DrawBelow(generator, kEffectTrackFullTurnDegrees))
          : seed.degrees;
  living.azimuth_degrees =
      static_cast<std::int16_t>(DrawBelow(generator, kEffectTrackFullTurnDegrees));
  return tracks;
}

PooledEffectNode SowEffectArcRibbonNode(const EffectArcRibbonNodePattern& pattern, glm::vec3 anchor,
                                        std::int32_t effect_life_ticks, std::uint32_t& generator,
                                        float instance_scale, float body_lift) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kArcRibbons;
  pooled.ground_position = anchor;
  // L'INSTANCE, que le mouvement relit à chaque tic : voir `kArcRibbonSlotOfHeightGap`.
  pooled.owner_position = anchor;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.node.life_ticks = effect_life_ticks;
  const EffectArcRibbonStyleSeed* style_seed = EffectArcRibbonStyleSeedFor(pattern.style);
  if (style_seed != nullptr) {
    pooled.tracks =
        SeedEffectArcRibbonTracks(*style_seed, anchor, generator, instance_scale, body_lift);
  }
  return pooled;
}

void FollowArcRibbonOwner(PooledEffectNode& pooled) {
  pooled.ground_position = pooled.owner_position;
  if (EffectTrack& living = pooled.tracks.front(); living.active) {
    living.anchor = {pooled.owner_position.x,
                     pooled.owner_position.y + living.value[kArcRibbonSlotOfHeightGap],
                     pooled.owner_position.z};
  }
}

}  // namespace ro::render
