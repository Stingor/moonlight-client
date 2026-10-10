#include "ro/render/effect_screen_burst_seed.h"

#include <algorithm>
#include <cstddef>

#include "ro/render/effect_node.h"

namespace ro::render {
namespace {

// LE TOUR COMPLET, sur lequel l'angle de départ d'un éclat se tire.
//
// ⚠ IL EST ÉCRIT ICI plutôt qu'emprunté à la piste du monde : une piste n'a
// rien à faire dans la voie d'écran, et lier les deux ferait croire à une
// parenté que la mesure ne donne pas.
constexpr std::int32_t kTourComplet = 360;

[[nodiscard]] std::int32_t DrawBelow(std::uint32_t& rng_state, std::int32_t exclusive_bound) {
  return static_cast<std::int32_t>(NextEffectNodeDraw(rng_state) %
                                   static_cast<std::uint32_t>(exclusive_bound));
}

// UN TIRAGE DU CLIENT, ou la valeur fixe qu'une étendue nulle désigne.
[[nodiscard]] float DrawRange(const EffectScreenBurstDraw& plage, std::uint32_t& rng_state) {
  const float brut =
      plage.spread > 0 ? static_cast<float>(DrawBelow(rng_state, plage.spread)) : 0.0F;
  return (brut + plage.floor) / plage.divisor;
}

// LA COURONNE : rien n'y est tiré. Ses cinq nombres viennent du bâtisseur ou du
// constructeur.
[[nodiscard]] EffectScreenNode SeedRing(const EffectScreenBurstPattern& node_pattern) {
  EffectScreenNode node;
  node.shape = EffectScreenShape::kRing;
  node.life_ticks = node_pattern.life_ticks;
  node.fade_out_tick = node_pattern.fade_out_tick;
  node.screen_offset = {0.0F, node_pattern.screen_offset_y};
  node.alpha = node_pattern.alpha;
  node.alpha_max = node_pattern.alpha_max;
  node.alpha_step = node_pattern.alpha_step;
  node.flags = node_pattern.flags;
  node.angle_degrees = node_pattern.angle_degrees;
  node.outer_radius = node_pattern.outer_radius;
  node.outer_radius_step = node_pattern.outer_radius_step;
  node.outer_radius_acceleration = node_pattern.outer_radius_acceleration;
  node.band_width_target = node_pattern.band_width_target;
  return node;
}

// L'ÉCLAT : cinq tirages, et l'ordre est celui du client.
[[nodiscard]] EffectScreenNode SowShard(const EffectScreenBurstPattern& node_pattern,
                                        std::uint32_t& rng_state) {
  EffectScreenNode node;
  node.shape = EffectScreenShape::kWedge;
  node.life_ticks = node_pattern.life_ticks;
  node.fade_out_tick = node_pattern.fade_out_tick;
  node.second_step_tick = node_pattern.second_step_tick;
  node.screen_offset = {0.0F, node_pattern.screen_offset_y};
  node.alpha = node_pattern.alpha;
  node.alpha_max = node_pattern.alpha_max;
  node.alpha_step = node_pattern.alpha_step;
  node.flags = node_pattern.flags;

  // ⚠ L'ORDRE DES TIRAGES EST CELUI DU CLIENT, et le mélanger décalerait tout
  // ce qui suit : l'angle de départ, la vitesse de rotation, la demi-ouverture,
  // le rayon, sa croissance, puis la vitesse d'après.
  node.turn_degrees = static_cast<float>(DrawBelow(rng_state, kTourComplet));
  node.turn_speed =
      (static_cast<float>(DrawBelow(rng_state, kScreenWedgeSpinSpread)) + kScreenWedgeSpinFloor) /
      kScreenWedgeSpinDivisor;
  // ⚠ ET L'ACCÉLÉRATION SE CALCULE, elle ne se tire pas : la rotation s'arrête
  // aux deux tiers de la vie. Une vie nulle ne la ferait pas ralentir.
  node.turn_acceleration =
      node_pattern.life_ticks != 0
          ? -(node.turn_speed / static_cast<float>(node_pattern.life_ticks)) / kScreenWedgeSpinDecay
          : 0.0F;

  node.angle_degrees = DrawRange(node_pattern.half_angle, rng_state);
  node.half_height = DrawRange(node_pattern.radius, rng_state);
  node.half_height_step = DrawRange(node_pattern.radius_step, rng_state);
  // ⚠ CE QUE LA ROTATION DEVIENT AU SECOND PAS, et le pas de rayon avec : le
  // client range zéro dans le second, ce qui FIGE le rayon. La sentinelle du
  // quad n'a pas cours ici.
  node.turn_speed_after = DrawRange(node_pattern.turn_speed_after, rng_state);
  node.turn_acceleration_after = 0.0F;
  node.half_height_step_after = 0.0F;
  return node;
}

}  // namespace

std::span<const EffectScreenBurstPattern> EffectScreenBurstPatternsFor(std::uint32_t effect_id) {
  const std::span<const EffectScreenBurstPattern> table = EffectScreenBurstPatternTable();
  const auto first =
      std::ranges::lower_bound(table, effect_id, {}, &EffectScreenBurstPattern::effect_id);
  const auto last =
      std::ranges::upper_bound(table, effect_id, {}, &EffectScreenBurstPattern::effect_id);
  return table.subspan(static_cast<std::size_t>(first - table.begin()),
                       static_cast<std::size_t>(last - first));
}

std::vector<EffectScreenNode> SowEffectScreenBurstNodes(const EffectScreenBurstPattern& pattern,
                                                        std::uint32_t& generator) {
  std::vector<EffectScreenNode> nodes;
  nodes.reserve(pattern.count);
  for (std::uint16_t rang = 0; rang < pattern.count; ++rang) {
    nodes.push_back(pattern.shape == EffectScreenShape::kRing ? SeedRing(pattern)
                                                              : SowShard(pattern, generator));
  }
  return nodes;
}

}  // namespace ro::render
