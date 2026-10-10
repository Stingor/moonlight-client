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

// LE SEMIS ET LE MOUVEMENT DE L'ANNEAU DE NUAGE, sous-type 62.
//
// Le mouvement est celui que le client donne au sous-type 62 : une rampe à
// quatre pistes.
//
// TROIS bâtisseurs. Celui des anneaux à quatre pistes tient DIX sites pour SIX
// identifiants — quatre d'entre eux l'appellent DEUX FOIS, avec deux octets
// différents ; celui du nuage de soin en tient un ; celui du nuage violet et
// rouge en tient deux, et c'est lui qui nappe le portail de teleportation en
// attente. La figure vit dans `effect_track_figures.h`.

// CE QUE LE BÂTISSEUR POSE POUR TOUTES LES PISTES.
inline constexpr std::int16_t kCloudRingStartSweepDegrees = 30;
inline constexpr std::int16_t kCloudRingDegreesPerTrack = 90;
inline constexpr float kCloudRingSegmentLength = 1.0F;

// L'INCLINAISON, la seule chose que deux octets sur quatre tirent au sort.
inline constexpr std::int32_t kCloudRingTiltSpread = 21;
inline constexpr std::int16_t kCloudRingTiltFloor = 80;

// LES DEUX STYLES, et c'est l'octet du site qui tranche : UN pour les octets
// zéro et un, DEUX pour les deux autres.
//
// ⚠ LE STYLE DÉCIDE DE TOUT LE MOUVEMENT : le plafond du balayage, le moment où
// l'alpha tombe, et si le rayon croît. Les confondre figerait l'anneau ouvert à
// cent vingt degrés au lieu de cent quatre-vingts.
inline constexpr std::int16_t kCloudRingSlowStyle = 1;
inline constexpr std::int16_t kCloudRingWideStyle = 2;

// UN ANNEAU DE NUAGE qu'un effet pose.
// LEQUEL DES DEUX BÂTISSEURS POSE CE NŒUD.
//
// ⚠ DEUX FONCTIONS POSENT LE SOUS-TYPE 62, et elles ne sèment pas de la même
// façon. `kCloudRings4` reçoit un OCTET de son site, dont quatre formules
// tirent la clé, le rayon et le style — deux de ses quatre rayons se TIRENT au
// sort. `kHealCloud` ne reçoit rien : ses quatre pistes sont écrites en toutes
// lettres, sans un seul tirage, et leurs quatre valeurs descendent
// régulièrement. Lui appliquer les formules de l'octet donnerait un nuage qui
// ne ressemble à rien de ce que le client montre. ⚠ ET UN TROISIÈME, qui ne
// ressemble à ni l'un ni l'autre : `kVioletRedCloud` reçoit un STYLE — zéro ou
// trois — qui ne choisit QUE la teinte ; ses quatre pistes sont les mêmes pour
// les deux, en toutes lettres et sans un tirage.
enum class EffectCloudRingBuilder : std::uint8_t {
  kCloudRings4,
  kHealCloud,
  kVioletRedCloud,
};

struct EffectCloudRingPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::string_view texture;
  // L'OCTET DU SITE, de zéro à trois : il choisit le style, la clé, le rayon et
  // la variante de palette.
  //
  // ⚠ IL NE VEUT RIEN DIRE POUR LE NUAGE DE SOIN, dont le site ne passe aucun
  // octet. Le champ reste à zéro, et `builder` dit de ne pas le lire.
  std::uint8_t style_byte = 0;
  // LA VARIANTE DE PALETTE, que le client range dans le nœud.
  std::uint8_t trail_variant = 5;
  EffectCloudRingBuilder builder = EffectCloudRingBuilder::kCloudRings4;
  // ⚠ QUAND cet effet pose CE nuage-ci. Les deux premiers bâtisseurs sont
  // appelés une fois, au tic zéro ; le troisième est rappelé à chaque tic, et
  // sa garde est une ÉGALITÉ — il pose son nœud de sous-type soixante au tic
  // zéro, celui du soixante-trois au tic UN et celui-ci au tic DEUX. Le semer
  // d'emblée le ferait naître avant les anneaux qu'il enveloppe.
  EffectNodeSchedule schedule;
};

// CE QUE LE NUAGE DE SOIN SÈME, rang par rang — tout en constantes.
//
// Son balayage se resserre de vingt degrés par rang, son azimut s'ouvre de dix,
// son rayon croît de deux dixièmes et la longueur de ses segments retombe d'un
// demi. Quatre suites régulières, et aucune n'est celle de son frère.
inline constexpr std::int16_t kHealCloudFirstSweepDegrees = 90;
inline constexpr std::int16_t kHealCloudSweepFallPerRank = 20;
inline constexpr std::int16_t kHealCloudAzimuthPerRank = 10;
inline constexpr float kHealCloudLiftKey = 8.0F;
inline constexpr float kHealCloudFirstRadius = 3.0F;
inline constexpr float kHealCloudRadiusGrowthPerRank = 0.2F;
inline constexpr float kHealCloudFirstSegmentLength = 3.0F;
inline constexpr float kHealCloudSegmentFallPerRank = 0.5F;
inline constexpr std::int16_t kHealCloudTiltDegrees = 90;

