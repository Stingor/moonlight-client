#include "ro/net/command_settings.h"

#include <algorithm>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "ro/core/byte_reader.h"
#include "ro/core/byte_writer.h"
#include "ro/net/player_settings.h"
#include "ro/proto/map_packets.h"

namespace ro::net {
namespace {

// La plus haute des protections contre le vol de kill : une valeur au-delà ne
// désigne rien, et se lit comme « aucune ».
constexpr std::uint32_t kHighestKillStealGuard =
    static_cast<std::uint32_t>(sim::KillStealGuard::kGuild);
// Le taux de ramassage le plus haut, en pour-cent.
constexpr std::uint32_t kHighestAutolootRatePercent = 100;

// Pose UN réglage. Rend faux si ce client ne le lit pas.
bool TellSetting(sim::CommandSettings& settings, const PlayerSetting& setting) {
  const bool on = setting.value != 0;
  switch (setting.id) {
    case kSettingShowExp:
      settings.show_exp = on;
      return true;
    case kSettingShowZeny:
      settings.show_zeny = on;
      return true;
    case kSettingShowMobInfo:
      settings.show_mob_info = on;
      return true;
    case kSettingSeparateKills:
      settings.separate_kills = on;
      return true;
    case kSettingBlockExp:
      settings.block_exp = on;
      return true;
    case kSettingAutolootRares:
      settings.autoloot_rares = on;
      return true;
    case kSettingAutolootRate:
      settings.autoloot_rate_percent =
          static_cast<int>(std::min(setting.value, kHighestAutolootRatePercent));
      return true;
    case kSettingAutolootMinZeny:
      // ⚠ SEIZE BITS AU PLUS : c'est ce que le serveur promet, et ce qui garde le
      // produit loin du débordement d'un `int`.
      settings.autoloot_min_zeny = static_cast<int>(std::min<std::uint32_t>(
                                       setting.value, std::numeric_limits<std::uint16_t>::max())) *
                                   kAutolootMinZenyWireUnit;
      return true;
    case kSettingAutolootTypes:
      settings.autoloot_type_mask = static_cast<std::uint16_t>(setting.value);
      return true;
    case kSettingShowSkillDelay:
      settings.show_skill_delay = on;
      return true;
    case kSettingShowSpeed:
      settings.show_speed = on;
      return true;
    case kSettingSellStuff:
      settings.sell_stuff = on;
      return true;
    case kSettingSellItem:
      settings.sell_item = on;
      return true;
    case kSettingNoAsk:
      settings.no_ask = on;
      return true;
    case kSettingKillStealGuard:
      settings.kill_steal_guard = setting.value > kHighestKillStealGuard
                                      ? sim::KillStealGuard::kOff
                                      : static_cast<sim::KillStealGuard>(setting.value);
      return true;
    case kSettingWings:
      settings.wings = on;
      return true;
    case kSettingAutolootMvp:
      settings.autoloot_mvp = on;
      return true;
    case kSettingAutolootMvpRewards:
      settings.autoloot_mvp_rewards = on;
      return true;
    case kSettingFlyWingMarker:
      settings.fly_wing_marker = on;
      return true;
    default:
      return false;
  }
}

// Lit la liste des presets. Rien, plutôt qu'une liste coupée : un preset à demi
// lu porterait un nom inventé.
[[nodiscard]] FrameOutcome TellPresets(const Frame& frame, sim::CommandSettings& settings,
                                       const sim::ItemFacts& facts) {
  proto::ZcMoonlightPresetList head;
  if (!proto::Decode(frame.bytes, head)) {
    return FrameOutcome::kUndecodable;
  }
  core::ByteReader reader(frame.bytes);
  reader.Skip(kAutolootPresetListHeader);
  std::vector<sim::AutolootPreset> presets;
  presets.reserve(head.count);
  for (std::uint8_t rank = 0; rank < head.count; ++rank) {
    sim::AutolootPreset preset;
    preset.slot = reader.U8();
    preset.autoload = reader.U8() != 0;
    const std::uint8_t name_bytes = reader.U8();
    const std::span<const std::uint8_t> name = reader.Bytes(name_bytes);
    if (!reader.ok()) {
      return FrameOutcome::kUndecodable;
    }
    preset.name.assign(name.begin(), name.end());
    presets.push_back(std::move(preset));
  }
  settings.presets = std::move(presets);
  settings.active_preset = head.active_no;
  // LA LISTE ANNONCÉE AVANT EST CELLE QUE CE PRESET PORTE : le serveur répond
  // ainsi à chaque sauvegarde, chargement ou suppression.
  settings.saved_autoloot = facts.autoloot();
  ++settings.presets_told_count;
  return FrameOutcome::kApplied;
}

}  // namespace

FrameOutcome ApplyCommandSettingsFrame(const Frame& frame, sim::CommandSettings& settings,
                                       const sim::ItemFacts& facts) {
  if (frame.opcode == proto::ZcMoonlightPresetList::kOpcode) {
    return TellPresets(frame, settings, facts);
  }
  if (frame.opcode != proto::ZcMoonlightSettings::kOpcode) {
    return FrameOutcome::kUnhandled;
  }
  const std::vector<PlayerSetting> read = ReadPlayerSettings(frame.bytes);
  if (read.empty()) {
    return FrameOutcome::kUndecodable;
  }
  bool told = false;
  for (const PlayerSetting& setting : read) {
    told = TellSetting(settings, setting) || told;
  }
  settings.told = settings.told || told;
  return FrameOutcome::kIgnored;
}

void SendAutolootPresetOrder(Connection& link, AutolootPresetOrder order, std::uint8_t slot,
                             std::string_view name) {
  // ⚠ ÉCRITE À LA MAIN : le nom est une queue de longueur variable, que le
  // générateur ne sait pas modéliser — il en ferait un champ d'un octet.
  const std::string_view kept = name.substr(0, std::min(name.size(), kAutolootPresetNameBytes));
  std::vector<std::uint8_t> bytes;
  core::ByteWriter writer(bytes);
  writer.U16(proto::CzMoonlightPresetCmd::kOpcode);
  writer.U16(static_cast<std::uint16_t>(kAutolootPresetOrderHeader + kept.size()));
  writer.U8(static_cast<std::uint8_t>(order));
  writer.U8(slot);
  writer.Text(kept);
  link.Send(bytes);
}

}  // namespace ro::net
