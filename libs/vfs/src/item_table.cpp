#include "ro/vfs/item_table.h"

#include <cstdint>
#include <expected>
#include <string>
#include <utility>
#include <vector>

#include "ro/vfs/encoding.h"

#include "item_info_text.h"

namespace ro::vfs {
namespace {

core::Error Refused(std::string detail) {
  return core::Error{core::ErrorCode::kCorruptData, "table des objets : " + std::move(detail)};
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
// ⚠ ICI ET NON AU RENDU : ce module rend les lignes que la fonction d'ajout du
// client aurait produites, et tout ce qui les affiche — fenêtre, aperçu,
// infobulle — les reçoit déjà justes. Laissé au rendu, chaque afficheur aurait
// sa version de la règle, et l'un d'eux l'oublierait.
void AppendLines(std::vector<std::string>&& told, std::vector<std::string>& lines) {
  for (std::string& shown : told) {
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

// L'objet tel que le client le montre, ou la raison de son refus.
[[nodiscard]] core::Result<ItemEntry> Described(ItemInfoRecord&& record) {
  // ⚠ L'ALLER-RETOUR SE VÉRIFIE : un caractère que le CP949 n'a pas peut se
  // remplacer en silence par un caractère de repli, et la ressource désignerait
  // alors un autre fichier.
  auto archived = Utf8ToCp949(record.resource);
  const auto back = archived ? Cp949ToUtf8(*archived) : archived;
  if (!back || *back != record.resource) {
    return std::unexpected(
        Refused("objet " + std::to_string(record.item_id) + " : ressource intraduisible en CP949"));
  }
  ItemEntry described;
  described.name = std::move(record.name);
  described.resource = std::move(*archived);
  described.view_id = record.view;
  described.card_slots = record.slots;
  described.costume = record.costume;
  // ⚠ LES TROIS LIGNES DU SCRIPT AVANT CELLES DE LA TABLE : voir l'en-tête.
  described.description = HeaderLines(record.item_id, record.view);
  AppendLines(std::move(record.description), described.description);
  return described;
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

core::Result<ItemTable> ParseItemTable(std::string_view yaml_text) {
  ItemTable table;
  auto read = ReadItemInfoText(yaml_text, [&table](ItemInfoRecord&& record) -> core::Result<void> {
    const std::uint32_t item_id = record.item_id;
    auto described = Described(std::move(record));
    if (!described) {
      return std::unexpected(described.error());
    }
    table.Put(item_id, std::move(*described));
    return {};
  });
  if (!read) {
    return std::unexpected(read.error());
  }
  return table;
}

core::Result<ItemTable> ReadItemTable(Vfs& vfs) {
  auto bytes = vfs.Read(std::string(kItemInfoPath));
  if (!bytes) {
    if (bytes.error().code == core::ErrorCode::kNotFound) {
      return ItemTable{};
    }
    return std::unexpected(bytes.error());
  }
  const std::string_view text(reinterpret_cast<const char*>(bytes->data()), bytes->size());
  return ParseItemTable(text);
}

}  // namespace ro::vfs
