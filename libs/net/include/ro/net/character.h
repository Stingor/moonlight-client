#pragma once

#include <cstdint>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// LA FICHE DU PERSONNAGE : ce qu'on dépense, et ce que le serveur en répond.
//
// Les nombres du personnage arrivent presque tous par les trois paquets de
// paramètre et par la feuille `ZC_STATUS` — c'est `world_update.cpp` qui les
// range. Ce fichier ne porte que le geste qui les FAIT CHANGER, et son accusé.

// L'accusé d'une montée de caractéristique — `ZC_STATUS_CHANGE_ACK`, 0x00BC.
//
// 🔴 CE N'EST PAS UN ACCUSÉ DE RÉCEPTION, C'EST LE SEUL PORTEUR DE LA NOUVELLE
// VALEUR. `pc_statusup` n'envoie `clif_updatestatus(type)` QUE si la valeur
// dépasse 255, pour corriger la troncature ; en dessous — c'est-à-dire toujours
// — la caractéristique montée ne voyage que dans ce paquet. L'ignorer laisserait
// le bouton « + » sans effet visible, alors que le serveur aurait bien agi.
//
// Le coût du cran suivant et le solde de points, eux, arrivent bien par
// `ZC_PAR_CHANGE`.
[[nodiscard]] FrameOutcome ApplyCharacterFrame(const Frame& frame, sim::World& world);

// « Je monte cette caractéristique. »
//
// ⚠ ÉMET, ET RIEN DE PLUS, comme les verbes du sac : c'est le serveur qui
// décide, et il refuse pour trois raisons distinctes — numéro hors des six,
// plafond du personnage atteint, points insuffisants. Avancer la valeur chez
// nous montrerait une force que le monde ne partage pas.
//
// `parameter` est le numéro du serveur : `SP_STR` à `SP_LUK`, soit 13 à 18.
[[nodiscard]] core::Result<void> RequestStatUp(Connection& link, std::uint16_t parameter,
                                               std::uint8_t amount);

}  // namespace ro::net
