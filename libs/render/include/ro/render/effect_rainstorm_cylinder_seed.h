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

// LE SEMIS DU CYLINDRE DE NOTES, sous-type 173.
//
// UN SEUL bâtisseur et UN SEUL cas : l'effet 1286. Le cas l'appelle QUATRE FOIS
// de suite, sur la même instance, et ne change d'un appel à l'autre que le
// rayon qu'il passe en dernier argument : 3,0, 3,0, 8,0 et 8,0.
//
// ⚠ ET CE SONT BIEN QUATRE NŒUDS, PAS UN SEUL ÉCRIT QUATRE FOIS : la garde du
// bâtisseur est l'âge nul de l'INSTANCE, que les quatre appels d'un même tic
// voient tous nuls. Chacun bâtit son nœud et le range par-dessus le précédent,
// qui vit quand même. Aux tics suivants la garde tombe et le bâtisseur ne fait
// plus que recaler l'instance sur son porteur. Aucune horloge.
//
// ⚠ DEUX DES ARGUMENTS NE SONT LUS PAR PERSONNE. Le cas passe 45 et une
// chaîne ; le bâtisseur ne touche ni l'un ni l'autre. Le troisième, l'octet 3,
// est rangé au style des pistes, que ni le mouvement ni le dessinateur ne
// lisent.

// LE MOT DE RENDU que le bâtisseur écrit en dur. Voir `effect_render_word.h`.
inline constexpr std::uint32_t kRainstormCylinderRenderWord = 5;

// LA VARIANTE DE PALETTE : le bâtisseur écrit 0,0, que la primitive relit en
// entier. Zéro : blanc, en mélange ordinaire. Voir `effect_trail_palette.h`.
inline constexpr std::uint8_t kRainstormCylinderTrailVariant = 0;

// COMBIEN D'IMAGES le nœud porte : `music_01` à `music_09`, numérotées à
// partir de UN. La figure en choisit une par le compteur de ses pistes, modulo
// ce nombre.
inline constexpr std::size_t kRainstormCylinderImageCount = 9;

// CE QUE LE BÂTISSEUR ÉCRIT EN DUR sur les quatre pistes.
inline constexpr std::int16_t kRainstormCylinderSweepDegrees = 120;
inline constexpr float kRainstormCylinderLiftKey = 20.0F;
// L'AZIMUT DE DÉPART de chaque piste — quatre arcs de cent vingt degrés à un
// quart de tour d'écart, qui se recouvrent donc d'un tiers.
inline constexpr std::array<std::int16_t, kEffectTracksPerNode> kRainstormCylinderAzimuths = {
    0, 90, 180, 270};
// L'INCLINAISON de chaque piste.
inline constexpr std::array<std::int16_t, kEffectTracksPerNode> kRainstormCylinderTilts = {60, 55,
                                                                                           65, 70};

// UN APPEL DU CAS : l'effet, le rayon qu'il passe, l'octet qu'il passe, et le
// plancher que le bâtisseur pose à la vie de l'instance avant de la recopier
// dans son nœud.
struct EffectRainstormCylinderPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  float radius = 0.0F;
  std::uint8_t style = 0;
  std::int32_t instance_life_floor_ticks = 0;
};

// La table entière, triée par identifiant — un patron par APPEL du cas.
[[nodiscard]] std::span<const EffectRainstormCylinderPattern> EffectRainstormCylinderPatternTable();

[[nodiscard]] std::span<const EffectRainstormCylinderPattern> EffectRainstormCylinderPatternsFor(
    std::uint32_t effect_id);

// LES NEUF IMAGES, dans l'ordre où le bâtisseur les charge.
[[nodiscard]] std::span<const std::string_view> RainstormCylinderImages();

// LA VIE DU NŒUD : celle de l'effet, relevée au plancher que pose le bâtisseur.
//
// ⚠ LA DURÉE DE LA TABLE DU CLIENT N'EST PAS CONNUE POUR 1286.
// `effect_life_ticks` est alors le défaut du constructeur, et le plancher de
// cent vingt l'emporte.
[[nodiscard]] std::int32_t RainstormCylinderLifeTicks(const EffectRainstormCylinderPattern& pattern,
                                                      std::int32_t effect_life_ticks);

// LES QUATRE PISTES d'un nœud, dans l'ordre du client : allumage, compteur à
// zéro, balayage, clé, azimut, alpha nul, rayon, inclinaison, puis le style.
//
// ⚠ LES VINGT ET UNE LONGUEURS, LES VINGT ET UNE MARQUES ET L'ORDONNÉE DE
// L'ANCRE RESTENT À ZÉRO, comme le bâtisseur les pose : le mouvement réécrit
// les longueurs à chaque tic
// avant le dessin, et personne ne lit les deux autres.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedRainstormCylinderTracks(
    const EffectRainstormCylinderPattern& pattern);

// LE NŒUD qu'un appel décrit, né à la place du porteur.
[[nodiscard]] PooledEffectNode SowRainstormCylinderNode(
    const EffectRainstormCylinderPattern& pattern, glm::vec3 owner_position,
    std::int32_t effect_life_ticks);

}  // namespace ro::render
