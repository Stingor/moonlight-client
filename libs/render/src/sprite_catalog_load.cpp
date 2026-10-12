// Le CHARGEMENT du catalogue : lire les tables du client, et n'en garder que
// ce qui sert à habiller un acteur.
//
// Séparé de la construction des chemins, qui n'a besoin que du catalogue déjà
// lu : l'une parle au VFS et évalue du Lua, l'autre assemble des chaînes. Les
// tenir ensemble faisait un fichier que rien ne reliait sinon son sujet.

#include <string>
#include <utility>

#include "ro/formats/lua_table.h"
#include "ro/render/sprite_catalog.h"

namespace ro::render {

core::Result<formats::LuaTable> ReadSpriteCatalogGlobals(vfs::Vfs& vfs) {
  formats::LuaTable read;
  for (const std::string_view file : kSpriteCatalogFiles) {
    auto bytes = vfs.Read(std::string(file));
    if (!bytes) {
      if (bytes.error().code == core::ErrorCode::kNotFound) {
        continue;
      }
      return std::unexpected(bytes.error());
    }
    auto globals = formats::ReadLuaFile(*bytes, std::move(read));
    if (!globals) {
      return std::unexpected(
          core::Error{globals.error().code, std::string(file) + " : " + globals.error().detail});
    }
    read = std::move(*globals);
  }
  return read;
}

core::Result<SpriteCatalog> LoadSpriteCatalog(vfs::Vfs& vfs) {
  SpriteCatalog catalog;
  auto globals = ReadSpriteCatalogGlobals(vfs);
  if (!globals) {
    return std::unexpected(globals.error());
  }
  catalog.globals = std::move(*globals);

  // La table des objets se lit À PART, et par `vfs` — voir
  // `ro/vfs/item_table.h`. Elle n'est pas du Lua, et ses dizaines de milliers
  // d'entrées n'ont rien à faire parmi les globales du catalogue.
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
