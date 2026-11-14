// LES RÉGLAGES DE COMMANDE, SUR LE FIL : ce que le serveur pousse des réglages
// `@showexp`, `@noks`, `@autoloot`…, la liste des presets de ramassage, et
// l'ordre qu'on lui envoie sur un preset.
//
// 🔴 DEUX VOCABULAIRES : le fil porte le seuil de zeny divisé par cent. Une
// conversion oubliée dans un sens ne se voit pas — la case affiche un nombre
// plausible, cent fois trop petit.

#include "ro/net/command_settings.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/player_settings.h"
#include "ro/proto/map_packets.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/wire_bytes.h"

using ro::net::ApplyCommandSettingsFrame;
using ro::net::AutolootPresetOrder;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::net::PlayerSetting;
using ro::sim::CommandSettings;
using ro::sim::ItemFacts;
using ro::test::Connect;
using ro::test::LoopbackServer;
using ro::test_support::PutU16;
using ro::test_support::PutU32;

namespace {

constexpr std::uint32_t kCharId = 150000;
constexpr std::uint32_t kJellopy = 909;
constexpr std::uint32_t kApple = 512;

// Une trame 0x0F05 telle que le serveur l'écrit.
[[nodiscard]] Frame Settings(const std::vector<PlayerSetting>& settings) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, ro::proto::ZcMoonlightSettings::kOpcode);
  PutU16(bytes, static_cast<std::uint16_t>(ro::net::kPlayerSettingsHeader +
                                           settings.size() * ro::net::kPlayerSettingSize));
  PutU32(bytes, kCharId);
  PutU16(bytes, static_cast<std::uint16_t>(settings.size()));
  for (const PlayerSetting& setting : settings) {
    PutU16(bytes, setting.id);
    PutU32(bytes, setting.value);
  }
  return Frame{.opcode = ro::proto::ZcMoonlightSettings::kOpcode, .bytes = std::move(bytes)};
}

struct Preset {
  std::uint8_t slot;
  bool autoload;
  std::string name;
};

// Une trame 0x0F07 : le preset chargé, puis chaque preset.
[[nodiscard]] Frame Presets(std::uint8_t active, const std::vector<Preset>& presets) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, ro::proto::ZcMoonlightPresetList::kOpcode);
  PutU16(bytes, 0);
  bytes.push_back(active);
  bytes.push_back(static_cast<std::uint8_t>(presets.size()));
  for (const Preset& preset : presets) {
    bytes.push_back(preset.slot);
    bytes.push_back(preset.autoload ? 1 : 0);
    bytes.push_back(static_cast<std::uint8_t>(preset.name.size()));
    bytes.insert(bytes.end(), preset.name.begin(), preset.name.end());
  }
  bytes[2] = static_cast<std::uint8_t>(bytes.size() & 0xFFU);
  bytes[3] = static_cast<std::uint8_t>(bytes.size() >> 8U);
  return Frame{.opcode = ro::proto::ZcMoonlightPresetList::kOpcode, .bytes = std::move(bytes)};
}

}  // namespace

TEST_CASE("Réglages de commande : chaque valeur se lit dans son unité", "[net][settings]") {
  CommandSettings settings;
  const ItemFacts facts;
  CHECK_FALSE(settings.told);
  REQUIRE(
      ApplyCommandSettingsFrame(Settings({{.id = ro::net::kSettingShowExp, .value = 1},
                                          {.id = ro::net::kSettingKillStealGuard, .value = 2},
                                          {.id = ro::net::kSettingAutolootRate, .value = 40},
                                          {.id = ro::net::kSettingAutolootMinZeny, .value = 250},
                                          {.id = ro::net::kSettingAutolootTypes, .value = 0x48},
                                          {.id = ro::net::kSettingAutolootMvpRewards, .value = 1},
                                          {.id = ro::net::kSettingFlyWingMarker, .value = 1}}),
                                settings, facts) == FrameOutcome::kIgnored);
  CHECK(settings.told);
  CHECK(settings.show_exp);
  CHECK_FALSE(settings.show_zeny);
  CHECK(settings.kill_steal_guard == ro::sim::KillStealGuard::kParty);
  CHECK(settings.autoloot_rate_percent == 40);
  // ⚠ CENT FOIS CE QUE LE FIL PORTE.
  CHECK(settings.autoloot_min_zeny == 25000);
  CHECK(settings.autoloot_type_mask == 0x48);
  CHECK(settings.autoloot_mvp_rewards);
  CHECK(settings.fly_wing_marker);
}

