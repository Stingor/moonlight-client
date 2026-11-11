#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include "ro/core/error.h"

namespace moonclient::patcher {

// Le suffixe d'un fichier en usage qu'un patch remplace — une bibliothèque que
// le client a chargée : Windows refuse de l'écraser, pas de le renommer.
inline constexpr std::string_view kInUseSetAsideSuffix = ".moonclient-old";

// LE JOURNAL DES FICHIERS ÉCARTÉS, à la racine du jeu : un chemin relatif
// UTF-8 par ligne. Un fichier écarté reste en usage jusqu'à la fin du
// processus ; c'est le lancement suivant qui l'efface.
inline constexpr std::string_view kSetAsideJournalFileName = "moonclient-patch-aside.txt";

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

// Écarte `disk_path`, un fichier en usage sous `install_root`, sous
// `kInUseSetAsideSuffix`, pour qu'un nouveau prenne son nom, et l'inscrit au
// journal. Rend le chemin écarté, pour le remettre si l'écriture échoue.
[[nodiscard]] ro::core::Result<std::filesystem::path> SetInUseFileAside(
    const std::filesystem::path& install_root, const std::filesystem::path& disk_path);

// EFFACE LES FICHIERS ÉCARTÉS aux lancements précédents. Ce qui résiste — un
// autre client lancé du même dossier le tient encore — reste au journal.
//
// ⚠ UNE LIGNE QUI NE NOMME PAS UN FICHIER ÉCARTÉ SOUS L'INSTALLATION EST
// IGNORÉE : le journal est un fichier du disque, et n'efface que ce que le
// patcher a lui-même écarté.
void RemoveSetAsideFiles(const std::filesystem::path& install_root);

// LE NOUVEL EXÉCUTABLE REPREND LES DROITS DE L'ANCIEN, écarté sous `aside`.
// Un fichier reçu du réseau naît sans le droit d'exécution : sous Linux, le
// client remplacé ne pourrait plus se relancer.
void KeepExecutablePermissions(const std::filesystem::path& aside,
                               const std::filesystem::path& running_executable);

// L'empreinte SHA-256 d'un fichier, en hexadécimal minuscule, lue par morceaux.
[[nodiscard]] ro::core::Result<std::string> Sha256OfFile(const std::filesystem::path& path);

}  // namespace moonclient::patcher
