#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "game/client_config.h"

namespace moonclient {

inline constexpr int kDefaultWidth = 1600;
inline constexpr int kDefaultHeight = 900;

// D'où le client part, et à qui il parle.
//
// 🔴 LE JOUEUR LANCE SANS ARGUMENT, et la construction livrée n'en lit aucun :
// le dossier du jeu est celui de l'exécutable, et son `moonclient.yml` déclare
// le serveur, les archives et la mise à jour. Seule la construction Debug
// accepte les options d'essai — voir `Usage`.
struct Launch {
  // Le dossier du jeu : `moonclient.yml`, les réglages, le skin déposé.
  // Vide : celui de l'exécutable. Rempli par l'appelant, qui lit le disque.
  std::filesystem::path install_folder;
  // Où lire les archives que la configuration nomme. Vide : le dossier du jeu.
  //
  // ⚠ SÉPARÉ DU DOSSIER DU JEU POUR LES ESSAIS : on monte en lecture les
  // archives d'une installation sans y écrire ni réglage ni skin.
  std::filesystem::path archive_folder;
  // Où enregistrer le flux du serveur de zone ; vide pour ne rien enregistrer.
  std::filesystem::path capture_file;

  // Ce que `moonclient.yml` déclare, lu par l'appelant.
  //
  // ⚠ UNE CHAÎNE POSSÉDÉE pour l'hôte : une vue sur le fichier mourrait avec
  // lui.
  std::string host;
  std::uint16_t port = 0;
  // L'adresse du service web du serveur. Vide quand on la tait : la partie se
  // joue alors sans emblèmes de guilde.
  std::string web;
  // Le fuseau horaire du serveur : voir `ClientConfig::server_time_zone`.
  std::string server_time_zone = std::string(kDefaultServerTimeZone);
  // Les archives à monter, par priorité décroissante.
  std::vector<std::string> archives;
  // La mise à jour. Absente : on ne se patche pas — c'est le cas d'un dossier
  // d'essai, qui pointe sur un serveur local.
  std::optional<PatcherConfig> patcher;
};

// Lit la ligne de commande, ou rend rien si elle n'a pas de sens.
[[nodiscard]] std::optional<Launch> ParseLaunch(std::span<const std::string_view> args);

// Écrit l'usage sur la sortie d'erreur, et rend le code de sortie qui va avec.
int Usage();

}  // namespace moonclient
