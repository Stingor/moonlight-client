// Les réglages de session, et le DROIT qui s'y cache.
//
// Ce qui se vérifie ici, c'est la lecture d'une liste que le générateur ne
// modélise pas : l'en-tête seul est engendré, la queue se lit à la main. Une
// disposition fausse ne se verrait pas autrement — le serveur n'attend aucune
// réponse à ce paquet, et un droit lu de travers ouvrirait une fenêtre à qui n'y
// a pas droit, ou la refuserait à qui l'a.

#include "ro/net/player_settings.h"

#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/world.h"

#include "support/loopback_server.h"
#include "support/wire_bytes.h"

using ro::net::ApplyPlayerSettingsFrame;
using ro::net::FrameOutcome;
using ro::net::PlayerSetting;
using ro::net::ReadPlayerSettings;
using ro::test::LoopbackServer;
using ro::test_support::PutU16;
using ro::test_support::PutU32;

namespace {

// La disposition déclarée par le serveur (`ZC_MOONLIGHT_SETTINGS`) :
//   [packetType:2][packetLength:2][char_id:4][count:2] puis count × {id:2, value:4}
constexpr std::uint16_t kOpcode = 0x0F05;
constexpr std::uint32_t kCharId = 150000;

// Une trame telle que le serveur l'écrit, à partir des réglages donnés.
[[nodiscard]] std::vector<std::uint8_t> SettingsFrame(const std::vector<PlayerSetting>& settings) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, kOpcode);
  PutU16(bytes, static_cast<std::uint16_t>(ro::net::kPlayerSettingsHeader +
                                           settings.size() * ro::net::kPlayerSettingSize));
  PutU32(bytes, kCharId);
  PutU16(bytes, static_cast<std::uint16_t>(settings.size()));
  for (const PlayerSetting& setting : settings) {
    PutU16(bytes, setting.id);
    PutU32(bytes, setting.value);
  }
  return bytes;
}

[[nodiscard]] ro::net::Frame FrameOf(std::vector<std::uint8_t> bytes) {
  ro::net::Frame frame;
  frame.opcode = kOpcode;
  frame.bytes = std::move(bytes);
  return frame;
}

}  // namespace

TEST_CASE("Réglages de session : la queue se lit entière", "[net][settings]") {
  const std::vector<PlayerSetting> sent = {{.id = 0, .value = 1},
                                           {.id = ro::net::kSettingGroupLevel, .value = 99},
                                           {.id = 6, .value = 42}};
  const std::vector<PlayerSetting> read = ReadPlayerSettings(SettingsFrame(sent));

  REQUIRE(read.size() == sent.size());
  for (std::size_t rank = 0; rank < sent.size(); ++rank) {
    CHECK(read[rank].id == sent[rank].id);
    CHECK(read[rank].value == sent[rank].value);
  }
}

TEST_CASE("Réglages de session : une trame tronquée ne rend RIEN", "[net][settings]") {
  // 🔴 TOUT OU RIEN, et c'est ce qui compte ici : un début de liste suivi de
  // valeurs inventées donnerait un droit au hasard. On préfère l'absence.
  std::vector<std::uint8_t> bytes =
      SettingsFrame({{.id = ro::net::kSettingGroupLevel, .value = 99}});
  bytes.pop_back();
  CHECK(ReadPlayerSettings(bytes).empty());

  // Le compte annoncé déborde de ce que la trame porte.
  std::vector<std::uint8_t> lying;
  PutU16(lying, kOpcode);
  PutU16(lying, static_cast<std::uint16_t>(ro::net::kPlayerSettingsHeader));
  PutU32(lying, kCharId);
  PutU16(lying, 4);
  CHECK(ReadPlayerSettings(lying).empty());

  // 🔴 LE CAS QUI SÉPARE VRAIMENT LES DEUX ÉCRITURES, et les trois ci-dessus ne
  // le font pas : ils ne laissent tenir AUCUNE entrée, si bien qu'une lecture
  // « tolérante » — lire ce qui tient — rendrait vide elle aussi, et passerait
  // pour juste. Ici, DEUX entrées sur trois sont présentes. Tolérante, la
  // lecture en rendrait deux et le droit se prendrait dans une liste amputée ;
  // ce client préfère n'en rendre aucune.
  std::vector<std::uint8_t> two_of_three;
  PutU16(two_of_three, kOpcode);
  PutU16(two_of_three, static_cast<std::uint16_t>(ro::net::kPlayerSettingsHeader +
                                                  3 * ro::net::kPlayerSettingSize));
  PutU32(two_of_three, kCharId);
  PutU16(two_of_three, 3);
  PutU16(two_of_three, 0);
  PutU32(two_of_three, 1);
  PutU16(two_of_three, ro::net::kSettingGroupLevel);
  PutU32(two_of_three, 99);
  CHECK(ReadPlayerSettings(two_of_three).empty());

  // Un en-tête à lui seul incomplet.
  CHECK(ReadPlayerSettings(std::vector<std::uint8_t>{0x05, 0x0F}).empty());
}

