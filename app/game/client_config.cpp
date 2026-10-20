#include "game/client_config.h"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <expected>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <yaml-cpp/yaml.h>

namespace moonclient {
namespace {

constexpr const char* kServersKey = "servers";
constexpr const char* kNameKey = "name";
constexpr const char* kAddressKey = "address";
constexpr const char* kPortKey = "port";
constexpr const char* kWebKey = "web";
constexpr const char* kServerTimeZoneKey = "server_time_zone";
constexpr const char* kTestServerKey = "test_server";
constexpr const char* kWebLoginKey = "web_login";
// Au premier niveau, la liste des GRF ; dans une fiche de serveur de patchs,
// le dossier des archives de patch. Même mot, deux étages.
constexpr const char* kGameArchivesKey = "archives";
constexpr const char* kPatcherKey = "patcher";
constexpr const char* kMirrorsKey = "mirrors";
constexpr const char* kListKey = "list";
constexpr const char* kArchivesKey = "archives";
constexpr const char* kManifestKey = "manifest";
constexpr const char* kFilesKey = "files";
constexpr const char* kDefaultGrfKey = "default_grf";
constexpr const char* kRequireHashesKey = "require_hashes";
constexpr const char* kCreateGrfKey = "create_grf";
constexpr std::string_view kSecureScheme = "https://";

[[nodiscard]] std::unexpected<ro::core::Error> Broken(const std::string& why) {
  return ro::core::Fail(ro::core::ErrorCode::kCorruptData,
                        std::string(kClientConfigFileName) + " : " + why);
}

// La chaîne sous `key`, ou vide.
[[nodiscard]] std::string ScalarAt(const YAML::Node& map, const char* key) {
  const YAML::Node value = map[key];
  return value && value.IsScalar() ? value.as<std::string>() : std::string();
}

[[nodiscard]] bool IsSecureUrl(std::string_view url) {
  return url.size() > kSecureScheme.size() && url.starts_with(kSecureScheme);
}

// Un chemin relatif qui reste dans l'installation.
[[nodiscard]] bool StaysInsideInstall(std::string_view name) {
  if (name.empty() || name.starts_with('/') || name.starts_with('\\') ||
      name.find(':') != std::string_view::npos) {
    return false;
  }
  std::size_t start = 0;
  while (start <= name.size()) {
    const std::size_t end = std::min(name.find_first_of("/\\", start), name.size());
    if (name.substr(start, end - start) == "..") {
      return false;
    }
    start = end + 1;
  }
  return true;
}

[[nodiscard]] ro::core::Result<std::vector<std::string>> ParseArchives(const YAML::Node& list) {
  if (!list || !list.IsSequence() || list.size() == 0) {
    return Broken("aucune archive déclarée sous « archives »");
  }
  std::vector<std::string> archives;
  for (std::size_t rank = 0; rank < list.size(); ++rank) {
    const YAML::Node entry = list[rank];
    const std::string name = entry.IsScalar() ? entry.as<std::string>() : std::string();
    if (!StaysInsideInstall(name)) {
      return Broken("archive n° " + std::to_string(rank + 1) +
                    " : un nom relatif au dossier du jeu est attendu");
    }
    archives.push_back(name);
  }
  return archives;
}

[[nodiscard]] ro::core::Result<PatcherConfig> ParsePatcher(const YAML::Node& section) {
  if (!section.IsMap()) {
    return Broken("« patcher » n'est pas une fiche");
  }
  const YAML::Node mirrors = section[kMirrorsKey];
  if (!mirrors || !mirrors.IsSequence() || mirrors.size() == 0) {
    return Broken("patcher : aucun serveur de patchs sous « mirrors »");
  }
  PatcherConfig patcher;
  for (std::size_t rank = 0; rank < mirrors.size(); ++rank) {
    const YAML::Node entry = mirrors[rank];
    const std::string which = "serveur de patchs n° " + std::to_string(rank + 1);
    if (!entry.IsMap()) {
      return Broken(which + " : ce n'est pas une fiche nom / list / archives");
    }
    PatchMirror mirror;
    mirror.list_url = ScalarAt(entry, kListKey);
    mirror.archive_url = ScalarAt(entry, kArchivesKey);
    if (!IsSecureUrl(mirror.list_url) || !IsSecureUrl(mirror.archive_url)) {
      return Broken(which + " : « list » et « archives » doivent être des adresses https://");
    }
    if (!mirror.archive_url.ends_with('/')) {
      mirror.archive_url.push_back('/');
    }
    mirror.manifest_url = ScalarAt(entry, kManifestKey);
    mirror.files_url = ScalarAt(entry, kFilesKey);
    if (mirror.manifest_url.empty() != mirror.files_url.empty()) {
      return Broken(which + " : « manifest » et « files » vont ensemble");
    }
    if (!mirror.manifest_url.empty()) {
      if (!IsSecureUrl(mirror.manifest_url) || !IsSecureUrl(mirror.files_url)) {
        return Broken(which + " : « manifest » et « files » doivent être des adresses https://");
      }
      if (!mirror.files_url.ends_with('/')) {
        mirror.files_url.push_back('/');
      }
    }
    mirror.name = ScalarAt(entry, kNameKey);
    if (mirror.name.empty()) {
      mirror.name = mirror.list_url;
    }
    patcher.mirrors.push_back(std::move(mirror));
  }
  patcher.default_grf = ScalarAt(section, kDefaultGrfKey);
  if (patcher.default_grf.empty()) {
    return Broken("patcher : pas d'archive par défaut sous « default_grf »");
  }
  for (const auto& [key, flag] : {std::pair{kRequireHashesKey, &patcher.require_hashes},
                                  std::pair{kCreateGrfKey, &patcher.create_grf}}) {
    if (const YAML::Node value = section[key]; value) {
      if (!value.IsScalar() || !YAML::convert<bool>::decode(value, *flag)) {
        return Broken(std::string("patcher : « ") + key + " » n'est ni true ni false");
      }
    }
  }
  return patcher;
}

}  // namespace

std::optional<ServerAddress> ParseServerAddress(std::string_view text) {
  const std::size_t colon = text.rfind(':');
  if (colon == std::string_view::npos) {
    return std::nullopt;
  }
  std::string_view host = text.substr(0, colon);
  if (host.size() >= 2 && host.front() == '[' && host.back() == ']') {
    host = host.substr(1, host.size() - 2);
  } else if (host.find(':') != std::string_view::npos) {
    // Une adresse IPv6 sans crochets : son dernier « : » n'annonce pas un port.
    return std::nullopt;
  }
  const std::string_view digits = text.substr(colon + 1);
  int port = 0;
  if (host.empty() || digits.empty() ||
      std::from_chars(digits.data(), digits.data() + digits.size(), port).ptr !=
          digits.data() + digits.size() ||
      port <= 0 || port > std::numeric_limits<std::uint16_t>::max()) {
    return std::nullopt;
  }
  return ServerAddress{.host = std::string(host), .port = static_cast<std::uint16_t>(port)};
}

ro::core::Result<ClientConfig> ParseClientConfig(std::string_view text) {
  YAML::Node file;
  try {
    file = YAML::Load(std::string(text));
  } catch (const YAML::Exception& broken) {
    return Broken(std::string("illisible — ") + broken.what());
  }

  const YAML::Node servers = file[kServersKey];
  if (!servers || !servers.IsSequence() || servers.size() == 0) {
    return Broken("aucun serveur déclaré sous « servers »");
  }

  ClientConfig config;
  for (std::size_t rank = 0; rank < servers.size(); ++rank) {
    const YAML::Node entry = servers[rank];
    const std::string which = "serveur n° " + std::to_string(rank + 1);
    if (!entry.IsMap()) {
      return Broken(which + " : ce n'est pas une fiche nom / adresse / port");
    }
    ServerEntry server;
    if (const YAML::Node name = entry[kNameKey]; name && name.IsScalar()) {
      server.name = name.as<std::string>();
    }
    const YAML::Node address = entry[kAddressKey];
    if (!address || !address.IsScalar() || address.as<std::string>().empty()) {
      return Broken(which + " : pas d'adresse");
    }
    server.address = address.as<std::string>();
    const YAML::Node port = entry[kPortKey];
    int port_number = 0;
    if (!port || !port.IsScalar() || !YAML::convert<int>::decode(port, port_number) ||
        port_number <= 0 || port_number > std::numeric_limits<std::uint16_t>::max()) {
      return Broken(which + " : le port n'est pas un nombre de 1 à 65535");
    }
    server.port = static_cast<std::uint16_t>(port_number);
    if (server.name.empty()) {
      server.name = server.address;
    }
    if (const YAML::Node web_login = entry[kWebLoginKey]; web_login) {
      std::string origin = web_login.IsScalar() ? web_login.as<std::string>() : std::string();
      if (origin.ends_with('/')) {
        origin.pop_back();
      }
      if (!IsSecureUrl(origin) || origin.find('/', kSecureScheme.size()) != std::string::npos) {
        return Broken(which + " : « web_login » doit être une origine https://hôte[:port]");
      }
      server.web_login = std::move(origin);
    }
    config.servers.push_back(std::move(server));
  }

  if (const YAML::Node web = file[kWebKey]; web && web.IsScalar()) {
    config.web = web.as<std::string>();
  }
  if (const YAML::Node zone = file[kServerTimeZoneKey]; zone && zone.IsScalar()) {
    config.server_time_zone = zone.as<std::string>();
  }
  if (const YAML::Node test = file[kTestServerKey]; test) {
    const std::optional<ServerAddress> address =
        test.IsScalar() ? ParseServerAddress(test.as<std::string>()) : std::nullopt;
    if (!address.has_value()) {
      return Broken("« test_server » n'est pas une adresse hôte:port");
    }
    config.test_server = *address;
  }
  auto archives = ParseArchives(file[kGameArchivesKey]);
  if (!archives) {
    return std::unexpected(archives.error());
  }
  config.archives = std::move(*archives);
  if (const YAML::Node section = file[kPatcherKey]; section) {
    auto patcher = ParsePatcher(section);
    if (!patcher) {
      return std::unexpected(patcher.error());
    }
    config.patcher = std::move(*patcher);
  }
  return config;
}

}  // namespace moonclient
