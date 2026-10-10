// LES PISTES D'UN ANNEAU EN COUCHES, et quel effet en sème un.
//
// 2 bâtisseur(s) :
//   4 piste(s), les mêmes pour chaque valeur de teinte — 5 valeur(s).
//   3 piste(s), les mêmes pour chaque valeur de style — 2 valeur(s).
//
// 20 nœuds répartis sur 19 effets. Les valeurs employées sèment les
// MÊMES pistes chez un même bâtisseur — vérifié par le générateur — et ne
// diffèrent que par leur teinte.
//
// ENGENDRÉ par scripts/gen_client_effect_layered_rings.py — ne pas
// modifier à la main.

#include <algorithm>
#include <array>

#include "ro/render/effect_layered_ring_seed.h"
#include "ro/render/effect_node.h"

namespace ro::render {
namespace {

// LES PISTES DES DEUX BÂTISSEURS, À PLAT : chaque patron y prend sa
// tranche. Voir `EffectLayeredRingTracksFor`.
constexpr std::array<EffectLayeredRingTrackSeed, 7> kLayeredRingTracks = {{
    // Le bâtisseur par teinte
    {315, 0, 70, 180, 4.5F, 25.0F},
    {315, 90, 57, 180, 5.0F, 22.0F},
    {315, 180, 45, 180, 5.5F, 19.0F},
    {360, 0, 89, 70, 4.0F, 250.0F},
    // Le bâtisseur par style
    {315, 0, 55, 120, 3.9F, 15.0F},
    {315, 90, 50, 120, 4.1F, 13.0F},
    {315, 180, 45, 120, 4.3F, 11.0F},
}};

constexpr std::array<EffectLayeredRingNodePattern, 20> kLayeredRingNodes = {{
    {54, "Effect_Build_LayeredRingNode", "data\\texture\\effect\\ring_blue.tga", 2, 0, 4, 70,
     false},
    {55, "Effect_Build_LayeredRingNode", "data\\texture\\effect\\ring_yellow.tga", 1, 0, 4, 70,
     false},
    {56, "Effect_Build_LayeredRingNode", "data\\texture\\effect\\Magic_Green.tga", 0, 0, 4, 70,
     true},
    {57, "Effect_Build_LayeredRingNode", "data\\texture\\effect\\ring_yellow.tga", 0, 0, 4, 70,
     false},
    {59, "Effect_Build_LayeredRingNode", "data\\texture\\effect\\Magic_Violet.tga", 0, 0, 4, 70,
     true},
    {172, "Effect_Build_LayeredRingNode", "data\\texture\\effect\\ring_black.tga", 3, 0, 4, 70,
     false},
    {173, "Effect_Build_LayeredRingNode", "data\\texture\\effect\\ring_red.tga", 3, 0, 4, 70,
     false},
    {174, "Effect_Build_LayeredRingNode", "data\\texture\\effect\\ring_blue.tga", 3, 0, 4, 70,
     false},
    {175, "Effect_Build_LayeredRingNode", "data\\texture\\effect\\ring_yellow.tga", 3, 0, 4, 70,
     false},
    {176, "Effect_Build_LayeredRingNode", "data\\texture\\effect\\ring_jadu.tga", 3, 0, 4, 70,
     false},
    {177, "Effect_Build_LayeredRingNode", "data\\texture\\effect\\ring_brown.tga", 3, 0, 4, 70,
     false},
    {178, "Effect_Build_LayeredRingNode", "data\\texture\\effect\\ring_white.tga", 3, 0, 4, 70,
     false},
    {179, "Effect_Build_LayeredRingNode", "data\\texture\\effect\\ring_purple.tga", 3, 0, 4, 70,
     false},
    {200, "Effect_Build_TripleRingsFromParam17", "data\\texture\\effect\\ring_blue.tga", 4, 4, 3,
     kEffectNodeDefaultLifeTicks, false},
    {397, "Effect_Build_TripleRingsFromParam17", "data\\texture\\effect\\ring_blue.tga", 4, 4, 3,
     kEffectNodeDefaultLifeTicks, false},
    {679, "Effect_Build_TripleRingsFromParam17", "data\\texture\\effect\\ring_white.tga", 7, 4, 3,
     kEffectNodeDefaultLifeTicks, false},
    {1449, "Effect_Build_LayeredRingNode", "data\\texture\\effect\\new_jackfrost\\ring_ice.tga", 2,
     0, 4, 70, false},
    {1664, "Effect_Build_LayeredRingNode", "data\\texture\\effect\\ring_yellow.tga", 0, 0, 4, 70,
     true},
    {1709, "Effect_Build_LayeredRingNode",
     "data\\texture\\effect\\mineffect\\new_earthdrive\\ring_brown.tga", 0, 0, 4, 70, true},
    {1709, "Effect_Build_LayeredRingNode", "data\\texture\\effect\\new_earthdrive\\ring_brown.tga",
     0, 0, 4, 70, true},
}};

}  // namespace

std::span<const EffectLayeredRingTrackSeed> EffectLayeredRingTrackSeedTable() {
  return kLayeredRingTracks;
}

std::span<const EffectLayeredRingTrackSeed> EffectLayeredRingTracksFor(
    const EffectLayeredRingNodePattern& pattern) {
  return std::span<const EffectLayeredRingTrackSeed>{kLayeredRingTracks}.subspan(
      pattern.first_track, pattern.track_count);
}

std::span<const EffectLayeredRingNodePattern> EffectLayeredRingNodePatternTable() {
  return kLayeredRingNodes;
}

std::span<const EffectLayeredRingNodePattern> EffectLayeredRingNodePatternsFor(
    std::uint32_t effect_id) {
  const auto [first, last] = std::ranges::equal_range(kLayeredRingNodes, effect_id, {},
                                                      &EffectLayeredRingNodePattern::effect_id);
  return {first, last};
}

}  // namespace ro::render
