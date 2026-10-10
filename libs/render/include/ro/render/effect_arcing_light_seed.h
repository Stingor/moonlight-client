#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DE LA LUEUR BLANCHE QUI PASSE EN ARC, sous-type 109.
//
// UN SEUL bâtisseur, et un seul cas qui l'appelle : l'effet 503. Il ne lit de
// son cas que le nom de l'image ; tout le reste est écrit en dur, ou tiré.
//
// ⚠ UN NŒUD AU PREMIER TIC SEULEMENT : la garde est l'âge nul de l'instance.
// Aux tics suivants le bâtisseur ne fait plus que recaler l'instance sur son
// porteur, et c'est de là que le mouvement recopie la place du nœud.
//
// ⚠ LE NŒUD NAÎT SUR L'INSTANCE, sans écart.

// LE MOT DE RENDU que le bâtisseur écrit en dur. Voir `effect_render_word.h`.
inline constexpr std::uint32_t kArcingLightRenderWord = 5;

// LA DEMI-DIAGONALE DU CARRÉ, deux et demi.
inline constexpr float kArcingLightRadius = 2.5F;

// QUATRE ANGLES TIRÉS sur le tour, `rand() % 360`, que ni le mouvement ni la
// figure ne relisent : ils ne comptent que pour l'ordre des tirages.
inline constexpr std::size_t kArcingLightUnreadAngles = 4;
inline constexpr std::int32_t kArcingLightAngleDraws = 360;

// LA PLACE DE NAISSANCE, que le premier tic réécrit : l'écart latéral et la
// hauteur de l'angle zéro, `−cos(0) · 30` et `−sin(0) · 20 − 40`.
inline constexpr float kArcingLightBirthSideways = -30.0F;
inline constexpr float kArcingLightBirthDepthBelow = -40.0F;

// UN SITE : l'effet, la fonction du client et l'image qu'il passe.
struct EffectArcingLightPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::string_view texture;
};

// La table entière, triée par identifiant.
[[nodiscard]] std::span<const EffectArcingLightPattern> EffectArcingLightPatternTable();

[[nodiscard]] std::span<const EffectArcingLightPattern> EffectArcingLightPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES DU NŒUD, la première seule allumée. Le générateur avance de
// quatre tirages, dans l'ordre du client.
//
// `designated_actor_id` est l'acteur que la suite de la compétence nomme, sur
// qui l'effet 502 naîtra ; zéro n'en désigne aucun.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedArcingLightTracks(
    std::uint32_t& generator, std::uint32_t designated_actor_id = 0);

// LE NŒUD qu'un patron décrit, né à la place du porteur.
//
// `effect_life_ticks` est la vie de l'effet, que le bâtisseur recopie dans le
// nœud.
[[nodiscard]] PooledEffectNode SowArcingLightNode(const EffectArcingLightPattern& pattern,
                                                  glm::vec3 owner_position,
                                                  std::int32_t effect_life_ticks,
                                                  std::uint32_t& generator,
                                                  std::uint32_t designated_actor_id = 0);

}  // namespace ro::render
