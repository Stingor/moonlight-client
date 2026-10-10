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

// LE SEMIS DE L'ÉMETTEUR EN SPIRALE, sous-type 151.
//
// UN SEUL bâtisseur, et deux cas qui l'appellent : 849 une fois, 931 DEUX fois
// au même tic, sous deux octets. Il lit de son cas l'image — qu'il charge et
// que rien ne peint — et l'octet ; l'acteur désigné vient de l'instance, et
// tout le reste est écrit en dur, ou tiré.
//
// ⚠ UN NŒUD AU PREMIER TIC SEULEMENT, par appel : la garde est l'âge nul de
// l'instance.

// LE MOT DE RENDU que le bâtisseur écrit en dur, et que personne ne relit.
inline constexpr std::uint32_t kSpiralEmitterRenderWord = 5;

// LA DEMI-DIAGONALE DU CARRÉ qui ne sert qu'à l'épreuve du premier coin.
inline constexpr float kSpiralEmitterSquareRadius = 2.5F;

// LE RAYON DE DÉPART DE LA SPIRALE.
inline constexpr float kSpiralEmitterFirstRadius = 5.0F;

// QUATRE ANGLES TIRÉS sur le tour, que ni le mouvement ni l'émission ne
// relisent : ils ne comptent que pour l'ordre des tirages.
inline constexpr std::size_t kSpiralEmitterUnreadAngles = 4;
inline constexpr std::int32_t kSpiralEmitterAngleDraws = 360;

// L'ÉCART DE NAISSANCE, que le premier tic réécrit avant toute émission.
inline constexpr float kSpiralEmitterBirthOffsetX = -30.0F;
inline constexpr float kSpiralEmitterBirthOffsetDown = -40.0F;

// UN SITE : l'effet, celui que son nœud fait naître, la fonction du client,
// l'image et l'octet.
//
// ⚠ L'EFFET QUI NAÎT SE DÉDUIT DE L'OCTET — voir `SpiralEmitterChildOf`. Il
// est en table pour que la mesure sache que l'émetteur se montre par lui, et un
// essai tient les deux d'accord.
struct EffectSpiralEmitterPattern {
  std::uint32_t effect_id = 0;
  std::uint32_t spawned_effect_id = 0;
  std::string_view client_function;
  std::string_view texture;
  std::uint8_t byte = 0;
};

// La table entière, triée par identifiant, les sites d'un même effet dans
// l'ordre de leurs appels.
[[nodiscard]] std::span<const EffectSpiralEmitterPattern> EffectSpiralEmitterPatternTable();

[[nodiscard]] std::span<const EffectSpiralEmitterPattern> EffectSpiralEmitterPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES DU NŒUD, la première seule allumée. Le générateur avance de
// quatre tirages, dans l'ordre du client.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedSpiralEmitterTracks(
    std::uint8_t byte, std::uint32_t designated_actor_id, std::uint32_t& generator);

// LE NŒUD qu'un patron décrit, né à la place du porteur. `effect_life_ticks`
// est la vie de l'effet, que le bâtisseur recopie dans le nœud.
[[nodiscard]] PooledEffectNode SowSpiralEmitterNode(const EffectSpiralEmitterPattern& pattern,
                                                    glm::vec3 owner_position,
                                                    std::int32_t effect_life_ticks,
                                                    std::uint32_t designated_actor_id,
                                                    std::uint32_t& generator);

}  // namespace ro::render
