#include "ro/net/chat_room.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ro/core/text.h"
#include "ro/net/chat_author.h"
#include "ro/proto/chat_room_packets.h"
#include "ro/proto/manual_packets.h"
#include "ro/proto/map_packets.h"

#include "frame_apply.h"
#include "send_encoded.h"

namespace ro::net {
namespace {

// L'opcode de l'écho : le paquet ne le porte pas dans sa structure.
constexpr std::uint16_t kPlayerChatOpcode = 0x008e;
// Le premier octet qui n'est plus de l'ASCII.
constexpr unsigned char kFirstNonAscii = 0x80;

core::Result<void> SendFrame(Connection& link, std::vector<std::uint8_t> frame,
                             std::string_view what) {
  if (frame.empty()) {
    return core::Fail(core::ErrorCode::kInvalidArgument,
                      std::string(what) + " ne tient pas dans une trame");
  }
  link.Send(frame);
  return {};
}

// L'enseigne du salon visé, telle que la zone l'a montrée.
[[nodiscard]] const sim::Signboard* RoomBoard(const sim::World& world, std::uint32_t room_id) {
  for (const auto& [id, actor] : world.actors()) {
    if (actor.signboard.has_value() && actor.signboard->kind == sim::SignboardKind::kChatRoom &&
        actor.signboard->room_id == room_id) {
      return &*actor.signboard;
    }
  }
  return nullptr;
}

[[nodiscard]] core::Result<void> RefusedSettings() {
  return core::Fail(core::ErrorCode::kInvalidArgument,
                    "réglages de salon que le client d'origine refuserait");
}

}  // namespace

FrameOutcome ApplyChatRoomFrame(const Frame& frame, sim::ChatRoom& room, sim::World& world) {
  switch (frame.opcode) {
    case proto::ZcAckCreateChatroom::kOpcode:
      return Decoded<proto::ZcAckCreateChatroom>(frame, [&](const auto& packet) {
        room.TellCreateAnswer(packet.flag, world.player_name());
        return FrameOutcome::kApplied;
      });

    case proto::ZcRefuseEnterRoom::kOpcode:
      return Decoded<proto::ZcRefuseEnterRoom>(frame, [&](const auto& packet) {
        room.TellEnterRefused(packet.result);
        return FrameOutcome::kApplied;
      });

    case proto::ZcEnterRoom::kOpcode:
      return Decoded<proto::ZcEnterRoom>(frame, [&](const auto& packet) {
        // 🔴 SANS L'ENSEIGNE, RIEN NE S'OUVRE : c'est elle qui dit le titre, la
        // limite et l'accès. Le serveur nous tient pourtant pour entrés : on en
        // ressort, plutôt que de rester occupant d'un salon que rien ne montre.
        const sim::Signboard* board = RoomBoard(world, packet.chatId);
        if (board == nullptr) {
          room.TellBlindEntry();
          return FrameOutcome::kApplied;
        }
        sim::ChatRoomInside inside;
        inside.room_id = packet.chatId;
        inside.title = board->title;
        inside.status = board->status;
        inside.limit = board->limit;
        for (const proto::ZcEnterRoomSub& member : packet.members) {
          inside.members.push_back(
              sim::ChatRoomMember{.name = core::Utf8FromWire(member.name),
                                  .owner = member.flag == sim::kChatRoomOwnerRole});
        }
        room.TellEntered(std::move(inside), world.player_name());
        // L'enseigne du salon où l'on entre disparaît : on est dedans.
        static_cast<void>(world.ClearRoom(packet.chatId));
        return FrameOutcome::kApplied;
      });

    case proto::ZcMemberNewentry::kOpcode:
      return Decoded<proto::ZcMemberNewentry>(frame, [&](const auto& packet) {
        room.TellMemberJoined(packet.count, core::Utf8FromWire(packet.name));
        return FrameOutcome::kApplied;
      });

    case proto::ZcMemberExit::kOpcode:
      return Decoded<proto::ZcMemberExit>(frame, [&](const auto& packet) {
        room.TellMemberLeft(packet.count, core::Utf8FromWire(packet.name), packet.kicked != 0,
                            world.player_name());
        return FrameOutcome::kApplied;
      });

    case proto::ZcChangeChatroom::kOpcode:
      return Decoded<proto::ZcChangeChatroom>(frame, [&](const auto& packet) {
        room.TellSettings(packet.limit, packet.users, packet.flag,
                          core::Utf8FromWire(packet.title));
        return FrameOutcome::kApplied;
      });

    case proto::ZcRoleChange::kOpcode:
      return Decoded<proto::ZcRoleChange>(frame, [&](const auto& packet) {
        room.TellRole(static_cast<std::uint32_t>(packet.flag), core::Utf8FromWire(packet.name),
                      world.player_name());
        return FrameOutcome::kApplied;
      });

    case proto::ZcDestroyRoom::kOpcode: {
      proto::ZcDestroyRoom packet;
      if (proto::Decode(frame.bytes, packet)) {
        room.TellRoomGone(packet.chatId);
      }
      // ⚠ LUE SANS ÊTRE CONSOMMÉE : l'enseigne est l'affaire du monde.
      return FrameOutcome::kUnhandled;
    }

    case proto::ZcNpcackMapmove::kOpcode:
    case proto::ZcNpcackServermove::kOpcode:
      room.Leave();
      return FrameOutcome::kUnhandled;

    default:
      return FrameOutcome::kUnhandled;
  }
}

FrameOutcome ClaimChatRoomLine(const Frame& frame, sim::ChatRoom& room, const sim::World& world,
                               const sim::BannedWords* banned, std::uint32_t now_ms) {
  if (!room.in_room()) {
    return FrameOutcome::kUnhandled;
  }
  switch (frame.opcode) {
    case proto::ZcNotifyChat::kOpcode:
      return Decoded<proto::ZcNotifyChat>(frame, [&](const auto& packet) {
        AuthoredText said = ReadAuthoredText(frame, packet.Message);
        room.RecordLine(sim::ChatKind::kPublic, packet.GID,
                        sim::BannedWords::MaskWith(banned, said.text), kChatRoomSpeechColor, now_ms,
                        std::move(said.author));
        return FrameOutcome::kApplied;
      });

    case kPlayerChatOpcode:
      return Decoded<proto::ZcNotifyPlayerchat>(frame, [&](const auto& packet) {
        std::string said = core::Utf8FromWire(packet.Message);
        const std::string prefix =
            world.player_name() + std::string(proto::ChatRequest::kNameSeparator);
        const bool mine = !world.player_name().empty() && said.starts_with(prefix);
        room.RecordLine(mine ? sim::ChatKind::kSelf : sim::ChatKind::kServer,
                        mine ? world.player_id() : 0, sim::BannedWords::MaskWith(banned, said),
                        kChatRoomOwnSpeechColor, now_ms);
        return FrameOutcome::kApplied;
      });

    default:
      return FrameOutcome::kUnhandled;
  }
}

bool ChatRoomTitleIsAscii(std::string_view title) noexcept {
  for (const char byte : title) {
    if (static_cast<unsigned char>(byte) >= kFirstNonAscii) {
      return false;
    }
  }
  return true;
}

ChatRoomSettingsFault CheckChatRoomSettings(const sim::ChatRoomSettings& settings) {
  if (settings.title.empty()) {
    return ChatRoomSettingsFault::kNoTitle;
  }
  if (!ChatRoomTitleIsAscii(settings.title)) {
    return ChatRoomSettingsFault::kTitleNotAscii;
  }
  if (settings.title.size() > sim::kChatRoomTitleMostBytes) {
    return ChatRoomSettingsFault::kTitleTooLong;
  }
  if (settings.is_public) {
    return ChatRoomSettingsFault::kNone;
  }
  const std::optional<std::string> password = core::ExactCp1252FromUtf8(settings.password);
  if (!password.has_value()) {
    return ChatRoomSettingsFault::kPasswordNotEncodable;
  }
  if (password->empty()) {
    return ChatRoomSettingsFault::kNoPassword;
  }
  if (password->size() < sim::kChatRoomPasswordFewestBytes) {
    return ChatRoomSettingsFault::kPasswordTooShort;
  }
  if (password->size() > sim::kChatRoomPasswordMostBytes) {
    return ChatRoomSettingsFault::kPasswordTooLong;
  }
  return ChatRoomSettingsFault::kNone;
}

core::Result<void> RequestCreateChatRoom(Connection& link, sim::ChatRoom& room,
                                         sim::ChatRoomSettings settings, std::uint32_t now_ms) {
  if (room.in_room() || room.creating()) {
    return core::Fail(core::ErrorCode::kInvalidArgument,
                      "déjà dans un salon, ou une création attend sa réponse");
  }
  if (CheckChatRoomSettings(settings) != ChatRoomSettingsFault::kNone) {
    return RefusedSettings();
  }
  settings.limit = std::clamp(settings.limit, sim::kChatRoomFewestSeats, sim::kChatRoomMostSeats);
  proto::CzCreateChatroom request;
  request.limit = settings.limit;
  request.type = settings.is_public ? sim::kRoomPublic : sim::kRoomPrivate;
  // ⚠ UN SALON PUBLIC N'A PAS DE MOT DE PASSE : rien ne part dans le champ.
  request.password = settings.is_public ? std::string() : core::Cp1252FromUtf8(settings.password);
  request.title = settings.title;
  if (auto sent = SendFrame(link, proto::Encode(request), "la création d'un salon"); !sent) {
    return sent;
  }
  room.NoteCreateSent(std::move(settings), now_ms);
  return {};
}

core::Result<void> RequestChangeChatRoom(Connection& link, sim::ChatRoom& room,
                                         const sim::ChatRoomSettings& settings) {
  if (!room.in_room() || !room.inside()->owned) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "réglages d'un salon qu'on ne tient pas");
  }
  if (CheckChatRoomSettings(settings) != ChatRoomSettingsFault::kNone) {
    return RefusedSettings();
  }
  proto::CzChangeChatroom request;
  request.limit = std::clamp(settings.limit, sim::kChatRoomFewestSeats, sim::kChatRoomMostSeats);
  request.type = settings.is_public ? sim::kRoomPublic : sim::kRoomPrivate;
  request.password = settings.is_public ? std::string() : core::Cp1252FromUtf8(settings.password);
  request.title = settings.title;
  if (auto sent = SendFrame(link, proto::Encode(request), "les réglages d'un salon"); !sent) {
    return sent;
  }
  room.NoteSettingsSent(settings);
  return {};
}

