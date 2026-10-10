#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"

namespace moonclient::patcher {

// UN PATCH DE LA LISTE : son rang, son archive, et l'empreinte qui l'atteste.
struct PatchEntry {
  std::uint64_t patch_index = 0;
  // Un NOM de fichier, jamais un chemin : il s'ajoute au dossier des archives
  // du serveur et au dossier de téléchargement local.
  std::string archive_name;
  // SHA-256 de l'archive, en hexadécimal minuscule, ou vide.
  std::string sha256_hex;
};

// LIT LA LISTE DES PATCHS publiée par le serveur.
//
// Une ligne par patch : `<index> <archive> [<sha256>]`, séparés par des blancs.
// Les lignes vides et celles qui commencent par `//` ou `#` sont ignorées, de
// même qu'une colonne au-delà de la troisième.
//
// ⚠ UNE LIGNE MAL FORMÉE EST UNE ERREUR, et l'erreur la nomme. L'ignorer
// sauterait un patch sans rien dire, et le client suivant se croirait à jour
// sur une installation à laquelle il manque une archive.
//
// ⚠ UN NOM D'ARCHIVE QUI SORT DE SON DOSSIER EST REFUSÉ : ni barre, ni `..`,
// ni point de tête. La liste vient du réseau, et ce nom devient un chemin sur
// le disque du joueur.
//
// Les patchs sont rendus triés par index croissant.
[[nodiscard]] ro::core::Result<std::vector<PatchEntry>> ParsePatchList(std::string_view text);

// LES PATCHS QUI RESTENT À APPLIQUER, dans l'ordre.
//
// Ceux dont l'index dépasse le dernier appliqué. ⚠ SI CE DERNIER N'EST PLUS
// DANS LA LISTE — le serveur a renuméroté sa chaîne —, l'état local ne dit plus
// rien de sûr, et toute la liste est à appliquer.
[[nodiscard]] std::vector<PatchEntry> PendingPatches(
    const std::vector<PatchEntry>& patches, std::optional<std::uint64_t> last_applied_index);

}  // namespace moonclient::patcher
