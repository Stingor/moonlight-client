#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LES ANNEAUX D'UN PORTAIL DE TÉLÉPORTATION, sous-type 65.
//
// Sa figure est la JUPE D'ANNEAU, la même que le sous-type 63 — les deux
// passent par le même dessinateur. Seuls son semis et son mouvement lui sont
// propres, comme pour les anneaux qui pulsent.
//
// 🔴 CE QUE LE JOUEUR VOIT À UN WARP. Un PNJ de classe 45 pose l'effet 321
// (0x141) dès qu'il paraît — une seule fois, derrière un loquet, et seulement si
// l'acteur est visible.
//
// ⚠ ET IL Y EN A DEUX ÉTAGES. Le bâtisseur pose un nœud au tic UN puis un second au tic DEUX,
// décalés d'un degré d'azimut et d'un cinquième d'unité de rayon. C'est ce décalage qui épaissit
// les anneaux sans les dédoubler.

// LES QUATRE ANNEAUX D'UN ÉTAGE. Le sous-type en emploie quatre, là où les
// anneaux pulsants n'en dessinent que trois et gardent la dernière piste comme
// registre : ici les quatre se dessinent.
inline constexpr std::size_t kWarpRingCount = 4;

// LE TOUR ENTIER : des cercles fermés, jamais des arcs.
inline constexpr std::int16_t kWarpRingSweepDegrees = 360;

// DE COMBIEN LE RAYON SE REFERME À CHAQUE TIC.
//
// Un anneau parti de dix unités met donc deux cents tics — trois secondes et
// deux dixièmes — à rejoindre le centre. Les quatre étant semés à 2,5 / 5 / 7,5
// et 10, ils arrivent à cinquante tics d'intervalle, et la figure ne s'arrête
// jamais.
inline constexpr float kWarpRingRadiusStepPerTick = 0.05F;

// OÙ LE RAYON REPART quand il a passé zéro. C'est ce rembobinage
// qui rend la figure perpétuelle sans que rien ne la resème.
inline constexpr float kWarpRingRewindRadius = 10.0F;

// L'INCLINAISON SE DÉDUIT DU RAYON, elle ne se range pas : le client la
// recalcule à chaque tic, `90 − 9 × rayon`. Un
// anneau large est donc presque à plat et se redresse en se refermant.
//
// ⚠ LES INCLINAISONS QUE LE BÂTISSEUR ÉCRIT AU SEMIS NE SURVIVENT PAS AU
// PREMIER TIC. Les porter serait porter une valeur que rien ne lit.
inline constexpr float kWarpRingTiltBaseDegrees = 90.0F;
inline constexpr float kWarpRingTiltPerRadius = 9.0F;

// L'ALPHA MONTE D'UN PAR TIC JUSQU'À SOIXANTE-DIX, et redescend de TROIS par
// tic dans les quarante derniers tics de la vie du nœud. Un anneau rembobiné
// repart d'alpha nul, ce qui le fait renaître en fondu au bord extérieur.
inline constexpr std::int16_t kWarpRingAlphaCeiling = 70;
inline constexpr std::int16_t kWarpRingAlphaRisePerTick = 1;
inline constexpr std::int16_t kWarpRingAlphaFallPerTick = 3;
inline constexpr std::int32_t kWarpRingFadeOutTicks = 40;

// UN ÉTAGE D'ANNEAUX qu'un effet pose.
struct EffectWarpRingPattern {
  std::uint32_t effect_id = 0;
  // LA FONCTION DU CLIENT qui le pose.
  std::string_view client_function;
  std::string_view texture;
  // LA VARIANTE DE TEINTE, prise au champ de style du nœud — onze au premier étage,
  // quatre au second. Voir `effect_trail_palette.h`.
  std::uint8_t trail_variant = 0;
  // LE TIC DE L'EFFET auquel le bâtisseur pose ce nœud. Un étage au tic un,
  // l'autre au tic deux : les semer ensemble les superposerait exactement, et
  // le second étage ne se verrait pas.
  std::int32_t first_tick = 0;
  // LES QUATRE RAYONS DE DÉPART, en unités du monde. Ce sont eux, et eux seuls,
  // qui séparent les deux étages avec l'azimut.
  std::array<float, kWarpRingCount> radii{};
  // LES QUATRE AZIMUTS DE DÉPART, en degrés.
  std::array<std::int16_t, kWarpRingCount> azimuths{};
};

// Tous les étages d'anneaux connus, par identifiant d'effet.
[[nodiscard]] std::span<const EffectWarpRingPattern> EffectWarpRingPatternTable();

// TOUS les étages d'un effet, ou un span VIDE.
[[nodiscard]] std::span<const EffectWarpRingPattern> EffectWarpRingPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES d'un étage, semées à leur rayon et à leur azimut.
//
// ⚠ LA CLÉ EST LE RAYON, et ce n'est pas une commodité : le client recopie
// l'une dans l'autre au semis comme à chaque tic, et le mouvement les tient
// égales. La jupe lit la clé comme la largeur de son ruban — un anneau large
// porte donc un ruban large, ce qui est exactement ce qu'on voit.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectWarpRingTracks(
    const EffectWarpRingPattern& pattern);

// LE NŒUD qu'un patron décrit, pistes semées, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectWarpRingNode(const EffectWarpRingPattern& pattern,
                                                     glm::vec3 anchor,
                                                     std::int32_t effect_life_ticks);

// FAIT AVANCER LES QUATRE ANNEAUX D'UN TIC.
//
// Le rayon se referme, se rembobine à dix quand il passe zéro, l'inclinaison se
// recalcule depuis lui, la clé le suit, et l'alpha monte puis retombe à
// l'approche de la fin de vie.
void AdvanceWarpRingTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                           std::int32_t node_life_ticks);

}  // namespace ro::render
