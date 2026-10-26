#include "survivors/survivors_codex_keys.h"

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "survivors/survivors_gems.h"

namespace moonclient {
namespace {

constexpr std::array<const char*, kCodexCategoryCount> kCategoryKeys{"skills", "foes", "plants",
                                                                     "items", "events"};

struct ItemKey {
  ro::sim::PickupKind kind;
  const char* key;
};
constexpr std::array<ItemKey, 8> kItemKeys{{
    {ro::sim::PickupKind::kChest, "chest"},
    {ro::sim::PickupKind::kMeat, "meat"},
    {ro::sim::PickupKind::kGold, "gold"},
    {ro::sim::PickupKind::kManual, "manual"},
    {ro::sim::PickupKind::kHaste, "haste"},
    {ro::sim::PickupKind::kHolyWater, "holy_water"},
    {ro::sim::PickupKind::kMagnetAll, "magnet_all"},
    {ro::sim::PickupKind::kCard, "card"},
}};

struct GemKey {
  std::uint16_t item_id;
  const char* key;
};
constexpr std::array<GemKey, 4> kGemKeys{{
    {kBlueGemstoneItem, "blue_gem"},
    {kYellowGemstoneItem, "yellow_gem"},
    {kRedGemstoneItem, "red_gem"},
    {kEmperiumItem, "emperium_gem"},
}};

struct EventKindKey {
  ro::sim::EventKind kind;
  const char* key;
};
constexpr std::array<EventKindKey, 4> kEventKindKeys{{
    {ro::sim::EventKind::kSwarm, "swarm"},
    {ro::sim::EventKind::kRaid, "raid"},
    {ro::sim::EventKind::kRing, "ring"},
    {ro::sim::EventKind::kGroup, "group"},
}};

// Les chiffres d'un identifiant RO : cinq au plus pour 16 bits.
constexpr std::size_t kMostIdDigits = 5;

[[nodiscard]] std::string DecimalOf(std::uint16_t value) {
  std::array<char, kMostIdDigits> digits{};
  const auto written = std::to_chars(digits.data(), digits.data() + digits.size(), value);
  return {digits.data(), written.ptr};
}

}  // namespace

const char* CodexCategoryKey(CodexCategory category) noexcept {
  return kCategoryKeys[static_cast<std::size_t>(category)];
}

std::optional<CodexCategory> CodexCategoryOf(std::string_view key) noexcept {
  for (std::size_t index = 0; index < kCategoryKeys.size(); ++index) {
    if (key == kCategoryKeys[index]) return static_cast<CodexCategory>(index);
  }
  return std::nullopt;
}

std::string CodexSkillKey(const ro::sim::SurvivorsSkillRule& rule) { return rule.key; }

std::string CodexFoeKey(std::uint16_t foe_id) { return DecimalOf(foe_id); }

std::string CodexPlantKey(std::uint16_t foe_id) { return DecimalOf(foe_id); }

const char* CodexItemKey(ro::sim::PickupKind kind) noexcept {
  for (const ItemKey& known : kItemKeys) {
    if (known.kind == kind) return known.key;
  }
  return kItemKeys.back().key;
}

const char* CodexGemKey(std::uint16_t item_id) noexcept {
  for (const GemKey& known : kGemKeys) {
    if (known.item_id == item_id) return known.key;
  }
  return "";
}

std::string CodexEventKey(const ro::sim::EventRule& event) {
  const char* kind_key = kEventKindKeys.front().key;
  for (const EventKindKey& known : kEventKindKeys) {
    if (known.kind == event.kind) kind_key = known.key;
  }
  return std::string(kind_key) + "_" + DecimalOf(event.foe_id);
}

}  // namespace moonclient
