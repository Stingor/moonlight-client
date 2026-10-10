#include "ro/net/map_name.h"

#include "ro/core/ascii.h"

namespace ro::net {

std::string MapBaseName(std::string_view server_name) {
  constexpr std::string_view kWalkabilitySuffix = ".gat";
  if (server_name.size() > kWalkabilitySuffix.size() &&
      core::AsciiLower(server_name.substr(server_name.size() - kWalkabilitySuffix.size())) ==
          kWalkabilitySuffix) {
    server_name.remove_suffix(kWalkabilitySuffix.size());
  }
  return std::string(server_name);
}

}  // namespace ro::net
