#pragma once

#include <filesystem>

#include "ro/core/error.h"
#include "ro/render/map_fix.h"
#include "ro/vfs/vfs.h"

namespace moonclient {

// LA TABLE DE RETOUCHES QUE L'ÉTABLI TIENT : voir ro/render/map_fix.h.
//
// L'établi l'enregistre dans le dossier du client, sous le chemin même où
// l'installation la porte. Dans une installation ordinaire, c'est le même
// fichier ; un client d'essai, qui lit les archives d'une installation sans y
// écrire, garde ainsi la sienne à part.

// Le fichier que l'établi écrit.
[[nodiscard]] std::filesystem::path MapFixFilePath(const std::filesystem::path& client_root);

// Celle du dossier du client quand il en a une, sinon celle de l'installation.
// Une table du client illisible n'en donne aucune, et le journal le dit : la
// remplacer en silence par celle de l'installation ferait perdre au staff ce
// qu'il croit enregistré.
[[nodiscard]] ro::render::MapFixTable LoadMapFixFile(const std::filesystem::path& client_root,
                                                     ro::vfs::Vfs& vfs);

// Écrit `table` dans le dossier du client, d'un seul tenant.
[[nodiscard]] ro::core::Result<void> SaveMapFixFile(const std::filesystem::path& client_root,
                                                    const ro::render::MapFixTable& table);

}  // namespace moonclient
