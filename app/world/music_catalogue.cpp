#include "world/music_catalogue.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <optional>
#include <system_error>

#include "ro/core/ascii.h"
#include "ro/core/utf8_path.h"

namespace moonclient {
namespace {

// Ce que le mélangeur sait jouer. Le dossier peut porter autre chose — une
// note, une liste —, qui n'a rien à faire dans le lecteur.
constexpr std::array<std::string_view, 3> kMusicExtensions = {".mp3", ".ogg", ".wav"};

[[nodiscard]] char Folded(char letter) noexcept {
  return letter == '/' ? '\\'
                       : static_cast<char>(ro::core::AsciiLower(static_cast<std::uint8_t>(letter)));
}

[[nodiscard]] bool IsMusicFile(std::string_view file_name) {
  const std::string lowered = ro::core::AsciiLower(file_name);
  return std::ranges::any_of(
      kMusicExtensions, [&](std::string_view extension) { return lowered.ends_with(extension); });
}

[[nodiscard]] std::string LabelOf(std::string_view file_name) {
  const std::size_t dot = file_name.rfind('.');
  return std::string(dot == std::string_view::npos ? file_name : file_name.substr(0, dot));
}

// Le numéro d'un morceau dont le nom n'est fait que de chiffres.
[[nodiscard]] std::optional<unsigned> NumberOf(std::string_view label) {
  unsigned number = 0;
  const auto [end, error] = std::from_chars(label.data(), label.data() + label.size(), number);
  if (error != std::errc() || end != label.data() + label.size()) {
    return std::nullopt;
  }
  return number;
}

[[nodiscard]] bool ComesBefore(const MusicTrack& left, const MusicTrack& right) {
  const std::optional<unsigned> left_number = NumberOf(left.label);
  const std::optional<unsigned> right_number = NumberOf(right.label);
  if (left_number.has_value() != right_number.has_value()) {
    return left_number.has_value();
  }
  if (left_number.has_value() && *left_number != *right_number) {
    return *left_number < *right_number;
  }
  return ro::core::AsciiLower(left.label) < ro::core::AsciiLower(right.label);
}

}  // namespace

bool SameMusic(std::string_view left, std::string_view right) noexcept {
  return std::ranges::equal(left, right,
                            [](char one, char other) { return Folded(one) == Folded(other); });
}

std::vector<MusicTrack> CatalogueMusic(std::span<const std::string> file_names,
                                       const ro::formats::BgmTable& table) {
  std::vector<MusicTrack> tracks;
  for (const std::string& file_name : file_names) {
    if (!IsMusicFile(file_name)) {
      continue;
    }
    MusicTrack track{.game_path = std::string(kMusicFolder) + '\\' + file_name,
                     .label = LabelOf(file_name)};
    // La table est rangée par carte : ses cartes sortent déjà dans l'ordre.
    for (const auto& [map_name, game_path] : table.tracks()) {
      if (SameMusic(game_path, track.game_path)) {
        track.maps.push_back(map_name);
      }
    }
    tracks.push_back(std::move(track));
  }
  std::ranges::sort(tracks, ComesBefore);
  return tracks;
}

std::vector<MusicTrack> ListMusicTracks(const std::filesystem::path& client_root,
                                        const ro::formats::BgmTable& table) {
  // ⚠ LE DOSSIER SE CHERCHE SANS ÉGARD À LA CASSE : l'installation porte
  // `BGM`, la table écrit « bgm », et Linux distingue les deux.
  std::vector<std::string> file_names;
  std::error_code code;
  for (const auto& folder : std::filesystem::directory_iterator(client_root, code)) {
    if (!folder.is_directory(code) ||
        ro::core::AsciiLower(ro::core::Utf8FromPath(folder.path().filename())) != kMusicFolder) {
      continue;
    }
    for (const auto& file : std::filesystem::directory_iterator(folder.path(), code)) {
      if (file.is_regular_file(code)) {
        file_names.push_back(ro::core::Utf8FromPath(file.path().filename()));
      }
    }
    break;
  }
  return CatalogueMusic(file_names, table);
}

}  // namespace moonclient
