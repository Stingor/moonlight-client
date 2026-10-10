#include "ro/net/player_settings.h"

#include <algorithm>
#include <limits>

#include "ro/core/byte_reader.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/world.h"

namespace ro::net {

std::vector<PlayerSetting> ReadPlayerSettings(std::span<const std::uint8_t> frame) {
  if (frame.size() < kPlayerSettingsHeader) {
    return {};
  }
  core::ByteReader reader(frame);
  // L'opcode et la longueur sont déjà connus de l'appelant : c'est par eux que
  // la trame lui est parvenue.
  reader.Skip(2);
  reader.Skip(2);
  // L'identifiant du personnage ne sert à rien ici : il n'y a qu'une session, et
  // c'est la nôtre.
  reader.Skip(4);
  const std::uint16_t count = reader.U16();
  if (!reader.ok()) {
    return {};
  }
  // TOUT OU RIEN : le compte annoncé doit tenir dans ce qui reste.
  if (static_cast<std::size_t>(count) * kPlayerSettingSize > reader.remaining()) {
    return {};
  }

  std::vector<PlayerSetting> read;
  read.reserve(count);
  for (std::uint16_t rank = 0; rank < count; ++rank) {
    PlayerSetting setting;
    setting.id = reader.U16();
    setting.value = reader.U32();
    read.push_back(setting);
  }
  if (!reader.ok()) {
    return {};
  }
  return read;
}

FrameOutcome ApplyPlayerSettingsFrame(const Frame& frame, sim::World& world) {
  if (frame.opcode != proto::ZcMoonlightSettings::kOpcode) {
    return FrameOutcome::kUnhandled;
  }
  const std::vector<PlayerSetting> settings = ReadPlayerSettings(frame.bytes);
  if (settings.empty()) {
    return FrameOutcome::kUndecodable;
  }
  for (const PlayerSetting& setting : settings) {
    // Le serveur pousse une trentaine de réglages, dont la plupart commandent
    // des fonctions que ce client n'a pas. Les ignorer est le comportement
    // voulu, pas un oubli.
    switch (setting.id) {
      case kSettingGroupLevel:
        // ⚠ BORNÉE ICI, et pas seulement à l'affichage : la valeur vient du fil,
        // et le niveau de groupe ne dépasse jamais 99 côté serveur.
        world.session().group_level = static_cast<std::uint8_t>(
            std::min<std::uint32_t>(setting.value, std::numeric_limits<std::uint8_t>::max()));
        break;
      case kSettingWalkSpeed:
        world.session().walk_speed = static_cast<std::uint16_t>(
            std::min<std::uint32_t>(setting.value, std::numeric_limits<std::uint16_t>::max()));
        break;
      case kSettingKiller:
        world.session().killer = setting.value != 0;
        break;
      case kSettingKillable:
        world.session().killable = setting.value != 0;
        break;
      case kSettingPlayerTest:
        world.session().player_test = setting.value != 0;
        break;
      case kSettingNoDelay:
        world.session().no_delay = setting.value != 0;
        break;
      case kSettingDiscordChat:
        world.session().discord_chat = setting.value != 0;
        break;
      default:
        break;
    }
  }
  return FrameOutcome::kIgnored;
}

void SendPlayerSetting(Connection& link, std::uint16_t id, std::uint32_t value) {
  proto::CzMoonlightSetting request;
  // ⚠ LA LONGUEUR S'ÉCRIT : le serveur la LIT, et la laisser à zéro ferait
  // rejeter la trame sans un mot. Même piège que la demande de style.
  request.packetLength = static_cast<std::int16_t>(proto::CzMoonlightSetting::kWireSize);
  request.id = static_cast<std::int16_t>(id);
  request.value = value;
  link.Send(proto::Encode(request));
}

}  // namespace ro::net
