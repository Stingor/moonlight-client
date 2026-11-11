#include "items/item_doll.h"

#include <array>

#include "ro/render/sprite_frame.h"
#include "ro/sim/equip_location.h"
#include "ro/sim/inventory.h"
#include "ro/sim/world.h"

#include "world/actor_view.h"

namespace moonclient {
namespace {

// La couche où la pièce se pose.
enum class DollLayer : std::uint8_t { kNone, kTop, kMid, kLow, kGarment };

// ⚠ LA TABLE DU CLIENT, masque par masque — pas un « et » de bits.
[[nodiscard]] DollLayer LayerOf(std::uint32_t wearable_at) noexcept {
  using namespace ro::sim;
  switch (wearable_at) {
    case kEquipHeadTop:
    case kEquipCostumeHeadTop:
    case kEquipCostumeHeadTop | kEquipCostumeHeadLow:
    case kEquipHeadTop | kEquipHeadMid:
    case kEquipCostumeHeadTop | kEquipCostumeHeadMid:
      return DollLayer::kTop;
    case kEquipHeadMid:
    case kEquipCostumeHeadMid:
    case kEquipHeadMid | kEquipHeadLow:
    case kEquipCostumeHeadMid | kEquipCostumeHeadLow:
      return DollLayer::kMid;
    case kEquipHeadLow:
    case kEquipCostumeHeadLow:
    case kEquipHeadTop | kEquipHeadMid | kEquipHeadLow:
    case kEquipCostumeHeadTop | kEquipCostumeHeadMid | kEquipCostumeHeadLow:
      return DollLayer::kLow;
    case kEquipGarment:
    case kEquipCostumeGarment:
      return DollLayer::kGarment;
    default:
      return DollLayer::kNone;
  }
}

// Le carrousel : marche, repos, assis.
constexpr std::array<std::size_t, 3> kCarousel = {
    ro::render::kAnimationWalk, ro::render::kAnimationIdle, ro::render::kAnimationSit};
constexpr std::uint32_t kCarouselStepMs = 2500;
// Les rangs de `ActorLook::headgears` : basse, milieu, haute.
constexpr std::size_t kLowRank = 0;
constexpr std::size_t kMidRank = 1;
constexpr std::size_t kTopRank = 2;

// LE BIT DE COSTUME QUI COUVRE CHAQUE PLACE QUI SE VOIT.
struct CostumeCover {
  std::uint32_t worn = 0;
  std::uint32_t costume = 0;
};
constexpr std::array<CostumeCover, 4> kCostumeCovers = {
    CostumeCover{.worn = ro::sim::kEquipHeadTop, .costume = ro::sim::kEquipCostumeHeadTop},
    CostumeCover{.worn = ro::sim::kEquipHeadMid, .costume = ro::sim::kEquipCostumeHeadMid},
    CostumeCover{.worn = ro::sim::kEquipHeadLow, .costume = ro::sim::kEquipCostumeHeadLow},
    CostumeCover{.worn = ro::sim::kEquipGarment, .costume = ro::sim::kEquipCostumeGarment}};

// Ôte de l'apparence la pièce qui donne cette apparence : son style ne
// portera pas sa teinture.
void TakeOffView(ro::render::ActorLook& look, std::uint16_t view_id, DollLayer layer) {
  if (layer == DollLayer::kGarment) {
    if (look.robe == view_id) {
      look.robe = 0;
    }
    return;
  }
  for (auto& headgear : look.headgears) {
    if (headgear == view_id) {
      headgear = 0;
    }
  }
}

}  // namespace

bool ShownByPlayer(const ro::sim::Inventory& inventory, const ro::sim::InventoryItem& held) {
  if (held.worn_at == 0 || held.view_id == 0) {
    return false;
  }
  if (ro::sim::IsCostumeLocation(held.worn_at)) {
    return true;
  }
  // 🔴 UN COSTUME QUI DONNE UNE APPARENCE COUVRE LA PIÈCE DESSOUS : c'est la
  // sienne que le serveur envoie, et la teinture de la pièce couverte reste à
  // la base.
  std::uint32_t covers = 0;
  for (const CostumeCover& cover : kCostumeCovers) {
    if ((held.worn_at & cover.worn) != 0) {
      covers |= cover.costume;
    }
  }
  for (const ro::sim::InventoryItem& other : inventory.items()) {
    if ((other.worn_at & covers) != 0 && other.view_id != 0) {
      return false;
    }
  }
  return true;
}

ItemDoll::ItemDoll(ro::render::ActorWardrobe* wardrobe) : wardrobe_(wardrobe) {
  if (wardrobe_ != nullptr) {
    doll_.emplace(*wardrobe_);
  }
}

bool ItemDoll::Shows(std::uint32_t wearable_at) const noexcept {
  return doll_.has_value() && LayerOf(wearable_at) != DollLayer::kNone;
}

ro::render::VignetteFraming ItemDoll::framing() const noexcept {
  return doll_.has_value() ? doll_->framing() : ro::render::VignetteFraming{};
}

void ItemDoll::Refresh(const ro::sim::World& world, std::uint16_t view_id,
                       std::uint32_t wearable_at, bool with_effect, DollDye dye,
                       ro::ui::DescriptionDollState& state, ActorStyles& dressing,
                       std::uint32_t now_ms) {
  state.image = ro::ui::SkinImage{};
  state.overlay = ro::ui::DollOverlay{};
  state.body_overlaid = false;
  state.dye_hidden = false;
  body_.reset();
  // Une pièce sans place sur le corps ne se porte pas : nu, sauf son effet.
  const DollLayer layer = view_id == 0 ? DollLayer::kNone : LayerOf(wearable_at);
  const ro::sim::Actor* player = world.player();
  if (!state.wanted || !doll_.has_value() || (layer == DollLayer::kNone && !with_effect) ||
      player == nullptr) {
    return;
  }
  ro::render::ActorVignette::Pose pose;
  pose.look = LookOf(*player);
  // 🔴 LA PIÈCE ESSAYÉE N'EMPRUNTE PAS LA TEINTURE D'UNE AUTRE : une teinture
  // se range sous l'apparence, et un chapeau du sac pareil à celui qu'on porte
  // prendrait sinon la couleur de celui-ci.
  if (dye != DollDye::kShown && layer != DollLayer::kNone) {
    TakeOffView(pose.look, view_id, layer);
  }
  state.dye_hidden = dye == DollDye::kHidden && layer != DollLayer::kNone;
  // SES COULEURS AVANT DE LE DÉSHABILLER : c'est le corps qui choisit la
  // variante, et aucune des pièces retirées ci-dessous ne le change.
  if (wardrobe_ != nullptr) {
    dressing.Dress(world.player_id(), pose.look, *wardrobe_, world.styles());
  }
  KeepBodyOnly(pose.look);
  switch (layer) {
    case DollLayer::kTop:
      pose.look.headgears[kTopRank] = view_id;
      break;
    case DollLayer::kMid:
      pose.look.headgears[kMidRank] = view_id;
      break;
    case DollLayer::kLow:
      pose.look.headgears[kLowRank] = view_id;
      break;
    case DollLayer::kGarment:
      pose.look.robe = view_id;
      break;
    case DollLayer::kNone:
      break;
  }
  pose.animation = kCarousel[(now_ms / kCarouselStepMs) % kCarousel.size()];
  pose.direction = state.direction;
  pose.animated = true;
  // Au repos et assis, le regard droit : seule la marche défile.
  pose.held_frame = VignetteHeldFrame(*player, pose.animation);
  pose.elapsed_ms = static_cast<float>(now_ms % kCarouselStepMs);
  pose.box.width = static_cast<std::int32_t>(ro::ui::kDescriptionDollWidthPixels);
  pose.box.height = static_cast<std::int32_t>(ro::ui::kDescriptionDollHeightPixels);
  pose.box.min_body_height = ro::ui::kDescriptionDollBodyPixels;
  const ro::render::IconTexture& drawn = doll_->Get(pose);
  if (!drawn.usable()) {
    return;
  }
  if (const ro::render::ComposedActor* composed = doll_->composed()) {
    // SA TÊTE DÉPLACÉE, que suit un effet posé sur elle : assise, en marche.
    const ro::render::ActorOutfit* outfit =
        wardrobe_ != nullptr ? wardrobe_->OutfitFor(pose.look) : nullptr;
    body_ = DollBody{.composed = *composed,
                     .animation_duration_ms = doll_->animation_duration_ms(),
                     .head_shift = outfit != nullptr
                                       ? HeadShiftOf(*wardrobe_, *outfit, pose.look, *composed)
                                       : std::nullopt};
  }
  state.image = ro::ui::SkinImage{.id = static_cast<ImTextureID>(drawn.texture.name()),
                                  .width = ro::ui::kDescriptionDollWidthPixels,
                                  .height = ro::ui::kDescriptionDollHeightPixels};
}

}  // namespace moonclient
