#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include <glm/glm.hpp>

#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// L'ÉMETTEUR EN SPIRALE, sous-type 151 : le mouvement et ce qu'il fait naître.
//
// Deux effets le posent, 849 et 931. Il ne dessine AUCUN quad : sa « figure »
// fait naître, à chaque peinture, un autre effet sur l'acteur qu'il désigne —
// 850 pour l'octet zéro, 932 sinon —, à un écart qui décrit une SPIRALE autour
// de cet acteur, vingt-cinq unités au-dessus de lui. Les lueurs laissées
// derrière dessinent donc un cercle qui s'élargit lentement.
//
// ⚠ L'ACTEUR DÉSIGNÉ N'EST PAS CELUI QUI PORTE L'INSTANCE : c'est celui que la
// suite de la compétence nomme. Voir `EffectSpawnRequest`.

// LES CASES DU SEMIS, DU MOUVEMENT ET DE L'ÉMISSION.
inline constexpr std::size_t kSpiralEmitterSlotOfDesignatedActor = 10;
// L'ALPHA, en flottant. Il ne se peint pas : il ne sert qu'à borner l'émission.
inline constexpr std::size_t kSpiralEmitterSlotOfAlpha = 13;
inline constexpr std::size_t kSpiralEmitterSlotOfUnreadFirst = 14;
inline constexpr std::size_t kSpiralEmitterSlotOfUnreadSecond = 15;
// LE RAYON DE LA SPIRALE, flottant, dans la queue de la piste.
inline constexpr std::size_t kSpiralEmitterSlotOfSpiralRadius = 0;
// L'ÉCART DE L'EFFET QUI NAÎT, dans le repère DESCENDANT du client.
inline constexpr std::size_t kSpiralEmitterSlotOfOffsetX = 3;
inline constexpr std::size_t kSpiralEmitterSlotOfOffsetDown = 4;
inline constexpr std::size_t kSpiralEmitterSlotOfOffsetZ = 5;
// L'OCTET DU SITE, dans la première marque.
inline constexpr std::size_t kSpiralEmitterMarkOfByte = 0;

// L'ALPHA : vingt de plus par tic jusqu'au dixième, puis cinq de moins par tic
// passé le deux cent soixante-dixième pour l'octet zéro, passé le
// quatre-vingt-dixième pour les autres, sans descendre sous zéro.
inline constexpr std::int32_t kSpiralEmitterRisesUntilTick = 10;
inline constexpr float kSpiralEmitterAlphaRise = 20.0F;
inline constexpr float kSpiralEmitterAlphaFall = 5.0F;
inline constexpr std::int32_t kSpiralEmitterFadesAfterTickOfByteZero = 270;
inline constexpr std::int32_t kSpiralEmitterFadesAfterTickOfOthers = 90;

// LA SPIRALE : le rayon gagne cinq centièmes par tic et se lit TRONQUÉ ;
// l'angle fait un tour en soixante tics, six degrés par tic, à rebours pour
// l'octet un ; la hauteur est constante.
inline constexpr float kSpiralEmitterRadiusStep = 0.05F;
inline constexpr std::int32_t kSpiralEmitterTicksPerTurn = 60;
inline constexpr std::int32_t kSpiralEmitterDegreesPerTick = 6;
inline constexpr std::uint8_t kSpiralEmitterReversedByte = 1;
inline constexpr float kSpiralEmitterOffsetDown = -25.0F;

// L'ÉMISSION : tant que la piste n'a pas trois cents tics et que son alpha,
// tronqué, est positif.
inline constexpr std::int32_t kSpiralEmitterEmitsUntilTick = 300;
inline constexpr std::uint32_t kSpiralEmitterChildOfByteZero = 850;
inline constexpr std::uint32_t kSpiralEmitterChildOfOthers = 932;

// LE MOUVEMENT DU SOUS-TYPE 151, d'un tic.
//
// Les deux angles de panneau se rangent d'abord dans les styles des pistes
// zéro et un, même éteintes. Puis, pour chaque piste allumée : le compteur
// avance, et tant qu'il n'est pas positif rien d'autre ; l'alpha monte ou
// pâlit ; le rayon croît ; l'écart se RÉÉCRIT depuis le rayon tronqué et
// l'angle du tic.
void AdvanceSpiralEmitterTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                std::int16_t billboard_turn_degrees,
                                std::int16_t billboard_tilt_degrees);

// L'EFFET QU'UNE PEINTURE DE CETTE PISTE FAIT NAÎTRE, ou rien.
//
// Rien quand la piste est éteinte, quand le premier coin de son carré — de
// demi-diagonale son rayon, redressé par les deux angles de panneau puis
// déporté de l'écart — passe SOUS le nœud, quand la piste a trois cents tics,
// ou quand son alpha tronqué n'est pas positif.
[[nodiscard]] std::optional<std::uint32_t> SpiralEmitterChildOf(const EffectTrack& track,
                                                                std::int16_t turn_degrees,
                                                                std::int16_t tilt_degrees);

// L'ÉCART DE L'EFFET QUI NAÎT, dans NOTRE repère : l'ordonnée et la profondeur
// changent de signe.
[[nodiscard]] glm::vec3 SpiralEmitterChildOffset(const EffectTrack& track);

}  // namespace ro::render
