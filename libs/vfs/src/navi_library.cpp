// Le chargement des tables de navigation. Voir l'en-tête.

#include "ro/vfs/navi_library.h"

#include <chrono>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/vfs/vfs.h"

namespace ro::vfs {
namespace {

using Reader = core::Result<void> (*)(std::span<const std::uint8_t>, formats::NaviData&);

struct Table {
  std::string_view file;
  Reader read;
  bool required;
};

// Dans l'ordre où elles se résolvent : les cartes d'abord, les liaisons avant
// leurs distances.
constexpr Table kTables[] = {
    {formats::kNaviMapTableFile, &formats::ReadNaviMaps, true},
    {formats::kNaviNpcTableFile, &formats::ReadNaviNpcs, false},
    {formats::kNaviMobTableFile, &formats::ReadNaviSpawns, false},
    {formats::kNaviLinkTableFile, &formats::ReadNaviLinks, false},
    {formats::kNaviLinkDistanceTableFile, &formats::ReadNaviLinkDistances, false},
    {formats::kNaviNpcDistanceTableFile, &formats::ReadNaviNpcDistances, false},
};

[[nodiscard]] std::uint32_t MillisecondsSince(std::chrono::steady_clock::time_point start) {
  return static_cast<std::uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                        std::chrono::steady_clock::now() - start)
                                        .count());
}

}  // namespace

core::Result<formats::NaviData> LoadNaviData(Vfs& vfs, NaviLoadReport& report) {
  report = NaviLoadReport{};
  formats::NaviData data;
  for (const Table& table : kTables) {
    const std::string path = std::string(formats::kNaviTableDirectory) + std::string(table.file);
    const auto read_start = std::chrono::steady_clock::now();
    auto bytes = vfs.Read(path);
    report.read_ms += MillisecondsSince(read_start);
    if (!bytes) {
      if (table.required) return std::unexpected(bytes.error());
      if (report.degraded.empty()) report.degraded = std::string(table.file) + " absente";
      continue;
    }
    report.bytes += bytes->size();
    const auto parse_start = std::chrono::steady_clock::now();
    auto parsed = table.read(*bytes, data);
    report.parse_ms += MillisecondsSince(parse_start);
    if (!parsed) {
      if (table.required) return std::unexpected(parsed.error());
      if (report.degraded.empty()) report.degraded = core::Describe(parsed.error());
    }
  }
  return data;
}

}  // namespace ro::vfs
