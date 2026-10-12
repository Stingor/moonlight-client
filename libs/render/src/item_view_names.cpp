// Ce que désigne l'apparence d'un objet. Voir l'en-tête.

#include "ro/render/item_view_names.h"

#include <limits>
#include <string_view>

#include "ro/formats/lua_table.h"

#include "client_tables.h"
#include "sprite_catalog_internal.h"

namespace ro::render {
namespace {

// Le souligné qui joint un nom de sprite au gabarit de son chemin.
constexpr char kTemplateJoint = '_';

[[nodiscard]] std::string WithoutJoint(std::string_view name) {
  if (!name.empty() && name.front() == kTemplateJoint) {
    name.remove_prefix(1);
  }
  return std::string(name);
}

// Le premier nom que la table des identifiants donne au numéro, dans l'ordre
// du texte, ou vide.
[[nodiscard]] std::string SymbolOf(const SpriteCatalog& catalog, std::string_view table_name,
                                   std::uint16_t view_id) {
  const formats::LuaValue* table = catalog.globals.Find(table_name);
  if (table == nullptr || table->table == nullptr) {
    return {};
  }
  for (const auto& [name, value] : table->table->by_name) {
    if (value.kind != formats::LuaValue::Kind::kNumber) {
      continue;
    }
    const auto number =
        formats::LuaWholeNumber(value.number, std::numeric_limits<std::uint16_t>::max());
    if (number.has_value() && *number == view_id) {
      return name;
    }
  }
  return {};
}

// Le nom de sprite et le nom symbolique d'un numéro, lus dans leurs deux
// tables.
[[nodiscard]] std::optional<ViewMeaning> Named(const SpriteCatalog& catalog,
                                               std::string_view sprites, std::string_view symbols,
                                               std::uint16_t view_id) {
  ViewMeaning meaning{
      .symbol = SymbolOf(catalog, symbols, view_id),
      .sprite = WithoutJoint(
          catalog_internal::TableName(catalog, sprites, view_id).value_or(std::string()))};
  if (meaning.sprite.empty() && meaning.symbol.empty()) {
    return std::nullopt;
  }
  return meaning;
}

}  // namespace

std::optional<ViewMeaning> DescribeItemView(const SpriteCatalog& catalog, ViewedPiece piece,
                                            std::uint16_t view_id) {
  switch (piece) {
    case ViewedPiece::kNothing:
      return std::nullopt;
    case ViewedPiece::kWeapon:
      return Named(catalog, kWeaponNameTableGlobal, kWeaponIdsGlobal, view_id);
    case ViewedPiece::kHeadgear:
      return Named(catalog, kAccessoryNameTableGlobal, kAccessoryIdsGlobal, view_id);
    case ViewedPiece::kGarment:
      return Named(catalog, kRobeNameTableGlobal, kRobeIdsGlobal, view_id);
    case ViewedPiece::kShield: {
      // La forme zéro est l'absence de forme : son nom est vide.
      const auto& names = client_tables::kShieldNames;
      if (view_id >= names.size() || names[view_id].empty()) {
        return std::nullopt;
      }
      return ViewMeaning{.symbol = {}, .sprite = WithoutJoint(names[view_id])};
    }
  }
  return std::nullopt;
}

}  // namespace ro::render
