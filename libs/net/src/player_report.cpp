// Le signalement d'un joueur : la demande et l'accusé. Voir l'en-tête.

#include "ro/net/player_report.h"

#include <string>

#include "ro/core/text.h"
#include "ro/proto/map_packets.h"

namespace ro::net {
namespace {

static_assert(kPlayerReportAckOpcode == proto::ZcAckReportUser::kOpcode);

// ⚠ COUPÉ APRÈS LA CONVERSION : CP1252 est mono-octet, la coupe ne tranche donc
// jamais un caractère en deux.
[[nodiscard]] std::string WireCut(std::string_view utf8, std::size_t most_bytes) {
  std::string wire = core::Cp1252FromUtf8(utf8);
  if (wire.size() > most_bytes) {
    wire.resize(most_bytes);
  }
  return wire;
}

}  // namespace

core::Result<void> SendPlayerReport(Connection& link, const PlayerReport& report) {
  proto::CzReqReportUser request;
  request.AID = report.reporter_aid;
  request.targetAID = report.target_aid;
  request.targetName = WireCut(report.target_name, kPlayerReportNameMostBytes);
  request.reason = static_cast<std::int16_t>(report.reason);
  request.message = WireCut(report.message, kPlayerReportMessageMostBytes);
  link.Send(proto::Encode(request));
  return {};
}

std::optional<PlayerReportAck> DecodePlayerReportAck(std::span<const std::uint8_t> frame) {
  proto::ZcAckReportUser packet;
  if (!proto::Decode(frame, packet)) {
    return std::nullopt;
  }
  return PlayerReportAck{.target_aid = packet.targetAID,
                         .target_name = core::Utf8FromWire(packet.targetName),
                         .result = packet.result};
}

std::optional<PlayerReportAck> ReadPlayerReportAck(const Frame& frame) {
  if (frame.opcode != proto::ZcAckReportUser::kOpcode) {
    return std::nullopt;
  }
  return DecodePlayerReportAck(frame.bytes);
}

}  // namespace ro::net
