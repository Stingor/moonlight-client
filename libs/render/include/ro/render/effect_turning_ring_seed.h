#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// L'ANNEAU QUI PULSE, sous-type 50 : un anneau qui tourne et dont le rayon
// respire, parcouru de la même onde que son frère.
//
// Un seul bâtisseur le sème. Quatre identifiants
// l'emploient — 280, 285, 783 et 784.
//
// ⚠ C'EST LE FRÈRE DU SOUS-TYPE 36. Même figure, même
// bâtisseur à quelques nombres près, et surtout LA MÊME ONDE : la boucle qui
// réécrit les vingt et une longueurs est identique instruction pour
// instruction. Voir `ApplyRibbonWaveLengths`.
//
// ⚠ ET POURTANT LEURS MOUVEMENTS N'ONT PAS UN CHAMP EN COMMUN HORS L'ONDE. Le
// 36 fait respirer son alpha et descendre son inclinaison ; celui-ci n'y touche
// pas, fait PULSER son rayon et TOURNER son azimut. C'est la preuve que la
// forme doit lier la figure ET le mouvement, et non la seule figure.

// CE QU'UN EFFET SÈME.
struct EffectTurningRingPattern {
  std::uint32_t effect_id = 0;
  // LE BÂTISSEUR qui pose ce nœud, par son nom.
  std::string_view client_function;
  std::string_view texture;
  // Le numéro de teinte que la primitive de ruban lit. C'est la SEULE chose que
  // le paramètre du site décide.
  std::uint8_t trail_variant = 0;
};

// Tous les anneaux connus, par identifiant d'effet puis par ordre de semis.
[[nodiscard]] std::span<const EffectTurningRingPattern> EffectTurningRingPatternTable();

// TOUS les anneaux qui pulsent d'un effet, ou un span VIDE.
[[nodiscard]] std::span<const EffectTurningRingPattern> EffectTurningRingPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES, dont UNE SEULE est vive.
//
// ⚠ QUATRE TIRAGES, ET LE DERNIER DÉCIDE DE LA PHASE DU POULS. Le client tire
// la clé, l'azimut, l'alpha, puis le STYLE — et c'est le style que son
// mouvement fait avancer pour en tirer le rayon. Le poser à zéro ferait battre
// d'un seul cœur deux anneaux semés ensemble.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectTurningRingTracks(
    std::uint32_t& generator);

// LE NŒUD qu'un patron décrit, piste semée, prêt à rejoindre la troupe.
//
// ⚠ IL SUIT SON PORTEUR, comme son frère : le répartiteur recopie à chaque tic
// la position de l'acteur dans celle du nœud.
[[nodiscard]] PooledEffectNode SowEffectTurningRingNode(const EffectTurningRingPattern& pattern,
                                                        glm::vec3 ground_position,
                                                        std::uint32_t& generator);

}  // namespace ro::render
