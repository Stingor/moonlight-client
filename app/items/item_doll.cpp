#include "items/item_doll.h"

#include <array>

#include "ro/render/sprite_frame.h"
#include "ro/sim/equip_location.h"
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

}  // namespace

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
                       std::uint32_t wearable_at, bool with_effect,
                       ro::ui::DescriptionDollState& state, ActorStyles& dressing,
                       std::uint32_t now_ms) {
  state.image = ro::ui::SkinImage{};
  state.overlay = ro::ui::DollOverlay{};
  state.body_overlaid = false;
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
  // SES COULEURS AVANT DE LE DÉSHABILLER : c'est le corps qui choisit la
  // variante, et aucune des pièces retirées ci-dessous ne le change.
  if (wardrobe_ != nullptr) {
    dressing.Dress(world.player_id(), pose.look, *wardrobe_, world.styles());
  }
  pose.look.headgears = {};
  pose.look.weapon = 0;
  pose.look.shield = 0;
  pose.look.robe = 0;
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
    body_ =
        DollBody{.composed = *composed, .animation_duration_ms = doll_->animation_duration_ms()};
  }
  state.image = ro::ui::SkinImage{.id = static_cast<ImTextureID>(drawn.texture.name()),
                                  .width = ro::ui::kDescriptionDollWidthPixels,
                                  .height = ro::ui::kDescriptionDollHeightPixels};
}

}  // namespace moonclient
