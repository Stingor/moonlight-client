#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"

namespace moonclient::patcher {

// La première ligne d'un manifeste : ce qui le distingue d'une page d'erreur
// que le serveur aurait rendue à sa place.
inline constexpr std::string_view kMirrorManifestHeader = "moonclient-mirror 1";

// UN FICHIER PUBLIÉ PAR LE MIROIR.
struct MirrorFile {
  // Relatif au dossier du jeu, UTF-8, séparé par `/`.
  std::string relative_path;
  std::uint64_t size_bytes = 0;
  // SHA-256 du contenu, en hexadécimal minuscule.
  std::string sha256_hex;
};

// LIT LE MANIFESTE du miroir : l'en-tête, puis une ligne par fichier,
// `<sha256> <taille> <chemin>`. Le chemin est le reste de la ligne, espaces
// compris.
//
// ⚠ UNE LIGNE MAL FORMÉE EST UNE ERREUR, et l'erreur la nomme : l'ignorer
// laisserait le joueur sans ce fichier, persuadé d'être à jour.
//
// ⚠ UN CHEMIN QUI SORT DU DOSSIER DU JEU EST REFUSÉ, de même qu'un chemin
// répété — à la casse près, puisque le disque de Windows l'ignore.
[[nodiscard]] ro::core::Result<std::vector<MirrorFile>> ParseMirrorManifest(std::string_view text);

// L'ADRESSE D'UN FICHIER DU MIROIR : le dossier, puis le chemin, chaque octet
// hors des caractères sûrs d'une URL écrit en `%XX`. Les `/` restent.
[[nodiscard]] std::string MirrorFileUrl(std::string_view files_url, std::string_view relative_path);

}  // namespace moonclient::patcher
