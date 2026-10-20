#pragma once

#include <cstddef>
#include <filesystem>
#include <stop_token>
#include <string_view>

#include "ro/core/error.h"

#include "patcher/patch_fetcher.h"
#include "patcher/patch_run.h"

namespace moonclient::patcher {

// Le suffixe d'un fichier en usage que le miroir remplace — une bibliothèque
// chargée par le client — : écarté sous ce nom, il s'efface à la
// synchronisation suivante.
inline constexpr std::string_view kMirrorSetAsideSuffix = ".moonclient-old";

struct MirrorSyncOutcome {
  std::size_t written_files = 0;
  // Vrai : un fichier en usage a été remplacé, et le client doit se relancer
  // pour employer le nouveau.
  bool restart_needed = false;
};

// MET LE DOSSIER DU JEU EN ACCORD AVEC LE MIROIR DE FICHIERS.
//
// Le serveur publie un manifeste de ce qu'il sert (`mirror_manifest.h`) ; un
// fichier local absent, d'une autre taille ou d'une autre empreinte se
// télécharge, se vérifie, puis prend sa place d'un renommage. Le miroir est le
// premier serveur qui déclare un manifeste et répond.
//
// ⚠ LE MIROIR N'EFFACE RIEN : un fichier retiré du serveur reste chez le
// joueur. Retirer passe par une archive de la chaîne.
//
// ⚠ TOUS LES CHEMINS SE VÉRIFIENT AVANT LE PREMIER TÉLÉCHARGEMENT, et un seul
// qui sortirait du dossier du jeu, ou viserait un fichier du patcher, refuse
// tout le manifeste.
//
// ⚠ UN FICHIER REÇU QUI NE CORRESPOND PAS AU MANIFESTE N'EST PAS POSÉ : c'est
// le cas d'un fichier que l'on dépose sur le serveur au même moment.
//
// Sans serveur qui déclare de manifeste, ne fait rien.
[[nodiscard]] ro::core::Result<MirrorSyncOutcome> SyncMirror(
    const PatchRunSettings& settings, const std::filesystem::path& running_executable,
    PatchFetcher& fetcher, PatchProgressBoard& board, PatchProgress& progress,
    std::stop_token stop);

}  // namespace moonclient::patcher