TEST_CASE("Réglages de commande : une resynchronisation de liste ne dit rien des réglages",
          "[net][settings]") {
  // Le serveur resynchronise la liste de ramassage par une trame qui ne porte
  // qu'elle : les réglages déjà lus ne doivent pas en souffrir, et une page
  // encore muette ne doit pas se croire renseignée.
  CommandSettings settings;
  const ItemFacts facts;
  REQUIRE(
      ApplyCommandSettingsFrame(Settings({{.id = ro::net::kSettingAutolootAdd, .value = 0},
                                          {.id = ro::net::kSettingAutolootAdd, .value = kJellopy}}),
                                settings, facts) == FrameOutcome::kIgnored);
  CHECK_FALSE(settings.told);

  settings.show_zeny = true;
  settings.told = true;
  REQUIRE(ApplyCommandSettingsFrame(Settings({{.id = ro::net::kSettingAutolootAdd, .value = 0}}),
                                    settings, facts) == FrameOutcome::kIgnored);
  CHECK(settings.show_zeny);
  CHECK(settings.told);
}

TEST_CASE("Réglages de commande : un mode de @noks inconnu se lit comme aucun", "[net][settings]") {
  CommandSettings settings;
  const ItemFacts facts;
  settings.kill_steal_guard = ro::sim::KillStealGuard::kGuild;
  REQUIRE(ApplyCommandSettingsFrame(Settings({{.id = ro::net::kSettingKillStealGuard, .value = 9}}),
                                    settings, facts) == FrameOutcome::kIgnored);
  CHECK(settings.kill_steal_guard == ro::sim::KillStealGuard::kOff);
}

TEST_CASE("Réglages de commande : la liste des presets, et la liste qu'elle sauvegarde",
          "[net][settings]") {
  CommandSettings settings;
  ItemFacts facts;
  facts.TellAutoloot({kJellopy, kApple});
  REQUIRE(ApplyCommandSettingsFrame(Presets(2, {{.slot = 1, .autoload = false, .name = "MVP"},
                                                {.slot = 2, .autoload = true, .name = ""}}),
                                    settings, facts) == FrameOutcome::kApplied);
  REQUIRE(settings.presets.size() == 2);
  CHECK(settings.presets[0].slot == 1);
  CHECK(settings.presets[0].name == "MVP");
  CHECK_FALSE(settings.presets[0].autoload);
  CHECK(settings.presets[1].name.empty());
  CHECK(settings.presets[1].autoload);
  CHECK(settings.active_preset == 2);
  CHECK(settings.saved_autoloot == std::vector<std::uint32_t>{kJellopy, kApple});
  CHECK(settings.presets_told_count == 1);
}

TEST_CASE("Réglages de commande : une liste de presets tronquée ne rend rien", "[net][settings]") {
  CommandSettings settings;
  settings.presets.push_back(ro::sim::AutolootPreset{.slot = 5, .name = "gardé"});
  const ItemFacts facts;
  Frame cut = Presets(1, {{.slot = 1, .autoload = false, .name = "Coupé"}});
  cut.bytes.pop_back();
  CHECK(ApplyCommandSettingsFrame(cut, settings, facts) == FrameOutcome::kUndecodable);
  REQUIRE(settings.presets.size() == 1);
  CHECK(settings.presets[0].name == "gardé");
  CHECK(settings.presets_told_count == 0);
}

TEST_CASE("Réglages de commande : un ordre de preset, octet pour octet", "[net][settings]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  ro::net::SendAutolootPresetOrder(link, AutolootPresetOrder::kSave, 3, "MVP");
  const std::vector<std::uint8_t> saved = server.Receive(9);
  const std::vector<std::uint8_t> expected_save = {0x06, 0x0F, 9, 0, 2, 3, 'M', 'V', 'P'};
  CHECK(saved == expected_save);
  // Sans nom, l'ordre s'arrête à l'emplacement.
  ro::net::SendAutolootPresetOrder(link, AutolootPresetOrder::kLoad, 3);
  const std::vector<std::uint8_t> loaded = server.Receive(6);
  const std::vector<std::uint8_t> expected_load = {0x06, 0x0F, 6, 0, 3, 3};
  CHECK(loaded == expected_load);
}
