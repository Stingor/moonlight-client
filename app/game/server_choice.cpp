#include "game/server_choice.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <optional>
#include <string>

namespace moonclient {
namespace {

[[nodiscard]] bool SameText(std::string_view left, std::string_view right) {
  return std::ranges::equal(left, right, [](char one, char other) {
    return std::tolower(static_cast<unsigned char>(one)) ==
           std::tolower(static_cast<unsigned char>(other));
  });
}

}  // namespace

bool IsLocalHost(std::string_view host) {
  using std::string_view_literals::operator""sv;
  static constexpr std::array kLocalNames = {"127.0.0.1"sv, "localhost"sv, "::1"sv, "[::1]"sv};
  return std::ranges::any_of(kLocalNames,
                             [host](std::string_view local) { return SameText(host, local); });
}

bool SameServer(const ServerAddress& left, const ServerAddress& right) {
  return left.port == right.port &&
         (SameText(left.host, right.host) || (IsLocalHost(left.host) && IsLocalHost(right.host)));
}

std::string WebLoginOriginOf(std::string_view declared, bool test,
                             std::string_view default_origin) {
  if (!declared.empty()) {
    return std::string(declared);
  }
  return test ? std::string() : std::string(default_origin);
}

std::vector<ServerChoice> ComposeServerChoices(std::span<const ServerEntry> listed,
                                               const ServerAddress& test_server, bool test_detected,
                                               std::string_view default_web_origin) {
  std::vector<ServerChoice> choices;
  bool test_listed = false;
  for (const ServerEntry& entry : listed) {
    const ServerAddress address{.host = entry.address, .port = entry.port};
    const bool is_test_server = SameServer(address, test_server);
    test_listed = test_listed || is_test_server;
    const bool test = is_test_server || IsLocalHost(entry.address);
    choices.push_back(
        ServerChoice{.name = entry.name,
                     .address = address,
                     .test = test,
                     .web_origin = WebLoginOriginOf(entry.web_login, test, default_web_origin)});
  }
  if (test_detected && !test_listed) {
    choices.push_back(ServerChoice{.address = test_server, .test = true, .probed = true});
  }
  return choices;
}

std::string ServerMemoryKey(const ServerAddress& address) {
  return address.host + ":" + std::to_string(address.port);
}

std::size_t PreferredServer(std::span<const ServerChoice> choices, std::string_view remembered,
                            const ServerAddress& test_server, bool test_detected) {
  if (remembered.empty()) {
    return 0;
  }
  const std::optional<ServerAddress> wanted = ParseServerAddress(remembered);
  if (!wanted.has_value() || (!test_detected && SameServer(*wanted, test_server))) {
    return 0;
  }
  const auto found = std::ranges::find_if(
      choices, [&wanted](const ServerChoice& one) { return SameServer(one.address, *wanted); });
  return found == choices.end() ? 0 : static_cast<std::size_t>(found - choices.begin());
}

ServerOffer OfferServers(std::span<const ServerEntry> listed, const ServerAddress& test_server,
                         bool test_detected, std::string_view remembered,
                         std::string_view default_web_origin) {
  ServerOffer offer{
      .entries = ComposeServerChoices(listed, test_server, test_detected, default_web_origin)};
  offer.chosen = PreferredServer(offer.entries, remembered, test_server, test_detected);
  return offer;
}

}  // namespace moonclient
