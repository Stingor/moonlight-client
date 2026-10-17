#include "ro/proto/chat_room_packets.h"

#include <array>
#include <string>
#include <string_view>

#include "ro/core/byte_writer.h"
#include "ro/proto/manual_packets.h"
#include "ro/proto/packet_lengths.h"

#include "layout_holds.h"

namespace ro::proto {
namespace {

using core::ByteWriter;
using core::ErrorCode;

constexpr std::string_view kRoleHandler = "clif_parse_ChangeChatOwner";
// Le rôle juste après l'opcode, puis le nom.
constexpr std::array<std::uint16_t, 2> kRoleOffsets = {2, 6};
constexpr std::string_view kExpelHandler = "clif_parse_KickFromChat";
// Le nom, juste après l'opcode.
constexpr std::array<std::uint16_t, 1> kExpelOffsets = {2};
constexpr std::string_view kExitHandler = "clif_parse_ChatLeave";

[[nodiscard]] std::unexpected<core::Error> LayoutChanged(std::string_view what) {
  return core::Fail(ErrorCode::kUnsupportedVersion,
                    std::string(what) + " ne correspond plus à ce que le serveur déclare lire");
}

}  // namespace

core::Result<std::vector<std::uint8_t>> Encode(const ChatRoomRoleRequest& request) {
  if (!FixedLayoutHolds<ChatRoomRoleRequest>(kRoleHandler, kRoleOffsets)) {
    return LayoutChanged("la cession des droits d'un salon");
  }
  if (request.name.empty()) {
    return core::Fail(ErrorCode::kInvalidArgument, "cession des droits d'un salon sans membre");
  }
  std::vector<std::uint8_t> frame;
  frame.reserve(ChatRoomRoleRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(ChatRoomRoleRequest::kOpcode);
  writer.U32(request.role);
  writer.FixedString(request.name, kChatRoomNameBytes);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const ChatRoomExpelRequest& request) {
  if (!FixedLayoutHolds<ChatRoomExpelRequest>(kExpelHandler, kExpelOffsets)) {
    return LayoutChanged("l'expulsion d'un membre de salon");
  }
  if (request.name.empty()) {
    return core::Fail(ErrorCode::kInvalidArgument, "expulsion d'un salon sans membre");
  }
  std::vector<std::uint8_t> frame;
  frame.reserve(ChatRoomExpelRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(ChatRoomExpelRequest::kOpcode);
  writer.FixedString(request.name, kChatRoomNameBytes);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const ChatRoomExitRequest& /*request*/) {
  if (!FixedLayoutHolds<ChatRoomExitRequest>(kExitHandler, std::array<std::uint16_t, 0>{})) {
    return LayoutChanged("la sortie d'un salon");
  }
  std::vector<std::uint8_t> frame;
  frame.reserve(ChatRoomExitRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(ChatRoomExitRequest::kOpcode);
  return frame;
}

}  // namespace ro::proto
