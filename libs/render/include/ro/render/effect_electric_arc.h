#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LA DISPERSION D'ÉCLAIRS : un arc rouge qui part du porteur, et trois nappes
// qui le rattachent à trois arcs fantômes.
//
// La figure sort par la voie A, et le mouvement est celui du sous-type 135. UN
// identifiant l'emploie, l'effet 794 — le balayage infrarouge du Mechanic —,
// qui en pose DEUX nœuds.
//
// ⚠ C'EST LE JOINT DE L'ÉCLAIR DU SOUS-TYPE 101, PAS SA FIGURE. Les deux
// dessinateurs tirent le même tremblement, posent les mêmes joints et le même
// tube à trois faces — `EffectLightningFanJoint` et `DrawLightningFanJitters`
// servent aux deux. Mais celui-ci :
//
//   - ne peint le TUBE que de la PISTE ZÉRO en ROUGE —
//     le rouge plein, le vert et le bleu montant de trois par segment ;
//   - fait des trois autres pistes des NAPPES : un quad par segment, du bord de
//     la piste au bord de la piste zéro, rouge pur, à l'alpha de la piste
//     divisé par trois, par deux, puis entier.
//
// Porter la figure de l'éventail telle quelle aurait peint quatre éclairs bleus
// là où le client en peint un rouge et trois voiles.
//
// ⚠ ET CETTE FIGURE CALCULE DANS LE REPÈRE DU CLIENT, puis retourne le tout
// d'un coup (`EffectClientOffsetInWorld`) autour de l'ancre, qui est dans le
// nôtre. L'arc part d'un cap tiré du PORTEUR : le retourner à moitié le ferait
// partir en miroir de ce qu'il regarde.

// LA VALEUR DE BALAYAGE QUI REND LA TEINTE DU TUBE ALÉATOIRE. Le bâtisseur de
// l'effet 794 pose zéro : c'est le dégradé qui se voit.
inline constexpr std::int16_t kElectricArcRandomColorSweep = 3;

// LE DÉGRADÉ DU TUBE : le rouge plein, et de combien le vert et le bleu montent
// par segment.
inline constexpr std::uint8_t kElectricArcRed = 255;
inline constexpr std::uint8_t kElectricArcGradientPerSegment = 3;

// LA TEINTE ALÉATOIRE DU TUBE : le bleu se tire EN PREMIER, sur vingt-six crans
// au-dessus de cent ; le vert ensuite, sur cinquante et un.
inline constexpr std::int32_t kElectricArcRandomBlueSpread = 26;
inline constexpr std::int32_t kElectricArcRandomBlueFloor = 100;
inline constexpr std::int32_t kElectricArcRandomGreenSpread = 51;

// LA PISTE DONT LE TUBE SE PEINT, et à laquelle les trois autres se rattachent.
inline constexpr std::size_t kElectricArcLeadTrack = 0;

// DE COMBIEN L'ALPHA D'UNE NAPPE EST DIVISÉ, par rang de piste. La piste de
// tête n'a pas de nappe ; la quatrième la peint à son alpha entier.
inline constexpr std::array<std::int16_t, 4> kElectricArcSheetAlphaDivisor = {1, 3, 2, 1};

// OÙ LE BOUT D'UN SEGMENT SE TROUVE, en face `face_degrees` du tube, dans le
// repère du CLIENT et relativement à l'ancre de la piste.
//
// `plan_degrees` est le cap du plan du tube, `jitter` le tremblement du joint.
[[nodiscard]] glm::vec3 ElectricArcRimPoint(const EffectTrack& track, std::size_t segment,
                                            glm::vec3 jitter, std::int32_t plan_degrees,
                                            std::int32_t face_degrees);

// LES QUADS D'UN NŒUD, ses quatre pistes ensemble.
//
// ⚠ LES QUATRE PISTES SE COMPOSENT ENSEMBLE, et dans l'ordre : les nappes des
// pistes un à trois se tendent vers le bord que la piste zéro vient de poser.
// Une piste composée seule n'aurait rien vers quoi se tendre.
//
// ⚠ `generator` AVANCE : trois tirages par segment de CHAQUE piste, vive ou
// non, et deux de plus par face du tube quand sa teinte est aléatoire.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeElectricArcQuads(
    const std::array<EffectTrack, kEffectTracksPerNode>& tracks, std::uint32_t& generator,
    const EffectOwnerTint& owner = {});

// LE MOUVEMENT DU SOUS-TYPE 135 : l'arc suit son porteur, tourne, et s'éteint.
//
// Pour chaque piste vive, dans cet ordre : le compteur avance ; l'ancre se
// RECOPIE du porteur, dix unités plus haut — même en retard ; puis, compteur
// positif : le cap tourne de deux degrés par tic (dans un sens ou l'autre selon
// le style) une fois passé dix tics par rang ; passé le soixantième tic l'alpha
// perd six ; et au premier tic l'historique s'écrit d'un coup, comme celui de
// l'éclair du 101.
//
// `owner_position` est la place du porteur, dans NOTRE repère.
void AdvanceElectricArcTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                              glm::vec3 owner_position);

}  // namespace ro::render
