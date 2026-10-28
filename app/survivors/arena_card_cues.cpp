#include "survivors/arena_card_cues.h"

#include "ro/sim/survivors_content.h"

namespace moonclient {

void PlanArenaRareGlows(std::span<const ro::sim::SurvivorsPickup> pickups,
                        ArenaEffectSwitches switches, std::vector<ArenaRareGlow>& out) {
  out.clear();
  if (!switches.shown) return;
  for (const ro::sim::SurvivorsPickup& pickup : pickups) {
    if (out.size() >= kMostArenaRareGlows) return;
    if (pickup.kind == ro::sim::PickupKind::kCard) {
      out.push_back(ArenaRareGlow{.actor_id = pickup.serial, .at = pickup.position});
    } else if (pickup.kind == ro::sim::PickupKind::kChest) {
      out.push_back(
          ArenaRareGlow{.actor_id = kFirstChestGlowActorId + pickup.serial, .at = pickup.position});
    }
  }
}

ArenaCardSounds CardSoundsOf(const ro::sim::SurvivorsNews& news) noexcept {
  ArenaCardSounds sounds;
  if (!news.cards_dropped.empty()) {
    sounds.drop = ro::render::ItemDropPillarOf(kRareGlowPillar).sound;
  }
  if (!news.cards_found.empty()) sounds.pickup = kCardPickupSound;
  return sounds;
}

void CardVignettes::Follow(std::uint64_t now_ms) noexcept {
  if (now_ms < last_now_ms_) waiting_.clear();
  last_now_ms_ = now_ms;
}

void CardVignettes::Notice(std::span<const ro::sim::SurvivorsCardNews> found,
                           std::uint64_t now_ms) {
  Follow(now_ms);
  // Une file finie ne retarde pas la carte suivante : elle paraît tout de
  // suite.
  static_cast<void>(ShownAt(now_ms));
  for (const ro::sim::SurvivorsCardNews& card : found) {
    if (waiting_.size() >= kMostCardVignettes) return;
    if (waiting_.empty()) front_since_ms_ = now_ms;
    waiting_.push_back(card.card_item_id);
  }
}

std::optional<CardVignetteShown> CardVignettes::ShownAt(std::uint64_t now_ms) {
  Follow(now_ms);
  while (!waiting_.empty() && now_ms >= front_since_ms_ + kCardVignetteMs) {
    front_since_ms_ += kCardVignetteMs;
    waiting_.erase(waiting_.begin());
  }
  if (waiting_.empty()) return std::nullopt;
  return CardVignetteShown{.card_item_id = waiting_.front(),
                           .fraction = static_cast<float>(now_ms - front_since_ms_) /
                                       static_cast<float>(kCardVignetteMs)};
}

void CardVignettes::Clear() noexcept {
  waiting_.clear();
  front_since_ms_ = 0;
  last_now_ms_ = 0;
}

}  // namespace moonclient
