#include "character/equip_window_compose.h"

#include <algorithm>
#include <array>
#include <string>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/net/inventory.h"
#include "ro/render/sprite_frame.h"
#include "ro/sim/companion.h"
#include "ro/sim/equip_location.h"
#include "ro/sim/world.h"
#include "ro/vfs/item_name.h"

#include "game/skin_image_of.h"
#include "game/trade_items.h"

namespace moonclient {
namespace {

using ro::ui::EquipWindowTab;
using ro::ui::kEquipWindowPlaceCount;

// LES MONTURES QUI ONT LEUR PROPRE LIBELLÉ : les dragons et le Mado Gear se
// lisent dans les options de l'acteur (`e_option` du serveur), le griffon dans
// le métier qu'il porte.
constexpr std::uint32_t kOptionDragons =
    0x00080000U | 0x00800000U | 0x01000000U | 0x02000000U | 0x04000000U;  // OPTION_DRAGON1..5
constexpr std::uint32_t kOptionMadogear = 0x00400000U;
// La Garde Royale et ses variantes — renaissante, bébé, impériale —, à pied et
// en griffon.
constexpr std::array<std::uint32_t, 8> kGryphonJobs = {4066, 4073, 4102, 4258,
                                                       4082, 4083, 4110, 4281};

// L'accusé : zéro quand le serveur a tout retiré ; un et deux sont ses deux
// refus, que le jeu dit pareil.
constexpr std::uint8_t kTakeOffAllBusy = 1;
constexpr std::uint8_t kTakeOffAllRefused = 2;

// Les places du sac de tout ce qui est porté, triées : le témoin de « tout
// retirer ».
[[nodiscard]] std::vector<std::uint16_t> WornSlots(const ro::sim::Inventory& bag) {
  std::vector<std::uint16_t> slots;
  for (const ro::sim::InventoryItem& item : bag.items()) {
    if (item.equipped()) {
      slots.push_back(item.client_slot);
    }
  }
  std::ranges::sort(slots);
  return slots;
}

[[nodiscard]] const char* MountLabel(const ro::sim::Actor* self) {
  if (self != nullptr) {
    if ((self->option & kOptionDragons) != 0) {
      return ro::core::Tr("Descendre du dragon");
    }
    if ((self->option & kOptionMadogear) != 0) {
      return ro::core::Tr("Quitter le Mado Gear");
    }
    if (std::ranges::find(kGryphonJobs, self->look.job) != kGryphonJobs.end()) {
      return ro::core::Tr("Descendre du griffon");
    }
  }
  return ro::core::Tr("Descendre de monture");
}

}  // namespace

EquipWindowCompose::EquipWindowCompose(const ro::vfs::ItemTable* items,
                                       const ro::vfs::ItemWords* words,
                                       ro::render::ItemIcons* icons,
                                       ro::render::ActorWardrobe* wardrobe,
                                       ro::render::GameImages* images)
    : items_(items), words_(words), icons_(icons), images_(images) {
  if (wardrobe != nullptr) {
    avatar_.emplace(*wardrobe);
  }
}

void EquipWindowCompose::Compose(const ro::sim::InventoryItem* piece,
                                 ro::ui::EquipWindowPiece& into, std::string& name) {
  into = ro::ui::EquipWindowPiece{};
  name.clear();
  if (piece == nullptr) {
    return;
  }
  if (items_ != nullptr && words_ != nullptr) {
    ro::vfs::ItemNameFacts facts = ItemNameFactsOf(*piece);
    creators_.Fill(facts);
    name = ro::vfs::ItemNameText(ro::vfs::ComposeItemName(facts, *items_, *words_));
    into.cards_past_slots = ro::vfs::CardsPastSlots(facts, *items_, *words_);
  }
  into.piece.client_slot = piece->client_slot;
  into.piece.item_id = piece->item_id;
  into.piece.refine_level = piece->refine_level;
  into.piece.amount = piece->amount;
  into.piece.damaged = piece->damaged;
  if (icons_ != nullptr) {
    into.piece.icon = SizedSkinImageOf(icons_->Get(piece->item_id));
  }
  into.option_count = static_cast<std::uint8_t>(std::ranges::count_if(
      piece->options, [](const ro::sim::ItemOption& option) { return option.id != 0; }));
  into.grade_badge = GradeBadgeOf(piece->grade, images_);
}

void EquipWindowCompose::Refresh(const ro::sim::World& world, ro::ui::EquipWindowView& view) {
  const ro::sim::Inventory& bag = world.inventory();
  for (std::size_t place = 0; place < kEquipWindowPlaceCount; ++place) {
    Compose(EquipWindowPieceOn(bag, EquipWindowLocationOf(place, EquipWindowTab::kGeneral)),
            view.general[place], general_names_[place]);
    Compose(EquipWindowPieceOn(bag, EquipWindowLocationOf(place, EquipWindowTab::kCostume)),
            view.costume[place], costume_names_[place]);
  }
  Compose(bag.WornAt(ro::sim::kEquipAmmo), view.ammo, ammo_name_);
  // ⚠ LES NOMS SE DÉSIGNENT APRÈS LA COMPOSITION : chaque `Compose` remet la
  // place à neuf.
  for (std::size_t place = 0; place < kEquipWindowPlaceCount; ++place) {
    view.general[place].piece.name = general_names_[place];
    view.costume[place].piece.name = costume_names_[place];
  }
  view.ammo.piece.name = ammo_name_;

  // LA PIÈCE TRAÎNÉE À L'IMAGE PRÉCÉDENTE, et les places où elle peut aller.
  view.highlighted = {};
  if (const ro::sim::InventoryItem* dragged = bag.Find(dragged_slot_); dragged != nullptr) {
    view.highlighted = EquipWindowPlacesFor(dragged->wearable_at);
  }

  // « TOUT RETIRER » REDEVIENT POSSIBLE dès que ce qui est porté a changé : le
  // serveur a répondu par ses retraits, même si son accusé se perd.
  if (take_off_all_pending_ && WornSlots(bag) != worn_when_asked_) {
    take_off_all_pending_ = false;
  }
  view.take_off_all_pending = take_off_all_pending_;

  RefreshRiders(world);
  view.riders = riders_;
  view.cart = ro::sim::CompanionActive(world.companions(), ro::sim::CompanionKind::kCart);
}

void EquipWindowCompose::RefreshRiders(const ro::sim::World& world) {
  riders_.clear();
  const ro::sim::CompanionState& told = world.companions();
  // ⚠ LE RANG EST CELUI DE LA SORTE, comme dans la colonne de la fiche : c'est
  // lui que `CharacterView::Apply` traduit en demande.
  if (ro::sim::CompanionActive(told, ro::sim::CompanionKind::kCart)) {
    riders_.push_back({.rank = static_cast<int>(ro::sim::CompanionKind::kCart),
                       .label = ro::core::Tr("Retirer le cart")});
  }
  if (ro::sim::CompanionActive(told, ro::sim::CompanionKind::kMount)) {
    riders_.push_back({.rank = static_cast<int>(ro::sim::CompanionKind::kMount),
                       .label = MountLabel(world.player())});
  }
  if (ro::sim::CompanionActive(told, ro::sim::CompanionKind::kFalcon)) {
    riders_.push_back({.rank = static_cast<int>(ro::sim::CompanionKind::kFalcon),
                       .label = ro::core::Tr("Libérer le faucon")});
  }
}

void EquipWindowCompose::RefreshAvatar(const ro::sim::World& world, ImVec2 box,
                                       const ro::render::ActorLook& dressed, std::uint32_t now_ms,
                                       ro::ui::SkinImage& image) {
  image = ro::ui::SkinImage{};
  if (!avatar_.has_value() || world.player() == nullptr || box.x <= 0.0F || box.y <= 0.0F) {
    return;
  }
  ro::render::ActorVignette::Pose pose;
  pose.look = dressed;
  pose.animation = ro::render::kAnimationIdle;
  pose.direction = 0;
  pose.animated = true;
  pose.elapsed_ms = static_cast<float>(now_ms);
  pose.box.width = static_cast<std::int32_t>(box.x);
  pose.box.height = static_cast<std::int32_t>(box.y);
  // 🔴 À L'ÉCHELLE UN, comme le jeu le pose : un pixel de sprite par pixel
  // d'art, et ce qui dépasse sort de la boîte au lieu de rapetisser le
  // personnage.
  pose.box.scale = 1.0F;
  const ro::render::IconTexture& drawn = avatar_->Get(pose);
  if (!drawn.usable()) {
    return;
  }
  image = ro::ui::SkinImage{
      .id = static_cast<ImTextureID>(drawn.texture.name()), .width = box.x, .height = box.y};
}

void EquipWindowCompose::Apply(const ro::ui::EquipWindowAction& asked, ro::net::Connection& link,
                               const ro::sim::World& world) {
  dragged_slot_ = asked.dragged_slot;
  if (!asked.take_off_all_costume.has_value() || take_off_all_pending_) {
    return;
  }
  const EquipWindowTab tab =
      *asked.take_off_all_costume ? EquipWindowTab::kCostume : EquipWindowTab::kGeneral;
  if (const ro::core::Result<void> sent =
          ro::net::RequestTakeOffAll(link, EquipWindowTakeOffAllMask(tab));
      !sent) {
    ro::core::LogWarning("équipement : « tout retirer » non émis — {}",
                         ro::core::Describe(sent.error()));
    return;
  }
  take_off_all_pending_ = true;
  worn_when_asked_ = WornSlots(world.inventory());
}

ro::net::FrameOutcome EquipWindowCompose::HearTakeOffAll(const ro::net::Frame& frame,
                                                         ro::sim::World& world,
                                                         std::uint32_t now_ms) {
  const std::optional<std::uint8_t> answer = ro::net::TakeOffAllAnswerOf(frame);
  if (!answer.has_value()) {
    return ro::net::FrameOutcome::kUnhandled;
  }
  take_off_all_pending_ = false;
  if (*answer == kTakeOffAllBusy || *answer == kTakeOffAllRefused) {
    world.RecordChat(ro::sim::ChatKind::kServer, 0, ro::core::Tr("Réessaie dans 10 secondes."),
                     now_ms);
  }
  return ro::net::FrameOutcome::kApplied;
}

}  // namespace moonclient
