#include "items/item_enchant_book.h"

#include <algorithm>
#include <exception>
#include <string>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "ro/core/log.h"

namespace moonclient {
namespace {

constexpr int kSupportedVersion = 1;

constexpr const char* kVersionField = "version";
constexpr const char* kGroupsSection = "groups";
constexpr const char* kIdField = "id";
constexpr const char* kTargetsField = "targets";
constexpr const char* kLeastRefineField = "least_refine";
constexpr const char* kLeastGradeField = "least_grade";
constexpr const char* kRandomOptionsField = "random_options";
constexpr const char* kOrderField = "order";
constexpr const char* kResetField = "reset";
constexpr const char* kSlotsField = "slots";
constexpr const char* kSlotField = "slot";
constexpr const char* kZenyField = "zeny";
constexpr const char* kMaterialsField = "materials";
constexpr const char* kChanceField = "chance";
constexpr const char* kDrawsField = "draws";
constexpr const char* kGradeField = "grade";
constexpr const char* kEnchantsField = "enchants";
constexpr const char* kPerfectField = "perfect";
constexpr const char* kUpgradesField = "upgrades";

// `[objet, quantité]` et `[enchantement, probabilité]` : deux nœuds.
constexpr std::size_t kPairSize = 2;
// `[enchantement, zeny, matériaux]`.
constexpr std::size_t kPerfectSize = 3;
// `[enchantement, amélioration, zeny, matériaux]`.
constexpr std::size_t kUpgradeSize = 4;

[[nodiscard]] ro::core::Error Refused(std::string detail) {
  return ro::core::Error{ro::core::ErrorCode::kCorruptData, "item_enchant : " + std::move(detail)};
}

[[nodiscard]] std::vector<ro::sim::ItemEnchantMaterial> MaterialsOf(const YAML::Node& node) {
  std::vector<ro::sim::ItemEnchantMaterial> materials;
  if (!node || !node.IsSequence()) {
    return materials;
  }
  for (const YAML::Node& pair : node) {
    if (pair.IsSequence() && pair.size() == kPairSize) {
      materials.push_back(ro::sim::ItemEnchantMaterial{.item_id = pair[0].as<std::uint32_t>(0),
                                                       .amount = pair[1].as<std::uint16_t>(1)});
    }
  }
  return materials;
}

[[nodiscard]] ro::sim::ItemEnchantCost CostOf(const YAML::Node& zeny, const YAML::Node& materials) {
  return ro::sim::ItemEnchantCost{.zeny = zeny.as<std::int64_t>(0),
                                  .materials = MaterialsOf(materials)};
}

[[nodiscard]] ro::sim::ItemEnchantSlot SlotOf(const YAML::Node& entry) {
  ro::sim::ItemEnchantSlot slot{
      .slot = static_cast<std::uint8_t>(entry[kSlotField].as<unsigned>(0)),
      .cost = CostOf(entry[kZenyField], entry[kMaterialsField]),
      .chance = entry[kChanceField].as<std::uint32_t>(ro::sim::kItemEnchantCertain)};
  if (const YAML::Node draws = entry[kDrawsField]; draws && draws.IsSequence()) {
    for (const YAML::Node& grade : draws) {
      ro::sim::ItemEnchantDrawGrade drawn{
          .grade = static_cast<std::uint8_t>(grade[kGradeField].as<unsigned>(0))};
      if (const YAML::Node enchants = grade[kEnchantsField]; enchants && enchants.IsSequence()) {
        for (const YAML::Node& pair : enchants) {
          if (pair.IsSequence() && pair.size() == kPairSize) {
            drawn.enchants.push_back(ro::sim::ItemEnchantDraw{
                .item_id = pair[0].as<std::uint32_t>(0), .odds = pair[1].as<std::uint32_t>(0)});
          }
        }
      }
      slot.draws.push_back(std::move(drawn));
    }
  }
  if (const YAML::Node perfect = entry[kPerfectField]; perfect && perfect.IsSequence()) {
    for (const YAML::Node& row : perfect) {
      if (row.IsSequence() && row.size() == kPerfectSize) {
        slot.perfect.push_back(ro::sim::ItemEnchantPerfect{.item_id = row[0].as<std::uint32_t>(0),
                                                           .cost = CostOf(row[1], row[2])});
      }
    }
  }
  if (const YAML::Node upgrades = entry[kUpgradesField]; upgrades && upgrades.IsSequence()) {
    for (const YAML::Node& row : upgrades) {
      if (row.IsSequence() && row.size() == kUpgradeSize) {
        slot.upgrades.push_back(
            ro::sim::ItemEnchantUpgrade{.from_item_id = row[0].as<std::uint32_t>(0),
                                        .to_item_id = row[1].as<std::uint32_t>(0),
                                        .cost = CostOf(row[2], row[3])});
      }
    }
  }
  return slot;
}

[[nodiscard]] ro::sim::ItemEnchantGroup GroupOf(const YAML::Node& entry) {
  ro::sim::ItemEnchantGroup group{
      .group_id = entry[kIdField].as<std::uint64_t>(0),
      .least_refine = static_cast<std::uint8_t>(entry[kLeastRefineField].as<unsigned>(0)),
      .least_grade = static_cast<std::uint8_t>(entry[kLeastGradeField].as<unsigned>(0)),
      .random_options_allowed = entry[kRandomOptionsField].as<bool>(true)};
  if (const YAML::Node targets = entry[kTargetsField]; targets && targets.IsSequence()) {
    for (const YAML::Node& target : targets) {
      group.target_item_ids.push_back(target.as<std::uint32_t>(0));
    }
  }
  if (const YAML::Node order = entry[kOrderField]; order && order.IsSequence()) {
    for (const YAML::Node& slot : order) {
      group.order.push_back(static_cast<std::uint8_t>(slot.as<unsigned>(0)));
    }
  }
  if (const YAML::Node reset = entry[kResetField]; reset && reset.IsMap()) {
    group.reset =
        ro::sim::ItemEnchantReset{.chance = reset[kChanceField].as<std::uint32_t>(0),
                                  .cost = CostOf(reset[kZenyField], reset[kMaterialsField])};
  }
  if (const YAML::Node slots = entry[kSlotsField]; slots && slots.IsSequence()) {
    for (const YAML::Node& slot : slots) {
      group.slots.push_back(SlotOf(slot));
    }
  }
  return group;
}

}  // namespace

ro::core::Result<ItemEnchantBook> ItemEnchantBook::Parse(std::string_view yaml_text) {
  // yaml-cpp signale par exception : elle ne franchit pas cette fonction.
  try {
    const YAML::Node root = YAML::Load(std::string(yaml_text));
    if (!root.IsMap()) {
      return std::unexpected(Refused("racine qui n'est pas une table"));
    }
    if (root[kVersionField].as<int>(0) != kSupportedVersion) {
      return std::unexpected(Refused("version inconnue"));
    }
    ItemEnchantBook book;
    const YAML::Node section = root[kGroupsSection];
    if (!section || !section.IsSequence()) {
      return book;
    }
    for (const YAML::Node& entry : section) {
      book.groups_.push_back(GroupOf(entry));
    }
    return book;
  } catch (const std::exception& trouble) {
    return std::unexpected(Refused(std::string("YAML illisible — ") + trouble.what()));
  }
}

const ro::sim::ItemEnchantGroup* ItemEnchantBook::Find(std::uint64_t group_id) const noexcept {
  const auto found = std::ranges::find_if(
      groups_,
      [group_id](const ro::sim::ItemEnchantGroup& entry) { return entry.group_id == group_id; });
  return found == groups_.end() ? nullptr : &*found;
}

ItemEnchantBook ReadItemEnchantBook(ro::vfs::Vfs& vfs) {
  auto bytes = vfs.Read(std::string(kItemEnchantBookPath));
  if (!bytes) {
    ro::core::LogWarning("item_enchant : {} — ses fenêtres se refermeront aussitôt",
                         ro::core::Describe(bytes.error()));
    return {};
  }
  const std::string_view text(reinterpret_cast<const char*>(bytes->data()), bytes->size());
  ro::core::Result<ItemEnchantBook> book = ItemEnchantBook::Parse(text);
  if (!book) {
    ro::core::LogWarning("item_enchant : {} — ses fenêtres se refermeront aussitôt",
                         ro::core::Describe(book.error()));
    return {};
  }
  return std::move(*book);
}

}  // namespace moonclient
