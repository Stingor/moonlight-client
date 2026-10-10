#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DE LA JUPE QUI DÉFILE, et les effets du sous-type 144.
//
// Le bâtisseur reçoit un ALPHA DE DÉPART, et un style qui choisit à la fois le
// jeu d'images et la teinte.
//
// ⚠ QUATRE PISTES VIVES, ÉCHELONNÉES PAR LEUR ALPHA ET PAR LEUR CLÉ. Le
// bâtisseur leur donne quatre azimuts à quatre-vingt-dix degrés d'écart, quatre
// alphas décroissant de quarante-cinq en quarante-cinq depuis l'alpha du site,
// et quatre clés de trente à vingt-sept. C'est cet écart qui fait qu'un effet
// montre quatre anneaux imbriqués et non quatre fois le même.
//
// ⚠ ET LES IMAGES SE NUMÉROTENT À PARTIR DE UN. Le client formate son nom avec
// l'indice de boucle PLUS UN : vingt-sept bulles vont de `W_bubble01` à
// `W_bubble27`, jamais de zéro à vingt-six. C'est la même lecture qu'aux nœuds
// d'écran, et la manquer ne charge aucune image du premier coup.

// COMBIEN D'IMAGES CHAQUE JEU PORTE, et le format de leur nom.
inline constexpr std::size_t kCyclingSkirtBubbleCount = 27;
inline constexpr std::size_t kCyclingSkirtTopCount = 29;

// LES QUATRE CLÉS, une par piste, et les quatre écarts d'alpha.
inline constexpr std::array<float, kEffectTracksPerNode> kCyclingSkirtLiftKeys = {30.0F, 29.0F,
                                                                                  28.0F, 27.0F};
inline constexpr std::int16_t kCyclingSkirtAlphaStep = 45;

// UNE JUPE QU'UN EFFET POSE : ce que son site d'appel passe au bâtisseur.
struct EffectCyclingSkirtPattern {
  std::uint32_t effect_id = 0;
  // L'étiquette du bâtisseur du client qui pose cette jupe.
  std::string_view client_function;
  // L'ALPHA DE LA DERNIÈRE PISTE. Les trois autres montent de quarante-cinq en
  // quarante-cinq au-dessus de lui.
  std::int16_t base_alpha = 0;
  std::uint8_t builder_argument = 0;
};

// Toutes les jupes connues, par identifiant d'effet.
[[nodiscard]] std::span<const EffectCyclingSkirtPattern> EffectCyclingSkirtPatternTable();

// TOUTES les jupes d'un effet, ou un span VIDE.
[[nodiscard]] std::span<const EffectCyclingSkirtPattern> EffectCyclingSkirtPatternsFor(
    std::uint32_t effect_id);

// LES IMAGES QU'UN STYLE CHARGE, dans l'ordre où le nœud les range. Vide pour un
// style au-delà de trois : le client n'en charge alors AUCUNE, et son nœud n'a
// rien à montrer.
[[nodiscard]] std::vector<std::string> EffectCyclingSkirtTexturePaths(
    std::uint8_t builder_argument);

// LA TEINTE DU NŒUD QU'UN STYLE POSE. Quatre pour deux styles, zéro pour les
// autres : ce n'est pas une échelle, c'est un test d'égalité par jeu d'images.
[[nodiscard]] std::uint8_t EffectCyclingSkirtVariantFor(std::uint8_t builder_argument);

// LES QUATRE PISTES D'UNE JUPE, toutes vives.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectCyclingSkirtTracks(
    const EffectCyclingSkirtPattern& pattern);

// LE NŒUD qu'un patron décrit, pistes semées, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectCyclingSkirtNode(const EffectCyclingSkirtPattern& pattern,
                                                         glm::vec3 anchor,
                                                         std::int32_t effect_life_ticks);

}  // namespace ro::render