TEST_CASE("Réglages de session : le niveau de groupe atteint le monde", "[net][settings]") {
  ro::sim::World world;
  REQUIRE(world.group_level() == 0);

  const FrameOutcome outcome = ApplyPlayerSettingsFrame(
      FrameOf(
          SettingsFrame({{.id = 0, .value = 1}, {.id = ro::net::kSettingGroupLevel, .value = 80}})),
      world);
  // ⚠ `kIgnored` : rien de ce qui se dessine n'en dépend, et le relevé de trames
  // doit continuer de distinguer un droit d'un corps qui bouge.
  CHECK(outcome == FrameOutcome::kIgnored);
  CHECK(world.group_level() == 80);
}

TEST_CASE("Réglages de session : une valeur du fil est bornée", "[net][settings]") {
  // La valeur voyage sur quatre octets ; le niveau de groupe tient sur un. Une
  // valeur aberrante ne doit pas repasser par zéro en se rétrécissant.
  ro::sim::World world;
  static_cast<void>(ApplyPlayerSettingsFrame(
      FrameOf(SettingsFrame({{.id = ro::net::kSettingGroupLevel, .value = 0x0000'0100U}})), world));
  CHECK(world.group_level() == 255);
}

TEST_CASE("Réglages de session : le monde garde son niveau d'une carte à l'autre",
          "[net][settings]") {
  // Le droit vient de la SESSION, pas de la carte : le perdre en changeant de
  // carte fermerait l'établi à chaque portail.
  ro::sim::World world;
  static_cast<void>(ApplyPlayerSettingsFrame(
      FrameOf(SettingsFrame({{.id = ro::net::kSettingGroupLevel, .value = 99}})), world));
  world.Clear();
  CHECK(world.group_level() == 99);
}

TEST_CASE("Réglages de session : @killer et @killable suivent ce que le serveur redit",
          "[net][settings]") {
  // Le serveur les garde et les redit après chaque bascule : la trame suivante
  // fait foi, à la baisse comme à la hausse.
  ro::sim::World world;
  static_cast<void>(ApplyPlayerSettingsFrame(
      FrameOf(SettingsFrame({{.id = ro::net::kSettingKiller, .value = 1},
                             {.id = ro::net::kSettingKillable, .value = 0}})),
      world));
  CHECK(world.session().killer);
  CHECK_FALSE(world.session().killable);

  static_cast<void>(ApplyPlayerSettingsFrame(
      FrameOf(SettingsFrame({{.id = ro::net::kSettingKiller, .value = 0},
                             {.id = ro::net::kSettingKillable, .value = 1}})),
      world));
  CHECK_FALSE(world.session().killer);
  CHECK(world.session().killable);
}

TEST_CASE("Réglages de session : @playertest et @nodelay atteignent le monde", "[net][settings]") {
  ro::sim::World world;
  static_cast<void>(ApplyPlayerSettingsFrame(
      FrameOf(SettingsFrame({{.id = ro::net::kSettingPlayerTest, .value = 1},
                             {.id = ro::net::kSettingNoDelay, .value = 1}})),
      world));
  CHECK(world.session().player_test);
  CHECK(world.session().no_delay);

  static_cast<void>(ApplyPlayerSettingsFrame(
      FrameOf(SettingsFrame({{.id = ro::net::kSettingNoDelay, .value = 0}})), world));
  CHECK(world.session().player_test);
  CHECK_FALSE(world.session().no_delay);
}

TEST_CASE("Réglages de session : le relai Discord suit ce que le serveur garde",
          "[net][settings]") {
  // Vrai tant que le serveur n'a rien dit : c'est aussi son défaut.
  ro::sim::World world;
  CHECK(world.session().discord_chat);

  static_cast<void>(ApplyPlayerSettingsFrame(
      FrameOf(SettingsFrame({{.id = ro::net::kSettingDiscordChat, .value = 0}})), world));
  CHECK_FALSE(world.session().discord_chat);

  static_cast<void>(ApplyPlayerSettingsFrame(
      FrameOf(SettingsFrame({{.id = ro::net::kSettingDiscordChat, .value = 1}})), world));
  CHECK(world.session().discord_chat);
}

TEST_CASE("Réglages de session : un relai Discord dit coupé se rallume, une fois",
          "[net][settings]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  auto opened = ro::net::Connection::Open("127.0.0.1", server.port());
  REQUIRE(opened.has_value());
  REQUIRE(server.Accept());
  ro::net::Connection link = std::move(*opened);
  std::vector<std::vector<std::uint8_t>> sent;
  link.set_send_observer([&sent](std::span<const std::uint8_t> frame) {
    sent.emplace_back(frame.begin(), frame.end());
  });

  // Tant que le serveur n'a rien dit, ou qu'il le dit ouvert : rien ne part.
  ro::sim::World world;
  CHECK_FALSE(ro::net::ReviveDiscordRelay(link, world));
  static_cast<void>(ApplyPlayerSettingsFrame(
      FrameOf(SettingsFrame({{.id = ro::net::kSettingDiscordChat, .value = 1}})), world));
  CHECK_FALSE(ro::net::ReviveDiscordRelay(link, world));
  CHECK(sent.empty());

  // Dit coupé : la demande d'allumage part, et le miroir la tient pour faite.
  static_cast<void>(ApplyPlayerSettingsFrame(
      FrameOf(SettingsFrame({{.id = ro::net::kSettingDiscordChat, .value = 0}})), world));
  CHECK(ro::net::ReviveDiscordRelay(link, world));
  REQUIRE(sent.size() == 1);
  ro::proto::CzMoonlightSetting request;
  REQUIRE(ro::proto::Decode(sent.front(), request));
  CHECK(request.id == ro::net::kSettingDiscordChat);
  CHECK(request.value == 1U);
  CHECK(world.session().discord_chat);

  // Redit coupé dans la même entrée en jeu : on ne redemande pas.
  static_cast<void>(ApplyPlayerSettingsFrame(
      FrameOf(SettingsFrame({{.id = ro::net::kSettingDiscordChat, .value = 0}})), world));
  CHECK_FALSE(ro::net::ReviveDiscordRelay(link, world));
  CHECK(sent.size() == 1);
}

TEST_CASE("Réglages de session : un relai Discord coupé ne se demande pas hors ligne",
          "[net][settings]") {
  ro::net::Connection closed;
  ro::sim::World world;
  static_cast<void>(ApplyPlayerSettingsFrame(
      FrameOf(SettingsFrame({{.id = ro::net::kSettingDiscordChat, .value = 0}})), world));
  CHECK_FALSE(ro::net::ReviveDiscordRelay(closed, world));
  CHECK_FALSE(world.session().discord_chat);
}

TEST_CASE("Réglages de session : un autre opcode n'est pas pour nous", "[net][settings]") {
  ro::sim::World world;
  ro::net::Frame other = FrameOf(SettingsFrame({{.id = ro::net::kSettingGroupLevel, .value = 99}}));
  other.opcode = 0x0F06;
  CHECK(ApplyPlayerSettingsFrame(other, world) == FrameOutcome::kUnhandled);
  CHECK(world.group_level() == 0);
}
