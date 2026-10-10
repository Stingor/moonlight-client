#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DE L'ARC-EN-CIEL, et les effets du sous-type 93.
//
// Une image, et tout le reste écrit en dur. UNE seule piste vive.
//
// ⚠ SON INCLINAISON EST L'ANGLE DE CAMÉRA DU TIC DE NAISSANCE, et elle ne bouge
// plus jamais. Le bâtisseur la calcule une fois — trois cent soixante moins le
// lacet de SA caméra — et le mouvement se contente de la replier. C'est un cap
// du repère du client : la figure le ramène au nôtre. L'arc se dresse donc face
// au joueur tel qu'il regardait AU MOMENT du sort, et reste ainsi.
//
// ⚠ ET SON ALPHA PART À ZÉRO : rien ne se voit tant que le mouvement n'a pas
// tourné, et il met cent quatre-vingts tics à s'ouvrir en entier.

// LE RAYON DE L'ARC, écrit en dur : cinquante unités.
inline constexpr float kRainbowTrailRadius = 50.0F;
// SON BALAYAGE : un DEMI-TOUR, et c'est ce qui en fait un arc et non un anneau.
inline constexpr std::int16_t kRainbowTrailSweepDegrees = 180;

// UN ARC QU'UN EFFET POSE : ce que son site d'appel passe au bâtisseur.
struct EffectRainbowTrailPattern {
  std::uint32_t effect_id = 0;
  // Le nom de la fonction du client qui pose cet arc.
  std::string_view client_function;
  std::string_view texture;
};

// Tous les arcs connus, par identifiant d'effet.
[[nodiscard]] std::span<const EffectRainbowTrailPattern> EffectRainbowTrailPatternTable();

// TOUS les arcs d'un effet, ou un span VIDE.
[[nodiscard]] std::span<const EffectRainbowTrailPattern> EffectRainbowTrailPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES D'UN ARC : une seule vive, éteinte et figée face à la vue.
//
// `billboard_turn_degrees` est le tour de panneau AU TIC DE NAISSANCE — trois
// cent soixante moins le lacet de la caméra du client, voir
// `EffectBillboardTurnDegrees` : `render` ne connaît pas la caméra (CLAUDE.md),
// et l'appelant le donne. Il devient l'inclinaison tel quel.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectRainbowTrailTracks(
    std::int16_t billboard_turn_degrees);

// LE NŒUD qu'un patron décrit, piste semée, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectRainbowTrailNode(const EffectRainbowTrailPattern& pattern,
                                                         glm::vec3 anchor,
                                                         std::int32_t effect_life_ticks,
                                                         std::int16_t billboard_turn_degrees);

}  // namespace ro::render
