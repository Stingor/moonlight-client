#pragma once

#include <cstdint>

#include "ro/core/error.h"
#include "ro/net/connection.h"

namespace ro::net {

// LE RETOUR À LA SÉLECTION DE PERSONNAGE, depuis la partie.
//
// 🔴 ON NE REPASSE PAS PAR LE LOGIN. Le serveur de carte relaie la demande au
// serveur de personnages, qui ouvre une entrée d'authentification avec les
// jetons de la connexion initiale (`chmapif_parse_authok`) ; le client n'a plus
// qu'à s'y représenter avec eux, par `CharacterPhase::Start`. Le mot de passe
// ne resert jamais.
//
// ⚠ LE SERVEUR PEUT REFUSER : un personnage caché, ou qui vient de combattre
// (`prevent_logout`), ne quitte pas la carte. Le refus arrive par
// `ZC_ACK_REQ_DISCONNECT`, et la partie continue.

// Envoie `CZ_RESTART` avec le type « sélection de personnage ».
[[nodiscard]] core::Result<void> RequestCharacterSelect(Connection& link);

enum class CharacterSelectAnswer : std::uint8_t {
  // Cette trame ne répond pas à la demande.
  kNone,
  // `ZC_RESTART_ACK` type 1 : la main passe au serveur de personnages. Ce qui
  // suit sur la liaison de carte ne se lit plus.
  kGranted,
  // Refusé, et la partie continue.
  kRefused,
};

// CE QUE CETTE TRAME RÉPOND À LA DEMANDE.
//
// ⚠ À NE CONSULTER QU'APRÈS AVOIR DEMANDÉ : `ZC_ACK_REQ_DISCONNECT` répond aussi
// à la demande de DÉCONNEXION, et son zéro y veut dire « accordé ».
[[nodiscard]] CharacterSelectAnswer CharacterSelectAnswerOf(const Frame& frame);

}  // namespace ro::net
