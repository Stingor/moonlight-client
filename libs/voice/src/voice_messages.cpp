#include "ro/voice/voice_messages.h"

#include <string>
#include <utility>

#include <yaml-cpp/yaml.h>

namespace ro::voice {
namespace {

using core::ErrorCode;
using core::Fail;

// Le texte d'un booléen JSON.
[[nodiscard]] const char* JsonBool(bool value) { return value ? "true" : "false"; }

// Un ordre à bascule : `{"type":…,"value":…,"session_id":…}`.
[[nodiscard]] std::string Toggle(std::string_view type, bool value, std::uint64_t session_id) {
  return "{\"type\":\"" + std::string(type) + "\",\"value\":" + JsonBool(value) +
         ",\"session_id\":" + std::to_string(session_id) + "}";
}

// La valeur d'une clé, ou le défaut quand elle manque ou n'a pas ce type.
template <typename T>
[[nodiscard]] T ValueOr(const YAML::Node& object, const char* key, T fallback) {
  const YAML::Node value = object[key];
  if (!value || !value.IsScalar()) return fallback;
  try {
    return value.as<T>();
  } catch (const YAML::Exception&) {
    return fallback;
  }
}

[[nodiscard]] ServerMessage ReadNearby(const YAML::Node& object) {
  NearbyPlayers nearby;
  const YAML::Node players = object["players"];
  if (!players || !players.IsSequence()) return nearby;
  for (const YAML::Node& player : players) {
    if (!player.IsMap()) continue;
    NearbyPlayer entry;
    entry.character_id = ValueOr<std::uint32_t>(player, "id", 0);
    entry.name = ValueOr<std::string>(player, "name", "");
    if (entry.character_id != 0) {
      nearby.players.push_back(std::move(entry));
    }
  }
  return nearby;
}

[[nodiscard]] ServerMessage ReadChannel(const YAML::Node& object) {
  const auto channel = ValueOr<std::uint32_t>(object, "channel", 0);
  if (channel >= kVoiceChannelCount) return Unread{.type = "channel_ack"};
  return ChannelAccepted{.channel = static_cast<VoiceChannel>(channel)};
}

}  // namespace

std::string AuthMessage(const VoiceLogin& login) {
  return "{\"type\":\"auth\",\"account_id\":" + std::to_string(login.account_id) +
         ",\"char_id\":" + std::to_string(login.character_id) +
         ",\"session_id\":" + std::to_string(login.session_id) +
         ",\"login_id1\":" + std::to_string(login.login_id1) + "}";
}

std::string PingMessage(std::uint32_t stamp_ms) {
  return "{\"type\":\"ping\",\"t\":" + std::to_string(stamp_ms) + "}";
}

std::string SetChannelMessage(VoiceChannel channel) {
  return "{\"type\":\"set_channel\",\"channel\":" + std::to_string(static_cast<unsigned>(channel)) +
         "}";
}

std::string TalkMessage(bool talking, std::uint64_t session_id) {
  return Toggle("ptt", talking, session_id);
}

std::string MuteMessage(bool muted, std::uint64_t session_id) {
  return Toggle("mute", muted, session_id);
}

std::string DeafenMessage(bool deafened, std::uint64_t session_id) {
  return Toggle("deafen", deafened, session_id);
}

core::Result<ServerMessage> ParseServerMessage(std::string_view json) {
  // Le JSON est un sous-ensemble du YAML que le lecteur du client sait déjà
  // lire. ⚠ SES EXCEPTIONS S'ARRÊTENT ICI : un texte du serveur ne se croit pas.
  YAML::Node object;
  try {
    object = YAML::Load(std::string(json));
  } catch (const YAML::Exception& failure) {
    return Fail(ErrorCode::kCorruptData, std::string("message voix illisible : ") + failure.what());
  }
  if (!object.IsMap()) {
    return Fail(ErrorCode::kCorruptData, "message voix qui n'est pas un objet");
  }
  const std::string type = ValueOr<std::string>(object, "type", "");
  if (type.empty()) {
    return Fail(ErrorCode::kCorruptData, "message voix sans type");
  }
  if (type == "auth_ok") {
    return AuthAccepted{.udp_port = ValueOr<std::uint16_t>(object, "udp_port", 0),
                        .udp_token = ValueOr<std::uint64_t>(object, "udp_token", 0)};
  }
  if (type == "your_pos") {
    return OwnPlace{.map = ValueOr<std::string>(object, "map", ""),
                    .x = ValueOr<std::int32_t>(object, "x", 0),
                    .y = ValueOr<std::int32_t>(object, "y", 0)};
  }
  if (type == "nearby_players") return ReadNearby(object);
  if (type == "channel_ack") return ReadChannel(object);
  if (type == "war_state") {
    return WarState{.active = ValueOr<bool>(object, "active", false),
                    .recommended_channel = ValueOr<std::int32_t>(object, "recommended_channel", 0)};
  }
  if (type == "room_joined") return RoomJoined{};
  if (type == "room_left") return RoomLeft{};
  if (type == "pong") return Pong{};
  if (type == "error") {
    return Refusal{.kind = RefusalKind::kError,
                   .message = ValueOr<std::string>(object, "message", "")};
  }
  if (type == "admin_banned") return Refusal{.kind = RefusalKind::kBanned, .message = type};
  if (type == "license_required") {
    return Refusal{.kind = RefusalKind::kLicenseRequired, .message = type};
  }
  if (type == "flood_banned") return Refusal{.kind = RefusalKind::kFlooded, .message = type};
  return Unread{.type = type};
}

}  // namespace ro::voice
