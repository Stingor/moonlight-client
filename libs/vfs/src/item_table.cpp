#include "ro/vfs/item_table.h"

#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ro/formats/lua_table.h"
#include "ro/vfs/encoding.h"

namespace ro::vfs {
namespace {

// Les champs de la table, aux noms que le client leur donne.
//
// ⚠ `ViewID` EST CE QUE LA TABLE D'ORIGINE NOMME `ClassNum`. Le fichier fusionné
// de ce serveur l'a renommé ; chercher `ClassNum` ne rendrait rien.
constexpr std::string_view kNameField = "Name";
constexpr std::string_view kResourceField = "Res";
constexpr std::string_view kDescriptionField = "Desc";
constexpr std::string_view kViewField = "ViewID";
constexpr std::string_view kSlotField = "Slot";
constexpr std::string_view kCostumeField = "costume";

[[nodiscard]] const formats::LuaValue* FieldOf(const formats::LuaTable& entry,
                                               std::string_view name) {
  return entry.Find(name);
}

[[nodiscard]] std::string TextOf(const formats::LuaTable& entry, std::string_view name) {
  const formats::LuaValue* value = FieldOf(entry, name);
  if (value == nullptr || value->kind != formats::LuaValue::Kind::kString) {
    return {};
  }
  return value->text;
}

[[nodiscard]] double NumberOf(const formats::LuaTable& entry, std::string_view name) {
  const formats::LuaValue* value = FieldOf(entry, name);
  if (value == nullptr || value->kind != formats::LuaValue::Kind::kNumber) {
    return 0.0;
  }
  return value->number;
}

[[nodiscard]] bool FlagOf(const formats::LuaTable& entry, std::string_view name) {
  const formats::LuaValue* value = FieldOf(entry, name);
  return value != nullptr && value->kind == formats::LuaValue::Kind::kBoolean && value->boolean;
}

// LE TRAIT DE SÉPARATION DU CLIENT : vingt-quatre soulignés, ni plus ni moins.
//
// 🔴 CE N'EST PAS UN DESSIN MAIS DU TEXTE, et c'est le script du client qui
// l'écrit. Un vrai filet — pleine largeur, épaisseur du thème — ne serait pas ce
// que le joueur voit : le sien s'arrête net au bout de vingt-quatre caractères,
// et il se sélectionne et se copie avec le reste.
constexpr std::string_view kDescriptionRule = "________________________";

// Les lignes de description, telles que le CLIENT les pose.
//
// 🔴 UN `#` EN FIN DE LIGNE N'EST PAS DU TEXTE : c'est la marque d'un trait de
// séparation, et le script de chargement du client la traite ainsi —
//
//     if string.find(v, '#', -1) then
//         AddItemIdentifiedDesc(ItemID, v:sub(1, -2))
//         AddItemIdentifiedDesc(ItemID, "________________________")
//     else
//         AddItemIdentifiedDesc(ItemID, v)
//     end
//
// La ligne perd donc son dernier caractère, et une ligne de soulignés la suit.
//
// ⚠ ICI ET NON AU RENDU, parce que c'est ici que le client le fait : ce module
// rend les lignes que sa fonction d'ajout aurait produites, et tout ce qui les
// affiche — fenêtre, aperçu, infobulle — les reçoit déjà justes. Laissé au
// rendu, chaque afficheur aurait sa version de la règle, et l'un d'eux
// l'oublierait.
[[nodiscard]] std::vector<std::string> LinesOf(const formats::LuaTable& entry) {
  std::vector<std::string> lines;
  const formats::LuaValue* value = FieldOf(entry, kDescriptionField);
  if (value == nullptr || value->kind != formats::LuaValue::Kind::kTable ||
      value->table == nullptr) {
    return lines;
  }
  lines.reserve(value->table->by_index.size());
  for (const auto& [rank, line] : value->table->by_index) {
    if (line.kind != formats::LuaValue::Kind::kString) {
      continue;
    }
    std::string shown = Cp949ToUtf8Lossy(line.text);
    // ⚠ EN DERNIÈRE POSITION SEULEMENT : un « # » au milieu d'une phrase est un
    // dièse, et le script du client ne regarde lui aussi que le dernier
    // caractère.
    const bool ruled = !shown.empty() && shown.back() == '#';
    if (ruled) {
      shown.pop_back();
    }
    lines.push_back(std::move(shown));
    if (ruled) {
      lines.emplace_back(kDescriptionRule);
    }
  }
  return lines;
}

// LES TROIS LIGNES QUE LE SCRIPT DE CHARGEMENT POSE EN TÊTE. Voir l'en-tête.
[[nodiscard]] std::vector<std::string> HeaderLines(std::uint32_t item_id, std::uint16_t view_id) {
  const std::string number = std::to_string(item_id);
  std::vector<std::string> lines;
  std::string link(kDatabaseLinkPrefix);
  link += number;
  link += kDatabaseLinkInfo;
  link += ItemDatabaseUrl(item_id);
  link += kDatabaseLinkSuffix;
  lines.push_back(std::move(link));
  // ⚠ SEULEMENT S'IL Y EN A UNE : le script teste `ViewID > 0`, et une apparence
  // nulle veut dire « rien à montrer » — l'écrire serait une ligne de bruit sur
  // la moitié des objets.
  if (view_id > 0) {
    lines.push_back("ViewID : " + std::to_string(view_id));
  }
  lines.emplace_back(kDescriptionGap);
  return lines;
}

}  // namespace

void ItemTable::Put(std::uint32_t item_id, ItemEntry entry) { by_id_[item_id] = std::move(entry); }

const ItemEntry* ItemTable::Find(std::uint32_t item_id) const {
  const auto found = by_id_.find(item_id);
  return found != by_id_.end() ? &found->second : nullptr;
}

std::uint16_t ItemTable::ViewOf(std::uint32_t item_id) const {
  const ItemEntry* entry = Find(item_id);
  return entry != nullptr ? entry->view_id : 0;
}

std::string_view ItemTable::ResourceOf(std::uint32_t item_id) const {
  const ItemEntry* entry = Find(item_id);
  return entry != nullptr ? std::string_view(entry->resource) : std::string_view();
}

std::string ItemDatabaseUrl(std::uint32_t item_id) {
  return std::string(kDatabasePageUrl) + std::to_string(item_id);
}

core::Result<ItemTable> ReadItemTable(Vfs& vfs) {
  ItemTable table;
  auto bytes = vfs.Read(std::string(kItemTablePath));
  if (!bytes) {
    if (bytes.error().code == core::ErrorCode::kNotFound) {
      return table;
    }
    return std::unexpected(bytes.error());
  }

  // ⚠ ÉVALUÉE SEULE, sans les globales du catalogue de sprites : elle porte des
  // dizaines de milliers d'entrées, et les mêler ferait porter ce poids à tout
  // ce qui lit une autre table.
  auto chunk = formats::ReadLuaFile(*bytes);
  if (!chunk) {
    return std::unexpected(core::Error{chunk.error().code,
                                       std::string(kItemTablePath) + " : " + chunk.error().detail});
  }

  const formats::LuaValue* items = chunk->Find(kItemTableGlobal);
  if (items == nullptr || items->table == nullptr) {
    return table;
  }
  for (const auto& [key, entry] : items->table->by_index) {
    const std::optional<std::uint32_t> item_id =
        formats::LuaWholeNumber(key, std::numeric_limits<std::uint32_t>::max());
    if (!item_id.has_value() || entry.kind != formats::LuaValue::Kind::kTable ||
        entry.table == nullptr) {
      continue;
    }
    ItemEntry described;
    described.name = Cp949ToUtf8Lossy(TextOf(*entry.table, kNameField));
    described.resource = TextOf(*entry.table, kResourceField);
    if (const std::optional<std::uint32_t> view = formats::LuaWholeNumber(
            NumberOf(*entry.table, kViewField), std::numeric_limits<std::uint16_t>::max())) {
      described.view_id = static_cast<std::uint16_t>(*view);
    }
    // ⚠ LES TROIS LIGNES DU SCRIPT AVANT CELLES DE LA TABLE : voir l'en-tête.
    // C'est le seul endroit qui connaisse à la fois l'identifiant, l'apparence
    // et le texte.
    described.description = HeaderLines(*item_id, described.view_id);
    std::vector<std::string> told = LinesOf(*entry.table);
    described.description.insert(described.description.end(), std::make_move_iterator(told.begin()),
                                 std::make_move_iterator(told.end()));
    if (const std::optional<std::uint32_t> slots = formats::LuaWholeNumber(
            NumberOf(*entry.table, kSlotField), std::numeric_limits<std::uint8_t>::max())) {
      described.card_slots = static_cast<std::uint8_t>(*slots);
    }
    described.costume = FlagOf(*entry.table, kCostumeField);
    table.Put(*item_id, std::move(described));
  }
  return table;
}

}  // namespace ro::vfs
