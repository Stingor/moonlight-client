#include "ro/net/staff_requests.h"

#include <string>
#include <vector>

#include "ro/core/text.h"
#include "ro/proto/map_packets.h"
#include "ro/proto/staff_packets.h"

#include "send_encoded.h"

namespace ro::net {

core::Result<void> RequestNpcAdmin(Connection& link, std::uint32_t npc_id, NpcAdminAction action) {
  proto::CzMoonlightNpcAdmin request;
  // ⚠ LA LONGUEUR S'ÉCRIT : le serveur la LIT, et une trame qui la laisse à
  // zéro se rejette sans un mot.
  request.packetLength = static_cast<std::int16_t>(proto::CzMoonlightNpcAdmin::kWireSize);
  request.gid = npc_id;
  request.action = static_cast<std::uint8_t>(action);
  link.Send(proto::Encode(request));
  return {};
}

core::Result<void> RequestPlayerAdmin(Connection& link, std::uint32_t account_id,
                                      PlayerAdminAction action, std::int32_t param) {
  proto::CzMoonlightPlayerAdmin request;
  request.packetLength = static_cast<std::int16_t>(proto::CzMoonlightPlayerAdmin::kWireSize);
  request.aid = account_id;
  request.action = static_cast<std::uint8_t>(action);
  request.param = param;
  link.Send(proto::Encode(request));
  return {};
}

core::Result<void> RequestKick(Connection& link, std::uint32_t account_id) {
  proto::KickRequest request;
  request.account_id = account_id;
  return SendEncoded(link, proto::Encode(request));
}

core::Result<void> RequestFullStrip(Connection& link, std::uint32_t account_id) {
  proto::FullStripRequest request;
  request.account_id = account_id;
  return SendEncoded(link, proto::Encode(request));
}

namespace {

// Le texte d'une annonce, tel que le serveur le lit : page de codes du jeu,
// nul final compris. Vide quand il n'y a rien à annoncer.
[[nodiscard]] std::string AnnouncedText(std::string_view text) {
  if (text.empty()) {
    return {};
  }
  std::string wire = core::Cp1252FromUtf8(text);
  wire.push_back('\0');
  return wire;
}

// ⚠ UNE TRAME VIDE EST UN REFUS DE L'ENCODEUR : un texte trop long pour la
// longueur sur seize bits.
[[nodiscard]] core::Result<void> SendAnnounce(Connection& link,
                                              const std::vector<std::uint8_t>& frame) {
  if (frame.empty()) {
    return std::unexpected(core::Error{core::ErrorCode::kInvalidArgument, "annonce trop longue"});
  }
  link.Send(frame);
  return {};
}

}  // namespace

core::Result<void> RequestBroadcast(Connection& link, std::string_view text) {
  proto::CzBroadcast request;
  request.message = AnnouncedText(text);
  if (request.message.empty()) {
    return std::unexpected(core::Error{core::ErrorCode::kInvalidArgument, "rien a annoncer"});
  }
  return SendAnnounce(link, proto::Encode(request));
}

core::Result<void> RequestLocalBroadcast(Connection& link, std::string_view text) {
  proto::CzLocalbroadcast request;
  request.message = AnnouncedText(text);
  if (request.message.empty()) {
    return std::unexpected(core::Error{core::ErrorCode::kInvalidArgument, "rien a annoncer"});
  }
  return SendAnnounce(link, proto::Encode(request));
}

}  // namespace ro::net
