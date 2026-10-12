#pragma once

#include <cstdint>
#include <string_view>

#include "ro/core/error.h"
#include "ro/formats/navi_tables.h"

namespace ro::vfs {

class Vfs;

// LA TABLE DE NAVIGATION : `data\yaml_files\navigation.yml`, que
// `scripts/yaml_tables/navigation.py` tire du générateur de navigation du
// serveur. Voir `ro/formats/navi_tables.h`.
inline constexpr std::string_view kNaviTablePath = "data\\yaml_files\\navigation.yml";

// Ce que le chargement a coûté, pour le journal.
struct NaviLoadReport {
  std::uint32_t read_ms = 0;
  std::uint32_t parse_ms = 0;
  std::uint64_t bytes = 0;
};

// Charge la table de navigation. Sans elle, ou illisible, rien ne se résout.
//
// ⚠ LONG : quelques mégaoctets de texte. À appeler hors du fil de rendu.
[[nodiscard]] core::Result<formats::NaviData> LoadNaviData(Vfs& vfs, NaviLoadReport& report);

}  // namespace ro::vfs
