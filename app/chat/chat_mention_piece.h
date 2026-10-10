#pragma once

#include <string>
#include <string_view>

namespace moonclient {

// CE QU'UNE MENTION DE `name` AJOUTE À UNE SAISIE QUI PORTE DÉJÀ `before` :
// « @nom » comme un mot — une espace devant quand ce qui précède n'en finit
// pas —, et une espace derrière pour enchaîner.
//
// 🔴 EN TÊTE DE LIGNE, LE NOM SEUL : le serveur lit une ligne qui commence par
// « @ » comme une COMMANDE (voir `ro::net::IsServerCommandText`). Un nom qui
// porte celui d'une commande l'exécuterait, et pour un compte du staff la ligne
// serait avalée. La mention se reconnaît au nom, pas au « @ ».
[[nodiscard]] std::string MentionPiece(std::string_view before, std::string_view name);

}  // namespace moonclient
