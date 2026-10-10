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

// LE SEMIS DU TRAIT GAINÉ, sous-type 69.
//
// CINQ bâtisseurs, et ils écrivent les MÊMES champs dans le MÊME ORDRE.
//
// ⚠ LES CINQ SONT UN SEUL SEMIS. Ils ne diffèrent que par sept tirages, une
// image et un horaire. Les porter séparément aurait écrit cinq fois le même
// code, et la première correction n'en aurait touché qu'un.
//
// ⚠ ET LE CINQUIÈME VIENT DES ÉMETTEURS, non des effets : c'est l'émetteur
// 329. Il ressemble aux quatre autres à une constante près — c'est la figure
// qui fait la famille, pas le chemin qui la pose.
//
// ⚠ CETTE FAMILLE DÉPEND DE LA CIBLE DU SORT, que `render` ne connaît pas :
// l'azimut du trait en vient, et deux bâtisseurs y mesurent aussi leur portée.
// Le vecteur nul donne un cap de zéro et une distance nulle — le trait part
// vers l'est et ne doit sa longueur qu'à son tirage. C'est la convention de la
// colonne, de l'essaim et du tube d'étincelle.

// UN TIRAGE DU SEMIS : une largeur de tirage, un pas, et un plancher.
//
// ⚠ UNE LARGEUR NULLE VEUT DIRE « LA CONSTANTE », ET LE GÉNÉRATEUR N'AVANCE
// PAS. Trois bâtisseurs posent leur largeur de trait en dur, et le quatrième
// pose aussi son recul ; le client n'appelle alors pas son tirage. En appeler
// un quand même décalerait toute la suite de la séquence.
struct EffectSheathedBoltDraw {
  std::int32_t spread = 0;
  float step = 0.0F;
  float base = 0.0F;
};

// LEQUEL DES CINQ BÂTISSEURS un site appelle. Le style qui les nomme est
// celui qu'ils posent, et c'est lui qui choisit la teinte et la rampe d'alpha.
enum class EffectSheathedBoltBuilder : std::uint8_t {
  // Les deux traits que l'on lance vers une cible : leur portée se mesure sur
  // elle.
  kAimedWhite,
  kAimedPink,
  // Les trois traits d'impact : leur portée est posée en dur.
  kImpactBlue,
  kImpactDim,
  // Celui qui ne pose AUCUN des quatre styles nommés, et que la figure peint
  // donc de sa teinte par défaut. Voir `kSheathedBoltUnnamedStyle`.
  kImpactYellow,
};

// CE QU'UN BÂTISSEUR TIRE ET POSE, dans l'ordre où il le fait.
struct EffectSheathedBoltRecipe {
  // LE RETARD, en tics, et il est NÉGATIF : un plancher, qu'un tirage enfonce
  // encore. Tant qu'il l'est, la piste attend sans rien montrer.
  std::int32_t delay_floor = 0;
  std::int32_t delay_spread = 0;
  // L'ÉCART DE L'ANCRE autour du point de l'effet, tiré DEUX FOIS — une fois
  // en x, une fois en z — avec le même pas et le même plancher.
  EffectSheathedBoltDraw scatter;
  // DE COMBIEN L'ANCRE RECULE le long de l'azimut, donc dans le sens OPPOSÉ à
  // la cible : le trait commence derrière celui qui le lance.
  EffectSheathedBoltDraw pullback;
  // LE RAYON DE L'ÂME, que la figure lit dans la clé de décalage.
  EffectSheathedBoltDraw width;
  // DE COMBIEN LE TRAIT MONTE au-dessus de son ancre.
  EffectSheathedBoltDraw lift;
  // LA PORTÉE, soit seule, soit ajoutée à la distance jusqu'à la cible.
  EffectSheathedBoltDraw reach;
  bool reach_from_target = false;
  // LE STYLE, rangé en flottant dans la piste comme le client le range.
  float style = 0.0F;
};

// LES CINQ RECETTES, une par bâtisseur.
[[nodiscard]] const EffectSheathedBoltRecipe& EffectSheathedBoltRecipeOf(
    EffectSheathedBoltBuilder builder);

// UN TRAIT qu'un effet pose.
struct EffectSheathedBoltPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::string_view texture;
  EffectSheathedBoltBuilder builder = EffectSheathedBoltBuilder::kAimedWhite;
  // QUAND cet effet pose SES nœuds : le client bâtit un nœud par tic tant que
  // le tic de l'instance reste sous une borne. Voir `EffectNodeSchedule`.
  EffectNodeSchedule schedule;
};

[[nodiscard]] std::span<const EffectSheathedBoltPattern> EffectSheathedBoltPatternTable();

[[nodiscard]] std::span<const EffectSheathedBoltPattern> EffectSheathedBoltPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES d'un nœud, toutes vives et toutes tirées séparément.
//
// `anchor` est le point du monde où l'effet joue, et `toward_target` le vecteur
// qui va de là vers la cible du sort.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectSheathedBoltTracks(
    const EffectSheathedBoltPattern& pattern, glm::vec3 anchor, glm::vec3 toward_target,
    std::uint32_t& generator);

// LE NŒUD qu'un patron décrit, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectSheathedBoltNode(const EffectSheathedBoltPattern& pattern,
                                                         glm::vec3 anchor, glm::vec3 toward_target,
                                                         std::int32_t effect_life_ticks,
                                                         std::uint32_t& generator);

}  // namespace ro::render
