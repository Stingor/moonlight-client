#include "character/style_outfits.h"

#include <cstdint>
#include <utility>

#include "ro/net/style_outfits.h"
#include "ro/sim/world.h"

namespace moonclient {

std::optional<ro::sim::StyleOutfit> StyleOutfits::Capture(std::uint32_t own_gid,
                                                          const ro::sim::StyleBook& book) {
  ro::sim::StyleOutfit outfit;
  if (const ro::sim::StyleVariants* variants = book.Find(own_gid)) {
    for (const auto& [key, recipe] : variants->by_body) {
      if (outfit.bodies.size() == ro::sim::kStyleBodyVariantsMost) {
        break;
      }
      outfit.bodies.emplace_back(key, recipe);
    }
    // Le repli n'entre que s'il désigne l'un des corps gardés.
    for (const auto& [key, recipe] : outfit.bodies) {
      if (key == variants->fallback_body) {
        outfit.fallback_body = key;
      }
    }
  }
  if (const std::vector<ro::sim::StyleLayerRecipe>* layers = book.FindLayers(own_gid)) {
    for (const ro::sim::StyleLayerRecipe& layer : *layers) {
      if (outfit.layers.size() == ro::net::kOutfitEntriesMost) {
        break;
      }
      outfit.layers.push_back(layer);
    }
  }
  if (outfit.empty()) {
    return std::nullopt;
  }
  return outfit;
}

bool StyleOutfits::Served(const ro::sim::World& world) {
  return world.session().style_outfit_cadence_ms > 0;
}

bool StyleOutfits::CanSend(const ro::sim::World& world, std::uint32_t now_ms) const {
  return Ready(world, now_ms).has_value();
}

ro::core::Result<void> StyleOutfits::Ready(const ro::sim::World& world,
                                           std::uint32_t now_ms) const {
  if (!Served(world)) {
    return ro::core::Fail(ro::core::ErrorCode::kUnsupportedVersion,
                          "tenues : le serveur ne les reçoit pas");
  }
  if (sent_once_ && static_cast<std::int32_t>(next_allowed_ms_ - now_ms) > 0) {
    return ro::core::Fail(ro::core::ErrorCode::kInvalidArgument, "tenues : trop tôt");
  }
  return {};
}

void StyleOutfits::Sent(const ro::sim::World& world, std::uint32_t now_ms) {
  sent_once_ = true;
  next_allowed_ms_ = now_ms + world.session().style_outfit_cadence_ms;
}

namespace {

ro::core::Result<void> SendIfReady(ro::core::Result<void> ready, ro::net::Connection& link,
                                   const ro::net::OutfitRequest& request) {
  if (!ready) {
    return ready;
  }
  return ro::net::SendOutfitRequest(link, request);
}

}  // namespace

ro::core::Result<void> StyleOutfits::Publish(ro::net::Connection& link, const ro::sim::World& world,
                                             const ro::sim::StyleOutfit& outfit,
                                             std::uint32_t now_ms) {
  ro::core::Result<void> sent = SendIfReady(
      Ready(world, now_ms), link,
      ro::net::OutfitRequest{.action = ro::sim::OutfitAction::kPublish, .outfit = outfit});
  if (sent) {
    Sent(world, now_ms);
  }
  return sent;
}

ro::core::Result<void> StyleOutfits::Fetch(ro::net::Connection& link, const ro::sim::World& world,
                                           std::string_view typed, std::uint32_t now_ms) {
  const std::optional<std::string> code = ro::net::NormalizeOutfitCode(typed);
  if (!code.has_value()) {
    return ro::core::Fail(ro::core::ErrorCode::kInvalidArgument, "tenues : code mal formé");
  }
  ro::core::Result<void> sent =
      SendIfReady(Ready(world, now_ms), link,
                  ro::net::OutfitRequest{.action = ro::sim::OutfitAction::kFetch, .code = *code});
  if (sent) {
    Sent(world, now_ms);
  }
  return sent;
}

ro::core::Result<void> StyleOutfits::ListWardrobe(ro::net::Connection& link,
                                                  const ro::sim::World& world,
                                                  std::uint32_t now_ms) {
  ro::core::Result<void> sent =
      SendIfReady(Ready(world, now_ms), link,
                  ro::net::OutfitRequest{.action = ro::sim::OutfitAction::kWardrobeList});
  if (sent) {
    Sent(world, now_ms);
  }
  return sent;
}

ro::core::Result<void> StyleOutfits::Store(ro::net::Connection& link, const ro::sim::World& world,
                                           std::uint8_t slot, std::string_view name,
                                           const ro::sim::StyleOutfit& outfit,
                                           std::uint32_t now_ms) {
  ro::core::Result<void> sent = SendIfReady(Ready(world, now_ms), link,
                                            ro::net::OutfitRequest{
                                                .action = ro::sim::OutfitAction::kWardrobeStore,
                                                .slot = slot,
                                                .name = std::string(name),
                                                .outfit = outfit,
                                            });
  if (sent) {
    Sent(world, now_ms);
  }
  return sent;
}

ro::core::Result<void> StyleOutfits::Clear(ro::net::Connection& link, const ro::sim::World& world,
                                           std::uint8_t slot, std::uint32_t now_ms) {
  ro::core::Result<void> sent = SendIfReady(
      Ready(world, now_ms), link,
      ro::net::OutfitRequest{.action = ro::sim::OutfitAction::kWardrobeClear, .slot = slot});
  if (sent) {
    Sent(world, now_ms);
  }
  return sent;
}

void StyleOutfits::Update(const ro::sim::World& world) {
  const ro::sim::OutfitReply& last = world.outfits().last();
  if (last.sequence == 0 || last.sequence == heard_) {
    return;
  }
  heard_ = last.sequence;
  if (!last.accepted()) {
    return;
  }
  if (last.action == ro::sim::OutfitAction::kPublish) {
    published_code_ = last.code;
  }
  if (last.action == ro::sim::OutfitAction::kFetch && last.outfit.has_value()) {
    proposal_ = *last.outfit;
  }
}

bool StyleOutfits::ProposeFromWardrobe(const ro::sim::World& world, std::uint8_t slot) {
  const ro::sim::StyleWardrobeEntry* entry = world.outfits().Slot(slot);
  if (entry == nullptr) {
    return false;
  }
  proposal_ = entry->outfit;
  return true;
}

std::optional<ro::sim::StyleOutfit> StyleOutfits::TakeProposal() {
  return std::exchange(proposal_, std::nullopt);
}

}  // namespace moonclient
