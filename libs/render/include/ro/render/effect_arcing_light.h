#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LA LUEUR BLANCHE QUI PASSE EN ARC, sous-type 109 : la figure et le mouvement.
//
// Le mouvement et la figure sont propres à ce sous-type : aucun autre ne les
// emploie. Un seul effet la pose, le 503, que vingt-quatre compétences jouent
// sur leur cible.
//
// FIGURE — pour chaque piste allumée, un carré à plat de demi-diagonale le
// rayon de la piste, ses coins aux degrés 45, 135, 225 et 315, redressé vers la
// caméra par les deux angles de panneau, puis DÉPORTÉ : d'un écart latéral le
// long du tour de la caméra, et d'une hauteur. Il sort par la voie A — alpha de
// la piste, mode de mélange zéro (l'addition), image zéro, coordonnées
// relatives au nœud, bleu pâle, et l'image retournée.
//
// MOUVEMENT — le cas 109 du répartiteur des nœuds, écrit en ligne. La lueur
// s'allume en dix tics, passe de la GAUCHE à la DROITE du porteur, vue de la
// caméra, en ondulant autour de quarante unités de haut, s'arrête à droite, puis
// pâlit à partir du cent soixante et onzième tic.
//
// ⚠ UNE SEULE PISTE EST ALLUMÉE, la première ; le mouvement et la figure
// parcourent pourtant les quatre, et ne jugent que l'allumage de chacune.

// LES CASES QUE LE BÂTISSEUR, LE MOUVEMENT ET LA FIGURE PARTAGENT. Voir
// `effect_track.h` : ce sont leurs lectures qui leur donnent un sens.
//
// L'identifiant de l'acteur que la lueur désigne, rangé en flottant. Voir
// `ArcingLightSowsFollowUp`.
inline constexpr std::size_t kArcingLightSlotOfDesignatedActor = 10;
// L'ALPHA, en FLOTTANT : il monte et descend par pas entiers ou non, et la
// figure le tronque vers zéro au moment de peindre.
inline constexpr std::size_t kArcingLightSlotOfAlpha = 13;
// Deux cases posées à zéro que personne ne relit, voisines de l'alpha.
inline constexpr std::size_t kArcingLightSlotOfUnreadFirst = 14;
inline constexpr std::size_t kArcingLightSlotOfUnreadSecond = 15;
// L'ANGLE DE LA COURSE, en degrés, dans la queue de la piste.
inline constexpr std::size_t kArcingLightSlotOfSweepDegrees = 0;
// L'ÉCART LATÉRAL, la hauteur (repère DESCENDANT du client) et une profondeur
// toujours nulle, dans la queue de la piste — recalculés à chaque tic.
inline constexpr std::size_t kArcingLightSlotOfSideways = 3;
inline constexpr std::size_t kArcingLightSlotOfDepthBelow = 4;
inline constexpr std::size_t kArcingLightSlotOfForward = 5;

// L'ALPHA : vingt de plus par tic jusqu'au dixième — il atteint deux cents —,
// puis rien, puis cinq de moins par tic passé le cent soixante-dixième, sans
// descendre sous zéro : il s'éteint au deux cent dixième tic, après quarante
// tics de pâleur.
inline constexpr std::int32_t kArcingLightRisesUntilTick = 10;
inline constexpr float kArcingLightAlphaRise = 20.0F;
inline constexpr std::int32_t kArcingLightFadesAfterTick = 170;
inline constexpr float kArcingLightAlphaFall = 5.0F;

// LA COURSE : l'angle gagne `tic / 10 / 3` degrés par tic — DEUX divisions
// flottantes, dans cet ordre —, et s'arrête à trois cent cinquante-neuf. Il y
// arrive au cent quarante-sixième tic.
inline constexpr float kArcingLightSweepFirstDivisor = 10.0F;
inline constexpr float kArcingLightSweepSecondDivisor = 3.0F;
inline constexpr float kArcingLightSweepCeilingDegrees = 359.0F;

// L'ÉCART LATÉRAL, depuis l'angle TRONQUÉ : `angle / 6 − 30`, de moins trente
// à presque trente.
inline constexpr float kArcingLightSidewaysDivisor = 6.0F;
inline constexpr float kArcingLightSidewaysStart = 30.0F;
// LA HAUTEUR, dans le repère du client : `−sin(angle) · 20 − 40`, soit quarante
// unités AU-DESSUS du nœud, vingt de plus au quatre-vingt-dixième degré et
// vingt de moins au deux cent soixante-dixième.
inline constexpr float kArcingLightHeightSwing = 20.0F;
inline constexpr float kArcingLightHeightBase = 40.0F;

// LE CARRÉ DE DÉPART : quarante-cinq degrés, puis un quart de tour par coin.
inline constexpr std::int32_t kArcingLightFirstCornerDegrees = 45;
inline constexpr std::int32_t kArcingLightDegreesPerCorner = 90;

// LA TEINTE poussée en dur, rouge, vert, bleu : un bleu pâle.
inline constexpr std::array<std::uint8_t, 3> kArcingLightColor = {125, 125, 255};

// LE SEMIS D'UN EFFET QUI SUIT : tant que la piste n'a pas cent quatre-vingts
// tics, CHAQUE PEINTURE du carré fait naître l'effet 502 sur l'acteur que la
// lueur désigne — un clignotement du corps, voir `actor_effect_table.h`.
inline constexpr std::int32_t kArcingLightFollowUpUntilTick = 180;
inline constexpr std::uint32_t kArcingLightFollowUpEffectId = 502;

// LE MOUVEMENT DU SOUS-TYPE 109, d'un tic.
//
// ⚠ LES DEUX ANGLES DE PANNEAU SE RANGENT D'ABORD, hors de la boucle et sur les
// deux premières pistes même éteintes. Ils arrivent ici déjà complétés, et se
// rangent tels quels.
//
// Puis, pour chaque piste ALLUMÉE : le compteur avance, et tant qu'il n'est pas
// positif la piste ne fait rien d'autre ; l'alpha monte ou pâlit ; l'angle de
// la course avance ; l'écart latéral et la hauteur se RÉÉCRIVENT depuis lui.
void AdvanceArcingLightTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                              std::int16_t billboard_turn_degrees,
                              std::int16_t billboard_tilt_degrees);

// LE QUAD D'UNE PISTE, ou rien.
//
// `turn_degrees` et `tilt_degrees` sont les styles des pistes zéro et un, où le
// mouvement les a rangés. `node_origin` est l'origine du nœud dans NOTRE repère.
//
// Rien quand la piste est éteinte, ou quand le PREMIER coin, une fois déporté,
// passe SOUS le nœud — ce que la hauteur de la course ne fait jamais. Aucune
// garde d'alpha : un carré d'alpha nul s'émet, et la sortie le rend invisible.
[[nodiscard]] std::optional<EffectWorldQuad> ComposeArcingLightQuad(const EffectTrack& track,
                                                                    std::int16_t turn_degrees,
                                                                    std::int16_t tilt_degrees,
                                                                    glm::vec3 node_origin);

// VRAI QUAND CETTE PEINTURE DE LA PISTE FAIT NAÎTRE L'EFFET QUI SUIT : la
// piste se peint — voir `ComposeArcingLightQuad` — et son compteur est sous
// `kArcingLightFollowUpUntilTick`.
//
// ⚠ PAR PEINTURE, ET NON PAR TIC : c'est le dessin qui sème, à chaque image.
[[nodiscard]] bool ArcingLightSowsFollowUp(const EffectTrack& track, std::int16_t turn_degrees,
                                           std::int16_t tilt_degrees);

}  // namespace ro::render
