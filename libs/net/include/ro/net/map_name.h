#pragma once

#include <string>
#include <string_view>

namespace ro::net {

// Le nom d'une carte, sans extension, depuis celui que le SERVEUR emploie.
//
// Le serveur nomme les cartes du nom de leur fichier de MARCHABILITÉ, gonryun
// devenant gonryun.gat. Le reste du client les nomme sans extension, parce
// qu'une carte est trois fichiers — .gat, .gnd, .rsw — et qu'aucun des trois
// n'est plus légitime que les autres.
//
// La conversion vit ICI, et nulle part ailleurs : deux paquets l'annoncent —
// la remise au serveur de zone et le changement de carte — et deux conversions
// finiraient par diverger.
[[nodiscard]] std::string MapBaseName(std::string_view server_name);

}  // namespace ro::net
