#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// L'ÉCLAT QUI FUIT : la figure du sous-type 44.
//
// ⚠ CE N'EST PAS UN BILLBOARD (ADR 0017) : rien de son orientation ne vient de
// la caméra. C'est un BARREAU posé dans un plan VERTICAL, incliné d'un angle
// dans ce plan, et dont la distance au centre croît à chaque tic.
//
// Les quatre identifiants qui l'emploient sont les éclats de coup — EF_TEIHIT1,
// 1X, 1T et 3.

// LES TROIS ANGLES ET LES DEUX RAYONS d'un éclat.
//
// ⚠ DEUX ANGLES QUI NE SONT PAS DE MÊME NATURE. L'INCLINAISON donne l'azimut
// du PLAN vertical où l'éclat vit ; les DEGRÉS donnent son orientation DANS ce
// plan. Les confondre poserait tous les éclats à plat.
//
// ⚠ ET SA LONGUEUR VIENT D'UNE MARQUE. Le barreau va du rayon moins « marque
// fois demi-largeur » au rayon plus autant : la SECONDE marque est donc un
// ÉLANCEMENT, et rien d'autre ne le donne. La lire comme un numéro rendrait
// tous les éclats carrés.
inline constexpr std::size_t kFlyingShardSlotOfHalfWidth = 1;
inline constexpr std::size_t kFlyingShardSlotOfGrowth = 0;
inline constexpr std::size_t kFlyingShardMarkOfColor = 0;
inline constexpr std::size_t kFlyingShardMarkOfLength = 1;

// LES QUATRE TEINTES, choisies par la PREMIÈRE marque.
inline constexpr std::array<std::uint8_t, 3> kFlyingShardGold = {225, 225, 80};
inline constexpr std::array<std::uint8_t, 3> kFlyingShardWhite = {255, 255, 255};
inline constexpr std::array<std::uint8_t, 3> kFlyingShardSky = {65, 184, 240};
inline constexpr std::array<std::uint8_t, 3> kFlyingShardPale = {200, 200, 255};

// ⚠ UNE MARQUE SUR QUATRE PASSE DEVANT TOUT, la marque DEUX : voir
// `EffectQuadDepth`.
inline constexpr std::uint8_t kFlyingShardMarkInFront = 2;

// LA TEINTE ET LE MÉLANGE d'un éclat.
[[nodiscard]] std::array<std::uint8_t, 3> EffectFlyingShardColor(std::uint8_t mark);
[[nodiscard]] std::int16_t EffectFlyingShardBlendMode(std::uint8_t mark);

// LE BARREAU D'UNE PISTE, ou rien.
//
// Rend `nullopt` quand la piste est éteinte ou son alpha nul.
[[nodiscard]] std::optional<EffectWorldQuad> ComposeFlyingShardQuad(const EffectTrack& track,
                                                                    glm::vec3 anchor);

}  // namespace ro::render
