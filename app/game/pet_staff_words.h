#pragma once

// LES COMMANDES DU STAFF SUR LE FAMILIER : le texte qu'une intention de la
// fiche devient. La fenêtre émet « régler la faim », c'est `app` qui sait que
// cela s'écrit « @pethungry ».
//
// ⚠ RIEN N'Y EST VALIDÉ HORS DES BORNES : le serveur revalide selon le groupe
// du compte, et son refus arrive dans la conversation.

#include <string>

namespace moonclient {

// « @pethungry <faim> », la faim ramenée dans ses bornes.
[[nodiscard]] std::string PetStaffHungerCommand(int hunger);

// « @petfriendly <intimité> », l'intimité ramenée dans ses bornes.
//
// 🔴 JAMAIS ZÉRO : le serveur détruirait l'œuf du familier.
[[nodiscard]] std::string PetStaffIntimacyCommand(int intimacy);

}  // namespace moonclient
