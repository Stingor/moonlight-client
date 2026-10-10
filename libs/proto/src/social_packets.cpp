#include "ro/proto/social_packets.h"

#include <algorithm>
#include <array>
#include <string_view>

#include "ro/core/byte_reader.h"
#include "ro/core/byte_writer.h"
#include "ro/proto/manual_packets.h"
#include "ro/proto/packet_lengths.h"

namespace ro::proto {
namespace {

using core::ByteReader;
using core::ByteWriter;
using core::ErrorCode;

// Le gestionnaire du serveur qui lit chaque paquet, et les décalages que sa
// table déclare.
constexpr std::string_view kFriendAddHandler = "clif_parse_FriendsListAdd";
constexpr std::array<std::uint16_t, 1> kFriendAddOffsets = {2};
constexpr std::string_view kFriendRemoveHandler = "clif_parse_FriendsListRemove";
constexpr std::array<std::uint16_t, 2> kFriendRemoveOffsets = {2, 6};
constexpr std::string_view kFriendAnswerHandler = "clif_parse_FriendsListReply";
constexpr std::array<std::uint16_t, 3> kFriendAnswerOffsets = {2, 6, 10};
constexpr std::string_view kLeaderHandler = "clif_parse_PartyChangeLeader";
constexpr std::array<std::uint16_t, 1> kLeaderOffsets = {2};
constexpr std::string_view kOptionsHandler = "clif_parse_PartyChangeOption";
constexpr std::array<std::uint16_t, 3> kOptionsOffsets = {2, 6, 7};
constexpr std::string_view kWhisperBlockHandler = "clif_parse_PMIgnore";
constexpr std::array<std::uint16_t, 2> kWhisperBlockOffsets = {2, 26};
constexpr std::string_view kWhisperListHandler = "clif_parse_PMIgnoreList";
constexpr std::string_view kAdoptionHandler = "clif_parse_Adopt_request";
constexpr std::array<std::uint16_t, 1> kAdoptionOffsets = {2};

constexpr std::size_t kNameSize = 24;
// Le serveur n'écrit jamais plus d'entrées de partage que de places dans un
// groupe ; une trame qui en annoncerait davantage se lit jusque-là.
constexpr std::size_t kPartyShareMost = 24;

core::Error LayoutDrift() {
  return core::Error{ErrorCode::kUnsupportedVersion,
                     "la disposition écrite pour ce paquet de groupe ou d'amis ne correspond "
                     "plus à ce que le serveur déclare"};
}

[[nodiscard]] bool Declared(std::uint16_t opcode, std::string_view handler,
                            std::span<const std::uint16_t> offsets, std::size_t wire_size) {
  if (!MatchesServerLayout(opcode, handler, offsets)) {
    return false;
  }
  const auto length = PacketLength(opcode);
  return length.has_value() && static_cast<std::size_t>(*length) == wire_size;
}

// Une trame reçue de longueur fixe : le bon opcode, et la longueur que la table
// du serveur déclare, qui doit être aussi celle qu'on sait lire.
[[nodiscard]] core::Result<ByteReader> FixedFrame(std::span<const std::uint8_t> frame,
                                                  std::uint16_t opcode, std::size_t wire_size) {
  const auto length = PacketLength(opcode);
  if (!length.has_value() || static_cast<std::size_t>(*length) != wire_size) {
    return std::unexpected(LayoutDrift());
  }
  if (frame.size() != wire_size) {
    return core::Fail(ErrorCode::kUnexpectedEnd, "trame de groupe ou d'amis de mauvaise longueur");
  }
  ByteReader reader(frame);
  if (reader.U16() != opcode) {
    return core::Fail(ErrorCode::kBadSignature, "trame d'un autre paquet");
  }
  return reader;
}

}  // namespace

core::Result<std::vector<std::uint8_t>> Encode(const FriendAddRequest& request) {
  if (!Declared(FriendAddRequest::kOpcode, kFriendAddHandler, kFriendAddOffsets,
                FriendAddRequest::kWireSize)) {
    return std::unexpected(LayoutDrift());
  }
  std::vector<std::uint8_t> frame;
  frame.reserve(FriendAddRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(FriendAddRequest::kOpcode);
  writer.FixedString(request.name, FriendAddRequest::kNameSize);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const FriendRemoveRequest& request) {
  if (!Declared(FriendRemoveRequest::kOpcode, kFriendRemoveHandler, kFriendRemoveOffsets,
                FriendRemoveRequest::kWireSize)) {
    return std::unexpected(LayoutDrift());
  }
  std::vector<std::uint8_t> frame;
  frame.reserve(FriendRemoveRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(FriendRemoveRequest::kOpcode);
  writer.U32(request.account_id);
  writer.U32(request.char_id);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const FriendAnswerRequest& request) {
  if (!Declared(FriendAnswerRequest::kOpcode, kFriendAnswerHandler, kFriendAnswerOffsets,
                FriendAnswerRequest::kWireSize)) {
    return std::unexpected(LayoutDrift());
  }
  std::vector<std::uint8_t> frame;
  frame.reserve(FriendAnswerRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(FriendAnswerRequest::kOpcode);
  writer.U32(request.account_id);
  writer.U32(request.char_id);
  writer.U32(request.answer);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const PartyLeaderRequest& request) {
  if (!Declared(PartyLeaderRequest::kOpcode, kLeaderHandler, kLeaderOffsets,
                PartyLeaderRequest::kWireSize)) {
    return std::unexpected(LayoutDrift());
  }
  std::vector<std::uint8_t> frame;
  frame.reserve(PartyLeaderRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(PartyLeaderRequest::kOpcode);
  writer.U32(request.account_id);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const PartyOptionsRequest& request) {
  if (!Declared(PartyOptionsRequest::kOpcode, kOptionsHandler, kOptionsOffsets,
                PartyOptionsRequest::kWireSize)) {
    return std::unexpected(LayoutDrift());
  }
  std::vector<std::uint8_t> frame;
  frame.reserve(PartyOptionsRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(PartyOptionsRequest::kOpcode);
  writer.U32(request.exp_share);
  writer.U8(request.item_pickup);
  writer.U8(request.item_share);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const WhisperBlockRequest& request) {
  if (!Declared(WhisperBlockRequest::kOpcode, kWhisperBlockHandler, kWhisperBlockOffsets,
                WhisperBlockRequest::kWireSize)) {
    return std::unexpected(LayoutDrift());
  }
  std::vector<std::uint8_t> frame;
  frame.reserve(WhisperBlockRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(WhisperBlockRequest::kOpcode);
  writer.FixedString(request.name, WhisperBlockRequest::kNameSize);
  writer.U8(request.type);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const WhisperBlockListRequest& /*request*/) {
  if (!Declared(WhisperBlockListRequest::kOpcode, kWhisperListHandler, {},
                WhisperBlockListRequest::kWireSize)) {
    return std::unexpected(LayoutDrift());
  }
  std::vector<std::uint8_t> frame;
  frame.reserve(WhisperBlockListRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(WhisperBlockListRequest::kOpcode);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const AdoptionRequest& request) {
  if (!Declared(AdoptionRequest::kOpcode, kAdoptionHandler, kAdoptionOffsets,
                AdoptionRequest::kWireSize)) {
    return std::unexpected(LayoutDrift());
  }
  std::vector<std::uint8_t> frame;
  frame.reserve(AdoptionRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(AdoptionRequest::kOpcode);
  writer.U32(request.account_id);
  return frame;
}

core::Result<FriendRequestNotice> DecodeFriendRequestNotice(std::span<const std::uint8_t> frame) {
  auto reader = FixedFrame(frame, FriendRequestNotice::kOpcode, FriendRequestNotice::kWireSize);
  if (!reader) {
    return std::unexpected(reader.error());
  }
  FriendRequestNotice notice;
  notice.account_id = reader->U32();
  notice.char_id = reader->U32();
  notice.name = std::string(reader->FixedString(kNameSize));
  return notice;
}

core::Result<FriendAddResult> DecodeFriendAddResult(std::span<const std::uint8_t> frame) {
  auto reader = FixedFrame(frame, FriendAddResult::kOpcode, FriendAddResult::kWireSize);
  if (!reader) {
    return std::unexpected(reader.error());
  }
  FriendAddResult result;
  result.result = reader->U16();
  result.account_id = reader->U32();
  result.char_id = reader->U32();
  result.name = std::string(reader->FixedString(kNameSize));
  return result;
}

core::Result<FriendRemovedNotice> DecodeFriendRemovedNotice(std::span<const std::uint8_t> frame) {
  auto reader = FixedFrame(frame, FriendRemovedNotice::kOpcode, FriendRemovedNotice::kWireSize);
  if (!reader) {
    return std::unexpected(reader.error());
  }
  FriendRemovedNotice notice;
  notice.account_id = reader->U32();
  notice.char_id = reader->U32();
  return notice;
}

core::Result<PartyOptionsNotice> DecodePartyOptionsNotice(std::span<const std::uint8_t> frame) {
  auto reader = FixedFrame(frame, PartyOptionsNotice::kOpcode, PartyOptionsNotice::kWireSize);
  if (!reader) {
    return std::unexpected(reader.error());
  }
  PartyOptionsNotice notice;
  notice.exp_share = reader->U32();
  notice.item_pickup = reader->U8();
  notice.item_share = reader->U8();
  return notice;
}

core::Result<AdoptionInvitationNotice> DecodeAdoptionInvitationNotice(
    std::span<const std::uint8_t> frame) {
  auto reader =
      FixedFrame(frame, AdoptionInvitationNotice::kOpcode, AdoptionInvitationNotice::kWireSize);
  if (!reader) {
    return std::unexpected(reader.error());
  }
  AdoptionInvitationNotice notice;
  notice.parent_account_id = reader->U32();
  notice.spouse_account_id = reader->U32();
  notice.parent_name = std::string(reader->FixedString(kNameSize));
  return notice;
}

core::Result<AdoptionRefusalNotice> DecodeAdoptionRefusalNotice(
    std::span<const std::uint8_t> frame) {
  auto reader = FixedFrame(frame, AdoptionRefusalNotice::kOpcode, AdoptionRefusalNotice::kWireSize);
  if (!reader) {
    return std::unexpected(reader.error());
  }
  AdoptionRefusalNotice notice;
  notice.reason = reader->U32();
  return notice;
}

core::Result<PartyShareState> DecodePartyShareState(std::span<const std::uint8_t> frame) {
  ByteReader reader(frame);
  const std::uint16_t opcode = reader.U16();
  const std::uint16_t length = reader.U16();
  if (!reader.ok() || opcode != PartyShareState::kOpcode) {
    return core::Fail(ErrorCode::kBadSignature, "trame d'un autre paquet");
  }
  if (length < PartyShareState::kFixedSize || length > frame.size()) {
    return core::Fail(ErrorCode::kUnexpectedEnd, "état du partage tronqué");
  }
  PartyShareState state;
  state.idle_s = reader.U16();
  const std::uint16_t announced = reader.U16();
  const std::size_t carried = (length - PartyShareState::kFixedSize) / PartyShareState::kEntrySize;
  const std::size_t count =
      std::min({static_cast<std::size_t>(announced), carried, kPartyShareMost});
  state.entries.reserve(count);
  for (std::size_t rank = 0; rank < count; ++rank) {
    PartyShareEntry entry;
    entry.account_id = reader.U32();
    entry.flags = reader.U8();
    if (entry.account_id != 0) {
      state.entries.push_back(entry);
    }
  }
  if (!reader.ok()) {
    return core::Fail(ErrorCode::kUnexpectedEnd, "état du partage tronqué");
  }
  return state;
}

}  // namespace ro::proto
