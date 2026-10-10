#include "ro/vfs/status_table.h"

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>

#include "ro/formats/lua_table.h"
#include "ro/vfs/encoding.h"

namespace ro::vfs {
namespace {

// Lit un fichier de la chaîne PAR-DESSUS les globales déjà acquises. Une absence
// n'est pas une erreur : la table restera vide, et les statuts garderont leur
// numéro.
[[nodiscard]] core::Result<bool> ReadInto(Vfs& vfs, std::string_view internal_path,
                                          formats::LuaTable& globals) {
  auto bytes = vfs.Read(std::string(internal_path));
  if (!bytes) {
    if (bytes.error().code == core::ErrorCode::kNotFound) {
      return false;
    }
    return std::unexpected(
        core::Error{bytes.error().code, std::string(internal_path) + " : " + bytes.error().detail});
  }
  // ⚠ LES CLÉS QU'ON NE SAIT PAS RÉSOUDRE SE SAUTENT : `efstids` décrit mille
  // deux cent soixante-deux statuts quand les deux autres n'en nomment ou n'en
  // dessinent que huit cents, et une installation dépareillée ferait sinon
  // tomber les huit cents pour une constante manquante.
  auto read = formats::ReadLuaFile(*bytes, std::move(globals), formats::UnnamedKey::kSkipEntry);
  if (!read) {
    return std::unexpected(
        core::Error{read.error().code, std::string(internal_path) + " : " + read.error().detail});
  }
  globals = std::move(*read);
  return true;
}

// Le nom porté par une entrée : la première ligne de sa `descript`, dont le
// premier élément est le texte.
//
// ⚠ UNE LIGNE PORTE AUSSI SA COULEUR, en second élément, et certaines lignes ne
// sont que « %s » — la place de la durée. On ne prend donc PAS la première
// chaîne venue : on prend la première case de la première ligne, et rien
// d'autre.
[[nodiscard]] std::string_view TitleOf(const formats::LuaValue& entry) {
  if (entry.table == nullptr) {
    return {};
  }
  const formats::LuaValue* described = entry.table->Find(kStatusDescriptField);
  if (described == nullptr || described->table == nullptr) {
    return {};
  }
  const formats::LuaValue* first_line = described->table->Find(1.0);
  if (first_line == nullptr || first_line->table == nullptr) {
    return {};
  }
  const formats::LuaValue* title = first_line->table->Find(1.0);
  if (title == nullptr || title->kind != formats::LuaValue::Kind::kString) {
    return {};
  }
  return title->text;
}

// LE TEXTE D'UNE LIGNE de `descript`, ou rien. Une ligne est une petite table
// dont le premier élément est le texte et le second sa couleur.
[[nodiscard]] std::string_view TextOfLine(const formats::LuaValue& line) {
  if (line.table == nullptr) {
    return {};
  }
  const formats::LuaValue* text = line.table->Find(1.0);
  if (text == nullptr || text->kind != formats::LuaValue::Kind::kString) {
    return {};
  }
  return text->text;
}

// LES LIGNES DE DESCRIPTION, sans le nom et sans la place de la durée.
//
// 🔴 LA PLACE DE LA DURÉE EST UNE LIGNE, ET ELLE NE PORTE QUE « %s ». Ce n'est
// pas du texte manquant : c'est l'emplacement que le client remplit avec le
// temps restant, au MILIEU de la description et non en tête. La jeter perdrait
// sa place ; la garder écrirait « %s » à l'écran.
//
// ⚠ L'ENTRÉE DÉCLARE AUSSI CETTE PLACE, dans `posTimeLimitStr` — mais 763
// entrées la déclarent quand 603 seulement portent la ligne. On lit donc la
// LIGNE, qui ne peut pas mentir, et non le champ.
void ReadDescription(const formats::LuaValue& entry, StatusInfo& into) {
  if (entry.table == nullptr) {
    return;
  }
  const formats::LuaValue* described = entry.table->Find(kStatusDescriptField);
  if (described == nullptr || described->table == nullptr) {
    return;
  }
  // ⚠ LA PREMIÈRE LIGNE EST LE NOM, déjà lu : on part de la seconde.
  for (double rank = 2.0; rank < kMostDescriptionLines; rank += 1.0) {
    const formats::LuaValue* line = described->table->Find(rank);
    if (line == nullptr) {
      break;
    }
    const std::string_view text = TextOfLine(*line);
    if (text.empty()) {
      continue;
    }
    if (text == kStatusTimePlaceholder) {
      // Sa place est ce qu'on retient d'elle : le rang qu'elle occupe parmi les
      // lignes gardées.
      into.time_line = static_cast<int>(into.description.size());
      continue;
    }
    into.description.push_back(Cp949ToUtf8Lossy(text));
  }
}

void ReadNames(const formats::LuaTable& globals, StatusTable& into) {
  const formats::LuaValue* table = globals.Find(kStatusNameGlobal);
  if (table == nullptr || table->table == nullptr) {
    return;
  }
  for (const auto& [key, entry] : table->table->by_index) {
    const std::optional<std::uint32_t> effect_id =
        formats::LuaWholeNumber(key, std::numeric_limits<std::uint32_t>::max());
    if (!effect_id.has_value()) {
      continue;
    }
    const std::string_view title = TitleOf(entry);
    if (title.empty()) {
      continue;
    }
    // ⚠ TRANSCODÉ EN TOLÉRANT, comme les noms d'objets et de métiers : ces
    // tables viennent d'une traduction communautaire, et un octet douteux dans
    // un nom ne doit pas coûter la table entière.
    StatusInfo& info = into.by_id[*effect_id];
    info.name = Cp949ToUtf8Lossy(title);
    ReadDescription(entry, info);
  }
}

// Les images, rangées PAR SEAU DE PRIORITÉ : cinq tables, chacune nommant des
// statuts. Le seau n'est pas un rangement décoratif — c'est l'ordre dans lequel
// le client montre ce qui ne tient pas.
void ReadIcons(const formats::LuaTable& globals, StatusTable& into) {
  const formats::LuaValue* buckets = globals.Find(kStatusIconGlobal);
  if (buckets == nullptr || buckets->table == nullptr) {
    return;
  }
  for (const auto& [rank, bucket] : buckets->table->by_index) {
    if (bucket.table == nullptr || rank < 0.0 || rank >= static_cast<double>(kStatusPriorities)) {
      continue;
    }
    const auto priority = static_cast<StatusPriority>(static_cast<std::uint8_t>(rank));
    for (const auto& [key, file] : bucket.table->by_index) {
      const std::optional<std::uint32_t> effect_id =
          formats::LuaWholeNumber(key, std::numeric_limits<std::uint32_t>::max());
      if (!effect_id.has_value() || file.kind != formats::LuaValue::Kind::kString) {
        continue;
      }
      StatusInfo& info = into.by_id[*effect_id];
      info.icon_file = Cp949ToUtf8Lossy(file.text);
      info.priority = priority;
    }
  }
}

}  // namespace

const StatusInfo* StatusTable::Find(std::uint32_t effect_id) const {
  const auto found = by_id.find(effect_id);
  return found != by_id.end() ? &found->second : nullptr;
}

std::string_view StatusTable::NameOf(std::uint32_t effect_id) const {
  const StatusInfo* found = Find(effect_id);
  return found != nullptr ? std::string_view(found->name) : std::string_view();
}

std::size_t StatusTable::drawn() const noexcept {
  std::size_t count = 0;
  for (const auto& [effect_id, info] : by_id) {
    if (!info.icon_file.empty()) {
      ++count;
    }
  }
  return count;
}

core::Result<StatusTable> ReadStatusTable(Vfs& vfs) {
  StatusTable table;
  formats::LuaTable globals;

  for (const std::string_view path : kStatusTableFiles) {
    auto read = ReadInto(vfs, path, globals);
    if (!read) {
      return std::unexpected(read.error());
    }
    if (!*read) {
      // ⚠ UN FICHIER MANQUANT ARRÊTE LA CHAÎNE MAIS NE LA JETTE PAS : ce qui a
      // déjà été lu vaut mieux que rien — des noms sans images se montrent
      // encore, et c'est ce que fait le client quand une table lui manque.
      break;
    }
  }

  ReadNames(globals, table);
  ReadIcons(globals, table);
  return table;
}

}  // namespace ro::vfs
