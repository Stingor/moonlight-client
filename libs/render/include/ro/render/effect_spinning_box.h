#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LA PETITE BOÎTE QUI TOURNE, sous-type 114 : un pavé de six faces, carré en
// plan, qui tourne sur lui-même au-dessus de son porteur.
//
// Ce qu'une piste vive porte, et ce que la figure en fait :
//
//   · l'AZIMUT tourne la boîte, un degré par tic, et la fait BALANCER : le plan
//     de ses coins se tient à la clé de hauteur plus deux fois le sinus de
//     l'azimut ;
//   · l'INCLINAISON ne penche rien — elle fait RESPIRER la boîte : le rayon de
//     ses coins va de huit à dix dixièmes du rayon à l'ancre, au rythme de son
//     sinus ;
//   · le RAYON À L'ANCRE est aussi sa HAUTEUR, la boîte n'est donc pas un cube ;
//   · la MARQUE teint le rouge et le vert, de zéro à trente : la boîte est BLEUE.
//
// 🔴 CHAQUE FACE A SON BLEU, et c'est ce qui la fait lire comme un volume. Deux
// cent cinquante pour les deux faces carrées, puis deux cent vingt, cent
// quatre-vingt-dix, cent trente et cent soixante pour les quatre côtés, dans
// l'ordre des coins. Un seul bleu en ferait une silhouette plate.
//
// ⚠ L'ALPHA MONTE DE DEUX PAR TIC JUSQU'À CENT QUARANTE, et s'y tient : la boîte
// n'est jamais opaque, et elle s'ADDITIONNE à ce qui est derrière.

// Les cases de la piste que la figure lit.
inline constexpr std::size_t kSpinningBoxSlotOfLift = 0;
inline constexpr std::size_t kSpinningBoxMarkOfRedGreen = 0;

// Combien de faces, et le bleu de chacune dans l'ordre où le client les peint :
// le carré des coins, les quatre côtés, le carré du haut.
inline constexpr std::size_t kSpinningBoxFaces = 6;
inline constexpr std::array<std::uint8_t, kSpinningBoxFaces> kSpinningBoxFaceBlues = {
    250, 220, 190, 130, 160, 250};

// Le balancement : deux fois le sinus de l'azimut.
inline constexpr float kSpinningBoxBobAmplitude = 2.0F;
// La respiration du rayon : un plancher de huit dixièmes, un dixième de plus
// par unité de `1 + sinus`.
inline constexpr float kSpinningBoxRadiusFloorShare = 0.8F;
inline constexpr float kSpinningBoxRadiusWaveShare = 0.1F;

// Ce que le mouvement fait par tic.
inline constexpr std::int16_t kSpinningBoxAlphaStep = 2;
inline constexpr std::int16_t kSpinningBoxAlphaCeiling = 140;
inline constexpr std::int16_t kSpinningBoxTurnStepDegrees = 1;

// LES SIX FACES d'une piste vive, relatives à l'origine du nœud.
//
// ⚠ DANS NOTRE REPÈRE : la clé de hauteur et le balancement du client comptent
// vers le BAS, la profondeur à l'opposé. Le carré du haut est donc à la clé
// retournée, plus le rayon.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeSpinningBoxQuads(const EffectTrack& track,
                                                                   glm::vec3 node_origin);

// UN TIC : l'alpha monte, l'inclinaison et l'azimut avancent d'un degré et
// reviennent à zéro au tour.
void AdvanceSpinningBoxTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks);

}  // namespace ro::render
