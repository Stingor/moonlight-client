// Le nom lisible de chaque carte. Voir l'en-tête.

#include "ro/vfs/map_display_names.h"

#include <optional>
#include <string>
#include <utility>

#include "ro/vfs/encoding.h"

#include "lua_source_cut.h"

namespace ro::vfs {
namespace {

// Ce qui remplace l'extension : la table se range par le fichier du monde.
constexpr std::string_view kWorldExtension = "rsw";
// Ce que la retouche de l'extension retire, et la plus courte longueur à
// laquelle elle s'applique.
constexpr std::size_t kReplacedExtensionSize = 3;
constexpr std::size_t kShortestRenamed = 5;
// La retouche d'une carte d'instance : le « @ » doit être à cette position au
// moins, et les caractères retirés commencent autant de positions avant lui.
constexpr char kInstanceMark = '@';
constexpr std::size_t kInstanceMarkLeast = 4;
constexpr std::size_t kInstanceDropped = 3;

// Un texte de l'entrée, converti ; vide quand il manque ou n'est pas un texte.
[[nodiscard]] std::string TextOf(const formats::LuaTable& table, std::string_view field) {
  const formats::LuaValue* value = table.Find(field);
  if (value == nullptr || value->kind != formats::LuaValue::Kind::kString) {
    return {};
  }
  return Cp949ToUtf8Lossy(value->text);
}

// Le bandeau d'une entrée, ou rien quand elle n'a ni annonce ni titre.
//
// ⚠ UN `notifyEnter` ABSENT VAUT FAUX : c'est la fonction du fichier qui le
// pose avant de déclarer chaque carte.
[[nodiscard]] std::optional<MapSign> SignOfEntry(const formats::LuaTable& entry) {
  MapSign sign;
  const formats::LuaValue* notify = entry.Find(kMapInfoNotifyField);
  sign.announced =
      notify != nullptr && notify->kind == formats::LuaValue::Kind::kBoolean && notify->boolean;
  if (std::string background = TextOf(entry, kMapInfoBackgroundField); !background.empty()) {
    sign.background = std::move(background);
  }
  const formats::LuaValue* titles = entry.Find(kMapInfoSignField);
  if (titles != nullptr && titles->kind == formats::LuaValue::Kind::kTable &&
      titles->table != nullptr) {
    sign.subtitle = TextOf(*titles->table, kMapInfoSubtitleField);
    sign.title = TextOf(*titles->table, kMapInfoTitleField);
  }
  if (!sign.announced && !sign.titled()) {
    return std::nullopt;
  }
  return sign;
}

}  // namespace

std::string MapInfoKey(std::string_view gat_name) {
  std::string key(gat_name);
  if (key.size() >= kShortestRenamed) {
    key.resize(key.size() - kReplacedExtensionSize);
    key += kWorldExtension;
  }
  const std::size_t mark = key.rfind(kInstanceMark);
  if (mark != std::string::npos && mark >= kInstanceMarkLeast) {
    key.erase(mark - kInstanceMarkLeast, kInstanceDropped);
  }
  return key;
}

core::Result<MapDisplayNames> MapDisplayNames::Load(Vfs& vfs) {
  auto bytes = vfs.Read(kMapInfoPath);
  if (!bytes) {
    return std::unexpected(bytes.error());
  }
  const std::string_view text(reinterpret_cast<const char*>(bytes->data()), bytes->size());
  return ParseSource(text);
}

core::Result<MapDisplayNames> MapDisplayNames::ParseSource(std::string_view source) {
  // Le complément que la fin du fichier charge (`mapinfo_C.lub`) ne déclare que
  // sa propre table, que rien ne fusionne dans celle-ci : le couper ne perd rien.
  auto read = formats::ReadLuaSource(WithoutTrailingConditional(source), formats::LuaTable{});
  if (!read) {
    return std::unexpected(read.error());
  }
  return Parse(*read);
}

core::Result<MapDisplayNames> MapDisplayNames::Parse(const formats::LuaTable& globals) {
  const formats::LuaValue* table = globals.Find(kMapInfoTableGlobal);
  if (table == nullptr || table->kind != formats::LuaValue::Kind::kTable ||
      table->table == nullptr) {
    return core::Fail(core::ErrorCode::kCorruptData, std::string(kMapInfoTableGlobal) +
                                                         " n'est pas une table dans " +
                                                         std::string(kMapInfoPath));
  }
  MapDisplayNames loaded;
  for (const auto& [rsw_name, entry] : table->table->by_name) {
    if (entry.kind != formats::LuaValue::Kind::kTable || entry.table == nullptr) {
      continue;
    }
    std::string shown(kMapInfoUnnamed);
    const formats::LuaValue* display = entry.table->Find(kMapInfoDisplayField);
    if (display != nullptr && display->kind == formats::LuaValue::Kind::kString) {
      shown = Cp949ToUtf8Lossy(display->text);
    }
    loaded.names_.insert_or_assign(rsw_name, std::move(shown));
    if (std::optional<MapSign> sign = SignOfEntry(*entry.table); sign.has_value()) {
      loaded.signs_.insert_or_assign(rsw_name, std::move(*sign));
    }
  }
  return loaded;
}

const MapSign* MapDisplayNames::SignOf(std::string_view gat_name) const {
  const auto found = signs_.find(MapInfoKey(gat_name));
  return found == signs_.end() ? nullptr : &found->second;
}

std::optional<std::string_view> MapDisplayNames::DisplayNameOf(std::string_view gat_name) const {
  const auto found = names_.find(MapInfoKey(gat_name));
  if (found == names_.end()) {
    return std::nullopt;
  }
  return std::string_view(found->second);
}

}  // namespace ro::vfs
