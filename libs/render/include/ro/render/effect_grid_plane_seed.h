#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DE LA NAPPE AU SOL, et les effets du sous-type 72.
//
// Le bâtisseur ne prend qu'UN octet, et il
// décide de tout ce qui distingue deux nappes — l'image ET la marque de teinte,
// qui est aussi le rythme de son alpha.
//
// ⚠ SA PISTE NAÎT ÉTEINTE, alpha nul : rien ne se voit tant que le mouvement n'a
// pas tourné. C'est lui qui la fait paraître, de cinq en cinq ou de dix en dix
// selon la marque.
//
// ⚠ ET LE BÂTISSEUR DESCEND SON EFFET DE NEUF UNITÉS avant de bâtir. Ce n'est
// pas la hauteur de la nappe — celle-là est écrite en dur dans la figure —, c'est
// l'ancre de l'effet ENTIER qui bouge, et tout ce qu'il posera ensuite avec.

// DE COMBIEN LE BÂTISSEUR DESCEND L'ANCRE DE SON EFFET, dans le repère du
// client. Positif : ce repère descend.
inline constexpr float kGridPlaneAnchorDrop = 9.0F;

// UNE NAPPE QU'UN EFFET POSE : ce que son site d'appel passe au bâtisseur.
struct EffectGridPlanePattern {
  std::uint32_t effect_id = 0;
  // LA FONCTION DU CLIENT qui pose cette nappe.
  std::string_view client_function;
  std::string_view texture;
  std::uint8_t builder_argument = 0;
};

// Toutes les nappes connues, par identifiant d'effet.
[[nodiscard]] std::span<const EffectGridPlanePattern> EffectGridPlanePatternTable();

// TOUTES les nappes d'un effet, ou un span VIDE.
[[nodiscard]] std::span<const EffectGridPlanePattern> EffectGridPlanePatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES D'UNE NAPPE : une seule vive, et rien de tiré au sort.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectGridPlaneTracks(
    const EffectGridPlanePattern& pattern);

// LE NŒUD qu'un patron décrit, piste semée, prêt à rejoindre la troupe.
//
// `anchor` est l'ancre de l'effet AVANT la descente de neuf unités : c'est ici
// qu'elle s'applique, comme le bâtisseur le fait.
[[nodiscard]] PooledEffectNode SowEffectGridPlaneNode(const EffectGridPlanePattern& pattern,
                                                      glm::vec3 anchor,
                                                      std::int32_t effect_life_ticks);

}  // namespace ro::render
