// Les titres. Voir l'en-tête : le fil n'en porte qu'un numéro, le libellé est
// entièrement du côté du client.

#include "ro/render/title_table.h"

#include <algorithm>
#include <utility>

namespace ro::render {

core::Result<TitleTable> TitleTable::Load(vfs::Vfs& vfs) {
  auto bytes = vfs.Read(kTitleTablePath);
  if (!bytes) {
    return std::unexpected(bytes.error());
  }
  // ⚠ CE FICHIER SE LIT SEUL, à la différence des effets de chapeau : il
  // n'indexe ses libellés par aucune globale d'un autre fichier, seulement par
  // le numéro lui-même.
  auto read = formats::ReadLuaFile(*bytes, formats::LuaTable{});
  if (!read) {
    return std::unexpected(read.error());
  }
  return Parse(*read);
}

core::Result<TitleTable> TitleTable::Parse(const formats::LuaTable& globals) {
  const formats::LuaValue* table = globals.Find(kTitleTableGlobal);
  if (table == nullptr || table->kind != formats::LuaValue::Kind::kTable ||
      table->table == nullptr) {
    return core::Fail(core::ErrorCode::kCorruptData, std::string(kTitleTableGlobal) +
                                                         " n'est pas une table dans " +
                                                         std::string(kTitleTablePath));
  }

  TitleTable loaded;
  loaded.entries_.reserve(table->table->by_index.size());
  for (const auto& [index, value] : table->table->by_index) {
    if (value.kind != formats::LuaValue::Kind::kString) {
      continue;
    }
    loaded.entries_.push_back(Entry{static_cast<std::int32_t>(index), value.text});
  }
  std::ranges::sort(loaded.entries_, {}, &Entry::title_id);
  return loaded;
}

std::string_view TitleTable::Find(std::int32_t title_id) const {
  const auto found =
      std::ranges::lower_bound(entries_, title_id, {}, [](const Entry& e) { return e.title_id; });
  if (found == entries_.end() || found->title_id != title_id) {
    return {};
  }
  return found->label;
}

}  // namespace ro::render
