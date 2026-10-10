#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DE L'ANNEAU DOUBLÉ, et les effets du sous-type 106.
//
// Une image, et tout le reste écrit en dur. Les QUATRE pistes sont vives, et ce
// qui les distingue tient en deux nombres — leur TOUR de départ, à
// quatre-vingt-dix degrés d'écart, et leur OUVERTURE de départ, échelonnée de
// vingt-deux degrés.
//
// ⚠ CE BÂTISSEUR POSE DEUX NŒUDS, PAS UN, et c'est sa garde qui le dit : il
// bâtit quand le compteur de l'instance vaut ZÉRO ou QUATRE. Les deux nœuds sont
// identiques ; seul leur âge diffère. N'en poser qu'un donnerait une aura qui bat
// une fois sur deux.
//
// ⚠ ET SA VARIANTE DE TRAÎNÉE VAUT QUATRE. C'est elle qui décide de la couleur,
// dans la palette de la voie B, et non le bâtisseur ni la figure : quatre est un
// bleu pâle en mélange ADDITIF. Voir `effect_trail_palette.h`.

// LE RAYON DE L'ANNEAU OUVERT, et la clé dont son centre glisse. Les deux valent
// huit, et ce n'est pas la même grandeur : les confondre ferait dépendre le
// glissement du rayon.
inline constexpr float kDoubleRibbonRadius = 8.0F;
inline constexpr float kDoubleRibbonLiftKey = 8.0F;

// DE COMBIEN LES QUATRE PISTES SONT ÉCHELONNÉES.
inline constexpr std::int16_t kDoubleRibbonTurnPerTrack = 45;
inline constexpr float kDoubleRibbonOpeningPerTrack = 22.0F;

// LA VARIANTE DE TRAÎNÉE que le bâtisseur pose dans le nœud.
inline constexpr std::uint8_t kDoubleRibbonTrailVariant = 4;

// LE RETARD DU SECOND NŒUD, en tics et NÉGATIF : le mouvement ne touche pas une
// piste tant que son compteur n'est pas remonté à un.
inline constexpr std::int32_t kDoubleRibbonSecondNodeDelayTicks = -4;

// UNE AURA QU'UN EFFET POSE : ce que son site d'appel passe au bâtisseur.
struct EffectDoubleRibbonPattern {
  std::uint32_t effect_id = 0;
  // Le nom de référence du bâtisseur qui pose cette aura : une étiquette de
  // traçabilité, sans effet sur le rendu.
  std::string_view client_function;
  std::string_view texture;
};

// Toutes les auras connues, par identifiant d'effet.
[[nodiscard]] std::span<const EffectDoubleRibbonPattern> EffectDoubleRibbonPatternTable();

// TOUTES les auras d'un effet, ou un span VIDE.
[[nodiscard]] std::span<const EffectDoubleRibbonPattern> EffectDoubleRibbonPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES D'UN NŒUD, échelonnées en tour et en ouverture.
//
// `delay_ticks` est le retard du nœud : zéro pour le premier, moins quatre pour
// le second.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectDoubleRibbonTracks(
    std::int32_t delay_ticks);

// LE NŒUD qu'un patron décrit, pistes semées, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectDoubleRibbonNode(const EffectDoubleRibbonPattern& pattern,
                                                         glm::vec3 anchor, std::int32_t delay_ticks,
                                                         std::int32_t effect_life_ticks);

}  // namespace ro::render
