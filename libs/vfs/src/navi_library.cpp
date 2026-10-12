// Le chargement de la table de navigation. Voir l'en-tête.

#include "ro/vfs/navi_library.h"

#include <chrono>
#include <cstdint>
#include <string>

#include "ro/vfs/vfs.h"

namespace ro::vfs {
namespace {

[[nodiscard]] std::uint32_t MillisecondsSince(std::chrono::steady_clock::time_point start) {
  return static_cast<std::uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                        std::chrono::steady_clock::now() - start)
                                        .count());
}

}  // namespace

core::Result<formats::NaviData> LoadNaviData(Vfs& vfs, NaviLoadReport& report) {
  report = NaviLoadReport{};
  const auto read_start = std::chrono::steady_clock::now();
  auto bytes = vfs.Read(std::string(kNaviTablePath));
  report.read_ms = MillisecondsSince(read_start);
  if (!bytes) return std::unexpected(bytes.error());
  report.bytes = bytes->size();
  const auto parse_start = std::chrono::steady_clock::now();
  auto data = formats::ReadNaviTable(*bytes);
  report.parse_ms = MillisecondsSince(parse_start);
  return data;
}

}  // namespace ro::vfs
