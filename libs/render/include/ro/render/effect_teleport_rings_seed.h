#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_teleport_rings.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DES ANNEAUX DE TÉLÉPORTATION, sous-type 57.
//
// UN seul bâtisseur, quatre pistes, et un paramètre que le site d'appel pousse
// pour les échelonner toutes.
//
// ⚠ LES QUATRE PISTES NE SONT PAS QUATRE COPIES. Chacune a son alpha, son
// azimut, son rayon et son inclinaison, et c'est ce qui fait quatre anneaux
// concentriques au lieu d'un seul plus épais. Le mouvement ajoute la vitesse,
// qui dépend du rang.
//
// ⚠ ET LEUR INCLINAISON LES DRESSE. À quatre-vingt-dix degrés, l'écart radial
// d'un segment est nul et toute sa longueur part vers le HAUT : la figure de
// l'anneau donne alors une colonne, non une jupe. Les trois autres pistes
// s'inclinent d'un degré de plus chacune, ce qui les évase à peine.

// CE QU'UNE PISTE REÇOIT, et ce que le paramètre du site y change.
//
// ⚠ LE PARAMÈTRE JOUE SUR DEUX CHAMPS ET DANS DEUX SENS : il grandit la clé de
// soixante par unité, et RÉTRÉCIT le rayon de deux. Un effet plus grand est donc
// aussi plus serré.
struct EffectTeleportRingSeed {
  std::int16_t alpha = 0;
  std::int16_t azimuth_degrees = 0;
  std::int16_t tilt_degrees = 0;
  // La clé de décalage, que le mouvement emploie comme amplitude de sa
  // respiration — et que la figure lit tout autrement (ADR 0017).
  float lift_key = 0.0F;
  float radius_at_anchor = 0.0F;
};

// LES QUATRE PISTES, telles que le bâtisseur les pose.
[[nodiscard]] std::span<const EffectTeleportRingSeed> EffectTeleportRingSeeds();

// CE QUE LE PARAMÈTRE AJOUTE À LA CLÉ, et retranche au rayon, par unité.
inline constexpr float kTeleportRingKeyPerScale = 60.0F;
inline constexpr float kTeleportRingRadiusPerScale = 2.0F;

// UN EFFET DE CETTE FAMILLE.
struct EffectTeleportRingPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::string_view texture;
  // LE PARAMÈTRE QUE LE SITE D'APPEL POUSSE. Il n'est pas une échelle de rendu :
  // il entre dans les deux champs ci-dessus, et nulle part ailleurs.
  std::int16_t scale = 0;
};

[[nodiscard]] std::span<const EffectTeleportRingPattern> EffectTeleportRingPatternTable();

[[nodiscard]] std::span<const EffectTeleportRingPattern> EffectTeleportRingPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES d'un nœud, prêtes à respirer.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectTeleportRingTracks(
    const EffectTeleportRingPattern& pattern);

// LE NŒUD qu'un patron décrit, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectTeleportRingNode(const EffectTeleportRingPattern& pattern,
                                                         glm::vec3 anchor,
                                                         std::int32_t effect_life_ticks);

}  // namespace ro::render
