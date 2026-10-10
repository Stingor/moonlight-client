#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// L'ARC QUI SERPENTE : un tube à TROIS FACES qui part de l'ancre, bombe d'un
// côté puis de l'autre, et se raccourcit de segment en segment.
//
// La figure sort par la voie A, et son mouvement est le cas 70 du répartiteur.
// TROIS identifiants l'emploient, tous des arcs électriques.
//
// ⚠ SA PISTE EST UN HISTORIQUE, et c'est ce qui fait la figure. Les vingt et
// une valeurs portent la PHASE de chaque segment et les vingt et une marques
// leur RANG ; à chaque tic, tout le tableau se décale d'un cran et un nouveau
// couple entre en tête. Lire la piste comme vingt et un champs indépendants
// donnerait vingt segments figés au lieu d'un trait qui court.
//
// ⚠ ET LA LONGUEUR DÉCROÎT EN PROGRESSION GÉOMÉTRIQUE : chaque rang vaut sept
// dixièmes du précédent. Le vingtième segment mesure donc moins d'un millième
// du premier — l'arc s'éteint en pointe, il ne se coupe pas.

// COMBIEN DE SEGMENTS l'arc porte, et combien de faces son tube compte.
inline constexpr std::size_t kSnakingArcSegments = 20;
inline constexpr std::size_t kSnakingArcFaces = 3;
inline constexpr std::int32_t kSnakingArcFaceStepDegrees = 120;

// CE QUE CHAQUE RANG GARDE DU PRÉCÉDENT.
//
// Le client écrit ce flottant tel quel, et non sept dixièmes exacts : il est
// employé une vingtaine de fois par segment, et l'écart s'accumule.
inline constexpr float kSnakingArcDecay = 0.69999999F;

// DE COMBIEN L'ARC BOMBE, en part de la longueur d'un segment.
inline constexpr float kSnakingArcBowScale = 1.5F;

// LA PHASE D'UN SEGMENT : sur quel demi-tour elle court, et à quelle fraction
// de ce demi-tour la longueur d'un segment est atteinte.
//
// ⚠ CE N'EST PAS UN ANGLE DU MONDE mais une horloge : le sinus en tire le
// bombement, et le quotient la part parcourue.
inline constexpr float kSnakingArcPhaseFullDegrees = 180.0F;
inline constexpr float kSnakingArcPhaseHalfDegrees = 90.0F;

// DE COMBIEN L'ALPHA ET LA TEINTE CHANGENT D'UN SEGMENT AU SUIVANT.
//
// ⚠ L'ALPHA TOMBE DE CINQ PAR SEGMENT, et un segment dont l'alpha passe à zéro
// N'EST PAS DESSINÉ : c'est ce qui fait que l'arc se voit sur d'autant moins de
// segments qu'il est déjà pâle.
inline constexpr std::int16_t kSnakingArcAlphaPerSegment = 5;
inline constexpr std::uint8_t kSnakingArcTintPerSegment = 7;

// LES TROIS STYLES QUE LE DESSINATEUR NOMME, et les trois sites les emploient
// tous les trois.
inline constexpr std::int16_t kSnakingArcPinkStyle = 2;
inline constexpr std::int16_t kSnakingArcBlueStyle = 8;

// LES PLANCHERS DE TEINTE, au premier segment.
inline constexpr std::uint8_t kSnakingArcPinkFloor = 60;
inline constexpr std::uint8_t kSnakingArcOtherFloor = 10;
inline constexpr std::uint8_t kSnakingArcAmberGreen = 200;

// LA TEINTE d'un segment, selon le style de la piste.
[[nodiscard]] std::array<std::uint8_t, 3> EffectSnakingArcColor(std::int16_t style,
                                                                std::size_t segment);

// OÙ LE BOUT D'UN SEGMENT SE TROUVE, dans NOTRE repère et relativement à
// l'ancre de la piste.
//
// `segment` est le rang dans l'historique : zéro est la tête, vingt la queue.
[[nodiscard]] glm::vec3 EffectSnakingArcJoint(const EffectTrack& track, std::size_t segment);

// LE TUBE D'UNE PISTE : vingt segments de trois faces, moins ceux que l'alpha
// a déjà éteints.
//
// Rend un lot VIDE quand la piste est éteinte, quand son alpha n'est pas
// strictement positif, ou quand son compteur n'est pas encore positif.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeSnakingArcQuads(
    const EffectTrack& track, const EffectOwnerTint& owner = {});

// LE MOUVEMENT DU SOUS-TYPE 70 : la phase avance, et tout l'historique recule.
//
// ⚠ L'ORDRE COMPTE. Le client fait avancer la tête AVANT de décaler, si bien
// que la tête et le premier cran portent la même phase pendant un tic.
// Décaler d'abord perdrait un cran à chaque tour.
void AdvanceSnakingArcTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks);

}  // namespace ro::render
