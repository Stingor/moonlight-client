#include "ro/render/hat_effect_table.h"

#include <algorithm>
#include <utility>

#include "ro/formats/lua_table.h"

namespace ro::render {
namespace {

// Les noms des champs, tels que la table les écrit.
constexpr std::string_view kFile = "resourceFileName";
constexpr std::string_view kEffectId = "hatEffectID";
constexpr std::string_view kOffset = "hatEffectPos";
constexpr std::string_view kOffsetX = "hatEffectPosX";
constexpr std::string_view kAdjustPosition = "isAdjustPositionWhenShrinkState";
constexpr std::string_view kAdjustSize = "isAdjustSizeWhenShrinkState";
constexpr std::string_view kRenderBefore = "isRenderBeforeCharacter";
constexpr std::string_view kIgnoreRiding = "isIgnoreRiding";
constexpr std::string_view kAttachedHead = "isAttachedHead";
constexpr std::string_view kEffectPair = "isEffectPair";

[[nodiscard]] float Number(const formats::LuaTable& record, std::string_view field) {
  const formats::LuaValue* value = record.Find(field);
  if (value == nullptr || value->kind != formats::LuaValue::Kind::kNumber) {
    return 0.0F;
  }
  return static_cast<float>(value->number);
}

// ⚠ UN DRAPEAU ABSENT VAUT FAUX, et c'est ainsi que la table est écrite : aucune
// fiche ne porte `false`, elles omettent le champ. Le lire autrement rendrait
// 259 fiches toutes pareilles.
[[nodiscard]] bool Flag(const formats::LuaTable& record, std::string_view field) {
  const formats::LuaValue* value = record.Find(field);
  return value != nullptr && value->kind == formats::LuaValue::Kind::kBoolean && value->boolean;
}

[[nodiscard]] std::string Text(const formats::LuaTable& record, std::string_view field) {
  const formats::LuaValue* value = record.Find(field);
  if (value == nullptr || value->kind != formats::LuaValue::Kind::kString) {
    return {};
  }
  return value->text;
}

}  // namespace

core::Result<HatEffectTable> HatEffectTable::Load(vfs::Vfs& vfs) {
  formats::LuaTable globals;
  // ⚠ L'ORDRE COMPTE, et il n'est pas une commodité : la seconde table indexe
  // ses fiches par `HatEFID.QUELQUE_CHOSE`, que la première nomme. Lue seule,
  // elle rend des clés nulles — et le lecteur le REFUSE plutôt que de deviner.
  for (const std::string_view path : {kHatEffectIdsPath, kHatEffectInfoPath}) {
    auto bytes = vfs.Read(path);
    if (!bytes) {
      return std::unexpected(bytes.error());
    }
    auto read = formats::ReadLuaFile(*bytes, std::move(globals));
    if (!read) {
      return std::unexpected(read.error());
    }
    globals = std::move(*read);
  }
  return Parse(globals);
}

core::Result<HatEffectTable> HatEffectTable::Parse(const formats::LuaTable& globals) {
  const formats::LuaValue* table = globals.Find(kHatEffectTableGlobal);
  if (table == nullptr || table->kind != formats::LuaValue::Kind::kTable ||
      table->table == nullptr) {
    return core::Fail(core::ErrorCode::kCorruptData, std::string(kHatEffectTableGlobal) +
                                                         " n'est pas une table dans " +
                                                         std::string(kHatEffectInfoPath));
  }

  HatEffectTable loaded;
  loaded.entries_.reserve(table->table->by_index.size());
  for (const auto& [index, value] : table->table->by_index) {
    if (value.kind != formats::LuaValue::Kind::kTable || value.table == nullptr) {
      continue;
    }
    const formats::LuaTable& record = *value.table;
    HatEffect effect;
    effect.hat_effect_id = static_cast<std::uint16_t>(index);
    effect.file = Text(record, kFile);
    effect.effect_id = static_cast<std::uint32_t>(Number(record, kEffectId));
    effect.offset = Number(record, kOffset);
    effect.offset_x = Number(record, kOffsetX);
    effect.adjust_position_when_shrunk = Flag(record, kAdjustPosition);
    effect.adjust_size_when_shrunk = Flag(record, kAdjustSize);
    // ⚠ SANS DÉCALAGE VERTICAL, LE DRAPEAU NE COMPTE PAS : le client ne le lit
    // que sur une fiche qui nomme `hatEffectPos`, et répond faux pour les autres.
    effect.render_before_character = Flag(record, kRenderBefore) && record.Find(kOffset) != nullptr;
    effect.ignore_riding = Flag(record, kIgnoreRiding);
    effect.attached_head = Flag(record, kAttachedHead);
    effect.effect_pair = Flag(record, kEffectPair);
    loaded.entries_.push_back(std::move(effect));
  }
  std::ranges::sort(loaded.entries_, {}, &HatEffect::hat_effect_id);
  return loaded;
}

const HatEffect* HatEffectTable::FindByConcreteId(std::uint32_t effect_id) const {
  if (effect_id < kHatEffectIdBase) {
    return nullptr;
  }
  return Find(effect_id - kHatEffectIdBase);
}

const HatEffect* HatEffectTable::Find(std::uint32_t hat_effect_id) const {
  const auto found = std::ranges::lower_bound(entries_, hat_effect_id, {},
                                              [](const HatEffect& e) { return e.hat_effect_id; });
  if (found == entries_.end() || found->hat_effect_id != hat_effect_id) {
    return nullptr;
  }
  return &*found;
}

}  // namespace ro::render
