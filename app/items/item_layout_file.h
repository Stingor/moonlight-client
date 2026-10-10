#pragma once

#include <filesystem>
#include <string_view>

#include "game/save_folder.h"
#include "items/item_layout.h"

namespace moonclient {

// LE PLACEMENT LIBRE DU SAC, SUR LE DISQUE.
//
// ⚠ SON PROPRE FICHIER, comme les onglets de conversation et les touches : c'est
// une LISTE D'ENTRÉES et non des valeurs nommées, et le fichier de réglages ne
// sait décrire que des scalaires déclarés un à un.
//
// ⚠ IL NE VOYAGE PAS AVEC LE PERSONNAGE : la table dit où le JOUEUR aime poser
// ses objets, et il la retrouve sur chacun des siens. Un fichier par
// personnage coûterait pour un gain que personne n'a demandé.
inline constexpr std::string_view kItemLayoutFileName = "sac_placement.yml";

[[nodiscard]] std::filesystem::path ItemLayoutFilePath(const std::filesystem::path& client_root);

// Lit la table. Un fichier absent, illisible ou vide laisse la grille au
// remplissage automatique — ce qui est exactement ce qu'un joueur qui n'a jamais
// placé un objet attend.
void LoadItemLayout(const std::filesystem::path& client_root, ItemLayout& into);

// Écrit la table. Rend faux si le fichier n'a pas pu être écrit.
[[nodiscard]] bool SaveItemLayout(const std::filesystem::path& client_root, const ItemLayout& from);

}  // namespace moonclient
