#pragma once

#include <cstdint>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/world.h"

namespace ro::net {

// ── LES POINTS DE MANIÈRE : le silence du chat, en minutes ──────────────────
//
//   CZ 0x0149 [op][compte:4][sens:1][minutes:2]
//   ZC 0x014A [op][verdict:4]                 à l'émetteur, comme à la cible
//   ZC 0x014B [op][sens:1][nom de l'auteur:24]  à la cible
//
// Le serveur rejoue `@mute` sous les droits du groupe de l'émetteur — le niveau
// 40 la détient, et tous ceux au-dessus. Ce que la commande fait du compteur de
// la cible :
//
// * un MAUVAIS point de N ajoute N minutes de silence ;
// * un BON point en retire N, et lève le silence quand il n'en resterait plus.
//
// Ce qu'elle refuse — cible absente, ou d'un groupe plus haut que l'émetteur —,
// le serveur l'écrit lui-même dans la conversation.

enum class MannerPointWay : std::uint8_t {
  kGood = 0,
  kBad = 1,
};

// La saisie la plus haute que le client d'origine laisse partir ; il borne
// en dessous à zéro.
inline constexpr int kMannerPointsMostMinutes = 30000;

// `minutes` est borné à [0, kMannerPointsMostMinutes] avant de partir.
[[nodiscard]] core::Result<void> RequestMannerPoint(Connection& link, std::uint32_t account_id,
                                                    MannerPointWay way, int minutes);

// LES DEUX RÉPONSES, écrites dans la conversation comme le client d'origine
// les écrit :
//
// * ZC 0x014A, verdict 0 — l'émetteur : le point a été appliqué ; 3 et 4 — la
//   cible : un silence posé par le staff, ou par l'anti-spam. Les autres
//   numéros — dont le 5, que la cible reçoit à chaque point — n'écrivent rien.
// * ZC 0x014B, sens 0 — la cible : de qui vient le bon point. Le sens 1, un
//   mauvais point, n'écrit rien.
[[nodiscard]] FrameOutcome ApplyMannerPointFrame(const Frame& frame, sim::World& world,
                                                 std::uint32_t now_ms);

}  // namespace ro::net
