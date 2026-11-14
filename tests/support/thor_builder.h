#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "ro/formats/thor.h"

namespace ro::test_support {

// Construit une archive de patch THOR EN MÉMOIRE, dans la disposition que
// décrit `ro/formats/thor.h`, y compris sous des formes pathologiques.

struct ThorFileSpec {
  // Octets bruts du chemin, tels qu'ils iront dans la table.
  std::string path;
  std::vector<std::uint8_t> content;
  bool removed = false;
};

struct ThorBuildOptions {
  bool merge_into_grf = true;
  std::string target_grf_name;
  // Ajoute l'entrée d'intégrité, en TÊTE de table, avec le CRC-32 de chaque
  // fichier non supprimé.
  bool include_integrity = false;
  // Surcharges pour les essais de refus.
  std::uint16_t table_mode = formats::kThorTableModeCompressed;
  std::optional<std::uint32_t> declared_entry_count;
};

[[nodiscard]] std::vector<std::uint8_t> BuildThor(const std::vector<ThorFileSpec>& files,
                                                  const ThorBuildOptions& options = {});

}  // namespace ro::test_support
