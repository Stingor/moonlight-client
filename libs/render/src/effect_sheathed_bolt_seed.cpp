#include "ro/render/effect_sheathed_bolt_seed.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_sheathed_bolt.h"
#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// LES CINQ RECETTES, dans l'ordre de l'énumération.
constexpr std::array<EffectSheathedBoltRecipe, 5> kRecettes = {{
    // `kAimedWhite` : le trait blanc, court et prompt.
    {-45,
     20,
     {100, 0.08F, -4.0F},
     {100, 0.15F, 0.0F},
     {0, 0.0F, 0.05F},
     {100, 0.04F, 8.0F},
     {100, 0.6F, 0.0F},
     true,
     kSheathedBoltWhiteStyle},
    // `kAimedPink` : le trait rose, qui part bien plus tard et dont l'âme
    // elle-même se tire au sort.
    {-30,
     110,
     {100, 0.08F, -4.0F},
     {100, 0.15F, 0.0F},
     {11, 0.008F, 0.02F},
     {100, 0.06F, 4.0F},
     {100, 0.3F, 0.0F},
     true,
     kSheathedBoltPinkStyle},
    // `kImpactBlue` : le trait bleu d'un impact, de portée posée.
    {0,
     20,
     {100, 0.06F, -3.0F},
     {100, 0.15F, 0.0F},
     {0, 0.0F, 0.25F},
     {100, 0.04F, 8.0F},
     {100, 0.2F, 20.0F},
     false,
     kSheathedBoltBlueStyle},
    // `kImpactDim` : le bleu, cinq fois plus large et cinq fois plus long, et
    // dont le recul vaut CENT en dur — le trait traverse donc le point d'impact
    // au lieu d'en partir.
    {0,
     20,
     {100, 0.30F, -15.0F},
     {0, 0.0F, 100.0F},
     {0, 0.0F, 0.25F},
     {100, 0.04F, 8.0F},
     {100, 0.5F, 100.0F},
     false,
     kSheathedBoltDimStyle},
    // `kImpactYellow` : le trait bleu à UNE constante près.
    //
    // ⚠ SEUL LE STYLE CHANGE, et c'est mesuré champ par champ : les deux
    // bâtisseurs écrivent les mêmes tirages dans le même ordre, et le style
    // vaut zéro ici contre quatre là. La figure n'a donc pas de teinte pour lui
    // et retombe sur son défaut.
    {0,
     20,
     {100, 0.06F, -3.0F},
     {100, 0.15F, 0.0F},
     {0, 0.0F, 0.25F},
     {100, 0.04F, 8.0F},
     {100, 0.2F, 20.0F},
     false,
     kSheathedBoltUnnamedStyle},
}};

[[nodiscard]] std::int32_t DrawBelow(std::uint32_t& rng_state, std::int32_t exclusive_bound) {
  return static_cast<std::int32_t>(NextEffectNodeDraw(rng_state) %
                                   static_cast<std::uint32_t>(exclusive_bound));
}

// UN TIRAGE DE RECETTE. Une largeur nulle rend le plancher SANS faire avancer
// le générateur : c'est ce que fait le client, qui n'appelle alors pas `rand`.
[[nodiscard]] float DrawFrom(const EffectSheathedBoltDraw& draw, std::uint32_t& rng_state) {
  if (draw.spread <= 0) {
    return draw.base;
  }
  return static_cast<float>(DrawBelow(rng_state, draw.spread)) * draw.step + draw.base;
}

}  // namespace

const EffectSheathedBoltRecipe& EffectSheathedBoltRecipeOf(EffectSheathedBoltBuilder builder) {
  return kRecettes[static_cast<std::size_t>(builder)];
}

std::span<const EffectSheathedBoltPattern> EffectSheathedBoltPatternsFor(std::uint32_t effect_id) {
  const std::span<const EffectSheathedBoltPattern> table = EffectSheathedBoltPatternTable();
  const auto first =
      std::ranges::lower_bound(table, effect_id, {}, &EffectSheathedBoltPattern::effect_id);
  const auto last =
      std::ranges::upper_bound(table, effect_id, {}, &EffectSheathedBoltPattern::effect_id);
  return table.subspan(static_cast<std::size_t>(first - table.begin()),
                       static_cast<std::size_t>(last - first));
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectSheathedBoltTracks(
    const EffectSheathedBoltPattern& pattern, glm::vec3 anchor, glm::vec3 toward_target,
    std::uint32_t& generator) {
  const EffectSheathedBoltRecipe& recipe = EffectSheathedBoltRecipeOf(pattern.builder);
  const std::int32_t cap = HeadingOfVector(toward_target.x, toward_target.z);
  const float distance =
      std::sqrt(toward_target.x * toward_target.x + toward_target.z * toward_target.z);

  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (EffectTrack& track : tracks) {
    track.active = true;
    track.ticks = recipe.delay_floor - DrawBelow(generator, recipe.delay_spread);
    track.azimuth_degrees = static_cast<std::int16_t>(cap);
    // ⚠ IL NAÎT INVISIBLE : c'est le mouvement qui l'allume.
    track.alpha = 0;

    const float offset_x = DrawFrom(recipe.scatter, generator);
    const float offset_z = DrawFrom(recipe.scatter, generator);
    // ⚠ LE RECUL TIRE L'ANCRE VERS L'ARRIÈRE, dans le sens OPPOSÉ à la cible :
    // le client RETRANCHE le cap au lieu de l'ajouter. En recopier le sens
    // ferait partir le trait de derrière la cible.
    //
    // Le client le range au passage dans une case que personne ne relit ; le
    // garder ici ne servirait qu'à donner un sens à une case qui n'en a pas.
    //
    // La dispersion en profondeur est tirée dans le repère du client : elle se
    // RETRANCHE dans le nôtre, dont la profondeur est opposée.
    const float recul = DrawFrom(recipe.pullback, generator);
    track.anchor = {anchor.x + offset_x - CosOfDegrees(cap) * recul, anchor.y,
                    anchor.z - offset_z - SinOfDegrees(cap) * recul};
    // ⚠ LES DEUX BOUTS COÏNCIDENT À LA NAISSANCE, et le trait n'a donc aucune
    // longueur tant que le mouvement n'a pas joué. Son alpha est nul d'ici là,
    // si bien que rien ne se voit — mais poser le bout lointain au semis
    // montrerait le trait entier un tic trop tôt.
    track.tail[kSheathedBoltSlotOfFarX] = track.anchor.x;
    track.tail[kSheathedBoltSlotOfFarY] = track.anchor.y;
    track.tail[kSheathedBoltSlotOfFarZ] = track.anchor.z;

    track.lift_key = DrawFrom(recipe.width, generator);
    track.value[kSheathedBoltSlotOfLift] = DrawFrom(recipe.lift, generator);
    track.radius_at_anchor =
        DrawFrom(recipe.reach, generator) + (recipe.reach_from_target ? distance : 0.0F);
    track.value[kSheathedBoltSlotOfStyle] = recipe.style;
  }
  return tracks;
}

PooledEffectNode SowEffectSheathedBoltNode(const EffectSheathedBoltPattern& pattern,
                                           glm::vec3 anchor, glm::vec3 toward_target,
                                           std::int32_t effect_life_ticks,
                                           std::uint32_t& generator) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kSheathedBolt;
  pooled.ground_position = anchor;
  pooled.owner_position = anchor;
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectSheathedBoltTracks(pattern, anchor, toward_target, generator);
  return pooled;
}

}  // namespace ro::render
