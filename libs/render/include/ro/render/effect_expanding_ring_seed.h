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

// LE SEMIS DE L'ANNEAU QUI S'ÉLARGIT, sous-type 169.
//
// UN SEUL bâtisseur et UN SEUL site : l'effet 939 (EF_BOTTOM_DE2), qui lui
// passe `effect\ring_blue.tga` et l'octet zéro.
//
// ⚠ LE SITE LE RAPPELLE À CHAQUE TIC, ET IL SÈME QUATRE FOIS : tant que l'âge de
// l'instance ne dépasse pas trois. Quatre nœuds, un par tic, chacun né à la
// place que l'instance occupe à ce tic — le bâtisseur la recale sur le porteur à
// CHAQUE appel, avant sa garde. C'est pourquoi l'horloge les rappelle.
//
// ⚠ L'ÂGE ENTRE DANS LE RAYON : chaque nœud part de deux unités plus loin que le
// précédent (`2·âge`). Les quatre nœuds font donc seize anneaux
// étagés de un demi en un demi, de 0,5 à 8.
//
// ⚠ LE BÂTISSEUR PREND QUATRE ARGUMENTS et n'en lit que deux : l'image et
// l'octet. Les deux autres, que le site pose à zéro, ne sont lus nulle part.

// QUAND LE BÂTISSEUR SÈME : à chaque tic, du zéro au troisième.
inline constexpr EffectNodeSchedule kExpandingRingSchedule = {0, 3, 1, 0};

// LE MOT DE RENDU qu'il écrit en dur, cinq. Voir
// `effect_render_word.h`.
inline constexpr std::uint32_t kExpandingRingRenderWord = 5;

// CE QUE LE BÂTISSEUR ÉCRIT EN DUR sur les quatre pistes.
inline constexpr std::int16_t kExpandingRingSweepDegrees = 360;
inline constexpr std::int16_t kExpandingRingTiltAtBirthDegrees = 80;
// LES DEUX TIRAGES : l'azimut sur un tour et l'alpha de naissance sous cent.
inline constexpr std::uint32_t kExpandingRingAlphaDraw = 100;
// LE RAYON : deux unités par tic d'âge, plus une base par piste, puis un écart
// tiré en dixièmes sous vingt et un.
inline constexpr float kExpandingRingRadiusPerAgeTick = 2.0F;
inline constexpr std::uint32_t kExpandingRingJitterDraw = 21;
inline constexpr float kExpandingRingJitterStep = 0.1F;

// LA CLÉ DE CHAQUE PISTE, base du profil ondulé : 15, 16, 17, 18.
inline constexpr std::array<float, kEffectTracksPerNode> kExpandingRingLiftKeys = {15.0F, 16.0F,
                                                                                   17.0F, 18.0F};
// LA BASE DU RAYON DE CHAQUE PISTE, ajoutée à `2·âge` : 0.5, 1.0, 1.5, 2.0.
inline constexpr std::array<float, kEffectTracksPerNode> kExpandingRingBaseRadii = {0.5F, 1.0F,
                                                                                    1.5F, 2.0F};
// ⚠ LE SENS DE L'ÉCART ALTERNE : les pistes 0 et 2 le RETRANCHENT, les pistes 1
// et 3 l'AJOUTENT. Une piste
// sur deux naît donc en deçà de sa base, l'autre au-delà — et celle qui naît plus
// d'une unité au-delà s'efface dès son premier tic.
inline constexpr std::array<float, kEffectTracksPerNode> kExpandingRingJitterSigns = {-1.0F, 1.0F,
                                                                                      -1.0F, 1.0F};

// UN SITE : l'effet, l'image qu'il passe, et l'octet qu'il passe.
//
// ⚠ L'OCTET EST LA VARIANTE DE PALETTE : le bâtisseur le range en flottant, et
// la primitive du ruban le relit en ENTIER pour choisir sa teinte. Zéro : blanc, en
// mélange ordinaire. Voir `effect_trail_palette.h`.
struct EffectExpandingRingPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::string_view texture;
  std::uint8_t trail_variant = 0;
};

[[nodiscard]] std::span<const EffectExpandingRingPattern> EffectExpandingRingPatternTable();

[[nodiscard]] std::span<const EffectExpandingRingPattern> EffectExpandingRingPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES du nœud que le bâtisseur sème à l'âge `age_ticks`.
//
// Par piste, dans l'ordre du client : allumage, compteur à zéro, balayage d'un
// tour, clé, azimut tiré, alpha tiré, rayon de base rangé en QUEUE, puis le
// même rayon écarté du tirage, et l'inclinaison.
//
// ⚠ LES VINGT ET UNE LONGUEURS ET LES VINGT ET UNE MARQUES RESTENT À ZÉRO, comme
// le bâtisseur les pose : le mouvement réécrit les
// longueurs à chaque tic avant le dessin, et personne ne lit les marques.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectExpandingRingTracks(
    std::int32_t age_ticks, std::uint32_t& generator);

// LE NŒUD qu'un patron décrit, semé à `age_ticks` à la place du porteur.
//
// `life_ticks` est la vie qui reste à l'instance : le bâtisseur y recopie la
// sienne, et un nœud meurt avec son instance.
[[nodiscard]] PooledEffectNode SowEffectExpandingRingNode(const EffectExpandingRingPattern& pattern,
                                                          glm::vec3 owner_position,
                                                          std::int32_t age_ticks,
                                                          std::int32_t life_ticks,
                                                          std::uint32_t& generator);

}  // namespace ro::render
