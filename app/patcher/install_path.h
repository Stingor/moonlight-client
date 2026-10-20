#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include "ro/core/error.h"

namespace moonclient::patcher {

// LE CHEMIN DU DISQUE d'un chemin relatif venu du serveur, UTF-8, séparé par
// `/` ou `\`.
//
// ⚠ UN CHEMIN QUI SORTIRAIT DE L'INSTALLATION EST UNE ERREUR : absolu, avec
// lecteur, ou remontant par `..`. Il vient du réseau et devient un chemin sur
// le disque du joueur.
[[nodiscard]] ro::core::Result<std::filesystem::path> InstallPathOf(
    const std::filesystem::path& install_root, std::string_view utf8_path);

// Vrai si `candidate` est l'exécutable qui tourne. Un `running_executable`
// vide n'est jamais reconnu.
[[nodiscard]] bool IsRunningExecutable(const std::filesystem::path& candidate,
                                       const std::filesystem::path& running_executable);

// Écarte l'exécutable qui tourne (`.old`) pour qu'un nouveau prenne son nom.
// Rend le chemin écarté, pour le remettre si l'écriture échoue.
[[nodiscard]] ro::core::Result<std::filesystem::path> SetRunningExecutableAside(
    const std::filesystem::path& running_executable);

// L'empreinte SHA-256 d'un fichier, en hexadécimal minuscule, lue par morceaux.
[[nodiscard]] ro::core::Result<std::string> Sha256OfFile(const std::filesystem::path& path);

}  // namespace moonclient::patcher
