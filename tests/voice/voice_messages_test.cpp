// LES MESSAGES DE CONTRÔLE : ce que le client écrit, relu par le même lecteur
// que ceux du serveur, et chaque message du serveur tel qu'il l'écrit.

#include "ro/voice/voice_messages.h"

#include <string>
#include <variant>

#include <catch2/catch_test_macros.hpp>

using ro::voice::AuthAccepted;
using ro::voice::ParseServerMessage;
using ro::voice::ServerMessage;

namespace {

template <typename T>
[[nodiscard]] T Read(const std::string& json) {
  const ro::core::Result<ServerMessage> message = ParseServerMessage(json);
  REQUIRE(message.has_value());
  REQUIRE(std::holds_alternative<T>(*message));
  return std::get<T>(*message);
}

}  // namespace

TEST_CASE("Voix : l'authentification ne porte que l'identité", "[voice][messages]") {
  const std::string auth =
      ro::voice::AuthMessage(ro::voice::VoiceLogin{.account_id = 2000001,
                                                   .character_id = 150001,
                                                   .login_id1 = 4000000000U,
                                                   .session_id = 18000000000000000000ULL});
  CHECK(auth ==
        "{\"type\":\"auth\",\"account_id\":2000001,\"char_id\":150001,"
        "\"session_id\":18000000000000000000,\"login_id1\":4000000000}");
  CHECK(auth.find("secret") == std::string::npos);
}

TEST_CASE("Voix : les ordres du client", "[voice][messages]") {
  CHECK(ro::voice::SetChannelMessage(ro::voice::VoiceChannel::kGuild) ==
        "{\"type\":\"set_channel\",\"channel\":2}");
  CHECK(ro::voice::TalkMessage(true, 7) == "{\"type\":\"ptt\",\"value\":true,\"session_id\":7}");
  CHECK(ro::voice::DeafenMessage(false, 7) ==
        "{\"type\":\"deafen\",\"value\":false,\"session_id\":7}");
  CHECK(ro::voice::PingMessage(42) == "{\"type\":\"ping\",\"t\":42}");
  CHECK(ro::voice::BlockMessage(150002, true) ==
        "{\"type\":\"block_add\",\"target_char_id\":150002}");
  CHECK(ro::voice::BlockMessage(150002, false) ==
        "{\"type\":\"block_remove\",\"target_char_id\":150002}");
}

TEST_CASE("Voix : les blocages que dit le serveur", "[voice][messages]") {
  // ⚠ LA LISTE NOMME `char_id`, LA PROXIMITÉ `id` : chaque message a sa clé.
  const auto blocked = Read<ro::voice::BlockedPlayers>(
      "{\"type\":\"your_blocks\",\"blocked\":[{\"char_id\":150003,\"name\":\"Bavard\"},"
      "{\"id\":150004,\"name\":\"Mal nommé\"}]}");
  REQUIRE(blocked.players.size() == 1);
  CHECK(blocked.players[0].character_id == 150003);
  CHECK(blocked.players[0].name == "Bavard");

  const auto added = Read<ro::voice::BlockChanged>(
      "{\"type\":\"block_added\",\"target_char_id\":150003,\"target_name\":\"Bavard\"}");
  CHECK(added.blocked);
  CHECK(added.character_id == 150003);
  CHECK(added.name == "Bavard");
  const auto removed =
      Read<ro::voice::BlockChanged>("{\"type\":\"block_removed\",\"target_char_id\":150003}");
  CHECK_FALSE(removed.blocked);
  CHECK(removed.character_id == 150003);
}

TEST_CASE("Voix : ce que dit le serveur", "[voice][messages]") {
  const AuthAccepted accepted = Read<AuthAccepted>(
      "{\"type\":\"auth_ok\",\"udp_port\":7000,\"udp_token\":18446744073709551615}");
  CHECK(accepted.udp_port == 7000);
  CHECK(accepted.udp_token == 18446744073709551615ULL);

  const auto place =
      Read<ro::voice::OwnPlace>("{\"type\":\"your_pos\",\"x\":150,\"y\":180,\"map\":\"prontera\"}");
  CHECK(place.map == "prontera");
  CHECK(place.x == 150);

  const auto nearby = Read<ro::voice::NearbyPlayers>(
      "{\"type\":\"nearby_players\",\"players\":[{\"id\":150002,\"name\":\"Ami : à "
      "côté\"},{\"id\":0}]}");
  REQUIRE(nearby.players.size() == 1);
  CHECK(nearby.players[0].character_id == 150002);
  CHECK(nearby.players[0].name == "Ami : à côté");

  CHECK(Read<ro::voice::ChannelAccepted>("{\"type\":\"channel_ack\",\"channel\":1}").channel ==
        ro::voice::VoiceChannel::kParty);
  CHECK(Read<ro::voice::ListenAccepted>("{\"type\":\"listen_ack\",\"mask\":13}").listened == 5);
  CHECK(ro::voice::SetListenMessage(6) == "{\"type\":\"set_listen\",\"mask\":6}");
  const auto war = Read<ro::voice::WarState>(
      "{\"type\":\"war_state\",\"active\":true,\"recommended_channel\":2}");
  CHECK(war.active);
  CHECK(war.recommended_channel == 2);

  const auto refused =
      Read<ro::voice::Refusal>("{\"type\":\"error\",\"message\":\"vip required\"}");
  CHECK(refused.kind == ro::voice::RefusalKind::kError);
  CHECK(refused.message == "vip required");
  CHECK(Read<ro::voice::Refusal>("{\"type\":\"flood_banned\",\"duration_ms\":300000}").kind ==
        ro::voice::RefusalKind::kFlooded);
  CHECK(Read<ro::voice::Unread>("{\"type\":\"whisper_incoming\"}").type == "whisper_incoming");
  CHECK(Read<ro::voice::RefusalLifted>("{\"type\":\"admin_unbanned\"}").kind ==
        ro::voice::RefusalKind::kBanned);
  CHECK(Read<ro::voice::RefusalLifted>("{\"type\":\"license_granted\"}").kind ==
        ro::voice::RefusalKind::kLicenseRequired);
}

TEST_CASE("Voix : un texte qui n'est pas un message se refuse", "[voice][messages]") {
  CHECK_FALSE(ParseServerMessage("{\"type\":").has_value());
  CHECK_FALSE(ParseServerMessage("[1,2]").has_value());
  CHECK_FALSE(ParseServerMessage("{\"t\":1}").has_value());
}
