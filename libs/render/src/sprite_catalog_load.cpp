// Le CHARGEMENT du catalogue : lire les tables, et n'en garder que ce qui sert
// à habiller un acteur.
//
// Séparé de la construction des chemins, qui n'a besoin que du catalogue déjà
// lu : l'une parle au VFS, l'autre assemble des chaînes.

#include <string>
#include <utility>

#include "ro/render/sprite_catalog.h"

namespace ro::render {

core::Result<SpriteCatalog> LoadSpriteCatalog(vfs::Vfs& vfs) {
  SpriteCatalog catalog;
  auto tables = ReadSpriteTables(vfs);
  if (!tables) {
    return std::unexpected(tables.error());
  }
  catalog.tables = std::move(*tables);

  // La table des objets se lit À PART — voir `ro/vfs/item_table.h`. Ses
  // dizaines de milliers d'entrées n'ont rien à faire parmi les tables de
  // sprites.
  auto items = vfs::ReadItemTable(vfs);
  if (!items) {
    return std::unexpected(items.error());
  }
  catalog.items = std::move(*items);

  // Les gestes de créature : un fichier absent laisse la table vide, comme
  // chez le client.
  if (auto gestes = vfs.Read(std::string(kMonsterSkillInfoFile)); gestes) {
    catalog.monster_skill_actions = formats::MonsterSkillInfo::Parse(*gestes);
  } else if (gestes.error().code != core::ErrorCode::kNotFound) {
    return std::unexpected(gestes.error());
  }
  return catalog;
}

}  // namespace ro::render