// CE QUE LE NUAGE VIOLET ET ROUGE SÈME, rang par rang — tout en constantes, lui
// aussi, et sans un seul tirage.
//
// Sa clé monte de deux par rang, son rayon retombe d'un et demi : les quatre
// pistes sont donc quatre anneaux emboîtés, le plus large en bas. Son balayage
// de départ et son écart d'azimut sont ceux de la famille.
//
// 🔴 SON STYLE EST ZÉRO, ET C'EST UN TROISIÈME STYLE : ni le lent, ni le large.
// Le mouvement ne le traite que par exclusion — plafond de balayage bas, aucune
// chute d'alpha au vingtième tic, aucune croissance de rayon —, si bien qu'il
// ne lui reste QUE la descente tardive du mille quatre centième tic. C'est elle
// qui éteint la nappe du portail au bout de vingt-quatre secondes, et ce nœud est
// le seul du dépôt qui vive assez longtemps pour l'atteindre.
inline constexpr std::int16_t kVioletRedCloudStyle = 0;
inline constexpr float kVioletRedCloudFirstLiftKey = 1.5F;
inline constexpr float kVioletRedCloudLiftKeyPerRank = 2.0F;
inline constexpr float kVioletRedCloudFirstRadius = 9.0F;
inline constexpr float kVioletRedCloudRadiusFallPerRank = 1.5F;
inline constexpr float kVioletRedCloudSegmentLength = 1.5F;
inline constexpr std::int16_t kVioletRedCloudTiltDegrees = 90;

[[nodiscard]] std::span<const EffectCloudRingPattern> EffectCloudRingPatternTable();

[[nodiscard]] std::span<const EffectCloudRingPattern> EffectCloudRingPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES d'un nœud, toutes vives et à quatre-vingt-dix degrés
// d'écart.
//
// ⚠ LES OCTETS ZÉRO ET UN TIRENT DEUX FOIS par piste — leur rayon puis leur
// inclinaison —, les deux autres UNE SEULE. Les uniformiser décalerait la suite
// du générateur.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectCloudRingTracks(
    const EffectCloudRingPattern& pattern, std::uint32_t& generator);

// LE NŒUD qu'un patron décrit, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectCloudRingNode(const EffectCloudRingPattern& pattern,
                                                      glm::vec3 anchor,
                                                      std::int32_t effect_life_ticks,
                                                      std::uint32_t& generator);

// ---------------------------------------------------------------------------
// LE MOUVEMENT — l'anneau qui S'OUVRE
// ---------------------------------------------------------------------------

// DE COMBIEN LE BALAYAGE S'OUVRE par tic, et où il bute selon le style.
//
// ⚠ C'EST CE QUI FAIT L'EFFET : le ruban part d'un arc de trente degrés et se
// déroule. Le prendre pour une constante montrerait un arc figé.
inline constexpr std::int16_t kCloudRingSweepPerTick = 3;
inline constexpr std::int16_t kCloudRingWideSweepCap = 180;
inline constexpr std::int16_t kCloudRingSlowSweepCap = 120;

// DE COMBIEN L'AZIMUT TOURNE par tic.
inline constexpr std::int16_t kCloudRingSpinPerTick = 5;

// LA MONTÉE DE L'ALPHA, sous le douzième tic, et ses deux descentes.
inline constexpr std::int32_t kCloudRingRiseUntilTick = 12;
inline constexpr std::int16_t kCloudRingWideRise = 1;
inline constexpr std::int16_t kCloudRingSlowRise = 10;
inline constexpr std::int16_t kCloudRingSlowRiseCap = 250;
inline constexpr std::int32_t kCloudRingWideFallAfterTick = 50;
inline constexpr std::int32_t kCloudRingSlowFallAfterTick = 20;
inline constexpr std::int16_t kCloudRingWideFall = 1;
inline constexpr std::int16_t kCloudRingSlowFall = 2;

// ⚠ ET UNE TROISIÈME DESCENTE ATTEND LE MILLE QUATRE CENTIÈME TIC, pour tout
// style sauf le TROIS. Aucun nœud de cette famille ne vit aussi longtemps ; c'est
// le second bâtisseur qui s'en sert, avec son style mille quatre cents. Le
// retirer ferait mentir la figure le jour où il arrivera.
inline constexpr std::int32_t kCloudRingLateFallAfterTick = 1400;
inline constexpr std::int16_t kCloudRingLateFall = 3;
inline constexpr std::int16_t kCloudRingStyleWithoutLateFall = 3;

// DE COMBIEN LE RAYON CROÎT, et à partir de quand.
inline constexpr float kCloudRingRadiusPerTick = 0.1F;

void AdvanceCloudRingTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks);

}  // namespace ro::render
