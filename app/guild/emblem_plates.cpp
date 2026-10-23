#include "guild/emblem_plates.h"

#include "ro/sim/world.h"

namespace moonclient {

const ro::formats::Image* EmblemPlates::Demand(std::int32_t guild_id, std::int32_t version) {
  if (guild_id <= 0 || version <= 0) {
    return nullptr;
  }
  const Key key{guild_id, version};
  if (const auto held = images_.find(key); held != images_.end()) {
    return &held->second;
  }
  if (!rejected_.contains(key)) {
    wanted_.insert(key);
  }
  return nullptr;
}

const ro::formats::Image* EmblemPlates::Find(std::int32_t guild_id, std::int32_t version) const {
  const auto held = images_.find(Key{guild_id, version});
  return held == images_.end() ? nullptr : &held->second;
}

bool EmblemPlates::Settled(std::int32_t guild_id, std::int32_t version) const {
  const Key key{guild_id, version};
  return images_.contains(key) || rejected_.contains(key);
}

std::vector<EmblemPlates::Key> EmblemPlates::TakeWanted() {
  std::vector<Key> taken(wanted_.begin(), wanted_.end());
  wanted_.clear();
  return taken;
}

void EmblemPlates::Store(std::int32_t guild_id, std::int32_t version, ro::formats::Image image) {
  images_.try_emplace(Key{guild_id, version}, std::move(image));
}

void EmblemPlates::Reject(std::int32_t guild_id, std::int32_t version) {
  rejected_.insert(Key{guild_id, version});
}

std::int32_t PlateGuildIdOf(const ro::sim::Actor& actor, const ro::sim::World& world,
                            const ro::sim::GuildBook& guild) {
  if (actor.id == world.player_id()) {
    return guild.in_guild() ? guild.belonging()->guild_id : 0;
  }
  return static_cast<std::int32_t>(actor.guild_id);
}

const ro::formats::Image* EmblemOnPlate(const ro::sim::Actor& actor, const ro::sim::World& world,
                                        const ro::sim::GuildBook& guild, EmblemPlates& plates) {
  const std::int32_t guild_id = PlateGuildIdOf(actor, world, guild);
  if (guild_id <= 0) {
    return nullptr;
  }
  return plates.Demand(guild_id, guild.EmblemVersionOf(guild_id));
}

}  // namespace moonclient
