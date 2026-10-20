#include "patcher/mirror_manifest.h"

#include <algorithm>
#include <charconv>
#include <set>

namespace moonclient::patcher {
namespace {

using ro::core::ErrorCode;
using ro::core::Fail;
using ro::core::Result;

constexpr std::size_t kSha256HexLength = 64;
constexpr std::string_view kHexDigits = "0123456789ABCDEF";
constexpr unsigned kNibbleBits = 4;
constexpr unsigned kNibbleMask = 0x0F;

[[nodiscard]] bool IsLowerHex(std::string_view text) {
  return std::ranges::all_of(text, [](char digit) {
    return (digit >= '0' && digit <= '9') || (digit >= 'a' && digit <= 'f');
  });
}

[[nodiscard]] std::string LowerAscii(std::string_view text) {
  std::string lowered(text);
  for (char& character : lowered) {
    if (character >= 'A' && character <= 'Z') {
      character = static_cast<char>(character - 'A' + 'a');
    }
  }
  return lowered;
}

// Un chemin qui reste dans le dossier du jeu : relatif, sans lecteur, sans
// segment vide, `.` ni `..`.
[[nodiscard]] bool StaysInsideInstall(std::string_view path) {
  if (path.empty() || path.find('\\') != std::string_view::npos ||
      path.find(':') != std::string_view::npos) {
    return false;
  }
  std::size_t start = 0;
  while (start <= path.size()) {
    const std::size_t end = std::min(path.find('/', start), path.size());
    const std::string_view segment = path.substr(start, end - start);
    if (segment.empty() || segment == "." || segment == "..") {
      return false;
    }
    start = end + 1;
  }
  return true;
}

[[nodiscard]] bool IsUrlSafe(char character) {
  return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
         (character >= '0' && character <= '9') || character == '-' || character == '_' ||
         character == '.' || character == '~' || character == '/';
}

}  // namespace

Result<std::vector<MirrorFile>> ParseMirrorManifest(std::string_view text) {
  std::vector<MirrorFile> files;
  std::set<std::string> seen_paths;
  bool header_read = false;
  std::size_t line_number = 0;
  std::size_t start = 0;
  while (start < text.size()) {
    const std::size_t end = std::min(text.find('\n', start), text.size());
    std::string_view line = text.substr(start, end - start);
    start = end + 1;
    ++line_number;
    if (line.ends_with('\r')) {
      line.remove_suffix(1);
    }
    const std::string where = "manifeste, ligne " + std::to_string(line_number);
    if (!header_read) {
      if (line != kMirrorManifestHeader) {
        return Fail(ErrorCode::kCorruptData, "le manifeste ne commence pas par « " +
                                                 std::string(kMirrorManifestHeader) +
                                                 " » : ce n'est pas un manifeste de miroir");
      }
      header_read = true;
      continue;
    }
    if (line.empty()) {
      continue;
    }
    const std::size_t sha_end = line.find(' ');
    const std::size_t size_end =
        sha_end == std::string_view::npos ? sha_end : line.find(' ', sha_end + 1);
    if (size_end == std::string_view::npos) {
      return Fail(ErrorCode::kCorruptData, where + " : « <sha256> <taille> <chemin> » attendu");
    }
    MirrorFile file;
    file.sha256_hex = std::string(line.substr(0, sha_end));
    const std::string_view size_text = line.substr(sha_end + 1, size_end - sha_end - 1);
    file.relative_path = std::string(line.substr(size_end + 1));
    if (file.sha256_hex.size() != kSha256HexLength || !IsLowerHex(file.sha256_hex)) {
      return Fail(ErrorCode::kCorruptData, where + " : empreinte SHA-256 illisible");
    }
    const auto parsed =
        std::from_chars(size_text.data(), size_text.data() + size_text.size(), file.size_bytes);
    if (size_text.empty() || parsed.ec != std::errc() ||
        parsed.ptr != size_text.data() + size_text.size()) {
      return Fail(ErrorCode::kCorruptData, where + " : taille illisible");
    }
    if (!StaysInsideInstall(file.relative_path)) {
      return Fail(ErrorCode::kCorruptData,
                  where + " : chemin hors du dossier du jeu : " + file.relative_path);
    }
    if (!seen_paths.insert(LowerAscii(file.relative_path)).second) {
      return Fail(ErrorCode::kCorruptData, where + " : fichier répété : " + file.relative_path);
    }
    files.push_back(std::move(file));
  }
  if (!header_read) {
    return Fail(ErrorCode::kCorruptData, "le manifeste est vide");
  }
  return files;
}

std::string MirrorFileUrl(std::string_view files_url, std::string_view relative_path) {
  std::string url(files_url);
  for (const char character : relative_path) {
    if (IsUrlSafe(character)) {
      url.push_back(character);
      continue;
    }
    const auto byte = static_cast<unsigned char>(character);
    url.push_back('%');
    url.push_back(kHexDigits[byte >> kNibbleBits]);
    url.push_back(kHexDigits[byte & kNibbleMask]);
  }
  return url;
}

}  // namespace moonclient::patcher
