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

// LE SEMIS DES TROIS PLANS, sous-types 111 et 115.
//
// DEUX bâtisseurs, et ils ne se ressemblent pas : le premier allume DEUX
// pistes et tire douze angles par piste ; le second en allume QUATRE et n'en
// tire que six.
//
// ⚠ ET LE SECOND ATTEND LE TRENTIÈME TIC. Sa garde compare le compteur de
// l'instance à trente, pas à zéro ; le semer d'emblée montrerait EF_CARTTER
// une seconde et demie trop tôt.
//
// ⚠ LES TROIS DÉCALAGES PARTENT DE ZÉRO, et ce zéro est voulu : le premier
// bâtisseur les pose à la cible de l'instance, que le constructeur du client
// met à zéro et qu'aucun de ces quatre sites ne change. Voir
// `scripts/releve_pieces_des_trois_plans.py`.

// LEQUEL DES DEUX BÂTISSEURS un site appelle.
enum class EffectTriPlaneBuilder : std::uint8_t {
  // Le sous-type 111 : deux pistes, des plans qui DÉRIVENT (ADR 0017).
  kDrifting,
  // Le sous-type 115 : quatre pistes, des plans qui S'ENVOLENT.
  kFlying,
};

// CE QUE LE PREMIER BÂTISSEUR POSE.
inline constexpr std::size_t kDriftingTriPlaneTracks = 2;
inline constexpr float kDriftingTriPlaneStep = 0.07F;
inline constexpr std::int32_t kDriftingTriPlaneRadiusSpread = 11;
inline constexpr float kDriftingTriPlaneRadiusStep = 0.1F;
inline constexpr float kDriftingTriPlaneRadiusFloor = 1.0F;
// ⚠ L'OCTET DU SITE NE CHOISIT QUE LA TEINTE, et son troisième cas la TIRE AU
// SORT entre un et trois.
inline constexpr std::uint8_t kDriftingTriPlaneFixedColorByte = 2;
inline constexpr std::uint8_t kDriftingTriPlaneDrawnColorByte = 3;
inline constexpr std::int32_t kDriftingTriPlaneColorSpread = 3;

// CE QUE LE SECOND BÂTISSEUR POSE.
inline constexpr std::int32_t kFlyingTriPlaneSowTick = 30;
inline constexpr float kFlyingTriPlaneStep = 1.0F;
inline constexpr std::int32_t kFlyingTriPlaneRadiusSpread = 6;
inline constexpr float kFlyingTriPlaneRadiusStep = 0.1F;
inline constexpr float kFlyingTriPlaneRadiusFloor = 2.5F;
inline constexpr float kFlyingTriPlaneAlpha = 230.0F;
// ⚠ SA PENTE SE TIRE SUR UN DEMI-TOUR, et son cap sur un tour entier : les
// deux ne se tirent pas dans la même plage.
inline constexpr std::int32_t kFlyingTriPlanePitchSpread = 180;
inline constexpr std::int32_t kFlyingTriPlaneColorSpread = 3;

// UN NŒUD de trois plans qu'un effet pose.
struct EffectTriPlanePattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::string_view texture;
  EffectTriPlaneBuilder builder = EffectTriPlaneBuilder::kDrifting;
  // L'OCTET DU SITE. Le second bâtisseur n'en reçoit pas ; il vaut alors zéro.
  std::uint8_t style_byte = 0;
  // QUAND cet effet pose son nœud. Voir `EffectNodeSchedule`.
  EffectNodeSchedule schedule;
  // 🔴 COMBIEN DE NŒUDS LE SITE SÈME à ce tic : EF_CARTTER appelle son
  // bâtisseur quatre fois de suite, et chaque appel tire ses plans à la suite
  // du précédent. Un seul nœud en montrait le quart. ⚠ EN QUEUE.
  std::uint16_t copies = 1;
};

[[nodiscard]] std::span<const EffectTriPlanePattern> EffectTriPlanePatternTable();

[[nodiscard]] std::span<const EffectTriPlanePattern> EffectTriPlanePatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES d'un nœud — deux vives pour le 111, quatre pour le 115.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectTriPlaneTracks(
    const EffectTriPlanePattern& pattern, std::uint32_t& generator);

// LE NŒUD qu'un patron décrit, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectTriPlaneNode(const EffectTriPlanePattern& pattern,
                                                     glm::vec3 anchor,
                                                     std::int32_t effect_life_ticks,
                                                     std::uint32_t& generator);

}  // namespace ro::render
