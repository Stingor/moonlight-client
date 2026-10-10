#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_render_word.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// QUI POSE LA PETITE BOÎTE QUI TOURNE, et avec quelle image. Voir
// `effect_spinning_box.h` pour sa figure et son mouvement.
//
// Le bâtisseur ne lit de son cas que l'IMAGE ; tout le reste est écrit en dur,
// ici. Il ne sème qu'au premier tic de l'effet, un seul nœud, et ce nœud vit
// aussi longtemps que l'effet.
struct EffectSpinningBoxPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::string_view texture;
};

[[nodiscard]] std::span<const EffectSpinningBoxPattern> EffectSpinningBoxPatternTable();

[[nodiscard]] std::span<const EffectSpinningBoxPattern> EffectSpinningBoxPatternsFor(
    std::uint32_t effect_id);

// CE QUE LE BÂTISSEUR ÉCRIT dans son unique piste vive.
//
// Le rayon à l'ancre, qui est aussi la hauteur de la boîte : un virgule six.
inline constexpr float kSpinningBoxAnchorRadius = 1.6F;
// La clé de hauteur, dans le repère du client : douze unités au-dessus du
// porteur, comptées vers le bas.
inline constexpr float kSpinningBoxClientLift = -12.0F;
// La marque tire le rouge et le vert sur trente et une valeurs.
inline constexpr std::int32_t kSpinningBoxRedGreenDraws = 31;

// LE MOT DE RENDU : un mélange PROPRE à chaque face, que la figure veut
// additif.
inline constexpr std::uint32_t kSpinningBoxRenderWord =
    kEffectRenderWordOwnBlend | kEffectRenderWordOfConstructor;

// LES QUATRE PISTES, tirées dans l'ordre du client : l'inclinaison, l'azimut,
// puis la marque.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectSpinningBoxTracks(
    std::uint32_t& generator);

// Le nœud entier, posé sur son porteur.
[[nodiscard]] PooledEffectNode SowEffectSpinningBoxNode(const EffectSpinningBoxPattern& pattern,
                                                        glm::vec3 ground_position,
                                                        std::uint32_t& generator);

}  // namespace ro::render