core::Result<void> RequestEnterChatRoom(Connection& link, sim::ChatRoom& room,
                                        std::uint32_t room_id, std::string_view password_utf8,
                                        std::uint32_t now_ms) {
  if (room_id == 0 || !room.EnterMayLeave(now_ms)) {
    return core::Fail(core::ErrorCode::kInvalidArgument,
                      "déjà dans un salon, ou une entrée attend encore sa réponse");
  }
  proto::CzReqEnterRoom request;
  request.chat_id = room_id;
  request.password = core::Cp1252FromUtf8(password_utf8);
  if (auto sent = SendFrame(link, proto::Encode(request), "l'entrée dans un salon"); !sent) {
    return sent;
  }
  room.NoteEnterSent(now_ms);
  return {};
}

core::Result<void> RequestChatRoomOwner(Connection& link, const sim::ChatRoom& room,
                                        std::string_view member_utf8) {
  if (!room.in_room() || !room.inside()->owned) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "droits d'un salon qu'on ne tient pas");
  }
  return SendEncoded(
      link, proto::Encode(proto::ChatRoomRoleRequest{.role = proto::ChatRoomRoleRequest::kOwnerRole,
                                                     .name = core::Cp1252FromUtf8(member_utf8)}));
}

core::Result<void> RequestChatRoomExpel(Connection& link, const sim::ChatRoom& room,
                                        std::string_view member_utf8) {
  if (!room.in_room() || !room.inside()->owned) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "expulsion d'un salon qu'on ne tient pas");
  }
  return SendEncoded(
      link, proto::Encode(proto::ChatRoomExpelRequest{.name = core::Cp1252FromUtf8(member_utf8)}));
}

core::Result<void> RequestOwedChatRoomExit(Connection& link, sim::ChatRoom& room) {
  if (!room.TakeLeaveOwed()) {
    return {};
  }
  return SendEncoded(link, proto::Encode(proto::ChatRoomExitRequest{}));
}

core::Result<void> RequestLeaveChatRoom(Connection& link, sim::ChatRoom& room) {
  if (!room.in_room()) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "sortie d'un salon où l'on n'est pas");
  }
  if (auto sent = SendEncoded(link, proto::Encode(proto::ChatRoomExitRequest{})); !sent) {
    return sent;
  }
  room.Leave();
  return {};
}

}  // namespace ro::net
