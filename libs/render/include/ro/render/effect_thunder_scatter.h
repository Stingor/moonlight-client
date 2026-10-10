#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <glm/glm.hpp>

#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LES ÉTINCELLES QUI JAILLISSENT DE LA CIBLE : le sous-type 45.
//
// Un mouvement que le répartiteur n'appelle que pour ce sous-type, avant sa
// queue commune. La figure est celle du carré teinté —
// `ComposeTintedRotatedQuad`.
//
// ⚠ RIEN NE « POURSUIT » ICI. Chaque piste prend UN cap, une seule fois —
// celui qui va du nœud vers le point visé, à trente degrés près —, et file
// tout droit le long de lui. Les ancres naissant SUR le point visé,
// les quatre étincelles jaillissent de la cible et continuent dans le sens du
// coup, vers l'arrière de la victime.
//
// Chaque piste attend son tour (compteur négatif), prend son cap au premier tic
// utile, s'allume de quinze par tic pendant neuf tics, puis pâlit de trois par
// tic jusqu'à s'éteindre. Les quatre vivent chacune pour soi : la boucle du
// client les parcourt toutes.

// LE CARRÉ TOURNE À REBOURS de cinq degrés par tic, replié d'un tour quand il
// passe sous zéro. ⚠ AU PREMIER TIC UTILE AUSSI : la rotation précède la
// branche du cap.
inline constexpr std::int16_t kThunderScatterSpinPerTick = -5;

// LE CAP : celui du nœud vers le point visé, plus un écart tiré dans
// [−30, +29] — `rand() % 60 − 30`. ⚠ UN SEUL TIRAGE PAR
// PISTE, au premier tic utile, et aucun autre ensuite.
inline constexpr std::int32_t kThunderScatterHeadingDraws = 60;
inline constexpr std::int32_t kThunderScatterHeadingBias = 30;

// L'ALPHA, jugé sur le compteur D'AVANT l'incrément : plus quinze tant qu'il
// est sous dix, moins trois ensuite. ⚠ AUCUN PLAFOND : neuf pas de quinze
// arrêtent la montée à cent trente-cinq. Sous zéro, l'alpha revient à zéro et
// la piste S'ÉTEINT.
inline constexpr std::int32_t kThunderScatterFirstFadingTick = 10;
inline constexpr std::int16_t kThunderScatterAlphaRise = 15;
inline constexpr std::int16_t kThunderScatterAlphaFall = 3;

// LES CASES QUE LE MOUVEMENT LIT, aux rangs du client : la vitesse et la
// montée, toutes deux posées par le bâtisseur.
inline constexpr std::size_t kThunderScatterSlotOfSpeed = 1;
inline constexpr std::size_t kThunderScatterSlotOfRise = 2;

// Avance les quatre pistes d'un tic, et range les deux angles de caméra.
//
// `node_position` est la place du nœud à ce tic — celle du porteur, que le
// mouvement recopie depuis l'instance. `aim_point` est le point visé, recopié
// une fois pour toutes au semis ; le mouvement n'en lit pas la hauteur.
//
// ⚠ LES ANGLES DE CAMÉRA SONT RANGÉS AVANT LA BOUCLE, dans le style des pistes
// zéro et un : le carré teinté les y
// relit. Ce sont ceux que la troupe tient déjà prêts — `360 − lacet` et
// `450 − tangage` —, comme chez l'icône qui flotte.
//
// 🔴 LE CAP SE CALCULE DANS LE REPÈRE DU CLIENT, puis se nie pour entrer dans
// le nôtre — voir `effect_homing_gust.h`. L'écart tiré s'ajoute chez lui : le
// nier après coup garde la suite des tirages identique à la sienne.
//
// ⚠ L'ORDONNÉE DE L'ANCRE SE RETRANCHE : le client AJOUTE la montée à l'ancre
// dans un repère qui DESCEND.
void AdvanceThunderScatterTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                 glm::vec3 node_position, glm::vec3 aim_point,
                                 std::int16_t billboard_turn_degrees,
                                 std::int16_t billboard_tilt_degrees, std::uint32_t& generator);

}  // namespace ro::render
