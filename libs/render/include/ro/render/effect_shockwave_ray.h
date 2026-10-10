#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LES RAIS DE L'ONDE DE CHOC, sous-type 54 : la figure et le mouvement.
//
// Le mouvement et la figure sont propres à ce sous-type, et un seul effet le
// pose : le 300, deux nœuds de quatre pistes, chaque piste un rai.
//
// FIGURE — pour chaque piste allumée d'alpha positif, DEUX triangles fins du
// centre du nœud jusqu'au rayon de la piste, à son angle : un premier de trois
// degrés de part et d'autre, blanc, à la MOITIÉ de l'alpha ; un second de cinq
// degrés, bleu pâle, à l'alpha entier. Tous deux couchés dans le plan de la
// piste puis redressés vers la caméra par les deux angles de panneau, et peints
// en addition. Le client les passe comme des quads dont le premier et le
// dernier coin sont confondus au centre.
//
// MOUVEMENT — le cas 54 du répartiteur des nœuds. Les deux angles de panneau se
// rangent d'abord dans les styles des pistes zéro et un ; puis, pour chaque
// piste allumée, le compteur avance, l'alpha monte de dix-huit jusqu'au dixième
// tic, tient jusqu'au vingtième, puis descend de quinze sans passer sous zéro.
// ⚠ LA PISTE NE MEURT PAS à zéro : elle reste allumée et invisible jusqu'à la
// fin du nœud.

// L'ALPHA : dix-huit de plus par tic jusqu'au dixième — il atteint cent
// quatre-vingts, sans plafond —, rien jusqu'au vingtième, puis quinze de moins.
inline constexpr std::int32_t kShockwaveRayRisesUntilTick = 10;
inline constexpr std::int16_t kShockwaveRayAlphaRise = 18;
inline constexpr std::int32_t kShockwaveRayFadesAfterTick = 20;
inline constexpr std::int16_t kShockwaveRayAlphaFall = 15;

// LES DEUX TRIANGLES : leur demi-ouverture en degrés, la part de l'alpha et la
// teinte de chacun.
inline constexpr std::int32_t kShockwaveRayNarrowHalfDegrees = 3;
inline constexpr std::int32_t kShockwaveRayWideHalfDegrees = 5;
inline constexpr float kShockwaveRayNarrowAlphaShare = 0.5F;
inline constexpr std::array<std::uint8_t, 3> kShockwaveRayNarrowColor = {255, 255, 255};
inline constexpr std::array<std::uint8_t, 3> kShockwaveRayWideColor = {130, 130, 255};

// LE MOUVEMENT DU SOUS-TYPE 54, d'un tic.
void AdvanceShockwaveRayTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                               std::int16_t billboard_turn_degrees,
                               std::int16_t billboard_tilt_degrees);

// LES QUADS D'UNE PISTE : deux — l'étroit puis le large —, ou aucun quand la
// piste est éteinte ou que son alpha n'est pas positif.
//
// `turn_degrees` et `tilt_degrees` sont les styles des pistes zéro et un, où le
// mouvement les a rangés. `node_origin` est l'origine du nœud dans NOTRE
// repère ; les coins lui sont relatifs.
void ComposeShockwaveRayQuads(const EffectTrack& track, std::int16_t turn_degrees,
                              std::int16_t tilt_degrees, glm::vec3 node_origin,
                              std::vector<EffectWorldQuad>& quads);

}  // namespace ro::render
