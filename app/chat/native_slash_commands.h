#pragma once

#include <string_view>

namespace moonclient {

// LE CLIENT DU JOUEUR CONNAÎT-IL CETTE COMMANDE ?
//
// Il en connaît quelques centaines, qu'il traite lui-même. Une ligne en « / »
// qu'il ne connaît PAS part au serveur comme une parole ordinaire, barre
// oblique comprise, et tout le monde la lit. Nous faisons pareil : une commande
// que ni lui ni nous ne connaissons se dit, une commande qu'il connaît et que
// nous n'avons pas encore reprise se tait.
//
// Seul le premier mot compte, et la casse latine ne compte pas.
[[nodiscard]] bool NativeKnowsSlashCommand(std::string_view line);

}  // namespace moonclient
