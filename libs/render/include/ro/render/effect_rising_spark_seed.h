#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_node_seed.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// L'ÉTINCELLE QUI MONTE, et les effets qui l'emploient par le sous-type 59 —
// EF_HEAL en tête, le soin du prêtre.
//
// Quatre carrés teintés naissent autour de celui qu'ils entourent, montent en
// tournant sur eux-mêmes, s'allument puis s'éteignent. Leur figure est celle du
// carré teinté ; leur mouvement leur est propre, et c'est pourquoi la forme du
// nœud les distingue. Voir `AdvanceRisingSparkTracks`.
//
// ⚠ CE QUE LE SITE D'APPEL PASSE N'EST PAS LEUR TEXTURE. Les bâtisseurs de cette
// famille en posent plusieurs sortes, et l'argument nomme celle de l'ANNEAU qui
// les accompagne : EF_HEAL reçoit `alpha_down.tga` et ses étincelles sont des
// `pok1.tga`, écrites dans le bâtisseur même. Prendre l'argument peindrait
// l'étincelle avec la texture de son voisin.
//
// ⚠ ET LE STYLE NE SE PARTAGE PAS D'UN BÂTISSEUR À L'AUTRE. Le style un ne veut
// pas dire la même chose pour le bâtisseur d'EF_HEAL et pour celui des
// étincelles éparpillées : chaque identifiant porte donc son propre semis,
// plutôt qu'un renvoi vers une table de styles qui n'existe pas.

// CE QU'UN EFFET SÈME.
// ⚠ UN EFFET SEME PLUSIEURS FOIS, ET CETTE TABLE N'EN GARDE QU'UNE.
//
// EF_HEAL4 bâtit QUATRE nœuds chez le client —
//
//   sous-type 60, au tic 0        : l'anneau, texture du site d'appel
//   sous-type 60, au tic 1        : un SECOND anneau, `alpha_down.tga`
//   sous-type 59, tous les 9 tics jusqu'au 50e : les étincelles, SIX bouffées
//   sous-type 59, au tic 1        : une bouffée de plus
//
// Nous en semons DEUX, une fois chacun. Le vert est donc là, mais sept fois trop
// clairsemé, et le second anneau manque tout entier. Ce n'est ni la figure ni le
// mouvement qui sont en cause : les deux concordent avec le client au centième,
// piste par piste — semis, alpha par tic, et longueur de segment au tic trois
// comme au tic vingt-sept.
//
// Ce qui manque est l'HORAIRE, que `EffectNodePattern` porte déjà pour sa propre
// famille (`first_tick`, `last_tick`, `period_ticks`, et l'horloge de
// `PlaySeedsDueAt`). Cette table-ci et celle des anneaux semés ne le portent pas
// encore, et c'est la prochaine pièce.

struct EffectRisingSparkPattern {
  std::uint32_t effect_id = 0;
  // LA FONCTION DU CLIENT qui pose ces étincelles, par son nom.
  // Elle TRACE : d'un effet qui se montre mal on remonte au code qui le décrit.
  std::string_view client_function;
  // La texture d'un carré, et celle qu'il prend le temps de son ÉCLAT. La
  // seconde est VIDE quand le bâtisseur n'en charge qu'une, et l'éclat ne se
  // voit alors pas.
  std::string_view texture;
  std::string_view flash_texture;
  // ⚠ LE RAYON ET LA MONTÉE SE TIRENT AU SORT, carré par carré : le client les
  // tire dans la boucle qui sème les quatre pistes. Leur donner la même taille
  // ferait quatre jumeaux là où il en montre quatre différents.
  EffectNodeRange half_size;
  EffectNodeRange rise_per_tick;
  // La demi-largeur du carré où les quatre se posent autour de leur porteur.
  float scatter_half_width = 0.0F;
  // De combien l'ancre se tient AU-DESSUS du porteur, et le nom dit le SENS et
  // non le signe : le client RETRANCHE, dans un repère qui descend.
  float lift_above_owner = 0.0F;
  // La marque que le carré teinté lit pour sa couleur — onze est le vert de
  // Heal. Voir `EffectTintedQuadStyleFor`.
  std::uint8_t colour_mark = 0;
  // ⚠ SEUL LE SEMIS QUI CHARGE UNE SECONDE TEXTURE autorise l'éclat, et c'est
  // une MARQUE du client, pas une déduction de notre part.
  bool allows_flash = false;
  // QUAND cet effet lâche CETTE bouffée-ci. Voir `EffectNodeSchedule`.
  EffectNodeSchedule schedule;
  // Le NOMBRE DE VALEURS de l'écart tiré autour du porteur : voir
  // `EffectNodeRange::values`. ⚠ EN QUEUE : la table se pose par position.
  std::int32_t scatter_values = 0;
  // 🔴 LE RETARD D'UNE PISTE, tiré EN PREMIER : `plancher + rand() % étendue`,
  // négatif — la piste attend que son compteur remonte avant de naître. Une
  // étendue nulle ne tire rien et laisse le compteur à zéro.
  std::int32_t delay_floor_ticks = 0;
  std::int32_t delay_spread_ticks = 0;
  // LA DÉRIVE selon le cap, tirée APRÈS les deux écarts et AVANT la montée. Une
  // plage vide ne tire rien : l'étincelle monte droit.
  EffectNodeRange drift;
  // ⚠ COMBIEN DE PISTES VIVENT : les suivantes naissent éteintes et ne tirent
  // pas leur rayon, mais tirent encore leurs écarts et leur montée — l'ordre des
  // tirages des pistes vives n'en dépend pas moins.
  std::uint8_t lit_tracks = static_cast<std::uint8_t>(kEffectTracksPerNode);
  // COMBIEN DE NŒUDS le site sème à chaque semis : il appelle parfois son
  // bâtisseur en boucle, et chaque appel tire à la suite du précédent.
  std::uint16_t copies = 1;
};

// Tous les semis connus, par identifiant d'effet puis par ordre de semis.
[[nodiscard]] std::span<const EffectRisingSparkPattern> EffectRisingSparkPatternTable();

// TOUTES les étincelles d'un effet, ou un span VIDE.
[[nodiscard]] std::span<const EffectRisingSparkPattern> EffectRisingSparkPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES, semées : quatre carrés tirés au sort autour d'un point.
//
// ⚠ LE GÉNÉRATEUR AVANCE À CHAQUE TIRAGE, et il en faut quatre par piste : la
// taille, la montée, et les deux écarts du plan. Deux troupes semées à la suite
// ne se ressemblent donc pas, et la même graine rejoue la même.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectRisingSparkTracks(
    const EffectRisingSparkPattern& pattern, std::uint32_t& generator);

// LE NŒUD qu'un patron décrit, pistes semées, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectRisingSparkNode(const EffectRisingSparkPattern& pattern,
                                                        glm::vec3 ground_position,
                                                        std::uint32_t& generator);

}  // namespace ro::render
