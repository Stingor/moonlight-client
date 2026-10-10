#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DES FEUILLES QUI S'ÉTIRENT, sous-type 141.
//
// UN bâtisseur, un site : l'effet 811, à l'image `freezing_stick.bmp` et à
// l'octet zéro, que personne ne relit.
//
// ⚠ UN NŒUD AU PREMIER TIC SEULEMENT : la garde est l'âge nul de l'instance.
// ⚠ LE NŒUD SUIT SON PORTEUR : le mouvement recopie la place de l'instance à
// chaque tic, et le site la recale sur le porteur à chaque appel.
//
// Quatre pistes, toutes allumées et OPAQUES dès le semis, retardées de dix tics
// l'une après l'autre, et empilées : la première trente unités au-dessus du
// porteur, la dernière à sa hauteur.

// LE MOT DE RENDU que le bâtisseur écrit en dur. Voir `effect_render_word.h`.
inline constexpr std::uint32_t kTaperedSheetRenderWord = 5;

// CE QUE LE BÂTISSEUR ÉCRIT EN DUR dans chaque piste.
inline constexpr float kTaperedSheetRadius = 20.0F;
inline constexpr std::int16_t kTaperedSheetHeadingDegrees = 215;
inline constexpr std::int32_t kTaperedSheetDelayPerTrack = 10;
inline constexpr std::uint8_t kTaperedSheetUnreadTint = 2;
// LES QUATRE ÉTAGES, au-dessus du porteur, piste par piste.
inline constexpr std::array<float, kEffectTracksPerNode> kTaperedSheetLifts = {30.0F, 20.0F, 10.0F,
                                                                               0.0F};

// UN SITE : l'effet, la fonction du client, l'image et l'octet qu'il passe.
struct EffectTaperedSheetPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::string_view texture;
  std::uint8_t octet = 0;
};

// La table entière, triée par identifiant.
[[nodiscard]] std::span<const EffectTaperedSheetPattern> EffectTaperedSheetPatternTable();

[[nodiscard]] std::span<const EffectTaperedSheetPattern> EffectTaperedSheetPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES DU NŒUD, nées sur `owner_position`.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedTaperedSheetTracks(
    glm::vec3 owner_position, std::uint8_t octet);

// LE NŒUD qu'un patron décrit, né à la place du porteur.
[[nodiscard]] PooledEffectNode SowTaperedSheetNode(const EffectTaperedSheetPattern& pattern,
                                                   glm::vec3 owner_position,
                                                   std::int32_t effect_life_ticks);

}  // namespace ro::render
