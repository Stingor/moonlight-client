#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DES RAIS DE L'ONDE DE CHOC, sous-type 54.
//
// UN bâtisseur, appelé DEUX FOIS à la création de l'effet 300, aux angles
// trente et soixante : deux nœuds de quatre rais, soit huit rais en étoile.
// Son image est écrite en dur ; son seul argument est l'angle.
//
// ⚠ UN NŒUD PAR APPEL, au premier tic seulement : la garde est l'âge nul.
// ⚠ LE NŒUD NAÎT SUR L'INSTANCE, et n'en bouge plus. Le bâtisseur relève
// ensuite l'instance de neuf unités, mais le nœud est déjà né, et l'appel
// suivant recopie la place du porteur avant de semer : aucun nœud ne le voit.

// LE MOT DE RENDU que le bâtisseur écrit en dur. Voir `effect_render_word.h`.
inline constexpr std::uint32_t kShockwaveRayRenderWord = 5;

// LE RAYON DE CHAQUE RAI, et l'écart d'angle d'une piste à la suivante.
inline constexpr float kShockwaveRayRadius = 100.0F;
inline constexpr std::int32_t kShockwaveRayDegreesPerTrack = 90;

// UN SITE : l'effet, la fonction du client, l'image et l'angle qu'il passe.
struct EffectShockwaveRayPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::string_view texture;
  std::int16_t base_degrees = 0;
};

// La table entière, triée par identifiant.
[[nodiscard]] std::span<const EffectShockwaveRayPattern> EffectShockwaveRayPatternTable();

[[nodiscard]] std::span<const EffectShockwaveRayPattern> EffectShockwaveRayPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES DU NŒUD, toutes allumées, alpha nul, un quart de tour
// d'écart à partir de l'angle du site.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedShockwaveRayTracks(
    std::int16_t base_degrees);

// LE NŒUD qu'un patron décrit, né à la place de l'instance.
[[nodiscard]] PooledEffectNode SowShockwaveRayNode(const EffectShockwaveRayPattern& pattern,
                                                   glm::vec3 instance_position,
                                                   std::int32_t effect_life_ticks);

}  // namespace ro::render
