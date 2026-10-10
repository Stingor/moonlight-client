#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include <glm/glm.hpp>

#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LA BOURRASQUE QUI POURSUIT UN ACTEUR : sous-types 122 et 125.
//
// Leur FIGURE est celle du 119 — le carré plat de `ComposeGroundGustQuad`, même
// dessinateur — et leur écho aussi. Ce qui les sépare est le
// CAP : il ne se prend plus une fois pour toutes, il se CORRIGE à chaque tic
// vers un acteur.
//
// ⚠ AUCUN ACTEUR NE FRANCHIT LA FRONTIÈRE. Le client garde le GID de la cible
// dans le nœud, la cherche à chaque tic parmi les acteurs (ou prend le joueur
// quand le GID est le sien), et n'en lit QUE la position — x et z. `render`
// reçoit donc un point,
// comme le trait tendu du 38 : `PooledEffectNode::target_position`. Vide veut
// dire « introuvable », et les deux mouvements ont une branche pour cela.
//
// 🔴 LE CAP EST CALCULÉ DANS LE REPÈRE DU CLIENT, puis ramené dans le nôtre.
// Nos profondeurs sont opposées aux siennes (`S = diag(1, −1, −1)`) : un cap se
// NIE en franchissant la frontière, et un virage de cinq degrés à gauche chez
// lui en est un à droite chez nous. Le transcrire nombre pour nombre dans notre
// repère ferait tourner la bourrasque DANS L'AUTRE SENS — elle s'éloignerait de
// sa cible au lieu de s'en rapprocher. Et la fenêtre de trois degrés où le cap
// est tenu pour atteint se compare SANS repli au tour, chez lui : la comparer
// dans notre repère la déplacerait autour de zéro.

// LES MARQUES QUE LES DEUX MOUVEMENTS LISENT, en plus de la teinte du 119
// (`kGroundGustMarkOfColor`).
//
// L'ÉTAT DE LA POURSUITE, le CÔTÉ par lequel la bourrasque part et vire, et —
// pour le 122 seul — l'extinction HÂTIVE.
inline constexpr std::size_t kHomingGustMarkOfState = 1;
inline constexpr std::size_t kHomingGustMarkOfSide = 2;
inline constexpr std::size_t kHomingGustMarkOfEarlyFade = 4;

// LES TROIS ÉTATS DE LA POURSUITE, tels que le client les numérote.
//
// ⚠ CE N'EST PAS UNE ÉCHELLE : zéro poursuit, dix pâlit, vingt rentre. Le 122
// compare pourtant « au moins dix » pour savoir s'il a déjà renoncé — les deux
// derniers sont ceux où la cible ne sert plus.
inline constexpr std::uint8_t kHomingGustChasing = 0;
inline constexpr std::uint8_t kHomingGustFading = 10;
inline constexpr std::uint8_t kHomingGustReturning = 20;

// LE MOUVEMENT DU SOUS-TYPE 122 : la poursuite qui vire.
//
// La bourrasque part d'un quart de tour de côté, vire de cinq degrés par tic
// jusqu'à viser l'acteur, et s'éteint après cent quarante tics — quatre-vingt-dix
// si elle est hâtive. Si l'acteur disparaît, elle RENTRE vers le point d'où elle
// est partie, et s'éteint après cinquante tics.
//
// `node_position` est la place du nœud au semis : l'origine du premier
// cap, et le point où elle rentre.
void AdvanceHomingGustSteeredTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                    glm::vec3 node_position,
                                    const std::optional<glm::vec3>& target);

// LE MOUVEMENT DU SOUS-TYPE 125 : la poursuite qui part d'un écart au hasard.
//
// Elle part d'un écart TIRÉ AU SORT — vingt-cinq à quarante-cinq degrés — et
// vire de cinq degrés par tic ; dès qu'elle vise l'acteur à trois degrés près,
// ou au vingt et unième tic, elle s'éteint en tournant d'un degré par tic. Si
// l'acteur disparaît, elle s'éteint aussitôt.
//
// ⚠ ELLE TIRE, au premier tic utile et une seule fois : `generator` est celui
// de la troupe.
void AdvanceHomingGustSpreadTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                   glm::vec3 node_position, const std::optional<glm::vec3>& target,
                                   std::uint32_t& generator);

}  // namespace ro::render
