#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_terrain.h"

#include "survivors_rules_check.h"

namespace ro::sim::survivors {
namespace {

constexpr char kBlockedCell = '#';
constexpr char kFreeCell = '.';

// La première clé portée deux fois, ou une vue vide.
template <typename Rule>
[[nodiscard]] std::string_view RepeatedKey(std::span<const Rule> table) {
  for (std::size_t later = 1; later < table.size(); ++later) {
    for (std::size_t earlier = 0; earlier < later; ++earlier) {
      if (std::string_view(table[earlier].key) == std::string_view(table[later].key)) {
        return table[later].key;
      }
    }
  }
  return {};
}

// VRAI SI L'EMPRISE ENCLAVE UNE CASE LIBRE : le dessin, bordé d'un rang libre,
// s'inonde à quatre voisins depuis un coin de cette bordure ; une case libre
// que l'eau n'atteint pas ne rejoindrait jamais la bordure de son emplacement.
[[nodiscard]] bool EnclosesFreeCell(std::span<const std::string_view> rows) {
  const std::size_t height = rows.size() + 2;
  const std::size_t width = rows.front().size() + 2;
  std::vector<std::uint8_t> free(width * height, 1U);
  for (std::size_t row = 0; row < rows.size(); ++row) {
    for (std::size_t column = 0; column < rows[row].size(); ++column) {
      if (rows[row][column] == kBlockedCell) free[(row + 1) * width + column + 1] = 0U;
    }
  }
  std::vector<std::uint8_t> reached(width * height, 0U);
  std::vector<std::size_t> pending{0};
  reached[0] = 1U;
  while (!pending.empty()) {
    const std::size_t cell = pending.back();
    pending.pop_back();
    const std::size_t column = cell % width;
    const std::size_t row = cell / width;
    const auto visit = [&](std::size_t next) {
      if (free[next] != 0U && reached[next] == 0U) {
        reached[next] = 1U;
        pending.push_back(next);
      }
    };
    if (column > 0) visit(cell - 1);
    if (column + 1 < width) visit(cell + 1);
    if (row > 0) visit(cell - width);
    if (row + 1 < height) visit(cell + width);
  }
  for (std::size_t cell = 0; cell < free.size(); ++cell) {
    if (free[cell] != 0U && reached[cell] == 0U) return true;
  }
  return false;
}

[[nodiscard]] core::Result<void> CheckFootprint(const TerrainPieceRule& piece,
                                                const std::string& named) {
  const std::span<const std::string_view> rows = piece.footprint;
  if (rows.empty() || rows.front().empty()) return Refuse(named + "emprise vide");
  bool blocks = false;
  for (const std::string_view row : rows) {
    if (row.size() != rows.front().size()) return Refuse(named + "emprise de lignes inégales");
    for (const char cell : row) {
      if (cell != kBlockedCell && cell != kFreeCell) {
        return Refuse(named + "emprise au caractère « " + std::string(1, cell) + " »");
      }
      blocks = blocks || cell == kBlockedCell;
    }
  }
  if (!blocks) return Refuse(named + "emprise sans case bloquée");
  const std::int32_t most = piece.landmark ? kMostLandmarkSideCells : kMostPieceSideCells;
  const FootprintSize size = FootprintSizeOf(piece, 0);
  if (size.width > most || size.height > most) {
    return Refuse(named + "emprise de " + std::to_string(size.width) + " × " +
                  std::to_string(size.height) + ", au plus " + std::to_string(most) + " de côté");
  }
  if (EnclosesFreeCell(rows)) return Refuse(named + "emprise qui enclave une case libre");
  return {};
}

// La somme des poids d'une sorte de pièces : PickWeighted la tire en 32 bits.
[[nodiscard]] std::uint64_t WeightSum(std::span<const TerrainPieceRule> pieces, bool landmark) {
  std::uint64_t total = 0;
  for (const TerrainPieceRule& piece : pieces) {
    if (piece.landmark == landmark) total += piece.weight;
  }
  return total;
}

[[nodiscard]] bool WeightOutOfBounds(std::uint64_t total) noexcept {
  return total == 0 || total > std::numeric_limits<std::uint32_t>::max();
}

[[nodiscard]] core::Result<void> CheckPieces(const TerrainRule& terrain, const std::string& named) {
  if (const std::optional<std::uint16_t> repeated = RepeatedId(
          terrain.pieces, [](const TerrainPieceRule& piece) { return piece.piece_id; })) {
    return Refuse(named + "pièce " + std::to_string(*repeated) + " en double");
  }
  if (const std::string_view key = RepeatedKey(terrain.pieces); !key.empty()) {
    return Refuse(named + "clé de pièce « " + std::string(key) + " » en double");
  }
  bool has_landmark = false;
  for (const TerrainPieceRule& piece : terrain.pieces) {
    has_landmark = has_landmark || piece.landmark;
    if (auto checked =
            CheckFootprint(piece, named + "pièce " + std::to_string(piece.piece_id) + " : ");
        !checked) {
      return checked;
    }
  }
  if (WeightOutOfBounds(WeightSum(terrain.pieces, false))) {
    return Refuse(named + "somme des poids des pièces ordinaires " +
                  std::to_string(WeightSum(terrain.pieces, false)));
  }
  if (has_landmark && WeightOutOfBounds(WeightSum(terrain.pieces, true))) {
    return Refuse(named + "somme des poids des repères " +
                  std::to_string(WeightSum(terrain.pieces, true)));
  }
  if (has_landmark && terrain.landmark_period_chunks == 0) {
    return Refuse(named + "repères sans période");
  }
  // Le morceau élu de chaque carré perd ses pièces ordinaires pour un repère :
  // sans repère à tirer, il resterait nu.
  if (!has_landmark && terrain.landmark_period_chunks != 0) {
    return Refuse(named + "période sans repère");
  }
  return {};
}

[[nodiscard]] core::Result<void> CheckTerrain(const TerrainRule& terrain) {
  const std::string named = "terrain " + std::to_string(terrain.terrain_id) + " : ";
  if (terrain.terrain_id == 0) return Refuse("terrain d'Id nul, réservé à la carte");
  if (terrain.density_min_per_mille > kTerrainPerMille ||
      terrain.density_max_per_mille > kTerrainPerMille ||
      terrain.density_min_per_mille > terrain.density_max_per_mille) {
    return Refuse(named + "densités " + std::to_string(terrain.density_min_per_mille) + " à " +
                  std::to_string(terrain.density_max_per_mille) +
                  " pour mille, hors de [0, 1000] ou inversées");
  }
  if (terrain.density_period_chunks <= 0 || terrain.density_period_chunks > kTerrainWorldChunks) {
    return Refuse(named + "période de densité de " + std::to_string(terrain.density_period_chunks) +
                  " morceaux");
  }
  if (terrain.landmark_period_chunks < 0 || terrain.landmark_period_chunks > kTerrainWorldChunks) {
    return Refuse(named + "période des repères de " +
                  std::to_string(terrain.landmark_period_chunks) + " morceaux");
  }
  if (terrain.path_period_chunks < 0 || terrain.path_period_chunks > kTerrainWorldChunks) {
    return Refuse(named + "période des chemins de " + std::to_string(terrain.path_period_chunks) +
                  " morceaux");
  }
  // Un chemin ne quitte pas les deux cellules qu'il relie : c'est ce qui permet
  // à un morceau de ne chercher que quatre chemins.
  if (terrain.path_period_chunks > 0 &&
      (terrain.path_width_cells <= 0 || terrain.path_wiggle_cells < 0 ||
       terrain.path_width_cells + terrain.path_wiggle_cells > kTerrainPathMarginCells)) {
    return Refuse(named + "chemins de " + std::to_string(terrain.path_width_cells) +
                  " cases de large, écartés de " + std::to_string(terrain.path_wiggle_cells) +
                  " cases : hors de [1, " + std::to_string(kTerrainPathMarginCells) +
                  "] à eux deux");
  }
  if (CellsOutOfBounds(terrain.clearing_cells)) {
    return Refuse(named + "clairière de " + std::to_string(terrain.clearing_cells) +
                  " cases, hors de [0, " + std::to_string(kMostArenaSideCells) + "]");
  }
  return CheckPieces(terrain, named);
}

}  // namespace

core::Result<void> CheckTerrains(const SurvivorsRules& rules) {
  if (const std::optional<std::uint16_t> repeated = RepeatedId(
          rules.terrains, [](const TerrainRule& terrain) { return terrain.terrain_id; })) {
    return Refuse("terrain " + std::to_string(*repeated) + " en double");
  }
  if (const std::string_view key = RepeatedKey(rules.terrains); !key.empty()) {
    return Refuse("clé de terrain « " + std::string(key) + " » en double");
  }
  for (const TerrainRule& terrain : rules.terrains) {
    if (auto checked = CheckTerrain(terrain); !checked) return checked;
  }
  for (const ArenaRule& arena : rules.arenas) {
    if (arena.terrain_id == 0) continue;
    // La clairière et l'écart des repères se mesurent depuis le centre du
    // monde : une arène générée y fait naître le héros, sans départ à elle.
    if (arena.start) {
      return Refuse("arène " + std::to_string(arena.arena_id) +
                    " : générée, elle naît au centre du monde et ne porte pas de départ");
    }
    bool known = false;
    for (const TerrainRule& terrain : rules.terrains) {
      known = known || terrain.terrain_id == arena.terrain_id;
    }
    if (!known) {
      return Refuse("arène " + std::to_string(arena.arena_id) + " : terrain inconnu " +
                    std::to_string(arena.terrain_id));
    }
  }
  return {};
}

}  // namespace ro::sim::survivors
