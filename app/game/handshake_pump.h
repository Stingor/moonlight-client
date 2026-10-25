#pragma once

#include <cstdint>

#include "ro/net/connection.h"
#include "ro/net/moonclient_handshake.h"

namespace moonclient {

struct GamePieces;

// LA POIGNÉE DE MAIN D'INTÉGRITÉ, et ce qu'elle débloque — à chaque image, la
// liaison ouverte.
//
// Le serveur arme son minuteur à l'entrée en carte, et ce qu'on lui doit tient
// en un paquet. Une fois reconnus, on lui doit encore deux choses : la demande
// du catalogue de la boutique, une fois par partie, et nos annonces de
// surfaces, qu'il oublie à chaque entrée en zone.
//
// `reported` est le dernier état écrit au journal : seul un CHANGEMENT s'écrit,
// sinon le renvoi toutes les trois secondes noierait le reste.
//
// `reading_caps` : les bits de lecture que ce client n'annonce que s'il sait
// lire leurs trames, comme `SurvivorsRankingMode::caps`.
void PumpHandshake(ro::net::Connection& link, ro::net::MoonclientHandshake& handshake,
                   ro::net::MoonclientHandshakeState& reported, GamePieces& pieces,
                   bool mvp_tracker_enabled, std::uint32_t reading_caps, std::uint32_t now_ms);

}  // namespace moonclient
