#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DE L'ÉTINCELLE QUI CONVERGE, sous-type 159.
//
// UN SEUL bâtisseur et QUATRE sites, un par identifiant. Le site donne une image et un octet, et
// l'octet ne choisit QUE la teinte : tout le reste est le même pour les quatre.
//
// ⚠ L'ANCRE D'UNE PISTE EST ICI UN DÉCALAGE, et il vit dans le repère du
// CLIENT — celui qui descend. Le mouvement le fait reculer et la figure le
// retourne au dernier moment ; l'inverser ici le retournerait deux fois.

// LE RAYON DE LA SPHÈRE sur laquelle les quatre étincelles naissent.
inline constexpr float kHomingSparkSphereRadius = 70.0F;
// LA TAILLE D'UNE LAME, la même pour les quatre.
inline constexpr float kHomingSparkBladeRadius = 4.0F;
// DE COMBIEN LA LAME EST DÉCALÉE, dans le repère du client — donc VERS LE
// HAUT chez nous.
inline constexpr float kHomingSparkLift = -5.0F;
// DE COMBIEN L'ANCRE RECULE par tic.
inline constexpr float kHomingSparkStep = 0.5F;
// JUSQU'OÙ LE RETARD VA : de zéro à cent tics, et il est négatif.
inline constexpr std::int32_t kHomingSparkDelaySpread = 101;

// UNE ÉTINCELLE qu'un effet pose.
struct EffectHomingSparkPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::string_view texture;
  // L'OCTET DU SITE, et il ne choisit QUE la teinte des deux lames.
  std::uint8_t mark = 0;
};

[[nodiscard]] std::span<const EffectHomingSparkPattern> EffectHomingSparkPatternTable();

[[nodiscard]] std::span<const EffectHomingSparkPattern> EffectHomingSparkPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES d'un nœud, toutes vives, et chacune tire TROIS fois : son
// retard, son inclinaison et son azimut.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectHomingSparkTracks(
    const EffectHomingSparkPattern& pattern, std::uint32_t& generator);

// LE NŒUD qu'un patron décrit, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectHomingSparkNode(const EffectHomingSparkPattern& pattern,
                                                        glm::vec3 anchor,
                                                        std::int32_t effect_life_ticks,
                                                        std::uint32_t& generator);

}  // namespace ro::render
