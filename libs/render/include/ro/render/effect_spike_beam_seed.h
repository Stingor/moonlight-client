#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_pie_wedge_seed.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DU FAISCEAU EN PIQUE, et les effets du sous-type 164.
//
// C'est le JUMEAU du semis du quartier de tarte : même horaire de trois tics,
// même dispersion en trente degrés fois « le rang plus quatre fois le tic »,
// même hauteur de moins cinq, même rayon de cent. Les deux bâtisseurs ne
// diffèrent que par ce que leur figure demande en plus.
//
// ⚠ CE QUI LES SÉPARE TIENT EN TROIS CHAMPS : celui-ci charge UNE image, pose une
// clé de quinze — qui est à la fois le rayon de la section du faisceau et le pas
// entre ses segments — et met l'indice d'image à zéro.
//
// ⚠ ET LE FAISCEAU GRANDIT EN RECULANT. Son rayon de départ vaut cent, soit
// exactement la portée où la figure s'arrête : au premier tic un seul segment
// paraît. Le mouvement fait reculer ce départ d'une unité par tic, et le faisceau
// gagne un segment chaque fois qu'il descend de quinze. Lui donner un rayon de
// départ plus petit le montrerait entier d'emblée.

// LA CLÉ : le rayon de la section du faisceau, et le pas entre ses segments.
inline constexpr float kSpikeBeamStep = 15.0F;
// LE RAYON DE DÉPART, égal à la portée maximale : un seul segment au premier tic.
inline constexpr float kSpikeBeamFirstReach = 100.0F;
// LA HAUTEUR DES QUATRE COINS, NÉGATIVE : le repère du client descend.
inline constexpr float kSpikeBeamHeight = -5.0F;

// UN FAISCEAU QU'UN EFFET POSE : ce que son site d'appel passe au bâtisseur.
struct EffectSpikeBeamPattern {
  std::uint32_t effect_id = 0;
  // Le nom de la fonction du client qui pose ce faisceau.
  std::string_view client_function;
  std::string_view texture;
};

// Tous les faisceaux connus, par identifiant d'effet.
[[nodiscard]] std::span<const EffectSpikeBeamPattern> EffectSpikeBeamPatternTable();

// TOUS les faisceaux d'un effet, ou un span VIDE.
[[nodiscard]] std::span<const EffectSpikeBeamPattern> EffectSpikeBeamPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES D'UN NŒUD, toutes vives.
//
// `tick` est le tic de l'effet où ce nœud naît : c'est lui qui décide du quart de
// cercle que ces quatre-là balaient. Voir `kPieWedgeSowingTicks`, que les deux
// familles partagent.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectSpikeBeamTracks(
    std::int32_t tick);

// LE NŒUD qu'un patron décrit, pistes semées, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectSpikeBeamNode(const EffectSpikeBeamPattern& pattern,
                                                      glm::vec3 anchor, std::int32_t tick,
                                                      std::int32_t effect_life_ticks);

}  // namespace ro::render
