#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_heal_sparkle.h"
#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// CE QUE LE BÂTISSEUR DE L'ÉTINCELLE POSE. Une seule entrée, et la table existe
// quand même : la chaîne « données → table » vaut mieux qu'une constante écrite
// dans le code, parce qu'elle se régénère quand les données changent.
struct EffectHealSparklePattern {
  std::uint32_t effect_id = 0;
  std::string_view builder;
  std::string_view texture;
  // Laquelle des vingt parures du carré teinté. Le client l'écrit dans la
  // troisième marque de la piste ZÉRO.
  std::uint8_t mark = 0;
  // Le rayon du carré, avant que l'angle propre ne le fasse respirer.
  float radius = 0.0F;
};

[[nodiscard]] std::span<const EffectHealSparklePattern> EffectHealSparklePatternTable();

// Les patrons d'un identifiant d'effet, vides quand il n'en a pas.
[[nodiscard]] std::span<const EffectHealSparklePattern> EffectHealSparklePatternsFor(
    std::uint32_t effect_id);

// LA PISTE UNIQUE. Les trois autres restent éteintes : le bâtisseur n'allume que
// la première, et le mouvement comme la figure ne travaillent que sur elle.
//
// ⚠ ELLE NAÎT INVISIBLE, alpha nul : rien ne se montre tant que le mouvement n'a
// pas tourné. Le prendre pour un défaut et l'ouvrir à plein ferait apparaître
// l'étincelle d'un coup au lieu de la faire paraître en vingt tics.
//
// 🔴 ET ELLE TIRE DEUX ANGLES AU SORT, tous deux sur le tour entier : celui de
// ses COINS et son angle PROPRE. Le premier tourne le carré dans son plan, le
// second décide où sa respiration en est. Les laisser à zéro donnait la même
// étincelle, toujours dans le même sens — et c'est ce qui se voyait en côte à
// côte.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectHealSparkleTracks(
    const EffectHealSparklePattern& pattern, std::uint32_t& generator);

[[nodiscard]] PooledEffectNode SowEffectHealSparkleNode(const EffectHealSparklePattern& pattern,
                                                        glm::vec3 anchor,
                                                        std::int32_t effect_life_ticks,
                                                        std::uint32_t& generator);

}  // namespace ro::render
