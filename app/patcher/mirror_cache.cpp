#include "patcher/mirror_cache.h"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <sstream>

#include "ro/core/file_write.h"
#include "ro/core/log.h"

namespace moonclient::patcher {
namespace {

constexpr std::string_view kMirrorCacheHeader = "moonclient-mirror-cache 1";

template <typename Number>
[[nodiscard]] bool ParseNumber(std::string_view text, Number& value) {
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  return !text.empty() && parsed.ec == std::errc() && parsed.ptr == text.data() + text.size();
}

}  // namespace

MirrorCache ParseMirrorCache(std::string_view text) {
  MirrorCache cache;
  bool header_read = false;
  std::size_t start = 0;
  while (start < text.size()) {
    const std::size_t end = std::min(text.find('\n', start), text.size());
    const std::string_view line = text.substr(start, end - start);
    start = end + 1;
    if (!header_read) {
      if (line != kMirrorCacheHeader) {
        return {};
      }
      header_read = true;
      continue;
    }
    if (line.empty()) {
      continue;
    }
    // `<sha256> <taille> <tics> <chemin>`, le chemin jusqu'au bout de la ligne.
    const std::size_t sha_end = line.find(' ');
    const std::size_t size_end =
        sha_end == std::string_view::npos ? sha_end : line.find(' ', sha_end + 1);
    const std::size_t ticks_end =
        size_end == std::string_view::npos ? size_end : line.find(' ', size_end + 1);
    KnownFile known;
    if (ticks_end == std::string_view::npos ||
        !ParseNumber(line.substr(sha_end + 1, size_end - sha_end - 1), known.size_bytes) ||
        !ParseNumber(line.substr(size_end + 1, ticks_end - size_end - 1), known.write_ticks)) {
      return {};
    }
    known.sha256_hex = std::string(line.substr(0, sha_end));
    cache.insert_or_assign(std::string(line.substr(ticks_end + 1)), std::move(known));
  }
  return cache;
}

std::string FormatMirrorCache(const MirrorCache& cache) {
  std::string text(kMirrorCacheHeader);
  text.push_back('\n');
  for (const auto& [relative_path, known] : cache) {
    text += known.sha256_hex + ' ' + std::to_string(known.size_bytes) + ' ' +
            std::to_string(known.write_ticks) + ' ' + relative_path + '\n';
  }
  return text;
}

MirrorCache ReadMirrorCache(const std::filesystem::path& install_root) {
  std::ifstream file(install_root / kMirrorCacheFileName, std::ios::binary);
  if (!file) {
    return {};
  }
  std::ostringstream all;
  all << file.rdbuf();
  return ParseMirrorCache(all.str());
}

void WriteMirrorCache(const std::filesystem::path& install_root, const MirrorCache& cache) {
  if (auto saved = ro::core::WriteFileAtomically(install_root / kMirrorCacheFileName,
                                                 FormatMirrorCache(cache));
      !saved) {
    // Sans le cache, le prochain lancement rehache : plus lent, pas faux.
    ro::core::LogWarning("patcher : cache du miroir non enregistré — {}",
                         ro::core::Describe(saved.error()));
  }
}

}  // namespace moonclient::patcher
