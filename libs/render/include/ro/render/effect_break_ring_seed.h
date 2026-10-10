#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_node_seed.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DES COURONNES DE MAGNUM BREAK, sous-type 73.
//
// UN bâtisseur, un site : l'effet 339 sur `ring_yellow.tga`. Le bâtisseur est
// rappelé à CHAQUE tic, et sème un nœud aux tics multiples de trois entre le
// vingt-cinquième et le cinquante-cinquième — du vingt-septième au
// cinquante-quatrième, dix nœuds. Chacun porte deux couronnes, dont la taille,
// l'inclinaison et l'azimut dépendent de son RANG DANS LA SALVE : `tic − 25`.
//
// ⚠ LE NŒUD SUIT SON PORTEUR : le mouvement recopie la place de l'instance à
// chaque tic.

// L'HORAIRE : tous les trois tics, du vingt-septième au cinquante-quatrième.
inline constexpr EffectNodeSchedule kBreakRingSchedule = {27, 54, 3, 0};
// Le tic dont le rang dans la salve se compte.
inline constexpr std::int32_t kBreakRingFirstWindowTick = 25;

// LE MOT DE RENDU et la VARIANTE DE PALETTE que le bâtisseur écrit en dur.
inline constexpr std::uint32_t kBreakRingRenderWord = 5;
inline constexpr std::uint8_t kBreakRingTrailVariant = 5;

// CE QUE LE BÂTISSEUR ÉCRIT, selon le rang `d` dans la salve et la couronne `k` :
// balayage 360 ; clé `20 − 0,7·d` ; rayon `0,2·d + 5` ; azimut `6·d + 9·k` ;
// inclinaison `2·(35 − 4·k − d)` ; style `k`.
inline constexpr std::int16_t kBreakRingSweepDegrees = 360;
inline constexpr float kBreakRingKeyStart = 20.0F;
inline constexpr float kBreakRingKeyPerRank = 0.7F;
inline constexpr float kBreakRingRadiusStart = 5.0F;
inline constexpr float kBreakRingRadiusPerRank = 0.2F;
inline constexpr std::int32_t kBreakRingAzimuthPerRank = 6;
inline constexpr std::int32_t kBreakRingAzimuthPerRing = 9;
inline constexpr std::int32_t kBreakRingTiltBase = 35;
inline constexpr std::int32_t kBreakRingTiltPerRing = 4;
inline constexpr std::int32_t kBreakRingTiltScale = 2;
// Combien de couronnes un nœud allume : les deux premières pistes.
inline constexpr std::size_t kBreakRingRings = 2;

// UN SITE : l'effet, la fonction du client et l'image qu'il passe.
struct EffectBreakRingPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::string_view texture;
};

// La table entière, triée par identifiant.
[[nodiscard]] std::span<const EffectBreakRingPattern> EffectBreakRingPatternTable();

[[nodiscard]] std::span<const EffectBreakRingPattern> EffectBreakRingPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES D'UN NŒUD semé au tic `tick` de l'effet, les deux premières
// seules allumées.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedBreakRingTracks(std::int32_t tick);

// LE NŒUD qu'un patron décrit, né au tic `tick` à la place du porteur.
[[nodiscard]] PooledEffectNode SowBreakRingNode(const EffectBreakRingPattern& pattern,
                                                glm::vec3 owner_position, std::int32_t tick,
                                                std::int32_t effect_life_ticks);

}  // namespace ro::render
