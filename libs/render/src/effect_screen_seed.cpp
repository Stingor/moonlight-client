#include "ro/render/effect_screen_seed.h"

#include <cmath>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_node.h"
#include "ro/render/map_effect_math.h"

namespace ro::render {
namespace {

// Un tirage rendu à sa plage. Une plage vide donne sa borne basse plutôt qu'un
// tirage inutile : c'est ainsi qu'un patron dit « toujours cette valeur ».
//
// La même règle que `DrawWithin` d'`effect_node_pool.cpp`, et il le faut : deux
// conventions de tirage feraient diverger deux familles sur la même donnée.
[[nodiscard]] float DrawInRange(const EffectNodeRange& plage, std::uint32_t& rng_state) {
  return DrawEffectNodeRange(plage, rng_state);
}

// OÙ LE NŒUD NAÎT : son décalage fixe, plus un point du cercle de naissance.
//
// ⚠ L'ORDONNÉE D'ÉCRAN SE SOUSTRAIT, comme dans le déplacement : ce qui monte a
// une ordonnée qui diminue. Et le cap se TRONQUE, parce que le client interroge
// une table de trigonométrie indexée par degré ENTIER — un degré de travers pour
// un négatif, voir `NativeTableRadians`.
//
// 🔴 L'ABSCISSE PREND LE SINUS ET L'ORDONNÉE PERD LE COSINUS, comme le
// déplacement : les deux tables du client ont leurs noms échangés.
[[nodiscard]] glm::vec2 BirthOffset(const EffectScreenPattern& pattern, float heading_degrees,
                                    std::uint32_t& rng_state) {
  const glm::vec2 fixed = {pattern.screen_offset_x, pattern.screen_offset_y};
  if (pattern.birth_radius.high <= 0.0F && pattern.birth_radius.low <= 0.0F) {
    return fixed;
  }
  const float radius = DrawInRange(pattern.birth_radius, rng_state);
  const float cap = NativeTableRadians(static_cast<std::int32_t>(std::trunc(heading_degrees)));
  return fixed + glm::vec2{std::sin(cap) * radius, -std::cos(cap) * radius};
}

}  // namespace

std::span<const EffectScreenPattern> EffectScreenPatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectScreenPatternTable(), effect_id, &EffectScreenPattern::effect_id);
}

EffectScreenNode SowEffectScreenNode(const EffectScreenPattern& pattern, std::uint32_t& generator) {
  EffectScreenNode node;
  node.half_width = DrawInRange(pattern.half_width, generator);
  node.half_height = DrawInRange(pattern.half_height, generator);
  node.step = DrawInRange(pattern.step, generator);
  node.heading_degrees = DrawInRange(pattern.heading_degrees, generator);

  node.life_ticks = pattern.life_ticks;
  node.fade_out_tick = pattern.fade_out_tick;
  node.second_step_tick = pattern.second_step_tick;
  node.growth_stop_tick = pattern.growth_stop_tick;
  node.ticks_per_frame = pattern.ticks_per_frame;
  node.texture_count = pattern.texture_count;
  node.frame_loops = pattern.frame_loops;

  node.alpha = pattern.alpha;
  node.alpha_max = pattern.alpha_max;
  node.alpha_step = pattern.alpha_step;
  node.step_change = pattern.step_change;
  node.half_width_step = pattern.half_width_step;
  node.half_width_acceleration = pattern.half_width_acceleration;
  node.half_height_step = pattern.half_height_step;
  node.half_height_acceleration = pattern.half_height_acceleration;
  node.half_width_step_after = pattern.half_width_step_after;
  node.half_height_step_after = pattern.half_height_step_after;
  node.turn_speed = pattern.turn_speed;
  // L'ANGLE DU QUAD : le cap lui-même quand le bâtisseur écrit les deux du
  // même tirage, sinon le sien. Voir `EffectScreenPattern::turn_follows_heading`.
  node.turn_degrees = pattern.turn_follows_heading ? node.heading_degrees
                                                   : DrawInRange(pattern.turn_degrees, generator);

  node.screen_offset = BirthOffset(pattern, node.heading_degrees, generator);
  node.color = {pattern.red, pattern.green, pattern.blue};

  node.flags = pattern.flags;
  node.blend_mode = pattern.blend_mode;
  node.uses_blend_mode = pattern.uses_blend_mode;
  return node;
}

std::vector<std::string> EffectScreenTexturePaths(const EffectScreenPattern& pattern) {
  if (pattern.texture_format.empty()) {
    return {std::string(pattern.texture)};
  }
  // ⚠ LE CLIENT NUMÉROTE À PARTIR DE UN, et non de zéro : son `sprintf` reçoit
  // l'index de boucle PLUS UN. Partir de zéro chercherait une image qui n'existe
  // pas et en laisserait une dernière de côté.
  const std::string format(pattern.texture_format);
  const std::size_t mark = format.find("%d");
  if (mark == std::string::npos) {
    return {format};
  }
  std::vector<std::string> paths;
  const std::int32_t count = pattern.texture_count > 0 ? pattern.texture_count : 1;
  paths.reserve(static_cast<std::size_t>(count));
  for (std::int32_t number = 1; number <= count; ++number) {
    paths.push_back(format.substr(0, mark) + std::to_string(number) + format.substr(mark + 2));
  }
  return paths;
}

}  // namespace ro::render
