#include "game/survivors_coop_mode.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "ro/core/log.h"
#include "ro/proto/packet_lengths.h"

namespace moonclient {
namespace {

using ro::net::FrameOutcome;

// Les champs de l'apparence, à la queue de la part.
constexpr std::size_t kLookBytes = 26;
constexpr std::size_t kByteBits = 8;

void PutU16(std::vector<std::uint8_t>& out, std::uint32_t value) {
  out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
  out.push_back(static_cast<std::uint8_t>((value >> kByteBits) & 0xFFU));
}

[[nodiscard]] std::uint16_t U16At(std::span<const std::uint8_t> bytes, std::size_t at) {
  return static_cast<std::uint16_t>(bytes[at] | (std::uint32_t{bytes[at + 1U]} << kByteBits));
}

}  // namespace

std::vector<std::uint8_t> EncodeCoopLook(const ro::render::ActorLook& look) {
  std::vector<std::uint8_t> out;
  out.reserve(kLookBytes);
  out.push_back(look.sex);
  out.push_back(look.body_palette_names_file ? std::uint8_t{1} : std::uint8_t{0});
  PutU16(out, look.hair);
  PutU16(out, static_cast<std::uint16_t>(look.hair_palette));
  PutU16(out, static_cast<std::uint16_t>(look.body_palette));
  PutU16(out, look.robe);
  for (const std::uint16_t headgear : look.headgears) PutU16(out, headgear);
  // La place restante : réservée, pour qu'une part garde sa taille.
  out.resize(kLookBytes);
  return out;
}

std::optional<ro::render::ActorLook> DecodeCoopLook(std::span<const std::uint8_t> bytes) {
  if (bytes.size() != kLookBytes) return std::nullopt;
  ro::render::ActorLook look;
  look.player = true;
  look.sex = bytes[0];
  look.body_palette_names_file = bytes[1] != 0;
  std::size_t at = 2;
  const auto next = [&] {
    const std::uint16_t value = U16At(bytes, at);
    at += 2;
    return value;
  };
  look.hair = next();
  look.hair_palette = static_cast<std::int16_t>(next());
  look.body_palette = static_cast<std::int16_t>(next());
  look.robe = next();
  for (std::uint16_t& headgear : look.headgears) headgear = next();
  return look;
}

std::optional<SurvivorsCoopDeparture> DepartureOf(const ro::net::SurvivorsCoopStart& start,
                                                  std::uint32_t own_char_id,
                                                  const ro::sim::SurvivorsRules& rules) {
  if (start.players.size() > ro::sim::kMostHeroes) return std::nullopt;
  SurvivorsCoopDeparture departure;
  std::vector<ro::sim::SurvivorsCoopPart> parts;
  std::optional<std::size_t> local;
  for (std::size_t index = 0; index < start.players.size(); ++index) {
    const ro::net::SurvivorsCoopPlayer& player = start.players[index];
    std::size_t trailing = 0;
    std::optional<ro::sim::SurvivorsCoopPart> part =
        ro::sim::DecodeSurvivorsCoopPart(player.part, &trailing);
    if (!part.has_value()) return std::nullopt;
    const std::optional<ro::render::ActorLook> look =
        DecodeCoopLook(std::span<const std::uint8_t>(player.part).last(trailing));
    if (!look.has_value()) return std::nullopt;
    departure.looks[index] = *look;
    parts.push_back(std::move(*part));
    if (player.char_id == own_char_id) local = index;
  }
  if (!local.has_value()) return std::nullopt;
  std::optional<ro::sim::SurvivorsStart> team = ro::sim::TeamStartOf(start.seed, rules, parts);
  if (!team.has_value()) return std::nullopt;
  departure.start = std::move(*team);
  departure.local_player = *local;
  departure.input_delay_ticks = start.input_delay_ticks;
  return departure;
}

SurvivorsCoopMode::SurvivorsCoopMode(const PacketLengthOf& length_of) {
  const PacketLengthOf lengths = length_of ? length_of : PacketLengthOf(ro::proto::PacketLength);
  const auto known = [&lengths](std::uint16_t opcode) { return lengths(opcode).has_value(); };
  if (known(ro::net::kSurvivorsCoopLobbyOpcode) && known(ro::net::kSurvivorsCoopStartOpcode) &&
      known(ro::net::kSurvivorsCoopFrameOpcode) && known(ro::net::kSurvivorsCoopLeftOpcode)) {
    caps_ = ro::net::kUiCapSurvivorsCoop;
  }
}

FrameOutcome SurvivorsCoopMode::ApplyFrame(const ro::net::Frame& frame) {
  switch (frame.opcode) {
    case ro::net::kSurvivorsCoopLobbyOpcode: {
      std::optional<ro::net::SurvivorsCoopLobby> lobby =
          ro::net::DecodeSurvivorsCoopLobby(frame.bytes);
      if (!lobby.has_value()) return FrameOutcome::kUndecodable;
      announced_ = true;
      lobby_ = std::move(*lobby);
      return FrameOutcome::kApplied;
    }
    case ro::net::kSurvivorsCoopStartOpcode: {
      std::optional<ro::net::SurvivorsCoopStart> start =
          ro::net::DecodeSurvivorsCoopStart(frame.bytes);
      if (!start.has_value()) return FrameOutcome::kUndecodable;
      // Le fil est ordonné : ce qui précède un départ vient de la partie d'avant.
      frames_.clear();
      leaves_.clear();
      excluded_ = false;
      start_ = std::move(*start);
      return FrameOutcome::kApplied;
    }
    case ro::net::kSurvivorsCoopFrameOpcode: {
      std::optional<ro::net::SurvivorsCoopFrame> relayed =
          ro::net::DecodeSurvivorsCoopFrame(frame.bytes);
      if (!relayed.has_value()) return FrameOutcome::kUndecodable;
      frames_.push_back(std::move(*relayed));
      return FrameOutcome::kApplied;
    }
    case ro::net::kSurvivorsCoopLeftOpcode: {
      const std::optional<ro::net::SurvivorsCoopLeft> left =
          ro::net::DecodeSurvivorsCoopLeft(frame.bytes);
      if (!left.has_value()) return FrameOutcome::kUndecodable;
      leaves_.push_back(*left);
      return FrameOutcome::kApplied;
    }
    default:
      return FrameOutcome::kUnhandled;
  }
}

void SurvivorsCoopMode::Forget() {
  announced_ = false;
  lobby_ = {};
  start_.reset();
  frames_.clear();
  leaves_.clear();
  roster_.clear();
  names_.clear();
  excluded_ = false;
}

std::optional<ro::net::SurvivorsCoopStart> SurvivorsCoopMode::TakeStart() {
  return std::exchange(start_, std::nullopt);
}

void SurvivorsCoopMode::Ask(ro::net::Connection& link, ro::net::SurvivorsCoopAction action,
                            std::span<const std::uint8_t> part) {
  if (!announced_ || !link.is_open()) return;
  const std::optional<std::vector<std::uint8_t>> bytes =
      ro::net::EncodeSurvivorsCoopLobbyAsk(action, part);
  if (!bytes.has_value()) return;
  link.Send(*bytes);
}

void SurvivorsCoopMode::Forsake(ro::net::Connection& link) {
  if (lobby_.state != ro::net::SurvivorsCoopState::kLobby && !start_.has_value()) return;
  Ask(link, ro::net::SurvivorsCoopAction::kLeave);
  lobby_ = {};
  start_.reset();
}

bool SurvivorsCoopMode::TakeExcluded() noexcept { return std::exchange(excluded_, false); }

void SurvivorsCoopMode::Roster(const ro::net::SurvivorsCoopStart& start,
                               std::uint32_t own_char_id) {
  own_char_id_ = own_char_id;
  roster_.clear();
  names_.clear();
  for (const ro::net::SurvivorsCoopPlayer& player : start.players) {
    roster_.push_back(player.char_id);
    std::string name;
    for (const ro::net::SurvivorsCoopMember& member : lobby_.members) {
      if (member.char_id == player.char_id) name = member.name;
    }
    names_.push_back(std::move(name));
  }
}

std::optional<std::size_t> SurvivorsCoopMode::PlayerOf(std::uint32_t char_id) const noexcept {
  const auto found = std::ranges::find(roster_, char_id);
  if (found == roster_.end()) return std::nullopt;
  return static_cast<std::size_t>(found - roster_.begin());
}

void SurvivorsCoopMode::Carry(SurvivorsStage& stage, ro::net::Connection& link,
                              const SurvivorsCoopFastPath& fast_path) {
  if (!stage.coop()) {
    frames_.clear();
    leaves_.clear();
    return;
  }
  if (fast_path.take) {
    for (const ro::net::SurvivorsCoopFrame& frame : fast_path.take()) {
      if (const std::optional<std::size_t> player = PlayerOf(frame.char_id)) {
        stage.CoopHear(*player, frame.payload);
      }
    }
  }
  for (const ro::net::SurvivorsCoopFrame& frame : frames_) {
    if (const std::optional<std::size_t> player = PlayerOf(frame.char_id)) {
      stage.CoopReceive(*player, frame.payload);
    }
  }
  frames_.clear();
  // Un départ se lit APRÈS les trames : sur le TCP, il les suit toutes.
  for (const ro::net::SurvivorsCoopLeft& left : leaves_) {
    if (left.char_id == own_char_id_) {
      excluded_ = true;
    } else if (const std::optional<std::size_t> player = PlayerOf(left.char_id)) {
      stage.CoopLeave(*player);
    }
  }
  leaves_.clear();
  while (std::optional<std::vector<std::uint8_t>> payload = stage.TakeCoopFrame()) {
    if (const auto bytes = ro::net::EncodeSurvivorsCoopFrameAsk(*payload); bytes.has_value()) {
      if (link.is_open()) link.Send(*bytes);
    }
    if (fast_path.post) fast_path.post(std::move(*payload));
  }
}

}  // namespace moonclient
