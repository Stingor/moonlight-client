#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"
#include "ro/vfs/vfs.h"

namespace moonclient {

// LES SKINS DU JOUEUR, sur le disque de l'installation, comme le client
// d'origine les range : `skin\<nom>\` reproduit l'arborescence de la racine
// d'interface (`data\texture\유저인터페이스\`), et n'a besoin de porter QUE les
// pièces qu'il change — une pièce absente retombe sur les archives.
//
// ⚠ CE DOSSIER APPARTIENT AU JOUEUR : le client y dépose ce qui manque, et
// n'écrase jamais ce qui s'y trouve.
inline constexpr std::string_view kSkinFolderName = "skin";
// Le skin d'origine, le seul tant que le joueur n'en choisit pas d'autre.
inline constexpr std::string_view kDefaultSkinName = "default";

// Le chemin, dans le skin `skin_name`, d'une pièce d'interface donnée par son
// chemin d'archive UTF-8. Rien si ce chemin n'est pas sous la racine
// d'interface, ou s'il en sortirait.
[[nodiscard]] std::optional<std::filesystem::path> SkinFilePath(
    const std::filesystem::path& install_root, std::string_view skin_name,
    std::string_view utf8_ui_path);

// Le dossier du skin `skin_name` : `skin\<skin_name>\`.
[[nodiscard]] std::filesystem::path SkinFolder(const std::filesystem::path& install_root,
                                               std::string_view skin_name);

// LES SKINS DE L'INSTALLATION : les dossiers de `skin\`, par ordre de nom, et le
// skin d'origine TOUJOURS en tête — même avant que son dossier existe, au tout
// premier lancement.
[[nodiscard]] std::vector<std::string> InstalledSkins(const std::filesystem::path& install_root);

// LE FICHIER D'UNE PIÈCE, lue SANS ARCHIVE : dans le skin `skin_name`, sinon
// dans le skin d'origine, qui porte ce qu'on lit avant le montage. Rien si
// aucun des deux ne la porte.
[[nodiscard]] std::optional<std::filesystem::path> SkinPieceOnDisk(
    const std::filesystem::path& install_root, std::string_view skin_name,
    std::string_view utf8_ui_path);

// PORTE LE SKIN `skin_name` SUR LE VFS : une pièce sous la racine d'interface
// se lit d'abord dans `skin\<skin_name>\`, puis, absente, comme avant — le
// dossier `data\`, puis les archives. La règle du client d'origine, pour TOUTE
// lecture : découpage de l'habillage, icônes d'interface, illustrations. Voir
// `ro::vfs::Vfs::OverlayFolder`.
void UseSkin(ro::vfs::Vfs& vfs, const std::filesystem::path& install_root,
             std::string_view skin_name);

// DÉPOSE DANS LE SKIN D'ORIGINE les pièces qu'on doit pouvoir lire SANS
// ARCHIVE — l'illustration de l'écran de mise à jour, qui passe avant qu'on les
// ouvre. Une pièce déjà présente n'est pas touchée ; une pièce que les archives
// n'ont pas est passée.
void SeedDefaultSkin(ro::vfs::Vfs& vfs, const std::filesystem::path& install_root);

}  // namespace moonclient
