#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DE LA DISPERSION D'ÉCLAIRS, sous-type 135.
//
// UN bâtisseur et UN identifiant, l'effet 794, qui l'appelle DEUX FOIS : octet
// zéro puis octet un. L'octet est le SENS DE ROTATION de l'arc — deux arcs qui
// partent du même cap et tournent l'un contre l'autre.
//
// ⚠ L'EFFET L'APPELLE À CHAQUE TIC, et c'est le bâtisseur qui se garde : la
// garde d'âge de l'effet ne saute que le son « INFRAREDSCAN.wav » et retombe
// sur les deux appels ; le bâtisseur ne bâtit que si l'âge de l'instance est
// nul. Deux nœuds au tic zéro, donc — et à chaque tic, la place de l'instance
// recopiée du porteur, que notre troupe fait en translatant l'ancre
// (`MoveAnchored`).
//
// ⚠ LE CAP DE DÉPART VIENT DU PORTEUR, pas du bâtisseur : il reçoit l'angle du
// corps de l'acteur moins un quart de tour. Un cap figé en constante figerait
// tous les balayages du même côté.

// COMBIEN L'ARC PORTE DE SEGMENTS.
inline constexpr float kElectricArcSegments = 20.0F;

// LE RAYON DU TUBE et la PORTÉE de l'arc, que la phase de chaque segment
// parcourt.
inline constexpr float kElectricArcTubeRadius = 0.1F;
inline constexpr float kElectricArcReach = 20.0F;

// L'ALPHA DE NAISSANCE : deux cent cinquante pour la piste de tête, cent
// quarante moins trente par rang pour les autres — cent dix, quatre-vingts,
// cinquante.
//
// ⚠ IL NAÎT PLEIN ET NE MONTE JAMAIS : le mouvement ne fait que le faire
// tomber, passé le soixantième tic.
inline constexpr std::int16_t kElectricArcLeadAlpha = 250;
inline constexpr std::int16_t kElectricArcTrailingAlphaBase = 140;
inline constexpr std::int16_t kElectricArcTrailingAlphaStep = 30;

// CE QUE LE BÂTISSEUR TIRE, une fois par piste : le facteur de bombement, sur
// onze crans d'un centième.
inline constexpr std::int32_t kElectricArcBowSpread = 11;
inline constexpr float kElectricArcBowStep = 0.01F;

// DE COMBIEN L'ANCRE SE TIENT AU-DESSUS DU PORTEUR.
//
// ⚠ LE REPÈRE DU CLIENT DESCEND : il RETRANCHE dix, ce qui élève. Nous
// ajoutons.
inline constexpr float kElectricArcAnchorLift = 10.0F;

// LE QUART DE TOUR que le bâtisseur retranche à l'angle du corps.
inline constexpr float kElectricArcFacingOffsetDegrees = 90.0F;

// UN ARC qu'un effet lance.
struct EffectElectricArcPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::string_view texture;
  // L'OCTET DU SITE, rangé dans la piste : un fait tourner le cap
  // dans le sens croissant, tout le reste dans le sens décroissant.
  std::int16_t spin_style = 0;
};

[[nodiscard]] std::span<const EffectElectricArcPattern> EffectElectricArcPatternTable();

[[nodiscard]] std::span<const EffectElectricArcPattern> EffectElectricArcPatternsFor(
    std::uint32_t effect_id);

// LE CAP DE DÉPART DES QUATRE PISTES, en degrés du client, depuis l'angle de
// base d'un effet (`EffectOwnerFacingDegrees`).
//
// ⚠ LE CLIENT LIT L'ANGLE DU CORPS, et l'angle de base en est le complément :
// `180 − corps`. On remonte donc au corps, ramené dans le tour comme le client
// le range, avant de retrancher le quart de tour — et un cap négatif reçoit un
// tour.
[[nodiscard]] std::int16_t ElectricArcStartTiltDegrees(float owner_facing_degrees);

// LES QUATRE PISTES d'un nœud, toutes vives.
//
// `anchor` est la place du porteur, dans NOTRE repère.
//
// ⚠ LEUR HISTORIQUE NAÎT VIDE : c'est le premier tic de mouvement qui écrit
// l'arc entier d'un coup.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectElectricArcTracks(
    const EffectElectricArcPattern& pattern, glm::vec3 anchor, float owner_facing_degrees,
    std::uint32_t& generator);

// LE NŒUD qu'un patron décrit, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectElectricArcNode(const EffectElectricArcPattern& pattern,
                                                        glm::vec3 anchor,
                                                        float owner_facing_degrees,
                                                        std::int32_t effect_life_ticks,
                                                        std::uint32_t& generator);

}  // namespace ro::render
