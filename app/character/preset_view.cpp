#include "character/preset_view.h"

#include <cstdio>
#include <optional>
#include <span>
#include <utility>

#include "ro/core/error.h"
#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/core/printed.h"
#include "ro/net/inventory.h"
#include "ro/sim/world.h"
#include "ro/vfs/item_name.h"

#include "character/equip_preset_file.h"

namespace moonclient {
namespace {

[[nodiscard]] ro::ui::PresetPieceKind KindOf(PresetFamily family) {
  switch (family) {
    case PresetFamily::kEquipment:
      return ro::ui::PresetPieceKind::kEquipment;
    case PresetFamily::kCostume:
      return ro::ui::PresetPieceKind::kCostume;
    case PresetFamily::kAmmo:
      return ro::ui::PresetPieceKind::kAmmo;
  }
  return ro::ui::PresetPieceKind::kEquipment;
}

void SetStatus(ro::ui::PresetTabState& state, const char* text) {
  std::snprintf(state.status.data(), state.status.size(), "%s", text);
}

void Sent(const ro::core::Result<void>& sent) {
  if (!sent) {
    ro::core::LogWarning("presets : demande non émise — {}", ro::core::Describe(sent.error()));
  }
}

}  // namespace

PresetView::PresetView(const ro::vfs::ItemTable* items, const ro::vfs::ItemWords* words,
                       ro::render::ItemIcons* icons, std::filesystem::path client_root,
                       std::uint32_t character_id)
    : items_(items),
      words_(words),
      icons_(icons),
      client_root_(std::move(client_root)),
      character_id_(character_id),
      book_(LoadEquipPresets(client_root_, character_id_)),
      saved_revision_(book_.revision()) {}

void PresetView::Refresh(bool shown, const Hotkeys& table) {
  if (!shown) {
    return;
  }
  const Composed now{
      .book = book_.revision(), .table = table.revision(), .language = ro::core::LanguageSerial()};
  if (composed_ == now) {
    return;
  }
  composed_ = now;
  style_hotkey_ = LabelOfBinding(table.Of(GameAction::kOpenStyleEditor));
  const std::span<const EquipPreset> presets = book_.presets();
  piece_names_.clear();
  hotkeys_.clear();
  holders_.clear();
  for (std::size_t rank = 0; rank < presets.size(); ++rank) {
    for (const PresetPiece& piece : presets[rank].pieces) {
      piece_names_.push_back(PieceName(piece));
    }
    hotkeys_.push_back(LabelOfBinding(presets[rank].hotkey));
    holders_.push_back(
        PresetKeyHolder(table, book_, rank, presets[rank].hotkey).value_or(std::string()));
  }
  pieces_.clear();
  std::size_t named = 0;
  for (const EquipPreset& preset : presets) {
    for (const PresetPiece& piece : preset.pieces) {
      pieces_.push_back(ro::ui::PresetPieceView{.icon = PieceIcon(piece.item_id),
                                                .name = piece_names_[named++],
                                                .refine_level = piece.refine_level,
                                                .kind = KindOf(piece.family)});
    }
  }
  rows_.clear();
  std::size_t first = 0;
  for (std::size_t rank = 0; rank < presets.size(); ++rank) {
    const EquipPreset& preset = presets[rank];
    rows_.push_back(ro::ui::PresetRowView{
        .name = preset.name,
        .pieces =
            std::span<const ro::ui::PresetPieceView>(pieces_).subspan(first, preset.pieces.size()),
        .with_costumes = preset.with_costumes,
        .hotkey = hotkeys_[rank],
        .taken_by = holders_[rank]});
    first += preset.pieces.size();
  }
}

ro::ui::PresetTabView PresetView::view() const {
  return ro::ui::PresetTabView{
      .presets = rows_, .capacity = kPresetsPerCharacter, .style_hotkey = style_hotkey_};
}

void PresetView::Apply(const ro::ui::PresetAction& asked, ro::ui::PresetTabState& state,
                       ro::net::Connection& link, const ro::sim::World& world) {
  if (asked.undress.has_value()) {
    const std::vector<std::uint16_t> slots = PlanUndress(world.inventory(), *asked.undress);
    for (const std::uint16_t slot : slots) {
      Sent(ro::net::RequestUnequip(link, slot));
    }
    if (slots.empty()) {
      SetStatus(state, ro::core::Tr("Rien à déséquiper"));
    } else {
      std::snprintf(state.status.data(), state.status.size(),
                    ro::core::Tr("Tout déséquipé (%d pièce(s))"), static_cast<int>(slots.size()));
    }
  }
  if (asked.load.has_value()) {
    Wear(*asked.load, state, link, world);
  }
  if (asked.costumes.has_value()) {
    static_cast<void>(book_.SetWithCostumes(*asked.costumes, asked.with_costumes));
  }
  if (asked.clear_hotkey.has_value()) {
    static_cast<void>(book_.SetHotkey(*asked.clear_hotkey, Binding{}));
  }
  if (!asked.save_as.empty()) {
    const EquipPresetBook::Saved saved =
        book_.Save(asked.save_as, WornPresetPieces(world.inventory()));
    SetStatus(state, saved == EquipPresetBook::Saved::kFull
                         ? ro::core::Tr("Limite de 5 presets atteinte")
                         : ro::core::Tr("Preset enregistré"));
  }
  if (asked.remove.has_value() && book_.Erase(*asked.remove)) {
    state.status[0] = '\0';
    // L'attente d'une touche désigne un rang, que la suppression décale.
    state.capturing = -1;
  }
  // Les lignes pointent dans le carnet, qui vient de changer : elles se refont
  // à la prochaine image, avant le dessin.
  if (book_.revision() != saved_revision_) {
    rows_.clear();
    pieces_.clear();
    composed_.reset();
  }
  SaveIfChanged();
}

void PresetView::Capture(const Hotkeys& table, ro::ui::PresetTabState& state, Binding wanted) {
  if (state.capturing < 0) {
    return;
  }
  const auto rank = static_cast<std::size_t>(state.capturing);
  if (book_.At(rank) == nullptr) {
    state.capturing = -1;
    return;
  }
  // Refusée, l'attente continue : le joueur choisit une autre touche sans
  // recliquer.
  if (const std::optional<std::string> holder = PresetKeyHolder(table, book_, rank, wanted)) {
    std::snprintf(state.refusal.data(), state.refusal.size(),
                  ro::core::Tr("Déjà utilisé par %s — choisis un autre combo"), holder->c_str());
    return;
  }
  static_cast<void>(book_.SetHotkey(rank, wanted));
  state.capturing = -1;
  state.refusal[0] = '\0';
  SaveIfChanged();
}

bool PresetView::TriggerHotkey(Binding pressed, ro::ui::PresetTabState& state,
                               ro::net::Connection& link, const ro::sim::World& world) {
  const std::optional<std::size_t> rank = book_.Holding(pressed);
  if (!rank.has_value()) {
    return false;
  }
  Wear(*rank, state, link, world);
  return true;
}

std::string PresetView::ReleaseHotkey(Binding taken) {
  const std::optional<std::size_t> rank = book_.Release(taken);
  if (!rank.has_value()) {
    return std::string();
  }
  SaveIfChanged();
  return PresetOwnerLabel(book_.presets()[*rank].name);
}

void PresetView::Wear(std::size_t rank, ro::ui::PresetTabState& state, ro::net::Connection& link,
                      const ro::sim::World& world) {
  const EquipPreset* preset = book_.At(rank);
  if (preset == nullptr) {
    return;
  }
  const PresetPlan plan = PlanPreset(*preset, world.inventory());
  for (const std::uint16_t slot : plan.take_off) {
    Sent(ro::net::RequestUnequip(link, slot));
  }
  for (const PresetWear& wear : plan.wear) {
    Sent(ro::net::RequestEquip(link, wear.client_slot, wear.position));
  }
  if (plan.missing > 0) {
    std::snprintf(state.status.data(), state.status.size(),
                  ro::core::Tr("Appliqué (%d item(s) manquant(s))"), plan.missing);
  } else {
    SetStatus(state, ro::core::Tr("Preset appliqué"));
  }
}

void PresetView::SaveIfChanged() {
  if (book_.revision() == saved_revision_) {
    return;
  }
  // Recalé même si l'écriture échoue : réessayer à chaque image n'y changerait
  // rien, et le journal l'a dit.
  saved_revision_ = book_.revision();
  static_cast<void>(SaveEquipPresets(client_root_, character_id_, book_.presets()));
}

std::string PresetView::PieceName(const PresetPiece& piece) const {
  // Le nom nu, sans cartes ni emplacements, le raffinement devant.
  std::string bare = items_ != nullptr && words_ != nullptr
                         ? ro::vfs::ItemNameText(ro::vfs::ComposeItemName(
                               ro::vfs::ItemNameFacts{.item_id = piece.item_id}, *items_, *words_,
                               ro::vfs::ItemNameSlots::kHidden))
                         : "#" + std::to_string(piece.item_id);
  if (piece.refine_level <= 0) {
    return bare;
  }
  return ro::core::Printed("+%d ", piece.refine_level) + bare;
}

ro::ui::SkinImage PresetView::PieceIcon(std::uint32_t item_id) const {
  if (icons_ == nullptr) {
    return ro::ui::SkinImage{};
  }
  const ro::render::IconTexture& icon = icons_->Get(item_id);
  if (!icon.usable()) {
    return ro::ui::SkinImage{};
  }
  return ro::ui::SkinImage{.id = static_cast<ImTextureID>(icon.texture.name()),
                           .width = static_cast<float>(icon.width_px),
                           .height = static_cast<float>(icon.height_px)};
}

}  // namespace moonclient
