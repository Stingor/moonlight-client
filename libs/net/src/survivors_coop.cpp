// La coop du mode survivors sur le fil : codage et décodage, sans émettre. Voir
// l'en-tête pour la forme des trames.

#include "ro/net/survivors_coop.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ro/core/byte_reader.h"
#include "ro/core/byte_writer.h"
#include "ro/core/text.h"
#include "ro/proto/map_packets.h"

namespace ro::net {
namespace {

// Le contrat de l'en-tête est celui des structures que `packetgen` engendre des
// en-têtes du serveur : un écart arrête la bâtisse.
static_assert(kSurvivorsCoopLobbyAskOpcode == proto::CzMoonlightSurvivorsCoopLobby::kOpcode);
static_assert(kSurvivorsCoopLobbyOpcode == proto::ZcMoonlightSurvivorsCoopLobby::kOpcode);
static_assert(kSurvivorsCoopStartOpcode == proto::ZcMoonlightSurvivorsCoopStart::kOpcode);
static_assert(kSurvivorsCoopFrameAskOpcode == proto::CzMoonlightSurvivorsCoopFrame::kOpcode);
static_assert(kSurvivorsCoopFrameOpcode == proto::ZcMoonlightSurvivorsCoopFrame::kOpcode);
static_assert(kSurvivorsCoopLeftOpcode == proto::ZcMoonlightSurvivorsCoopLeft::kOpcode);
static_assert(kSurvivorsCoopLeftSize == proto::ZcMoonlightSurvivorsCoopLeft::kWireSize);

constexpr std::size_t kLobbyAskHeaderBytes = proto::CzMoonlightSurvivorsCoopLobby::kWireSize;
constexpr std::size_t kLobbyHeaderBytes = proto::ZcMoonlightSurvivorsCoopLobby::kWireSize;
constexpr std::size_t kLobbyEntryBytes = proto::ZcMoonlightSurvivorsCoopLobbyEntry::kWireSize;
constexpr std::size_t kStartHeaderBytes = proto::ZcMoonlightSurvivorsCoopStart::kWireSize;
constexpr std::size_t kFrameAskHeaderBytes = proto::CzMoonlightSurvivorsCoopFrame::kWireSize;
constexpr std::size_t kFrameHeaderBytes = proto::ZcMoonlightSurvivorsCoopFrame::kWireSize;
constexpr std::size_t kStartPlayerHeaderBytes = 6;
constexpr std::size_t kU32Bytes = 4;
constexpr std::size_t kU16Bytes = 2;
constexpr std::size_t kU64Bytes = 8;
constexpr std::size_t kByteBits = 8;

[[nodiscard]] std::uint16_t U16At(std::span<const std::uint8_t> bytes, std::size_t at) {
  return static_cast<std::uint16_t>(bytes[at] | (std::uint32_t{bytes[at + 1U]} << kByteBits));
}

[[nodiscard]] std::uint32_t U32At(std::span<const std::uint8_t> bytes, std::size_t at) {
  std::uint32_t value = 0;
  for (std::size_t index = 0; index < kU32Bytes; ++index) {
    value |= std::uint32_t{bytes[at + index]} << (index * kByteBits);
  }
  return value;
}

[[nodiscard]] std::uint64_t U64At(std::span<const std::uint8_t> bytes, std::size_t at) {
  std::uint64_t value = 0;
  for (std::size_t index = 0; index < kU64Bytes; ++index) {
    value |= std::uint64_t{bytes[at + index]} << (index * kByteBits);
  }
  return value;
}

// L'en-tête d'une trame : l'opcode attendu, et une longueur égale à la trame.
[[nodiscard]] bool HeaderHolds(std::span<const std::uint8_t> frame, std::uint16_t opcode,
                               std::size_t least_bytes) {
  return frame.size() >= least_bytes && U16At(frame, 0) == opcode &&
         U16At(frame, kU16Bytes) == frame.size();
}

}  // namespace

std::optional<std::vector<std::uint8_t>> EncodeSurvivorsCoopLobbyAsk(
    SurvivorsCoopAction action, std::span<const std::uint8_t> part) {
  if (part.size() > kSurvivorsCoopMostPartBytes) return std::nullopt;
  std::vector<std::uint8_t> bytes;
  bytes.reserve(kLobbyAskHeaderBytes + part.size());
  core::ByteWriter writer(bytes);
  writer.U16(kSurvivorsCoopLobbyAskOpcode);
  writer.U16(static_cast<std::uint16_t>(kLobbyAskHeaderBytes + part.size()));
  writer.U8(static_cast<std::uint8_t>(action));
  bytes.insert(bytes.end(), part.begin(), part.end());
  return bytes;
}

std::optional<std::vector<std::uint8_t>> EncodeSurvivorsCoopFrameAsk(
    std::span<const std::uint8_t> payload) {
  if (payload.size() < kSurvivorsCoopLeastPayloadBytes ||
      payload.size() > kSurvivorsCoopMostPayloadBytes) {
    return std::nullopt;
  }
  std::vector<std::uint8_t> bytes;
  bytes.reserve(kFrameAskHeaderBytes + payload.size());
  core::ByteWriter writer(bytes);
  writer.U16(kSurvivorsCoopFrameAskOpcode);
  writer.U16(static_cast<std::uint16_t>(kFrameAskHeaderBytes + payload.size()));
  bytes.insert(bytes.end(), payload.begin(), payload.end());
  return bytes;
}

std::vector<std::uint8_t> SurvivorsCoopChoicePart(std::uint16_t class_id,
                                                  std::uint16_t first_skill_id) {
  std::vector<std::uint8_t> part;
  part.reserve(2U * kU16Bytes);
  core::ByteWriter writer(part);
  writer.U16(class_id);
  writer.U16(first_skill_id);
  return part;
}

std::optional<SurvivorsCoopLobby> DecodeSurvivorsCoopLobby(std::span<const std::uint8_t> frame) {
  if (!HeaderHolds(frame, kSurvivorsCoopLobbyOpcode, kLobbyHeaderBytes)) return std::nullopt;
  const std::uint8_t state = frame[4];
  if (state > static_cast<std::uint8_t>(SurvivorsCoopState::kPlaying)) return std::nullopt;
  SurvivorsCoopLobby lobby;
  lobby.state = static_cast<SurvivorsCoopState>(state);
  lobby.refusals = U32At(frame, 5);
  const std::size_t count = frame[9];
  if (frame.size() != kLobbyHeaderBytes + count * kLobbyEntryBytes) return std::nullopt;
  for (std::size_t index = 0; index < count; ++index) {
    const std::size_t at = kLobbyHeaderBytes + index * kLobbyEntryBytes;
    SurvivorsCoopMember member;
    member.char_id = U32At(frame, at);
    member.ready = frame[at + kU32Bytes] != 0;
    const std::span<const std::uint8_t> name =
        frame.subspan(at + kU32Bytes + 1U, kSurvivorsCoopNameBytes);
    const auto end = std::ranges::find(name, std::uint8_t{0});
    member.name = core::Utf8FromCp1252(std::string(name.begin(), end));
    const std::size_t choice_at = at + kU32Bytes + 1U + kSurvivorsCoopNameBytes;
    member.class_id = U16At(frame, choice_at);
    member.first_skill_id = U16At(frame, choice_at + kU16Bytes);
    lobby.members.push_back(std::move(member));
  }
  return lobby;
}

std::optional<SurvivorsCoopStart> DecodeSurvivorsCoopStart(std::span<const std::uint8_t> frame) {
  if (!HeaderHolds(frame, kSurvivorsCoopStartOpcode, kStartHeaderBytes)) return std::nullopt;
  SurvivorsCoopStart start;
  start.seed = U64At(frame, 4);
  start.input_delay_ticks = frame[12];
  const std::size_t count = frame[13];
  std::size_t at = kStartHeaderBytes;
  for (std::size_t index = 0; index < count; ++index) {
    if (at + kStartPlayerHeaderBytes > frame.size()) return std::nullopt;
    SurvivorsCoopPlayer player;
    player.char_id = U32At(frame, at);
    const std::size_t part_bytes = U16At(frame, at + kU32Bytes);
    at += kStartPlayerHeaderBytes;
    if (part_bytes > kSurvivorsCoopMostPartBytes || at + part_bytes > frame.size()) {
      return std::nullopt;
    }
    const std::span<const std::uint8_t> part = frame.subspan(at, part_bytes);
    player.part.assign(part.begin(), part.end());
    at += part_bytes;
    start.players.push_back(std::move(player));
  }
  if (at != frame.size()) return std::nullopt;
  return start;
}

std::optional<SurvivorsCoopFrame> DecodeSurvivorsCoopFrame(std::span<const std::uint8_t> frame) {
  if (!HeaderHolds(frame, kSurvivorsCoopFrameOpcode, kFrameHeaderBytes)) return std::nullopt;
  const std::span<const std::uint8_t> payload = frame.subspan(kFrameHeaderBytes);
  if (payload.size() < kSurvivorsCoopLeastPayloadBytes ||
      payload.size() > kSurvivorsCoopMostPayloadBytes) {
    return std::nullopt;
  }
  return SurvivorsCoopFrame{.char_id = U32At(frame, 4),
                            .payload = std::vector<std::uint8_t>(payload.begin(), payload.end())};
}

std::optional<SurvivorsCoopLeft> DecodeSurvivorsCoopLeft(std::span<const std::uint8_t> frame) {
  if (frame.size() != kSurvivorsCoopLeftSize ||
      !HeaderHolds(frame, kSurvivorsCoopLeftOpcode, kSurvivorsCoopLeftSize)) {
    return std::nullopt;
  }
  return SurvivorsCoopLeft{.char_id = U32At(frame, 4), .through_tick = U32At(frame, 8)};
}

}  // namespace ro::net
