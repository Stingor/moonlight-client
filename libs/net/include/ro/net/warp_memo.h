#pragma once

#include <cstdint>
#include <optional>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"

namespace ro::net {

// LA COMMANDE QUI MÉMORISE LA POSITION comme point de Warp Portal, et ce que le
// serveur en répond.
//
//   CZ 0x011D [op]              « mémorise ici », sans champ
//   ZC 0x011E [op][type:1]      la réponse à la demande
//   ZC 0x0189 [op][type:2]      un refus tenant à la carte
//
// 🔴 LE SERVEUR NE RÉPOND QUE PAR UN CODE : les phrases sont celles de la table
// de messages de l'installation, et le sens de chaque code est rangé par `app`
// (`items/warp_memo_report.h`). Ce fichier rend le code tel que le fil le porte.

// « Mémorise ici. » ⚠ ÉMET, ET RIEN DE PLUS : le serveur répond, ou se tait
// quand le personnage est mort.
[[nodiscard]] core::Result<void> RequestWarpMemo(Connection& link);

// Le code d'une réponse à la demande, ou rien quand la trame n'en est pas une.
[[nodiscard]] std::optional<std::uint8_t> ReadWarpMemoAck(const Frame& frame);

// Le code d'un refus de carte, ou rien quand la trame n'en est pas un.
[[nodiscard]] std::optional<std::int16_t> ReadMapInfoNotice(const Frame& frame);

}  // namespace ro::net
