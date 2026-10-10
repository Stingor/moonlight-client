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

// LE SEMIS DE L'ÉCLAIR, sous-type 101.
//
// TROIS bâtisseurs et SIX lignes de table pour trois identifiants.
//
// ⚠ CHAQUE IDENTIFIANT POSE DEUX NŒUDS, l'un à l'inclinaison de base ZÉRO,
// l'autre à CENT QUATRE-VINGTS : un éventail vers l'avant, un vers l'arrière.
// N'en semer qu'un donnerait la moitié de l'éclair.
//
// ⚠ ET DEUX DES TROIS BÂTISSEURS SONT LE MÊME CODE : ils ne diffèrent d'aucun
// champ. Le troisième en diffère par deux : sa teinte se tire au sort, et il
// abaisse son ancre de cinq.

// COMBIEN L'ÉVENTAIL PORTE DE SEGMENTS, et de combien de degrés ses quatre
// pistes s'écartent.
inline constexpr float kLightningFanSegments = 20.0F;
inline constexpr std::int16_t kLightningFanTiltPerTrack = 45;

// LE RAYON DU TUBE, le même pour les trois.
inline constexpr float kLightningFanTubeRadius = 0.1F;

// CE QUE LE BÂTISSEUR TIRE : le facteur de bombement, sur onze crans d'un
// centième — de zéro à un dixième.
inline constexpr std::int32_t kLightningFanBowSpread = 11;
inline constexpr float kLightningFanBowStep = 0.01F;

// DE COMBIEN LE TROISIÈME BÂTISSEUR ÉLÈVE SON ANCRE.
//
// ⚠ LE REPÈRE DU CLIENT DESCEND : il RETRANCHE cinq, ce qui élève. Nous
// ajoutons.
inline constexpr float kLightningFanRaisedAnchor = 5.0F;

// UN ÉVENTAIL qu'un effet lance.
struct EffectLightningFanPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::string_view texture;
  // L'INCLINAISON DE LA PREMIÈRE PISTE : zéro pour l'éventail avant, cent
  // quatre-vingts pour l'arrière.
  std::int16_t base_tilt_degrees = 0;
  // LE BALAYAGE, que la figure lit comme un CHOIX DE TEINTE (ADR 0017) : trois
  // tire au sort, tout le reste donne le dégradé.
  std::int16_t sweep = 0;
  // VRAI pour le troisième bâtisseur seulement, celui qui tire sa teinte.
  bool raises_anchor = false;
  EffectNodeSchedule schedule;
};

[[nodiscard]] std::span<const EffectLightningFanPattern> EffectLightningFanPatternTable();

[[nodiscard]] std::span<const EffectLightningFanPattern> EffectLightningFanPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES d'un nœud, toutes vives.
//
// ⚠ LEUR HISTORIQUE NAÎT VIDE et leur rayon NUL : c'est le premier tic de
// mouvement qui écrit l'éclair entier d'un coup.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectLightningFanTracks(
    const EffectLightningFanPattern& pattern, glm::vec3 anchor, std::uint32_t& generator);

// LE NŒUD qu'un patron décrit, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectLightningFanNode(const EffectLightningFanPattern& pattern,
                                                         glm::vec3 anchor,
                                                         std::int32_t effect_life_ticks,
                                                         std::uint32_t& generator);

}  // namespace ro::render
