#include "guild/sample_emblem.h"

#include <cstdint>

#include "ro/render/clan_emblem.h"
#include "ro/sim/world.h"

#include "game/skin_image_of.h"
#include "guild/plate_emblem.h"

namespace moonclient {

ro::ui::SkinImage SampleEmblem(const ro::ui::EntitySample& sample, const ro::sim::World& world,
                               const ro::sim::GuildBook& guild, const GuildEmblems* emblems,
                               ro::render::GameImages* images,
                               const ro::render::EntityNamesLook& look) {
  if (sample.banner.empty() || sample.name != world.player_name()) {
    return {};
  }
  const std::int32_t guild_id = guild.in_guild() ? guild.belonging()->guild_id : 0;
  const ro::sim::Actor* me = world.Find(world.player_id());
  const PlateEmblemChoice choice =
      ChoosePlateEmblem(guild_id, me != nullptr ? ClanIdOf(*me) : 0, look);
  switch (choice.kind) {
    case PlateEmblemKind::kGuild:
      return emblems != nullptr ? emblems->Of(choice.owner_id) : ro::ui::SkinImage{};
    case PlateEmblemKind::kClan:
      return images != nullptr
                 ? SkinImageOf(images->GetOurs(ro::render::ClanEmblemPath(choice.owner_id)))
                 : ro::ui::SkinImage{};
    case PlateEmblemKind::kNone:
      break;
  }
  return {};
}

}  // namespace moonclient
