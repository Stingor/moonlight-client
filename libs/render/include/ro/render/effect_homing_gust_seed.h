#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DES BOURRASQUES QUI POURSUIVENT UN ACTEUR, sous-types 122 et 125.
//
// Deux bâtisseurs, un par sous-type :
//
//   - sous-type 122 : l'effet 553, Esma. Son site l'appelle DEUX FOIS, aux
//     octets zéro et un — deux bourrasques qui partent de part et d'autre.
//     L'effet 1206 fait de même aux octets quatre et cinq : les mêmes, en gris.
//   - sous-type 125 : l'effet 621, Wind Blade. Son site l'appelle UNE FOIS
//     PAR COUP — le nombre de coups que l'instance porte — à l'octet
//     `rang % 2`.
//
// ⚠ LES DEUX NE SÈMENT QU'AU TIC ZÉRO (âge == 0), même si leur site les
// rappelle à chaque tic.
//
// ⚠ LES QUATRE PISTES SONT ALLUMÉES, À ALPHA NUL, et trois ne sont que des
// échos : c'est le mouvement qui allume la meneuse et recopie sa trace.

// QUEL MOUVEMENT un patron attend.
enum class EffectHomingGustKind : std::uint8_t {
  // Le 122 : un quart de tour de côté, une poursuite, un retour si l'acteur
  // disparaît.
  kSteered,
  // Le 125 : un écart tiré au sort, et l'extinction dès que l'acteur est visé.
  kRandomSpread,
};

// L'OCTET QUE LE SITE PASSE au 125 : le rang du coup modulo deux, si bien que
// les bourrasques d'un même coup partent en alternance d'un côté et de l'autre.
inline constexpr std::uint32_t kHomingGustSpreadSides = 2;

// CE QUE LE BÂTISSEUR DU 122 POSE EN DUR : la vitesse, et la hauteur au-dessus
// du nœud.
//
// ⚠ LE REPÈRE DU CLIENT DESCEND, LE NÔTRE MONTE : il retranche dix, nous les
// ajoutons.
inline constexpr float kHomingGustSteeredSpeed = 2.0F;
inline constexpr float kHomingGustLiftAboveNode = 10.0F;

// CE QUE LE BÂTISSEUR DU 125 TIRE, piste par piste : un rayon de dix à quinze
// par demi-unités (`rand() % 11 × 0.5 + 10`), puis une vitesse de un à trois
// par dixièmes (`rand() % 21 × 0.1 + 1`).
inline constexpr float kHomingGustSpreadRadiusBase = 10.0F;
inline constexpr float kHomingGustSpreadRadiusStep = 0.5F;
inline constexpr std::int32_t kHomingGustSpreadRadiusDraws = 11;
inline constexpr float kHomingGustSpreadSpeedBase = 1.0F;
inline constexpr float kHomingGustSpreadSpeedStep = 0.1F;
inline constexpr std::int32_t kHomingGustSpreadSpeedDraws = 21;
// SA TEINTE, la deuxième du dessinateur : le vert, ajouté.
inline constexpr std::uint8_t kHomingGustSpreadColor = 1;

// UN PATRON : une ligne par appel du site.
struct EffectHomingGustPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::string_view texture;
  EffectHomingGustKind kind = EffectHomingGustKind::kSteered;
  // L'OCTET que le site passe au bâtisseur. Il est ignoré quand
  // `one_node_per_hit` est vrai : le site le tire alors du rang du coup.
  std::uint8_t octet = 0;
  // VRAI quand le site sème un nœud PAR COUP, et non un seul.
  bool one_node_per_hit = false;
};

[[nodiscard]] std::span<const EffectHomingGustPattern> EffectHomingGustPatternTable();

[[nodiscard]] std::span<const EffectHomingGustPattern> EffectHomingGustPatternsFor(
    std::uint32_t effect_id);

// CE QUE L'OCTET DU 122 DÉCIDE : le rayon, la teinte, le côté, et
// l'extinction hâtive.
//
// ⚠ SIX OCTETS Y SONT PRÉVUS, ET LES SITES N'EN PASSENT QUE QUATRE — zéro et
// un pour 553, quatre et cinq pour 1206. La table est transcrite entière parce
// qu'elle est le contrat du bâtisseur ; les octets deux et trois ne se voient
// jamais en jeu.
struct HomingGustSteeredStyle {
  float radius = 0.0F;
  std::uint8_t color = 0;
  // `nullopt` quand le bâtisseur n'écrit RIEN à cette marque, et la piste
  // garde alors son zéro de naissance.
  std::optional<std::uint8_t> side;
  std::uint8_t early_fade = 0;
};
[[nodiscard]] HomingGustSteeredStyle HomingGustSteeredStyleOf(std::uint8_t octet);

// LES QUATRE PISTES du 122 : toutes vives, toutes transparentes, au-dessus du
// nœud.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectHomingGustSteeredTracks(
    glm::vec3 anchor, std::uint8_t octet);

// LES QUATRE PISTES du 125 : deux tirages par piste, le rayon puis la vitesse.
//
// ⚠ SEULE LA VITESSE DE LA MENEUSE SERT : les échos ne reçoivent que son ancre,
// son inclinaison et son alpha. Leurs RAYONS, eux, se voient — chaque écho garde
// le sien.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectHomingGustSpreadTracks(
    glm::vec3 anchor, std::uint8_t octet, std::uint32_t& generator);

// LE NŒUD qu'un patron décrit, à l'octet donné.
//
// `target` est la place de l'acteur visé, que le mouvement relira à chaque tic
// dans `PooledEffectNode::target_position`. `render` ne connaît pas l'acteur.
[[nodiscard]] PooledEffectNode SowEffectHomingGustNode(const EffectHomingGustPattern& pattern,
                                                       glm::vec3 anchor,
                                                       const std::optional<glm::vec3>& target,
                                                       std::uint8_t octet,
                                                       std::int32_t effect_life_ticks,
                                                       std::uint32_t& generator);

}  // namespace ro::render
