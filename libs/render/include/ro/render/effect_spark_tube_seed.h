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

// LE SEMIS DU TUBE D'ÉTINCELLE, sous-type 100.
//
// TROIS bâtisseurs, et ils écrivent les mêmes champs. Tous chargent la même
// image, `effect\white02.bmp`, et l'écrivent EN DUR — aucun ne la reçoit.
//
// ⚠ CHACUN A SA PROPRE FENÊTRE DE TICS, et c'est là que vit le nombre de
// nœuds : le client rappelle son bâtisseur à chaque tic de l'instance et le
// garde derrière une comparaison sur ce tic. Deux nœuds pour l'un, trois,
// quatre ou cinq pour les autres — et l'un d'eux ne commence qu'au SIXIÈME
// tic. Tout semer au premier tic donnerait une seule gerbe au lieu d'une
// rafale.
//
// ⚠ ET UN IDENTIFIANT SUR CINQ NE SÈME RIEN DU TOUT. EF_HITLINE3 passe l'octet
// ZÉRO, pour lequel la fenêtre du premier bâtisseur est VIDE — la comparaison
// est `tic < 0`, jamais vraie. Le client lui-même ne montre rien. Le
// générateur le refuse bruyamment.

// L'IMAGE que les trois bâtisseurs écrivent en dur.
inline constexpr std::string_view kSparkTubeTexture = "data\\texture\\effect\\white02.bmp";

// LEQUEL DES TROIS BÂTISSEURS un site appelle.
enum class EffectSparkTubeBuilder : std::uint8_t {
  kRising,
  kRisingLate,
  kDirected,
};

// UN NŒUD D'ÉTINCELLE qu'un site pose.
struct EffectSparkTubePattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  EffectSparkTubeBuilder builder = EffectSparkTubeBuilder::kRising;
  // L'OCTET DU SITE. Il choisit la fenêtre de tics, le retard, la vitesse, la
  // longueur de la courbe ET le style — donc la teinte.
  std::uint8_t style = 0;
  // QUAND ce patron sème, en tics de l'instance.
  EffectNodeSchedule schedule;
};

[[nodiscard]] std::span<const EffectSparkTubePattern> EffectSparkTubePatternTable();

[[nodiscard]] std::span<const EffectSparkTubePattern> EffectSparkTubePatternsFor(
    std::uint32_t effect_id);

// COMBIEN DE TICS DE MOUVEMENT LE PREMIER BÂTISSEUR JOUE D'AVANCE, pour un seul
// de ses octets.
//
// ⚠ SEUL L'OCTET UN EST PRÉ-DÉROULÉ, et de DIX tics. Son étincelle naît donc
// déjà longue, là où les autres se déploient sous les yeux du joueur. Ne pas
// le faire donnerait à EF_STORMKICK un départ que le client ne montre pas.
inline constexpr std::int32_t kSparkTubePreRollTicks = 10;
inline constexpr std::uint8_t kSparkTubePreRolledStyle = 1;

// LES QUATRE PISTES d'un nœud : toutes vives, toutes retardées, toutes tirées
// séparément.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectSparkTubeTracks(
    const EffectSparkTubePattern& pattern, glm::vec3 anchor, glm::vec3 toward_target,
    std::uint32_t& generator);

// LE NŒUD qu'un patron décrit, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectSparkTubeNode(const EffectSparkTubePattern& pattern,
                                                      glm::vec3 anchor, glm::vec3 toward_target,
                                                      std::int32_t effect_life_ticks,
                                                      std::uint32_t& generator);

}  // namespace ro::render
