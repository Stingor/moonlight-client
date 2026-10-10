#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DE LA BOURRASQUE AU SOL, sous-type 119.
//
// UN SEUL bâtisseur et UN SEUL site : un identifiant, 547, sur l'image du vent.
//
// ⚠ LES QUATRE PISTES SONT ALLUMÉES, ET TROIS NE SONT QUE DES ÉCHOS. Elles
// naissent à alpha nul et ne se montrent que lorsque le mouvement y recopie ce
// que leur voisine portait. N'en allumer qu'une effacerait la traîne ; les
// faire vivre chacune pour soi donnerait quatre bourrasques.

// COMBIEN DE TICS la bourrasque se fait attendre. Le bâtisseur pose le compteur
// de la piste zéro à moins trois, et rien ne bouge tant qu'il n'est pas positif.
inline constexpr std::int32_t kGroundGustDelayTicks = 3;

// LA DEMI-DIAGONALE DU CARRÉ, la même pour les quatre pistes.
inline constexpr float kGroundGustRadius = 20.0F;

// CE QUE LA DÉRIVE VAUT AU DÉPART, avant que le mouvement ne l'enfle.
inline constexpr float kGroundGustStartSpeed = 0.8F;

// DE COMBIEN LA BOURRASQUE FLOTTE AU-DESSUS DU NŒUD, et de combien le tirage la
// redescend — de zéro à quatre.
//
// ⚠ LE REPÈRE DU CLIENT DESCEND, LE NÔTRE MONTE : il retranche dix et rajoute
// son tirage ; nous ajoutons dix et retranchons le même tirage. Recopier le
// signe au lieu du sens enfoncerait la figure sous la carte.
inline constexpr float kGroundGustLiftAboveNode = 10.0F;
inline constexpr std::int32_t kGroundGustLiftSpread = 5;

// UNE BOURRASQUE qu'un effet lance.
struct EffectGroundGustPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::string_view texture;
};

[[nodiscard]] std::span<const EffectGroundGustPattern> EffectGroundGustPatternTable();

[[nodiscard]] std::span<const EffectGroundGustPattern> EffectGroundGustPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES d'un nœud, toutes vives et toutes transparentes.
//
// `anchor` est la position du nœud dans le monde ; `toward_target` le vecteur
// qui va de là vers la cible du sort. `render` ne connaît pas cette cible
// (CLAUDE.md) : l'appelant la donne, comme il la donne à l'essaim.
//
// ⚠ LE CAP EST POSÉ ICI, ET LE CLIENT LE PREND AU PREMIER TIC UTILE. Les deux
// reviennent au même : ni la position du nœud ni la cible ne
// changent entre le semis et ce tic-là — le bâtisseur les recopie une fois, et
// le mouvement du 119 n'y touche jamais. Le poser au semis évite de traîner la
// cible dans le nœud pendant trois tics pour rien.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectGroundGustTracks(
    glm::vec3 anchor, glm::vec3 toward_target, std::uint32_t& generator);

// LE NŒUD qu'un patron décrit, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectGroundGustNode(const EffectGroundGustPattern& pattern,
                                                       glm::vec3 anchor, glm::vec3 toward_target,
                                                       std::int32_t effect_life_ticks,
                                                       std::uint32_t& generator);

}  // namespace ro::render
