#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DE LA NEIGE LANCÉE, et les effets du sous-type 134.
//
// QUATRE images écrites en dur, et tout le reste tiré au sort. Les quatre
// pistes sont vives.
//
// ⚠ IL A BESOIN DE LA CIBLE, et c'est ce qui le sépare de presque toute la
// famille : l'ancre d'une piste est le point VISÉ, pas celui d'où l'effet part.
// Le cap se calcule entre les deux, et la distance règle la vitesse. Sans cible,
// il n'y a pas de traînée à faire — voir `SowEffectFadingStreakNode`.
//
// ⚠ ET LE CAP N'EST PAS CELUI DE LA CIBLE : le client mesure l'angle de la cible
// VERS LA SOURCE, puis lui ajoute de cent soixante à deux cents degrés. C'est
// donc le cap inverse, DISPERSÉ de vingt degrés de part et d'autre : les quatre
// flocons ne suivent pas la même ligne.

// LE RAYON DU FLOCON : de deux et demie à quatre unités.
inline constexpr float kFadingStreakRadiusFloor = 2.5F;
inline constexpr float kFadingStreakRadiusStep = 0.08F;
inline constexpr std::uint32_t kFadingStreakRadiusSpread = 21;

// LA CLÉ DE VITESSE : de quarante à soixante tics pour couvrir la course.
inline constexpr std::uint8_t kFadingStreakSpeedKeyCeiling = 60;
inline constexpr std::uint32_t kFadingStreakSpeedKeySpread = 21;

// LA DISPERSION DU CAP, autour du demi-tour.
inline constexpr std::int32_t kFadingStreakHeadingFloor = 160;
inline constexpr std::uint32_t kFadingStreakHeadingSpread = 41;

// COMBIEN DE TEINTES le tirage couvre, et l'image que le bâtisseur choisit.
inline constexpr std::uint32_t kFadingStreakTintSpread = 4;
inline constexpr std::uint8_t kFadingStreakImage = 3;

// DE COMBIEN L'ANCRE DESCEND SOUS LA CIBLE. ⚠ PERSONNE NE LA LIT : ni la figure,
// qui prend sa hauteur du nœud, ni le mouvement. Le bâtisseur l'écrit quand même,
// et nous aussi — le jour où un second dessinateur du même sous-type la lira, la
// valeur y sera.
inline constexpr float kFadingStreakAnchorDrop = -12.0F;

// UNE NEIGE QU'UN EFFET LANCE : ce que son bâtisseur écrit en dur.
struct EffectFadingStreakPattern {
  std::uint32_t effect_id = 0;
  // L'étiquette de la routine du client qui lance cette neige.
  std::string_view client_function;
  // LES QUATRE IMAGES, dans l'ordre du client. La marque d'image en désigne une.
  std::span<const std::string_view> textures;
  // COMBIEN DE NŒUDS le cas pose, en appelant le bâtisseur autant de fois.
  //
  // ⚠ CE N'EST PAS UN DÉTAIL : les deux effets connus en posent TROIS, soit
  // douze flocons. N'en poser qu'un donnerait un tiers de la volée, et rien ne
  // le dirait — le seul témoin est le répartiteur, qui appelle trois fois le
  // bâtisseur.
  std::uint16_t node_count = 1;
};

// Toutes les neiges connues, par identifiant d'effet.
[[nodiscard]] std::span<const EffectFadingStreakPattern> EffectFadingStreakPatternTable();

// TOUTES les neiges d'un effet, ou un span VIDE.
[[nodiscard]] std::span<const EffectFadingStreakPattern> EffectFadingStreakPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES D'UNE VOLÉE, toutes vives et toutes dispersées.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectFadingStreakTracks(
    glm::vec3 source, glm::vec3 target, std::uint32_t& generator);

// LE NŒUD qu'un patron décrit, pistes semées, prêt à rejoindre la troupe.
//
// ⚠ SANS CIBLE, RIEN À SEMER, et l'appelant doit le savoir : `target` est le
// point visé, et c'est lui qui donne à la traînée sa direction et sa durée.
[[nodiscard]] PooledEffectNode SowEffectFadingStreakNode(const EffectFadingStreakPattern& pattern,
                                                         glm::vec3 source, glm::vec3 target,
                                                         std::int32_t effect_life_ticks,
                                                         std::uint32_t& generator);

}  // namespace ro::render
