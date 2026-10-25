#include "survivors/survivors_world_facts.h"

#include <cstdint>
#include <vector>

#include "ro/sim/actor.h"
#include "ro/sim/whisper.h"

namespace moonclient {
namespace {

[[nodiscard]] GroundCell CellOfActor(const ro::sim::Actor& actor) noexcept {
  return GroundCell{.x = actor.position.x, .y = actor.position.y};
}

[[nodiscard]] bool IsDead(const ro::sim::Actor* player) noexcept {
  return player != nullptr && player->posture == ro::sim::Posture::kDead;
}

}  // namespace

EntryGuardFacts EntryFactsOf(const ro::sim::World& world, const WorldAside& aside) {
  EntryGuardFacts facts;
  facts.on_town_map = aside.on_town_map;
  facts.on_indoor_map = aside.on_indoor_map;
  facts.trade_open = aside.trade_open;
  facts.npc_dialogue_open = world.npc_dialog().has_value();
  facts.shop_open = aside.shop_open;
  facts.storage_open = world.storage_open();
  const ro::sim::Actor* player = world.player();
  // Sans personnage nommé par le serveur, la carte n'est pas encore chargée.
  facts.loading = aside.loading || player == nullptr;
  facts.dead = IsDead(player);
  facts.link_closed = !aside.link_open;
  if (player != nullptr) {
    std::vector<GroundCell> hostiles;
    for (const auto& [actor_id, actor] : world.actors()) {
      if (actor.kind != ro::sim::ActorKind::kMonster) continue;
      if (actor.posture == ro::sim::Posture::kDead) continue;
      hostiles.push_back(CellOfActor(actor));
    }
    facts.nearest_hostile_cells = NearestCells(CellOfActor(*player), hostiles);
  }
  return facts;
}

RealWorldFacts RealFactsOf(const ro::sim::World& world, const WorldAside& aside,
                           std::uint32_t whispers) {
  RealWorldFacts facts;
  if (const ro::sim::Actor* player = world.player(); player != nullptr) {
    facts.hp = player->health;
    facts.max_hp = player->max_health;
  }
  facts.dead = IsDead(world.player());
  facts.map_entries = world.map_entries();
  facts.trade_offered = aside.trade_offered;
  facts.party_invited = aside.party_invited;
  facts.guild_invited = aside.guild_invited;
  facts.friend_invited = aside.friend_invited;
  facts.server_dialogue_open = world.npc_dialog().has_value();
  facts.whispers = whispers;
  facts.link_open = aside.link_open;
  return facts;
}

void WhisperCounter::Start(std::span<const ro::sim::ChatLine> lines) noexcept {
  received_ = 0;
  seen_seq_ = lines.empty() ? 0 : lines.back().seq;
}

std::uint32_t WhisperCounter::Follow(std::span<const ro::sim::ChatLine> lines) noexcept {
  for (const ro::sim::ChatLine& line : lines) {
    if (line.seq <= seen_seq_) continue;
    seen_seq_ = line.seq;
    if (line.whisper.side == ro::sim::WhisperSide::kReceived) ++received_;
  }
  return received_;
}

}  // namespace moonclient
