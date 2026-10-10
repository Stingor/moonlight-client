#include "ro/render/effect_life_table.h"

#include <algorithm>
#include <ranges>

#include "ro/render/effect_case_table.h"
#include "ro/render/effect_frame.h"
#include "ro/render/effect_node.h"

namespace ro::render {
namespace {

// L'entrée de cet identifiant dans une table triée, ou rien.
[[nodiscard]] const EffectLifeEntry* EntryIn(std::span<const EffectLifeEntry> table,
                                             std::uint32_t effect_id) {
  const auto found = std::ranges::lower_bound(table, effect_id, {}, &EffectLifeEntry::effect_id);
  if (found == table.end() || found->effect_id != effect_id) {
    return nullptr;
  }
  return &*found;
}

}  // namespace

std::int32_t EffectNodeLifeFor(std::uint32_t effect_id) {
  const EffectLifeEntry* entry = EntryIn(EffectLifeTable(), effect_id);
  return entry != nullptr ? entry->life_ticks : kEffectNodeDefaultLifeTicks;
}

bool EffectNodeLifeIsKnown(std::uint32_t effect_id) {
  return EntryIn(EffectLifeTable(), effect_id) != nullptr;
}

bool EffectInstanceLifeIsKnown(std::uint32_t effect_id) {
  return EffectNodeLifeIsKnown(effect_id) ||
         EntryIn(EffectInstanceOnlyLifeTable(), effect_id) != nullptr;
}

std::int32_t EffectInstanceLifeFor(std::uint32_t effect_id) {
  if (const EffectLifeEntry* entry = EntryIn(EffectLifeTable(), effect_id); entry != nullptr) {
    return entry->life_ticks;
  }
  const EffectLifeEntry* entry = EntryIn(EffectInstanceOnlyLifeTable(), effect_id);
  return entry != nullptr ? entry->life_ticks : kEffectNodeDefaultLifeTicks;
}

std::int32_t EffectLifeUnderCast(std::uint32_t effect_id, std::uint32_t cast_ms) {
  // LE PLANCHER DE CHAQUE BÂTISSEUR qui recopie la vie de son instance : les
  // anneaux à quatre pistes la relèvent à cinquante-six, les anneaux en couches
  // à soixante-dix.
  constexpr std::int32_t kRingsOfFourFloorTicks = 56;
  constexpr std::int32_t kLayeredRingsFloorTicks = 70;
  const EffectCaseEntry* found = EffectCaseFor(effect_id);
  if (found == nullptr) {
    return 0;
  }
  std::int32_t floor_ticks = 0;
  if (found->client_function == "Effect_Build_Rings4FromParam1e") {
    floor_ticks = kRingsOfFourFloorTicks;
  } else if (found->client_function == "Effect_Build_LayeredRingNode") {
    floor_ticks = kLayeredRingsFloorTicks;
  } else {
    return 0;
  }
  const auto cast_ticks =
      static_cast<std::int32_t>(cast_ms / static_cast<std::uint32_t>(kEffectTickMs));
  return std::max(cast_ticks, floor_ticks);
}

}  // namespace ro::render
