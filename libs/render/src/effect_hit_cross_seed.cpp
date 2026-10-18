// La croix d'un impact. Voir l'en-tête pour l'angle partagé et les trois champs
// calculés.

#include "ro/render/effect_hit_cross_seed.h"

#include <algorithm>
#include <array>
#include <cstddef>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_node.h"

namespace ro::render {
namespace {

// LA TABLE, tenue ici et non engendrée : deux bâtisseurs recopiés champ par
// champ.
//
// ⚠ LES DEUX SONT LE MÊME CODE À QUATRE NOMBRES PRÈS. Les écrire comme deux
// entrées plutôt que comme un patron paramétré est délibéré : ce sont deux
// relevés, et l'un pourrait démentir l'autre au prochain client.
constexpr std::array<EffectHitCrossPattern, 2> kHitCrosses = {{
    {4,
     "EffectUpdate_Hit5",
     "data\\texture\\effect\\lens2.tga",
     // vie : 17 tics, posés en clair
     17,
     // demi-largeur : rand() % 5 + 20
     {20.0F, 24.0F, 5},
     // demi-hauteur : rand() % 10 + 30
     {30.0F, 39.0F, 10},
     // l'étirement, en largeur puis en hauteur
     2.5F,
     0.5F,
     // la rotation
     -5.0F,
     1.5F,
     // le décalage vertical
     -20.0F},
    {5,
     "EffectUpdate_Hit6",
     "data\\texture\\effect\\lens2.tga",
     17,
     // demi-largeur : rand() % 5 + 10
     {10.0F, 14.0F, 5},
     // demi-hauteur : rand() % 5 + 15
     {15.0F, 19.0F, 5},
     // ⚠ 1,7 ET NON 2,5 : c'est le seul champ d'étirement qui sépare les deux
     // bâtisseurs.
     1.7F,
     0.5F,
     -5.0F,
     1.5F,
     -20.0F},
}};

// Un tirage rendu à sa plage, avec son nombre de valeurs : voir
// `EffectNodeRange::values`.
[[nodiscard]] float DrawInRange(const EffectNodeRange& plage, std::uint32_t& rng_state) {
  return DrawEffectNodeRange(plage, rng_state);
}

}  // namespace

std::span<const EffectHitCrossPattern> EffectHitCrossPatternTable() { return kHitCrosses; }

std::span<const EffectHitCrossPattern> EffectHitCrossPatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectHitCrossPatternTable(), effect_id,
                           &EffectHitCrossPattern::effect_id);
}

std::vector<EffectScreenNode> SowEffectHitCrossNodes(const EffectHitCrossPattern& pattern,
                                                     std::uint32_t& generator) {
  // 🔴 TIRÉ UNE SEULE FOIS, AVANT LA BOUCLE, comme le client : c'est ce tirage
  // partagé qui rend les deux branches PERPENDICULAIRES. Le tirer dans la boucle
  // donnerait deux barres sans rapport l'une avec l'autre.
  const float base_degrees = static_cast<float>(NextEffectNodeDraw(generator) %
                                                static_cast<std::uint32_t>(kHitCrossFullTurn));

  const auto life = static_cast<float>(std::max(1, pattern.life_ticks));
  std::vector<EffectScreenNode> arms;
  arms.reserve(static_cast<std::size_t>(kHitCrossArms));
  for (std::int32_t arm = 0; arm < kHitCrossArms; ++arm) {
    EffectScreenNode node;
    node.life_ticks = pattern.life_ticks;
    // ⚠ LE TIC DE FONDU EST CALCULÉ : la vie moins sa moitié, en division
    // ENTIÈRE — 17 donne 9, pas 8,5.
    node.fade_out_tick = pattern.life_ticks - pattern.life_ticks / 2;
    node.screen_offset = {0.0F, pattern.screen_offset_y};

    node.turn_degrees = base_degrees + static_cast<float>(arm) * kHitCrossArmDegrees;
    node.turn_speed = pattern.turn_speed;
    node.turn_acceleration = -(pattern.turn_speed / life) / pattern.turn_slowdown_divisor;

    node.half_width = DrawInRange(pattern.half_width, generator);
    node.half_height = DrawInRange(pattern.half_height, generator);
    // ⚠ CALCULÉ SUR LA LARGEUR TIRÉE, et non sur une borne : la branche
    // s'amincit jusqu'à disparaître en exactement une vie, quelle que soit sa
    // taille de naissance.
    node.half_width_step = -(node.half_width / life);
    node.half_height_step = pattern.half_height_step;
    node.half_height_acceleration = pattern.half_height_acceleration;

    arms.push_back(node);
  }
  return arms;
}

}  // namespace ro::render
