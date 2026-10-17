#pragma once

#include <string>

namespace moonclient {

// CE QUE L'ÉCRAN DE CONNEXION RETIENT D'UN LANCEMENT À L'AUTRE, dans le fichier
// de réglages.
//
// 🔴 LE COMPTE, ET PAS LE MOT DE PASSE : ce fichier est en clair, à côté du
// jeu.
struct LoginMemory {
  // Le compte que la case « Se souvenir du compte » repropose, ou vide.
  std::string account;
  // Le dernier serveur choisi, en `hôte:port`, ou vide : le premier de
  // `moonclient.yml`. Voir `PreferredServer`.
  std::string server;
};

}  // namespace moonclient
