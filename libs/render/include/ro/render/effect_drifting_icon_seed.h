#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"
#include "ro/render/effect_track_motion.h"

namespace ro::render {

// LE SEMIS DES ICÔNES QUI FLOTTENT, sous-type 123.
//
// UN SEUL bâtisseur et TROIS sites, un par octet : 573 à l'octet zéro, 574 à
// l'octet un, 575 à l'octet deux. L'octet ne choisit que l'image — des icônes
// d'objets, herbe blanche, potion blanche fine, ou l'une de trois fioles.
//
// ⚠ CHAQUE SITE APPELLE LE BÂTISSEUR QUATRE-VINGTS FOIS PAR TIC, et le
// bâtisseur ne sème qu'à l'âge zéro : QUATRE-VINGTS nœuds naissent ensemble,
// soit trois cent vingt icônes. Aucun plafond ne les retient.
//
// ⚠ LA PLACE EST CELLE DU JOUEUR LOCAL, pas celle de l'effet : le bâtisseur
// recopie la position du joueur dans l'instance avant toute chose. Voir
// `SowEffectDriftingIconNode`.

// COMBIEN DE NŒUDS un site sème d'un coup.
inline constexpr std::int32_t kDriftingIconNodesPerPlay = 80;

// LE CARRÉ OÙ CHAQUE ICÔNE SE POSE autour du joueur : `rand() % 160 − 80` en x
// puis en profondeur.
inline constexpr std::int32_t kDriftingIconScatterSpan = 160;
inline constexpr std::int32_t kDriftingIconScatterHalf = 80;

// SA HAUTEUR AU-DESSUS DU SOL : le client prend l'altitude du terrain sous
// l'icône, en RETRANCHE cinq puis `rand() % 66` — et son repère descend : chez
// nous, l'icône flotte de cinq à soixante-dix unités AU-DESSUS du sol.
inline constexpr float kDriftingIconLiftAboveGround = 5.0F;
inline constexpr std::int32_t kDriftingIconLiftSpread = 66;

// SON RAYON : `rand() % 11 × 0.1 + 3.0`.
inline constexpr float kDriftingIconRadiusBase = 3.0F;
inline constexpr float kDriftingIconRadiusStep = 0.1F;
inline constexpr std::int32_t kDriftingIconRadiusDraws = 11;

// LE SEUIL DE LA PÂLEUR : le bâtisseur range `vie − 50` dans le cap, que le
// mouvement relit comme le tic où l'alpha se met à baisser.
inline constexpr std::int32_t kDriftingIconFadeTicks = 50;

// LA MARQUE DE TEINTE qu'il pose (sept) : le blanc, AJOUTÉ,
// coins par paires échangées. Voir `EffectTintedQuadStyleFor`.
inline constexpr std::uint8_t kDriftingIconTint = 7;

// LE MOT DE RENDU qu'il écrit en dur (cinq) : le seau lit le
// mélange de chaque primitive. Voir `effect_render_word.h`.
inline constexpr std::uint32_t kDriftingIconRenderWord = 5;

// AU PLUS TROIS IMAGES par site : l'octet deux en tire une par nœud.
inline constexpr std::size_t kDriftingIconTextureChoices = 3;

// UN SITE : l'effet, son octet, et les images entre lesquelles il choisit.
//
// ⚠ UNE SEULE IMAGE NON VIDE veut dire « pas de tirage » : les octets zéro et un
// chargent la leur sans appeler `rand()` ; l'octet deux tire `rand() % 3` UNE
// FOIS PAR NŒUD, avant ses pistes, si bien que les quatre icônes d'un nœud
// partagent leur fiole.
struct EffectDriftingIconPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::uint8_t octet = 0;
  std::array<std::string_view, kDriftingIconTextureChoices> textures = {};
};

[[nodiscard]] std::span<const EffectDriftingIconPattern> EffectDriftingIconPatternTable();

[[nodiscard]] std::span<const EffectDriftingIconPattern> EffectDriftingIconPatternsFor(
    std::uint32_t effect_id);

// Combien d'images ce patron offre : les premières non vides.
[[nodiscard]] std::size_t EffectDriftingIconTextureCount(const EffectDriftingIconPattern& pattern);

// LES QUATRE PISTES d'un nœud semé autour de `player_position`.
//
// ONZE TIRAGES PAR PISTE, dans l'ordre du client : l'angle du carré, x,
// profondeur, hauteur, les quatre angles qui courent, le rayon, l'angle propre.
//
// ⚠ LA HAUTEUR SE LIT SUR LE SOL SOUS L'ICÔNE, par `world.ground_height`. Sans
// réponse, l'icône se pose sur la hauteur du joueur : c'est faux sur un relief,
// et préférable à une altitude inventée.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectDriftingIconTracks(
    glm::vec3 player_position, std::int32_t life_ticks, const EffectScatterWorld& world,
    std::uint32_t& generator);

// LE NŒUD qu'un patron décrit : l'image d'abord (tirée pour l'octet deux), puis
// les pistes. `life_ticks` est la vie de l'instance, que le bâtisseur recopie
// dans le nœud ET dont il tire le seuil de la pâleur.
[[nodiscard]] PooledEffectNode SowEffectDriftingIconNode(const EffectDriftingIconPattern& pattern,
                                                         glm::vec3 player_position,
                                                         std::int32_t life_ticks,
                                                         const EffectScatterWorld& world,
                                                         std::uint32_t& generator);

}  // namespace ro::render
