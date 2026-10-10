// La taille et les effets d'office de certains monstres. Voir l'en-tête.

#include "ro/vfs/monster_size_effects.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace ro::vfs {
namespace {

// Un numéro de classe ou d'effet voyage en trente-deux bits signés.
constexpr std::uint32_t kMostNumber =
    static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max());

// Les effets d'une entrée, dans l'ordre de la table. ⚠ UN NOMBRE N'EST PAS UNE
// LISTE : le client ne pose alors aucun effet, et `EF_NONE` vaut moins un.
[[nodiscard]] std::vector<std::uint32_t> EffectsOf(const formats::LuaTable& entry) {
  std::vector<std::uint32_t> effects;
  const formats::LuaValue* list = entry.Find(kMonsterEffectField);
  if (list == nullptr || list->kind != formats::LuaValue::Kind::kTable || list->table == nullptr) {
    return effects;
  }
  for (const auto& [index, value] : list->table->by_index) {
    if (value.kind != formats::LuaValue::Kind::kNumber) {
      continue;
    }
    if (const std::optional<std::uint32_t> effect =
            formats::LuaWholeNumber(value.number, kMostNumber);
        effect.has_value()) {
      effects.push_back(*effect);
    }
  }
  return effects;
}

}  // namespace

core::Result<MonsterSizeEffects> MonsterSizeEffects::Load(Vfs& vfs) {
  auto bytes = vfs.Read(kMonsterSizeEffectPath);
  if (!bytes) {
    return std::unexpected(bytes.error());
  }
  // ⚠ LE FICHIER SE TERMINE PAR `main`, qui déclare chaque entrée au client. Le
  // lecteur la saute : la donnée est dans la table.
  auto read = formats::ReadLuaFile(*bytes, formats::LuaTable{});
  if (!read) {
    return std::unexpected(read.error());
  }
  return Parse(*read);
}

core::Result<MonsterSizeEffects> MonsterSizeEffects::Parse(const formats::LuaTable& globals) {
  const formats::LuaValue* table = globals.Find(kMonsterSizeEffectGlobal);
  if (table == nullptr || table->kind != formats::LuaValue::Kind::kTable ||
      table->table == nullptr) {
    return core::Fail(core::ErrorCode::kCorruptData, std::string(kMonsterSizeEffectGlobal) +
                                                         " n'est pas une table dans " +
                                                         std::string(kMonsterSizeEffectPath));
  }
  MonsterSizeEffects loaded;
  for (const auto& [index, value] : table->table->by_index) {
    const std::optional<std::uint32_t> monster_id = formats::LuaWholeNumber(index, kMostNumber);
    if (!monster_id.has_value() || value.kind != formats::LuaValue::Kind::kTable ||
        value.table == nullptr) {
      continue;
    }
    // ⚠ LA TAILLE EST EXIGÉE, comme chez le client, qui refuse l'entrée sans
    // elle avant même de lire ses effets.
    const formats::LuaValue* size = value.table->Find(kMonsterSizeField);
    if (size == nullptr || size->kind != formats::LuaValue::Kind::kNumber ||
        !std::isfinite(size->number)) {
      continue;
    }
    loaded.entries_.push_back(Entry{.monster_id = *monster_id,
                                    .size = static_cast<float>(size->number),
                                    .effect_ids = EffectsOf(*value.table)});
  }
  std::ranges::sort(loaded.entries_, {}, &Entry::monster_id);
  return loaded;
}

const MonsterSizeEffects::Entry* MonsterSizeEffects::For(std::uint32_t monster_id) const {
  const auto found = std::ranges::lower_bound(entries_, monster_id, {}, &Entry::monster_id);
  if (found == entries_.end() || found->monster_id != monster_id) {
    return nullptr;
  }
  return &*found;
}

float MonsterSizeEffects::ScaleOf(std::uint32_t monster_id) const {
  const Entry* entry = For(monster_id);
  return entry == nullptr || entry->size <= 0.0F ? 1.0F : entry->size;
}

}  // namespace ro::vfs
