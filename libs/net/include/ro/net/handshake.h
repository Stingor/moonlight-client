#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include "ro/core/error.h"
#include "ro/net/session.h"

namespace ro::net {

// Fournit le code PIN quand le serveur le réclame.
//
// `refused` vaut vrai quand le serveur vient de rejeter le code précédent :
// c'est ce qui permet de dire « recommence » plutôt que de reposer la même
// question. Rendre nullopt abandonne la connexion — l'utilisateur a renoncé, ou
// il n'y a pas de terminal pour lui demander.
using PincodeProvider = std::function<std::optional<std::string>(bool refused)>;

// Fait avancer la phase de personnage jusqu'à ce qu'un choix soit possible, en
// répondant au code PIN si le serveur le demande.
//
// ⚠ Cette fonction BLOQUE, contrairement aux phases elles-mêmes. C'est elle qui
// attend, et l'appelant qui décide de s'en servir : une boucle de jeu, qui a une
// fenêtre à rafraîchir, n'appellerait pas ceci.
//
// Rend kWaiting si le temps est passé sans que la liste arrive — ce n'est pas
// une panne, mais ce n'est pas non plus un succès. Rend une erreur si le code
// PIN n'a pas pu être fourni ; les autres échecs restent dans la phase, comme
// pour Advance.
[[nodiscard]] core::Result<PhaseState> AwaitCharacterList(CharacterPhase& phase,
                                                          const PincodeProvider& pincode,
                                                          std::uint32_t timeout_ms);

}  // namespace ro::net
