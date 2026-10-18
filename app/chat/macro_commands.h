#pragma once

#include <cstddef>
#include <string>

namespace moonclient {

// Les dix valeurs d'usine des macros, telles que le client les pose.
//
// 🔴 CE SONT DES COMMANDES D'ÉMOTE, ET LEUR CHOIX N'EST PAS LE NÔTRE : le client
// remplit ses dix lignes avec les messages 0x220 à 0x223 puis 0x225 à 0x22A —
// il SAUTE le 0x224. ⚠ ET LE RANG SAUTÉ EST `/lv2`, pas le cinquième sprite :
// lu dans la table du client, ce bloc donne les dix PREMIÈRES émoticônes.
//
// ⚠ ELLES SE DÉRIVENT DE NOTRE PROPRE TABLE DE COMMANDES, et ne sont pas
// recopiées à la main : `EmotionCommandOf` est déjà la seule source du lien
// entre une émoticône et sa commande, et une seconde liste finirait par en
// différer.
//
// ⚠ LE MENU DU CLIC DROIT, LUI, VIENT DE `ChatCommandCatalogue` : la même liste
// que celle qui s'ouvre au-dessus de la barre de chat.
[[nodiscard]] std::string ChatMacroDefault(std::size_t slot);

}  // namespace moonclient
