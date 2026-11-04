#include "ro/net/cheat_report.h"

#include <string>

#include "ro/proto/map_packets.h"

namespace ro::net {

void SendCheatReport(Connection& link, std::string_view tool, std::string_view detail) {
  proto::CzMoonlightCheatReport report;
  // ⚠ LA LONGUEUR S'ÉCRIT : le serveur la LIT.
  report.packetLength = static_cast<std::int16_t>(proto::CzMoonlightCheatReport::kWireSize);
  report.tool_name = std::string(tool.substr(0, kCheatToolBytes));
  report.detail = std::string(detail.substr(0, kCheatDetailBytes));
  link.Send(proto::Encode(report));
}

}  // namespace ro::net
