#include "guild/plate_emblem.h"

#include "ro/sim/status_effects.h"
#include "ro/sim/world.h"

namespace moonclient {

PlateEmblemChoice ChoosePlateEmblem(std::int32_t guild_id, std::int32_t clan_id,
                                    const ro::render::EntityNamesLook& look) {
  if (guild_id > 0) {
    return look.guild_emblem ? PlateEmblemChoice{PlateEmblemKind::kGuild, guild_id}
                             : PlateEmblemChoice{};
  }
  if (clan_id > 0 && look.clan_emblem) {
    return PlateEmblemChoice{PlateEmblemKind::kClan, clan_id};
  }
  return PlateEmblemChoice{};
}

std::int32_t ClanIdOf(const ro::sim::Actor& actor) {
  if (!actor.status_effects.contains(ro::sim::kEfstClanInfo)) {
    return 0;
  }
  const auto clan = actor.status_second_values.find(ro::sim::kEfstClanInfo);
  return clan == actor.status_second_values.end() ? 0 : clan->second;
}

const ro::formats::Image* NamePlateEmblem(const ro::sim::Actor& actor, const ro::sim::World& world,
                                          const ro::sim::GuildBook& guild, EmblemPlates* plates,
                                          const ro::render::ClanEmblems& clans,
                                          const ro::render::EntityNamesLook& look) {
  const PlateEmblemChoice choice =
      ChoosePlateEmblem(PlateGuildIdOf(actor, world, guild), ClanIdOf(actor), look);
  switch (choice.kind) {
    case PlateEmblemKind::kGuild:
      return plates != nullptr ? EmblemOnPlate(actor, world, guild, *plates) : nullptr;
    case PlateEmblemKind::kClan:
      return clans.Of(choice.owner_id);
    case PlateEmblemKind::kNone:
      break;
  }
  return nullptr;
}

}  // namespace moonclient
