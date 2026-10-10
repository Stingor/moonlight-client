#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DE LA COLONNE D'AURA, et les effets du sous-type 131.
//
// Une image, et tout le reste en dur. Une seule piste vive, et ses dix alphas
// de couche à ZÉRO — rien ne se voit tant que le mouvement n'a pas tourné, et
// c'est lui qui ouvre la colonne couche par couche.
//
// ⚠ SA MARQUE DE FORME VAUT UN, non deux. Le dessinateur et le mouvement la
// lisent tous les deux : à deux, la colonne se serre et monte de dix unités de
// plus, et le mouvement cesse de suivre son porteur. Aucun de nos bâtisseurs ne
// pose deux ; le dire est ce qui permettra de s'en apercevoir le jour où l'un le
// ferait.

// LE RAYON DE LA COUCHE LA PLUS LARGE, écrit en dur.
inline constexpr float kAuraColumnRadius = 24.0F;
// LA MARQUE DE FORME QUE CE BÂTISSEUR POSE.
inline constexpr std::uint8_t kAuraColumnShapeMark = 1;

// UNE COLONNE QU'UN EFFET POSE : ce que son site d'appel passe au bâtisseur.
struct EffectAuraColumnPattern {
  std::uint32_t effect_id = 0;
  // L'étiquette de la routine du client qui pose cette colonne.
  std::string_view client_function;
  std::string_view texture;
};

// Toutes les colonnes connues, par identifiant d'effet.
[[nodiscard]] std::span<const EffectAuraColumnPattern> EffectAuraColumnPatternTable();

// TOUTES les colonnes d'un effet, ou un span VIDE.
[[nodiscard]] std::span<const EffectAuraColumnPattern> EffectAuraColumnPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES D'UNE COLONNE : une seule vive, et ses dix couches éteintes.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectAuraColumnTracks();

// LE NŒUD qu'un patron décrit, piste semée, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectAuraColumnNode(const EffectAuraColumnPattern& pattern,
                                                       glm::vec3 anchor,
                                                       std::int32_t effect_life_ticks);

}  // namespace ro::render
