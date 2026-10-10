#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LES COURONNES DE MAGNUM BREAK, sous-type 73 : la figure et le mouvement.
//
// Le mouvement et la figure sont propres à ce sous-type, et un seul effet le
// pose : le 339, dix nœuds de deux couronnes, un tous les trois tics.
//
// FIGURE — pour chaque piste allumée d'alpha positif, une couronne de vingt
// segments qui balaie `degrees` depuis son azimut : le bord intérieur sur le
// cercle de rayon `radius_at_anchor`, trois unités au-dessus du nœud ; le bord
// extérieur s'en écarte de la clé de la piste, inclinée de `tilt_degrees` — elle
// l'éloigne de `clé·cos` et le lève de `clé·sin`. Chaque segment sort par la
// primitive de ruban, qui choisit la teinte dans sa palette sur la variante du
// nœud.
//
// ⚠ CE N'EST PAS LA JUPE D'ANNEAU : la jupe prend une longueur PAR SEGMENT et
// ne se relève que pour deux clés exactes ; celle-ci prend UNE clé pour toute la
// couronne, et se relève toujours de trois.
//
// MOUVEMENT — le cas 73 du répartiteur des nœuds, en ligne. Le nœud suit son
// porteur ; pour chaque piste allumée, le compteur avance, l'inclinaison est
// ramenée à zéro si elle est négative, l'azimut replié sur le tour ; jusqu'au
// huitième tic l'alpha monte — de dix jusqu'à quatre-vingts pour le style zéro,
// de cinq jusqu'à quarante sinon —, puis il perd trente par tic et la piste
// MEURT sous zéro.

// LE STYLE qui choisit le rythme de l'alpha, et ses deux montées.
inline constexpr std::int16_t kBreakRingBrightStyle = 0;
inline constexpr std::int32_t kBreakRingRisesBeforeTick = 9;
inline constexpr std::int16_t kBreakRingBrightRise = 10;
inline constexpr std::int16_t kBreakRingBrightCeiling = 80;
inline constexpr std::int16_t kBreakRingDimRise = 5;
inline constexpr std::int16_t kBreakRingDimCeiling = 40;
inline constexpr std::int16_t kBreakRingAlphaFall = 30;

// LA FIGURE : combien de segments, et de combien le bord intérieur se lève
// (dans NOTRE repère, qui monte).
inline constexpr std::int32_t kBreakRingSegments = 20;
inline constexpr float kBreakRingLift = 3.0F;

// LE MOUVEMENT DU SOUS-TYPE 73, d'un tic.
void AdvanceBreakRingTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks);

// LES QUADS D'UNE PISTE, posés autour de `anchor` (NOTRE repère), sous la
// variante de palette `variant` du nœud. Vide quand la piste est éteinte, que
// son alpha n'est pas positif ou que son balayage est trop court pour avancer.
void ComposeBreakRingQuads(const EffectTrack& track, std::uint8_t variant, glm::vec3 anchor,
                           std::vector<EffectWorldQuad>& quads);

}  // namespace ro::render
