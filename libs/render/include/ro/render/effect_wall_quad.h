#pragma once

#include <array>
#include <cstdint>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_particle_quad.h"

namespace ro::render {

// LE MUR DRESSÉ ENTRE DEUX POINTS DU SOL, en un seul endroit.
//
// ⚠ IL EXISTE PARCE QUE DEUX FAMILLES DRESSENT LE MÊME MUR. Le cadre du
// sous-type 90 et la boîte du sous-type 64 posent chacun quatre murs sur les
// côtés d'un carré, depuis le même plancher, dans le même ordre de coins, avec
// la même image retournée. Ce qui les sépare — la rotation, la teinte, le
// mélange et les panneaux diagonaux — leur appartient ; ce qui les réunit est
// ici, et une seule fois.
//
// Ce n'est pas une figure : c'est le quad dont deux figures se servent.

// LE PLANCHER DES MURS, en unités du monde AU-DESSUS du plan du nœud.
//
// ⚠ LE CLIENT L'ÉCRIT MOINS UN et son repère descend : le mur part donc une
// unité au-dessus du nœud, pas au-dessous. Recopier le signe enterrerait les
// deux familles.
inline constexpr float kEffectWallFloorHeight = 1.0F;

// UN MUR dressé entre deux points du sol, de la hauteur donnée.
//
// Les coins dans l'ordre du client : le pied proche, le pied lointain, la tête
// lointaine, la tête proche. Ils sont RELATIFS à l'origine du nœud — ni l'un
// ni l'autre des deux dessinateurs ne pousse le drapeau d'absolu. Les pieds
// sont donnés dans le repère du client, `(abscisse, profondeur)` : la
// profondeur se nie ici.
[[nodiscard]] EffectWorldQuad ComposeEffectWallQuad(const glm::vec2& near_foot,
                                                    const glm::vec2& far_foot, float height,
                                                    std::int16_t alpha,
                                                    const std::array<std::uint8_t, 3>& color,
                                                    std::int16_t blend_mode, glm::vec3 node_origin,
                                                    const EffectOwnerTint& owner = {});

}  // namespace ro::render
