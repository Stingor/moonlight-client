#pragma once

#include <filesystem>
#include <string>

#include "ro/core/error.h"
#include "ro/formats/thor.h"

namespace moonclient::patcher {

// OÙ UNE ARCHIVE DE PATCH S'APPLIQUE.
struct PatchTarget {
  std::filesystem::path install_root;
  // L'archive GRF d'un patch qui ne nomme pas la sienne.
  std::string default_grf;
  bool create_grf = false;
  std::filesystem::path running_executable;
};

struct PatchApplied {
  bool executable_replaced = false;
};

// APPLIQUE UNE ARCHIVE À L'INSTALLATION : fusion dans sa GRF, ou fichiers du
// dossier du jeu, dans l'ordre de la table.
//
// Chaque fichier s'écrit à côté puis remplace l'ancien d'un renommage : un
// arrêt laisse l'ancien ou le nouveau, jamais un fichier tronqué.
//
// ⚠ UN CHEMIN QUI SORTIRAIT DE L'INSTALLATION EST REFUSÉ, et l'archive avec
// lui, avant qu'un seul fichier ne soit écrit.
//
// ⚠ L'EXÉCUTABLE QUI TOURNE NE S'ÉCRASE PAS : Windows le refuse. Il se
// renomme d'abord (`.old`), et le nouveau prend sa place ; il sera effacé au
// lancement suivant.
[[nodiscard]] ro::core::Result<PatchApplied> ApplyPatchArchive(ro::formats::ThorArchive& archive,
                                                               const PatchTarget& target);

}  // namespace moonclient::patcher
