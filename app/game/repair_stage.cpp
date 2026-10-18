#include "game/repair_stage.h"

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/net/repair_list.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/inventory.h"
#include "ro/sim/world.h"

#include "game/skin_image_of.h"
#include "items/repair_piece_name.h"

namespace moonclient {
namespace {

// Les deux teintes de l'issue, celles du client : cyan pour une pièce réparée,
// rouge pour un échec.
constexpr std::uint32_t kRepairedColor = 0x00FFFF;
constexpr std::uint32_t kFailedColor = 0xFF0000;

}  // namespace

void RepairStage::See(const ro::vfs::ItemTable* items, const ro::vfs::ItemWords* words,
                      ro::render::ItemIcons* icons, const CraftBook* book, CreatorNames creators) {
  items_ = items;
  words_ = words;
  icons_ = icons;
  book_ = book;
  creators_ = creators;
}

std::string RepairStage::NameOf(const ro::sim::RepairableItem& item, int weapon_level) const {
  if (items_ == nullptr || words_ == nullptr) {
    return std::to_string(item.item_id);
  }
  return RepairPieceName(item, weapon_level, *items_, *words_, creators_);
}

ro::ui::SkinImage RepairStage::IconOf(std::uint32_t item_id) const {
  if (icons_ == nullptr || item_id == 0) {
    return {};
  }
  return SkinImageOf(icons_->Get(item_id));
}

void RepairStage::Stage(const ro::sim::World& world, ro::ui::RepairWindowState& state,
                        ro::ui::RepairView& view) {
  state.open = offer_.open();
  names_.clear();
  materials_.clear();
  rows_.clear();
  if (!offer_.open()) {
    view = ro::ui::RepairView{};
    return;
  }
  // ⚠ DEUX PASSES : les vues pointent dans les chaînes, qui doivent avoir fini
  // de grandir.
  std::vector<std::uint32_t> material_ids;
  for (const ro::sim::RepairableItem& item : offer_.items()) {
    // Sans le livre, on ne sait pas trancher entre arme et armure : on se tait.
    const int weapon_level = book_ != nullptr ? book_->WeaponLevel(item.item_id) : -1;
    names_.push_back(NameOf(item, weapon_level));
    const std::uint32_t material = weapon_level >= 0 ? ro::sim::RepairMaterialFor(weapon_level) : 0;
    material_ids.push_back(material);
    const ro::vfs::ItemEntry* entry =
        items_ != nullptr && material != 0 ? items_->Find(material) : nullptr;
    materials_.push_back(entry != nullptr ? entry->name : std::string());
  }
  for (std::size_t rank = 0; rank < names_.size(); ++rank) {
    const std::uint32_t material = material_ids[rank];
    rows_.push_back(ro::ui::RepairRowView{
        .name = names_[rank],
        .icon = IconOf(offer_.items()[rank].item_id),
        .material = materials_[rank],
        .material_icon = IconOf(material),
        .material_owned = material != 0 ? world.inventory().AmountOf(material) : 0});
  }
  view = ro::ui::RepairView{.rows = rows_};
}

void RepairStage::Apply(const ro::ui::RepairWindowAction& asked, ro::net::Connection& link) {
  if (!asked.decided) {
    return;
  }
  const ro::sim::RepairableItem* chosen =
      asked.chosen.has_value() && *asked.chosen < offer_.items().size()
          ? &offer_.items()[*asked.chosen]
          : nullptr;
  if (auto sent = ro::net::RequestRepair(link, chosen); !sent) {
    ro::core::LogWarning("réparation : réponse non émise — {}", ro::core::Describe(sent.error()));
  }
  offer_.Close();
}

void RepairStage::SayOutcome(ro::sim::World& world, std::uint32_t now_ms) {
  const std::optional<bool> repaired = offer_.TakeOutcome();
  if (!repaired.has_value()) {
    return;
  }
  std::string text = *repaired ? ro::core::Tr("L'item a été réparé.")
                               : ro::core::Tr(
                                     "La réparation a échoué. Vérifiez que vous avez les matériaux "
                                     "nécessaires.");
  world.RecordChat(ro::sim::ChatKind::kServer, 0, std::move(text), now_ms,
                   *repaired ? kRepairedColor : kFailedColor);
}

}  // namespace moonclient
