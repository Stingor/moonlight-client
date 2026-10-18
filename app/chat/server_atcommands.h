#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace moonclient {

// LES COMMANDES « @ » DU SERVEUR, telles qu'il les enregistre.
//
// 🔴 RELEVÉES SUR LE SERVEUR, PAS ÉCRITES À LA MAIN : la table est engendrée par
// `scripts/gen_server_atcommands.py` depuis son relevé versionné
// (`scripts/server_atcommands.json`), lui-même tiré du dépôt du serveur. Une
// commande ajoutée au serveur se propose après un rafraîchissement du relevé,
// jamais par une retouche ici.
//
// ⚠ CE NE SONT PAS DES COMMANDES DU CLIENT : le client les propose et les envoie,
// c'est le serveur qui les exécute ou les refuse.
struct ServerAtCommand {
  // Le nom, sans l'arobase.
  std::string_view name;
  // Les autres noms qui mènent à elle, sans l'arobase, séparés par une espace.
  std::string_view aliases;
  // Ce qu'elle attend, tel que le serveur l'écrit (« <mapname> [<x> <y>] ») ;
  // vide quand il ne le dit pas.
  std::string_view params;
  // Son aide, dans la langue où le serveur l'écrit ; vide quand il n'en a pas.
  std::string_view help;
  // Le plus petit niveau de groupe qui y a accès.
  std::uint8_t min_group_level = 0;
};

// Le niveau d'une commande qu'aucun groupe n'atteint : elle ne se propose à
// personne. ⚠ LE MÊME NOMBRE QUE `AUCUN_NIVEAU` dans le générateur.
inline constexpr std::uint8_t kNoGroupLevel = 255;

// Toutes, triées par nom.
[[nodiscard]] std::span<const ServerAtCommand> ServerAtCommands();

}  // namespace moonclient
