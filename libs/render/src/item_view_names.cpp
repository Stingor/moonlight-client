// Ce que désigne l'apparence d'un objet. Voir l'en-tête.

#include "ro/render/item_view_names.h"

#include <string>
#include <string_view>
#include <unordered_map>

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

using NameTable = std::unordered_map<std::uint32_t, std::string>;

// Le nom de sprite et le nom symbolique d'un numéro, lus dans leurs deux
// tables.
[[nodiscard]] std::optional<ViewMeaning> Named(const NameTable& sprites, const NameTable& symbols,
                                               std::uint16_t view_id) {
  ViewMeaning meaning{
      .symbol = catalog_internal::TableName(symbols, view_id).value_or(std::string()),
      .sprite =
          WithoutJoint(catalog_internal::TableName(sprites, view_id).value_or(std::string()))};
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
      return Named(catalog.tables.weapon_sprites, catalog.tables.weapon_symbols, view_id);
    case ViewedPiece::kHeadgear:
      return Named(catalog.tables.headgear_sprites, catalog.tables.headgear_symbols, view_id);
    case ViewedPiece::kGarment:
      return Named(catalog.tables.garment_sprites, catalog.tables.garment_symbols, view_id);
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
