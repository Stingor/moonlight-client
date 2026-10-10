#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_camera_fan.h"
#include "ro/render/effect_node_fwd.h"
#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_node_seed.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DE L'ÉVENTAIL TOURNÉ VERS LA CAMÉRA, sous-type 68.
//
// DEUX bâtisseurs, et chacun pose DEUX nœuds à vingt tics d'écart : celui
// d'Asura Strike, trois traînées par nœud ; celui du Soul Link (503), quatre
// traînées sous la marque de style deux.
//
// ⚠ LES DEUX NŒUDS DE L'ASURA NE SE RESSEMBLENT QUE DE LOIN. Le premier étale
// ses traînées d'un côté, le second de l'autre : leurs décalages latéraux sont
// opposés, et leurs retards vont dans deux sens — le premier rattrape son
// retard, le second s'échelonne de plus en plus tard. Les six traînées ensemble
// font l'éventail. Ceux du Soul Link, eux, rattrapent TOUS DEUX leur retard.
//
// ⚠ ET CHAQUE TRAÎNÉE A SA PROPRE IMAGE. Le nœud en charge une par traînée, et
// la marque zéro de chaque piste dit laquelle. Les peindre toutes de la
// première perdrait la moitié de la figure.
//
// ⚠ LE PARAMÈTRE DU SITE CHOISIT UN JEU D'IMAGES ET UN GABARIT, et il n'est pas
// une échelle : `EF_BEGINASURA` (328) passe zéro, `EF_BEGINASURA11` (510) passe
// un — le second étale plus large, sur un rayon plus grand, et sous six autres
// images.

// CE QU'UN NŒUD DE CETTE FAMILLE POSE.
struct EffectCameraFanSeed {
  // Le retard de la PREMIÈRE traînée, en tics, et il est négatif.
  std::int32_t first_delay = 0;
  // De combien le retard change d'une traînée à la suivante — POSITIF pour le
  // nœud qui rattrape, négatif pour celui qui s'échelonne.
  std::int32_t delay_step = 0;
  // Le décalage latéral de la première traînée, et son pas. Les deux nœuds de
  // l'Asura les prennent opposés.
  float first_sideways = 0.0F;
  float sideways_step = 0.0F;
  float radius_at_anchor = 0.0F;
  std::uint8_t draw_mark = 0;
};

// COMBIEN DE TRAÎNÉES un nœud peut allumer : une par piste. L'Asura en allume
// trois et laisse la quatrième éteinte ; le Soul Link les allume toutes.
inline constexpr std::size_t kCameraFanMostTrailsPerNode = kEffectTracksPerNode;

// UN NŒUD qu'un effet pose, avec ses images et le tic où il naît.
struct EffectCameraFanPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  // UNE IMAGE PAR TRAÎNÉE, dans l'ordre où la marque de chaque traînée les
  // choisit. Les cases au-delà de `trail_count` restent vides.
  std::array<std::string_view, kCameraFanMostTrailsPerNode> textures;
  EffectCameraFanSeed seed;
  // QUAND ce nœud naît : le client teste le tic de l'instance et ne bâtit qu'à
  // celui-là. Voir `EffectNodeSchedule`.
  EffectNodeSchedule schedule;
  // ⚠ COMBIEN DE TRAÎNÉES S'ALLUMENT, les premières pistes dans l'ordre : les
  // autres restent éteintes, et le client les éteint explicitement avant de
  // semer.
  std::size_t trail_count = 0;
};

[[nodiscard]] std::span<const EffectCameraFanPattern> EffectCameraFanPatternTable();

[[nodiscard]] std::span<const EffectCameraFanPattern> EffectCameraFanPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES d'un nœud : les `trail_count` premières vives, les autres
// éteintes.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectCameraFanTracks(
    const EffectCameraFanPattern& pattern);

// LE NŒUD qu'un patron décrit, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectCameraFanNode(const EffectCameraFanPattern& pattern,
                                                      glm::vec3 anchor,
                                                      std::int32_t effect_life_ticks);

}  // namespace ro::render
