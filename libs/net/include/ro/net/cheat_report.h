#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ro/net/connection.h"

namespace ro::net {

// LE SIGNALEMENT D'UN OUTIL DE TRICHE, `CZ_MOONLIGHT_CHEAT_REPORT`.
//
// Le serveur l'écrit à son journal et ne sanctionne rien : un humain tranche.
// Il ne le lit que d'une session reconnue par la poignée de main, et n'en garde
// qu'UN PAR MINUTE et par session — les autres sont jetés sans un mot. C'est à
// l'émetteur de tenir cette cadence, sinon ses signalements se perdent.

// Les deux champs, à longueur fixe sur le fil. Un texte plus long est tronqué.
inline constexpr std::size_t kCheatToolBytes = 32;
inline constexpr std::size_t kCheatDetailBytes = 64;

// La cadence que le serveur tient, en millisecondes.
inline constexpr std::uint32_t kCheatReportIntervalMs = 60'000;

void SendCheatReport(Connection& link, std::string_view tool, std::string_view detail);

}  // namespace ro::net
