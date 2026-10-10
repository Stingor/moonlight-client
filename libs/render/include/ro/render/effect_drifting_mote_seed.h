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

// LE SEMIS DU GRAIN QUI DÉRIVE, sous-type 96.
//
// UN SEUL bâtisseur, et son entier choisit TROIS choses : à quel tic le semis a lieu,
// combien de pistes s'allument, et tous les nombres du grain.
//
// ⚠ L'ENTIER DU SITE EST AUSSI LE TIC DU SEMIS, et c'est la première famille
// du lot dans ce cas. Le client compare le tic de l'instance à cet entier —
// sauf pour la valeur UN, qui sème au tic ZÉRO, et pour la valeur ZÉRO, qui
// sème au tic TRENTE-CINQ. Deux exceptions sur une règle de deux cas.
//
// ⚠ ET TOUT AUTRE ENTIER BÂTIT UN NŒUD SANS AUCUNE PISTE VIVE. Le client
// éteint les quatre, puis n'entre dans aucune des deux branches. Aucun de nos
// quatre sites n'est dans ce cas ; le générateur refuserait bruyamment.

// LES DEUX DISPOSITIONS, par l'entier du site.
inline constexpr std::int16_t kDriftingMoteLoneVariant = 0;
inline constexpr std::int16_t kDriftingMoteSwarmVariant = 1;
inline constexpr std::int32_t kDriftingMoteLoneSowsAtTick = 35;

// CE QUE CHACUNE TIRE. Sept nombres, et aucun ne se déduit d'un autre.
//
// ⚠ LE GRAIN SOLITAIRE EST PLUS GROS, PLUS RAPIDE ET MOINS LOIN : rayon trois
// à quatre contre deux à deux et demi, clé un à deux dixièmes et demi contre
// huit à treize centièmes, et un cercle de départ de deux et demi contre trois
// et demi. Trois écarts, et ils vont dans deux sens contraires.
inline constexpr float kDriftingMoteLoneSpeedFloor = 0.1F;
inline constexpr float kDriftingMoteLoneSpeedStep = 0.01F;
inline constexpr std::int32_t kDriftingMoteLoneSpeedSpread = 16;
inline constexpr float kDriftingMoteSwarmSpeedFloor = 0.08F;
inline constexpr float kDriftingMoteSwarmSpeedStep = 0.005F;
inline constexpr std::int32_t kDriftingMoteSpeedSpread = 11;

inline constexpr float kDriftingMoteLoneRadiusFloor = 3.0F;
inline constexpr float kDriftingMoteLoneRadiusStep = 0.1F;
inline constexpr float kDriftingMoteSwarmRadiusFloor = 2.0F;
inline constexpr float kDriftingMoteSwarmRadiusStep = 0.05F;
inline constexpr std::int32_t kDriftingMoteRadiusSpread = 11;

inline constexpr float kDriftingMoteLoneRing = 2.5F;
inline constexpr float kDriftingMoteSwarmRing = 3.5F;

// LE RETARD ET LA HAUTEUR DE DÉPART, les mêmes pour les deux.
//
// ⚠ LA HAUTEUR EST TIRÉE SUR HUIT ET NÉGATIVE dans le repère du client : les
// grains naissent donc de zéro à SEPT unités AU-DESSUS du nœud, échelonnés.
inline constexpr std::int32_t kDriftingMoteDelaySpread = 26;
inline constexpr std::int32_t kDriftingMoteHeightSpread = 8;

// LES DEUX MARQUES que le bâtisseur pose, et qui choisissent la teinte.
inline constexpr std::uint8_t kDriftingMoteLoneMark = 1;
inline constexpr std::uint8_t kDriftingMoteSwarmMark = 2;

// UN GRAIN qu'un effet pose.
struct EffectDriftingMotePattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::string_view texture;
  // L'ENTIER DU SITE : zéro pour le grain solitaire, un pour la volée.
  std::int16_t variant = 0;
  // QUAND ce patron sème, en tics de l'instance.
  EffectNodeSchedule schedule;
  // 🔴 COMBIEN DE NŒUDS LE SITE SÈME à ce tic : chaque site appelle son
  // bâtisseur en boucle, trente fois, et chaque appel tire ses grains à la
  // suite du précédent. Un seul nœud montrait le trentième de la nuée. ⚠ EN
  // QUEUE.
  std::uint16_t copies = 1;
};

[[nodiscard]] std::span<const EffectDriftingMotePattern> EffectDriftingMotePatternTable();

[[nodiscard]] std::span<const EffectDriftingMotePattern> EffectDriftingMotePatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES d'un nœud : UNE pour le solitaire, quatre pour la volée.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectDriftingMoteTracks(
    const EffectDriftingMotePattern& pattern, std::uint32_t& generator);

// LE NŒUD qu'un patron décrit, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectDriftingMoteNode(const EffectDriftingMotePattern& pattern,
                                                         glm::vec3 anchor,
                                                         std::int32_t effect_life_ticks,
                                                         std::uint32_t& generator);

}  // namespace ro::render
