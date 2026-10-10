#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DU RUBAN QUI TOURNE, sous-types 77 et 91.
//
// DEUX bâtisseurs, l'un pour le 77 et l'autre pour le 91, et TROIS sites. Les
// deux sèment QUATRE pistes, écartées de quarante-cinq degrés d'inclinaison —
// c'est ce qui fait un bouquet de rubans et non un seul.
//
// ⚠ LES DEUX SE RESSEMBLENT DE PRÈS et ne se confondent pas : celui du 91 tire
// un RETARD par piste, part invisible, et pose un rayon trois fois plus petit.

// DE COMBIEN L'INCLINAISON D'UNE PISTE S'ÉCARTE DE LA PRÉCÉDENTE.
inline constexpr std::int16_t kSpinRibbonTiltSpacing = 45;

// CE QUE LES DEUX BÂTISSEURS TIRENT, et entre quelles bornes.
//
// La demi-largeur se tire sur onze crans d'un cinquième, au-dessus d'un
// plancher qui dépend du sous-type ; l'azimut sur le tour entier ; le balayage
// sur quatre-vingt-onze degrés au-dessus de cent quatre-vingts ; la vitesse de
// rotation sur quatre valeurs au-dessus de trois.
inline constexpr std::int32_t kSpinRibbonHalfWidthSpread = 11;
inline constexpr float kSpinRibbonHalfWidthStep = 0.2F;
inline constexpr std::int32_t kSpinRibbonSweepSpread = 91;
inline constexpr std::int16_t kSpinRibbonSweepFloor = 180;
inline constexpr std::int32_t kSpinRibbonSpinSpread = 4;
inline constexpr float kSpinRibbonSpinFloor = 3.0F;

// CE QUI SÉPARE LES DEUX SOUS-TYPES.
inline constexpr float kSpinRibbonHalfWidthFloor = 4.0F;
inline constexpr float kRisingSpinRibbonHalfWidthFloor = 2.0F;
inline constexpr float kSpinRibbonRadius = 14.0F;
inline constexpr float kRisingSpinRibbonRadius = 5.0F;
// ⚠ LE REPÈRE DU CLIENT DESCEND : ce qu'il pose en négatif monte ici.
inline constexpr float kSpinRibbonHeight = 14.0F;
inline constexpr float kRisingSpinRibbonHeight = 10.0F;
inline constexpr std::int16_t kSpinRibbonAlpha = 100;
// ⚠ ET CELUI DU 91 NAÎT INVISIBLE : c'est son mouvement qui le lève.
inline constexpr std::int16_t kRisingSpinRibbonAlpha = 0;
// SUR COMBIEN DE TICS LES QUATRE PISTES DU 91 S'ÉCHELONNENT.
inline constexpr std::int32_t kRisingSpinRibbonDelaySpread = 20;

// UN BOUQUET DE RUBANS qu'un effet pose.
struct EffectSpinRibbonPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::string_view texture;
  // L'INCLINAISON DE LA PREMIÈRE PISTE, que le site donne. Les trois sites
  // posent zéro ; le champ existe parce que le bâtisseur le prend.
  std::int16_t base_tilt_degrees = 0;
  // ⚠ VRAI POUR LE SOUS-TYPE 91 : il tire un retard, naît invisible, et son
  // mouvement le fait vivre. Le prendre pour l'autre donnerait un ruban qui ne
  // paraît jamais.
  bool rising = false;
  // LE NUMÉRO DE VARIANTE où la primitive de ruban prend sa teinte et son
  // mélange : quatre un bleu ajouté, dix un rose ajouté. ⚠ EN QUEUE.
  std::uint8_t trail_variant = 0;
  // 🔴 COMBIEN DE NŒUDS LE SITE SÈME, et de combien l'inclinaison de départ
  // avance d'un nœud au suivant. Chaque site appelle son bâtisseur en boucle —
  // cinq fois, de neuf en neuf degrés —, et chaque appel tire ses quatre
  // pistes à la suite des précédentes. Un seul nœud montrait un bouquet de
  // quatre rubans là où le client en montre vingt.
  std::uint8_t copies = 1;
  std::int16_t tilt_step_per_copy_degrees = 0;
};

[[nodiscard]] std::span<const EffectSpinRibbonPattern> EffectSpinRibbonPatternTable();

[[nodiscard]] std::span<const EffectSpinRibbonPattern> EffectSpinRibbonPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES d'un nœud, toutes vives.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectSpinRibbonTracks(
    const EffectSpinRibbonPattern& pattern, std::uint32_t& generator);

// LE NŒUD qu'un patron décrit, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectSpinRibbonNode(const EffectSpinRibbonPattern& pattern,
                                                       glm::vec3 anchor,
                                                       std::int32_t effect_life_ticks,
                                                       std::uint32_t& generator);

}  // namespace ro::render
