#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE RUBAN DOUBLÉ : un anneau debout qui s'ouvre, peint DEUX FOIS.
//
// C'est le dessinateur du sous-type 106, et il sort par la voie B — la primitive de ruban teinté,
// qui choisit sa couleur dans la variante du nœud (voir `effect_trail_palette.h`).
//
// ⚠ IL ÉMET CHAQUE SEGMENT DEUX FOIS, AUX MÊMES COINS ET À LA MÊME TEINTE, et
// c'est de là que vient son nom. Ce n'est pas une redite : le mélange de la
// variante quatre est l'ADDITION, si bien que la seconde passe DOUBLE la
// luminosité du ruban. N'en émettre qu'une donnerait une aura deux fois trop
// pâle, sans qu'aucun test de forme ne s'en aperçoive.
//
// LA FORME. Chaque piste tient UN ANNEAU DEBOUT, dans le plan vertical que
// l'azimut désigne, et le ruban est la bande entre l'anneau d'ouverture `a` et
// celui de `a + 10`. L'ouverture part de zéro et monte jusqu'au quart de tour :
// l'anneau naît en un point et s'épanouit en cercle plein.
//
// ⚠ ET LE CENTRE DE L'ANNEAU GLISSE PENDANT QU'IL S'OUVRE, le long de la
// PERPENDICULAIRE au regard du porteur : le mouvement range ce vecteur dans les
// deux premières cases de queue de la piste. L'écart vaut trois fois ce vecteur,
// plus la clé de la piste fois le cosinus de l'ouverture — donc maximal quand
// l'anneau est un point et nul quand il est plein. C'est ce glissement qui fait
// une COQUE et non un disque.

// L'ÉCART EN DEGRÉS ENTRE LES DEUX BORDS du ruban, et le plafond commun.
inline constexpr std::int32_t kDoubleRibbonEdgeGapDegrees = 10;
inline constexpr std::int32_t kDoubleRibbonOpeningCeilingDegrees = 90;

// EN COMBIEN DE SEGMENTS le balayage se coupe, et sur combien la coordonnée de
// texture se compte — c'est la primitive de la voie B qui divise par vingt et un.
inline constexpr std::int32_t kDoubleRibbonSegments = 20;
inline constexpr float kDoubleRibbonTextureSpan = 21.0F;

// DE COMBIEN LE CENTRE DE L'ANNEAU EST PORTÉ sur la perpendiculaire, avant même
// la part qui dépend de l'ouverture.
inline constexpr float kDoubleRibbonCentreSpread = 3.0F;

// LES DEUX CASES DE QUEUE OÙ LE MOUVEMENT RANGE LA PERPENDICULAIRE. Ce sont
// l'abscisse et la cote d'un vecteur horizontal ; sa hauteur n'existe pas, et la
// case du milieu n'est jamais écrite.
inline constexpr std::size_t kTailOfDoubleRibbonAxisX = 0;
inline constexpr std::size_t kTailOfDoubleRibbonAxisZ = 2;

// LA CASE QUI PORTE L'OUVERTURE DE L'ANNEAU, en degrés et en flottant — le
// mouvement la fait monter d'un degré par tic. Son sens appartient à cette
// figure (ADR 0017).
inline constexpr std::size_t kSlotOfDoubleRibbonOpening = 1;

// LES QUADS D'UNE PISTE : deux par segment, les deux identiques.
//
// `node_origin` est l'origine du nœud dans le monde ; les coins du client lui
// sont relatifs.
//
// Rend un lot VIDE quand la piste est éteinte, quand son alpha est nul ou quand
// son balayage est négatif.
//
// ⚠ ET AUSSI QUAND SON BALAYAGE EST TROP COURT POUR UN PAS. Le client divise le
// balayage par vingt sans se garder : sous vingt degrés son pas vaut zéro et sa
// boucle ne finit jamais. Aucun bâtisseur ne pose un tel balayage — celui du
// sous-type 106 pose le tour entier —, et rendre un lot vide est la seule
// transcription qui ne se fige pas.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeDoubleRibbonQuads(const EffectTrack& track,
                                                                    glm::vec3 node_origin,
                                                                    std::uint8_t trail_variant,
                                                                    std::size_t texture = 0);

}  // namespace ro::render
