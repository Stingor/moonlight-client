#include "ro/formats/bgm_table.h"

#include <string>
#include <string_view>

#include "ro/core/ascii.h"

namespace ro::formats {
namespace {

constexpr std::string_view kComment = "//";
constexpr char kFieldSeparator = '#';
constexpr std::string_view kMapExtension = ".rsw";
constexpr std::string_view kBlanks = " \t\r";

// Les antislashs sont DOUBLÉS dans le fichier, comme dans une chaîne de
// programme. Les laisser tels quels donnerait un chemin que le système ne
// trouve pas, sans que rien ne le dise.
std::string Undoubled(std::string_view path) {
  std::string plain;
  plain.reserve(path.size());
  for (std::size_t at = 0; at < path.size(); ++at) {
    plain.push_back(path[at]);
    if (path[at] == '\\' && at + 1 < path.size() && path[at + 1] == '\\') {
      ++at;
    }
  }
  return plain;
}

std::string_view Trimmed(std::string_view text) {
  const std::size_t first = text.find_first_not_of(kBlanks);
  if (first == std::string_view::npos) {
    return {};
  }
  return text.substr(first, text.find_last_not_of(kBlanks) + 1 - first);
}

}  // namespace

BgmTable BgmTable::Parse(std::span<const std::uint8_t> bytes) {
  BgmTable table;
  const std::string_view text(reinterpret_cast<const char*>(bytes.data()), bytes.size());

  std::size_t at = 0;
  while (at <= text.size()) {
    const std::size_t end = text.find('\n', at);
    std::string_view line = text.substr(at, end == std::string_view::npos ? end : end - at);
    at = end == std::string_view::npos ? text.size() + 1 : end + 1;

    // Un commentaire DÉSACTIVE aussi une entrée : le fichier en porte, pour des
    // musiques remplacées le temps d'un événement.
    const std::size_t comment = line.find(kComment);
    if (comment != std::string_view::npos) {
      line = line.substr(0, comment);
    }
    line = Trimmed(line);

    const std::size_t opening = line.find(kFieldSeparator);
    if (opening == std::string_view::npos) {
      continue;
    }
    const std::size_t closing = line.find(kFieldSeparator, opening + 1);
    if (closing == std::string_view::npos) {
      continue;
    }
    std::string_view map = Trimmed(line.substr(0, opening));
    const std::string_view track = Trimmed(line.substr(opening + 1, closing - opening - 1));
    if (map.empty() || track.empty()) {
      continue;
    }
    // La clé porte l'extension d'une carte, que personne n'emploie ailleurs
    // pour la nommer.
    if (core::AsciiEndsWithFolded(map, kMapExtension)) {
      map = map.substr(0, map.size() - kMapExtension.size());
    }
    // La DERNIÈRE ligne l'emporte : une table se lit de haut en bas, et ce
    // qu'on ajoute à la fin est ce qui doit primer.
    table.tracks_.insert_or_assign(core::AsciiLower(map), Undoubled(track));
  }
  return table;
}

std::string_view BgmTable::TrackFor(std::string_view map_name) const {
  const auto found = tracks_.find(core::AsciiLower(map_name));
  return found == tracks_.end() ? std::string_view{} : found->second;
}

}  // namespace ro::formats
