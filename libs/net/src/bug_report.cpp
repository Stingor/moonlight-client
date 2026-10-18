#include "ro/net/bug_report.h"

#include <string>
#include <utility>

#include "ro/core/text.h"
#include "ro/proto/manual_packets.h"
#include "ro/proto/map_packets.h"

#include "send_encoded.h"

namespace ro::net {

core::Result<void> SendBugReport(Connection& link, std::uint8_t category,
                                 std::string_view context_utf8, std::string_view message_utf8) {
  proto::BugReportRequest request;
  request.category = category;
  request.context = core::Cp1252FromUtf8(context_utf8);
  // ⚠ COUPÉ APRÈS LA CONVERSION : CP1252 est mono-octet, la coupe ne tranche
  // donc jamais un caractère en deux.
  request.message = core::Cp1252FromUtf8(message_utf8);
  if (request.message.size() > kBugReportMessageMostBytes) {
    request.message.resize(kBugReportMessageMostBytes);
  }
  return SendEncoded(link, proto::Encode(request));
}

std::optional<std::uint8_t> ReadBugReportAck(const Frame& frame) {
  if (frame.opcode != proto::ZcMoonlightBugReportAck::kOpcode) {
    return std::nullopt;
  }
  proto::ZcMoonlightBugReportAck packet;
  if (!proto::Decode(frame.bytes, packet)) {
    return std::nullopt;
  }
  return packet.status;
}

}  // namespace ro::net
