#include "ro/sim/survivors_terrain.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <vector>

#include "ro/core/pcg32.h"

namespace ro::sim {
namespace {

// Les sels des mélanges, en ASCII : « SURVTERR » pour la graine d'un morceau,
// « SURVDENS » pour les nœuds de densité, « SURVMARK » pour le morceau à
// repère de chaque carré.
constexpr std::uint64_t kTerrainSalt = 0x5355'5256'5445'5252ULL;
constexpr std::uint64_t kDensitySalt = 0x5355'5256'4445'4E53ULL;
constexpr std::uint64_t kLandmarkSalt = 0x5355'5256'4D41'524BULL;
// « SURVPATH » pour les carrefours, « SURVBEND » pour les coudes des chemins.
constexpr std::uint64_t kPathSalt = 0x5355'5256'5041'5448ULL;
constexpr std::uint64_t kBendSalt = 0x5355'5256'4245'4E44ULL;
// « SURVSIDE » pour le quart du site d'un repère autour de sa place.
constexpr std::uint64_t kSideSalt = 0x5355'5256'5349'4445ULL;
constexpr std::uint64_t kSiteEastBit = 1;
constexpr std::uint64_t kSiteNorthBit = 2;
// Le coude d'un chemin se mélange à la direction de son chemin.
constexpr std::uint64_t kEastward = 0;
constexpr std::uint64_t kNorthward = 1;
// Les constantes publiées de SplitMix64 : l'incrément (partie fractionnaire du
// nombre d'or) et les deux multiplicateurs du finaliseur.
constexpr std::uint64_t kSplitMixGamma = 0x9E37'79B9'7F4A'7C15ULL;
constexpr std::uint64_t kSplitMixFirstMultiplier = 0xBF58'476D'1CE4'E5B9ULL;
constexpr std::uint64_t kSplitMixSecondMultiplier = 0x94D0'49BB'1331'11EBULL;
constexpr unsigned kSplitMixFirstShift = 30;
constexpr unsigned kSplitMixSecondShift = 27;
constexpr unsigned kSplitMixLastShift = 31;
// Un mélange donne deux décalages, le morceau à repère comme le carrefour : x
// est le reste du mot entier de 64 bits, y celui de sa seule moitié haute.
constexpr unsigned kHalfWordBits = 32;
constexpr char kBlockedCell = '#';

constexpr std::int32_t kSlotCentreCells = kTerrainSlotCells / 2;

// Un composant entier signé entre dans le mélange par sa valeur modulo 2^64 :
// une conversion définie, et aucun décalage de signé.
[[nodiscard]] constexpr std::uint64_t Component(std::int64_t value) noexcept {
  return static_cast<std::uint64_t>(value);
}

// Un pas de SplitMix64 : l'état avance d'un composant et de l'incrément, puis
// passe par le finaliseur.
[[nodiscard]] constexpr std::uint64_t MixStep(std::uint64_t state,
                                              std::uint64_t component) noexcept {
  std::uint64_t mixed = state + component + kSplitMixGamma;
  mixed = (mixed ^ (mixed >> kSplitMixFirstShift)) * kSplitMixFirstMultiplier;
  mixed = (mixed ^ (mixed >> kSplitMixSecondShift)) * kSplitMixSecondMultiplier;
  return mixed ^ (mixed >> kSplitMixLastShift);
}

[[nodiscard]] constexpr std::uint64_t Mix(std::uint64_t run_seed, std::uint64_t salt,
                                          std::int64_t x, std::int64_t y) noexcept {
  return MixStep(MixStep(MixStep(run_seed, salt), Component(x)), Component(y));
}

[[nodiscard]] constexpr std::int64_t FloorDivide(std::int64_t value, std::int64_t by) noexcept {
  const std::int64_t quotient = value / by;
  return (value % by != 0 && (value < 0) != (by < 0)) ? quotient - 1 : quotient;
}

// La distance de Tchebychev du départ à la case la plus proche d'un rectangle
// [west, east] × [south, north], bornes comprises.
[[nodiscard]] std::int64_t DistanceFromStart(std::int64_t west, std::int64_t south,
                                             std::int64_t east, std::int64_t north) noexcept {
  const std::int64_t dx =
      std::max({west - kTerrainStartCell, std::int64_t{0}, std::int64_t{kTerrainStartCell} - east});
  const std::int64_t dy = std::max(
      {south - kTerrainStartCell, std::int64_t{0}, std::int64_t{kTerrainStartCell} - north});
  return std::max(dx, dy);
}

// La valeur d'un nœud de densité, dans [min, max] ; min seul si la règle les
// inverse.
[[nodiscard]] std::uint64_t DensityNode(const TerrainRule& rule, std::uint64_t run_seed,
                                        std::int64_t node_x, std::int64_t node_y) noexcept {
  const std::uint64_t least = rule.density_min_per_mille;
  if (rule.density_max_per_mille <= rule.density_min_per_mille) return least;
  const std::uint64_t span = std::uint64_t{rule.density_max_per_mille} - least + 1;
  return least + Mix(run_seed, kDensitySalt, node_x, node_y) % span;
}

// Les poids des pièces, ceux de l'autre sorte à zéro : un même index désigne
// la même pièce dans les deux tirages.
[[nodiscard]] std::vector<std::uint32_t> WeightsOf(std::span<const TerrainPieceRule> pieces,
                                                   bool landmark) {
  std::vector<std::uint32_t> weights;
  weights.reserve(pieces.size());
  for (const TerrainPieceRule& piece : pieces) {
    weights.push_back(piece.landmark == landmark ? piece.weight : 0U);
  }
  return weights;
}

// Le coin d'une pièce dans un emplacement de `slot_cells` : deux tirages,
// toujours, même quand la pièce ne tient pas.
struct Corner {
  std::int32_t east = 0;
  std::int32_t north = 0;
  bool fits = false;
};

[[nodiscard]] Corner DrawCorner(core::Pcg32& random, FootprintSize size, std::int32_t slot_cells) {
  const std::int32_t room = slot_cells - 2 * kTerrainBorderCells;
  const bool fits = size.width > 0 && size.height > 0 && size.width <= room && size.height <= room;
  const std::int32_t spare_east = fits ? room - size.width : 0;
  const std::int32_t spare_north = fits ? room - size.height : 0;
  const auto east =
      static_cast<std::int32_t>(random.Below(static_cast<std::uint32_t>(spare_east) + 1U));
  const auto north =
      static_cast<std::int32_t>(random.Below(static_cast<std::uint32_t>(spare_north) + 1U));
  return Corner{
      .east = kTerrainBorderCells + east, .north = kTerrainBorderCells + north, .fits = fits};
}

// Une pièce tirée pour un emplacement, posée ou non.
struct Draw {
  std::size_t piece_index = 0;
  std::uint8_t quarter = 0;
  Corner corner;
};

[[nodiscard]] Draw DrawPiece(core::Pcg32& random, std::span<const TerrainPieceRule> pieces,
                             std::span<const std::uint32_t> weights, std::int32_t slot_cells) {
  Draw draw;
  draw.piece_index = random.PickWeighted(weights);
  draw.quarter = static_cast<std::uint8_t>(random.Below(kTerrainQuarterTurns));
  const FootprintSize size = draw.piece_index < pieces.size()
                                 ? FootprintSizeOf(pieces[draw.piece_index], draw.quarter)
                                 : FootprintSize{};
  draw.corner = DrawCorner(random, size, slot_cells);
  return draw;
}

[[nodiscard]] std::size_t LocalCell(std::int32_t local_x, std::int32_t local_y) noexcept {
  return static_cast<std::size_t>(local_y) * static_cast<std::size_t>(kTerrainChunkCells) +
         static_cast<std::size_t>(local_x);
}

// Vrai si une case du rectangle [west, west + width) × [south, south + height),
// en coordonnées locales du morceau, est une case de chemin.
[[nodiscard]] bool TouchesPath(const TerrainChunk& chunk, std::int32_t west, std::int32_t south,
                               FootprintSize size) noexcept {
  for (std::int32_t north = 0; north < size.height; ++north) {
    for (std::int32_t east = 0; east < size.width; ++east) {
      if (chunk.path[LocalCell(west + east, south + north)] != 0U) return true;
    }
  }
  return false;
}

// Pose une pièce tirée, coin de l'emplacement en coordonnées locales du morceau.
// Une pièce ordinaire qui toucherait un chemin ne se pose pas ; une pièce
// franchissable se pose sans bloquer de case.
void Place(TerrainChunk& chunk, std::span<const TerrainPieceRule> pieces, const Draw& draw,
           std::int32_t slot_east, std::int32_t slot_north) {
  if (draw.piece_index >= pieces.size() || !draw.corner.fits) return;
  const TerrainPieceRule& piece = pieces[draw.piece_index];
  const FootprintSize size = FootprintSizeOf(piece, draw.quarter);
  const std::int32_t west = slot_east + draw.corner.east;
  const std::int32_t south = slot_north + draw.corner.north;
  if (!piece.landmark && TouchesPath(chunk, west, south, size)) return;
  for (std::int32_t north = 0; north < size.height && !piece.walkable; ++north) {
    for (std::int32_t east = 0; east < size.width; ++east) {
      if (!FootprintCovers(piece, draw.quarter, east, north)) continue;
      const std::size_t cell = LocalCell(west + east, south + north);
      chunk.passable[cell] = 0U;
      chunk.path[cell] = 0U;
    }
  }
  chunk.pieces.push_back(PlacedPiece{.piece_id = piece.piece_id,
                                     .x = chunk.chunk_x * kTerrainChunkCells + west,
                                     .y = chunk.chunk_y * kTerrainChunkCells + south,
                                     .quarter = draw.quarter});
}

// Le morceau élu du carré de repères (block_x, block_y) ; la règle a des repères.
[[nodiscard]] TerrainCell ElectedChunk(const TerrainRule& rule, std::uint64_t run_seed,
                                       std::int64_t block_x, std::int64_t block_y) noexcept {
  const std::int64_t period = rule.landmark_period_chunks;
  const std::uint64_t mixed = Mix(run_seed, kLandmarkSalt, block_x, block_y);
  return TerrainCell{
      .x = block_x * period + static_cast<std::int64_t>(mixed % static_cast<std::uint64_t>(period)),
      .y = block_y * period + static_cast<std::int64_t>((mixed >> kHalfWordBits) %
                                                        static_cast<std::uint64_t>(period))};
}

// Vrai si ce morceau est l'élu de son carré, hors de la clairière.
[[nodiscard]] bool ElectedOutsideClearing(const TerrainRule& rule, std::uint64_t run_seed,
                                          std::int64_t chunk_x, std::int64_t chunk_y) noexcept {
  const std::int64_t period = rule.landmark_period_chunks;
  if (period <= 0) return false;
  const TerrainCell elected =
      ElectedChunk(rule, run_seed, FloorDivide(chunk_x, period), FloorDivide(chunk_y, period));
  if (elected.x != chunk_x || elected.y != chunk_y) return false;
  const std::int64_t west = chunk_x * kTerrainChunkCells;
  const std::int64_t south = chunk_y * kTerrainChunkCells;
  return DistanceFromStart(west, south, west + kTerrainChunkCells - 1,
                           south + kTerrainChunkCells - 1) >= rule.clearing_cells;
}

[[nodiscard]] bool HasPaths(const TerrainRule& rule) noexcept {
  return rule.path_period_chunks > 0 && rule.path_width_cells > 0;
}

// Le premier élu hors clairière de la cellule de chemins (cell_x, cell_y), par
// carré de repères du sud au nord et d'ouest en est : le morceau de sa place.
struct PlazaChunk {
  bool exists = false;
  TerrainCell chunk;
};

[[nodiscard]] PlazaChunk PlazaChunkOf(const TerrainRule& rule, std::uint64_t run_seed,
                                      std::int64_t cell_x, std::int64_t cell_y) noexcept {
  const std::int64_t period = rule.path_period_chunks;
  const std::int64_t landmark_period = rule.landmark_period_chunks;
  if (!HasPaths(rule) || landmark_period <= 0) return PlazaChunk{};
  const std::int64_t first_x = cell_x * period;
  const std::int64_t first_y = cell_y * period;
  const std::int64_t last_x = first_x + period - 1;
  const std::int64_t last_y = first_y + period - 1;
  for (std::int64_t block_y = FloorDivide(first_y, landmark_period);
       block_y <= FloorDivide(last_y, landmark_period); ++block_y) {
    for (std::int64_t block_x = FloorDivide(first_x, landmark_period);
         block_x <= FloorDivide(last_x, landmark_period); ++block_x) {
      const TerrainCell elected = ElectedChunk(rule, run_seed, block_x, block_y);
      if (elected.x < first_x || elected.x > last_x || elected.y < first_y || elected.y > last_y ||
          !ElectedOutsideClearing(rule, run_seed, elected.x, elected.y)) {
        continue;
      }
      return PlazaChunk{.exists = true, .chunk = elected};
    }
  }
  return PlazaChunk{};
}

// Vrai si ce morceau porte un repère : l'élu de son carré, hors de la
// clairière ; avec des chemins, le seul morceau de place de sa cellule.
[[nodiscard]] bool CarriesLandmark(const TerrainRule& rule, std::uint64_t run_seed,
                                   std::int64_t chunk_x, std::int64_t chunk_y) noexcept {
  if (!ElectedOutsideClearing(rule, run_seed, chunk_x, chunk_y)) return false;
  if (!HasPaths(rule)) return true;
  const PlazaChunk plaza = PlazaChunkOf(rule, run_seed, TerrainPathCellOf(rule, chunk_x),
                                        TerrainPathCellOf(rule, chunk_y));
  return plaza.exists && plaza.chunk.x == chunk_x && plaza.chunk.y == chunk_y;
}

// La place qu'un morceau de place porte en son centre, et le quart de son site.
[[nodiscard]] TerrainPlaza PlazaIn(std::uint64_t run_seed, TerrainCell chunk) noexcept {
  const std::uint64_t mixed = Mix(run_seed, kSideSalt, chunk.x, chunk.y);
  return TerrainPlaza{
      .exists = true,
      .centre = TerrainCell{.x = chunk.x * kTerrainChunkCells + kTerrainChunkCells / 2,
                            .y = chunk.y * kTerrainChunkCells + kTerrainChunkCells / 2},
      .site_east = (mixed & kSiteEastBit) != 0U,
      .site_north = (mixed & kSiteNorthBit) != 0U};
}

// ── LES CHEMINS.

// Le disque d'un chemin, `width` cases de diamètre : les écarts d'une case à
// son centre, sur un axe, vont de `DiscLow(width)` à `DiscLow(width) + width − 1`.
[[nodiscard]] constexpr std::int64_t DiscLow(std::int64_t width) noexcept { return -(width / 2); }

// Vrai si l'écart (east, north) tombe dans le disque : comptées en demi-cases
// depuis le centre du carré qui l'entoure, ses coordonnées tiennent dans un
// cercle de rayon `width` demi-cases, à une demi-case carrée près pour qu'un
// disque d'une case garde sa case.
[[nodiscard]] constexpr bool InDisc(std::int64_t width, std::int64_t east,
                                    std::int64_t north) noexcept {
  const std::int64_t centre = 2 * DiscLow(width) + width - 1;
  const std::int64_t u = 2 * east - centre;
  const std::int64_t v = 2 * north - centre;
  return u * u + v * v <= width * width + 1;
}

// Ce qu'un morceau peint de ses chemins : son coin dans le monde et sa largeur.
struct PathCanvas {
  TerrainChunk& chunk;
  std::int64_t west = 0;
  std::int64_t south = 0;
  std::int64_t width = 0;

  // Un disque centré sur la case (x, y) du monde.
  void Stamp(std::int64_t x, std::int64_t y) const {
    const std::int64_t low = DiscLow(width);
    for (std::int64_t north = low; north < low + width; ++north) {
      for (std::int64_t east = low; east < low + width; ++east) {
        const std::int64_t local_x = x + east - west;
        const std::int64_t local_y = y + north - south;
        if (local_x < 0 || local_y < 0 || local_x >= kTerrainChunkCells ||
            local_y >= kTerrainChunkCells || !InDisc(width, east, north)) {
          continue;
        }
        chunk.path[LocalCell(static_cast<std::int32_t>(local_x),
                             static_cast<std::int32_t>(local_y))] = 1U;
      }
    }
  }

  // Le trait de `from` à `to`, case par case (Bresenham, huit voisins), s'il
  // passe à portée du morceau.
  void Line(TerrainCell from, TerrainCell to) const {
    const std::int64_t reach = width;
    if (std::max(from.x, to.x) + reach < west ||
        std::min(from.x, to.x) - reach >= west + kTerrainChunkCells ||
        std::max(from.y, to.y) + reach < south ||
        std::min(from.y, to.y) - reach >= south + kTerrainChunkCells) {
      return;
    }
    const std::int64_t step_x = from.x < to.x ? 1 : -1;
    const std::int64_t step_y = from.y < to.y ? 1 : -1;
    const std::int64_t span_x = from.x < to.x ? to.x - from.x : from.x - to.x;
    const std::int64_t span_y = -(from.y < to.y ? to.y - from.y : from.y - to.y);
    std::int64_t error = span_x + span_y;
    TerrainCell at = from;
    while (true) {
      Stamp(at.x, at.y);
      if (at == to) return;
      const std::int64_t doubled = 2 * error;
      if (doubled >= span_y) {
        error += span_y;
        at.x += step_x;
      }
      if (doubled <= span_x) {
        error += span_x;
        at.y += step_y;
      }
    }
  }
};

// Le carrefour d'une cellule de chemins, et sa place s'il en a une.
struct Crossing {
  TerrainCell centre;
  TerrainPlaza plaza;
};

[[nodiscard]] Crossing CrossingOf(const TerrainRule& rule, std::uint64_t run_seed,
                                  std::int64_t cell_x, std::int64_t cell_y) noexcept {
  const std::int64_t period = rule.path_period_chunks;
  if (period <= 0) return Crossing{};
  const PlazaChunk plaza_chunk = PlazaChunkOf(rule, run_seed, cell_x, cell_y);
  if (plaza_chunk.exists) {
    const TerrainPlaza plaza = PlazaIn(run_seed, plaza_chunk.chunk);
    return Crossing{.centre = plaza.centre, .plaza = plaza};
  }
  const std::uint64_t mixed = Mix(run_seed, kPathSalt, cell_x, cell_y);
  const std::uint64_t span =
      static_cast<std::uint64_t>(period * kTerrainChunkCells - 2 * kTerrainPathMarginCells) + 1;
  return Crossing{
      .centre = TerrainCell{.x = cell_x * period * kTerrainChunkCells + kTerrainPathMarginCells +
                                 static_cast<std::int64_t>(mixed % span),
                            .y = cell_y * period * kTerrainChunkCells + kTerrainPathMarginCells +
                                 static_cast<std::int64_t>((mixed >> kHalfWordBits) % span)}};
}

// La case à `cells` du centre de la place sur l'axe d'un chemin, vers l'est ou
// le nord quand `cells` est positif.
[[nodiscard]] TerrainCell AlongAxis(TerrainCell centre, bool eastward,
                                    std::int64_t cells) noexcept {
  return eastward ? TerrainCell{.x = centre.x + cells, .y = centre.y}
                  : TerrainCell{.x = centre.x, .y = centre.y + cells};
}

// Le chemin du carrefour `from` de la cellule (cell_x, cell_y) au carrefour
// `to` de sa voisine à l'est ou au nord : un coude tous les
// kTerrainPathBendCells cases le long de son axe, écarté de la droite par un
// mélange. Une place dont le site est du côté du chemin le fait passer par sa
// porte, en ligne droite.
void PaintPath(const TerrainRule& rule, std::uint64_t run_seed, const PathCanvas& canvas,
               std::int64_t cell_x, std::int64_t cell_y, std::uint64_t direction,
               const Crossing& start, const Crossing& end) {
  const bool eastward = direction == kEastward;
  const std::int64_t gate = TerrainPlazaGateCells(rule.path_width_cells);
  TerrainCell from = start.centre;
  if (start.plaza.exists && (eastward ? start.plaza.site_east : start.plaza.site_north)) {
    from = AlongAxis(start.centre, eastward, gate);
    canvas.Line(start.centre, from);
  }
  TerrainCell to = end.centre;
  if (end.plaza.exists && !(eastward ? end.plaza.site_east : end.plaza.site_north)) {
    to = AlongAxis(end.centre, eastward, -gate);
    canvas.Line(to, end.centre);
  }
  // L'axe du chemin et son travers ; un carrefour se tient loin du bord de sa
  // cellule, si bien que l'axe avance toujours.
  const std::int64_t along_from = eastward ? from.x : from.y;
  const std::int64_t along_span = (eastward ? to.x : to.y) - along_from;
  const std::int64_t across_from = eastward ? from.y : from.x;
  const std::int64_t across_span = (eastward ? to.y : to.x) - across_from;
  const std::int64_t bends = std::max<std::int64_t>(1, along_span / kTerrainPathBendCells);
  const std::uint64_t wiggle_span = 2 * static_cast<std::uint64_t>(rule.path_wiggle_cells) + 1;
  const std::uint64_t path_mix = MixStep(Mix(run_seed, kBendSalt, cell_x, cell_y), direction);
  TerrainCell previous = from;
  for (std::int64_t bend = 1; bend <= bends; ++bend) {
    const std::int64_t along = along_from + FloorDivide(along_span * bend, bends);
    std::int64_t across = across_from + FloorDivide(across_span * bend, bends);
    if (bend < bends) {
      const std::uint64_t mixed = MixStep(path_mix, Component(bend));
      across += static_cast<std::int64_t>(mixed % wiggle_span) - rule.path_wiggle_cells;
    }
    const TerrainCell next =
        eastward ? TerrainCell{.x = along, .y = across} : TerrainCell{.x = across, .y = along};
    canvas.Line(previous, next);
    previous = next;
  }
}

// Les chemins qui passent dans le morceau : ceux qui partent de sa cellule, et
// ceux qui y arrivent depuis la cellule à l'ouest ou au sud. Aucun autre ne
// l'atteint, puisqu'un chemin ne quitte pas les deux cellules qu'il relie. Les
// quatre chemins n'ont que cinq bouts : le carrefour de la cellule et ceux de
// ses quatre voisines, chacun calculé une fois.
void PaintPaths(const TerrainRule& rule, std::uint64_t run_seed, TerrainChunk& chunk) {
  if (rule.path_period_chunks <= 0 || rule.path_width_cells <= 0) return;
  const PathCanvas canvas{.chunk = chunk,
                          .west = std::int64_t{chunk.chunk_x} * kTerrainChunkCells,
                          .south = std::int64_t{chunk.chunk_y} * kTerrainChunkCells,
                          .width = rule.path_width_cells};
  const std::int64_t cell_x = TerrainPathCellOf(rule, chunk.chunk_x);
  const std::int64_t cell_y = TerrainPathCellOf(rule, chunk.chunk_y);
  const Crossing here = CrossingOf(rule, run_seed, cell_x, cell_y);
  const Crossing west = CrossingOf(rule, run_seed, cell_x - 1, cell_y);
  const Crossing east = CrossingOf(rule, run_seed, cell_x + 1, cell_y);
  const Crossing south = CrossingOf(rule, run_seed, cell_x, cell_y - 1);
  const Crossing north = CrossingOf(rule, run_seed, cell_x, cell_y + 1);
  PaintPath(rule, run_seed, canvas, cell_x - 1, cell_y, kEastward, west, here);
  PaintPath(rule, run_seed, canvas, cell_x, cell_y, kEastward, here, east);
  PaintPath(rule, run_seed, canvas, cell_x, cell_y - 1, kNorthward, south, here);
  PaintPath(rule, run_seed, canvas, cell_x, cell_y, kNorthward, here, north);
  // La place tient dans son morceau : seule celle de la cellule peut le toucher.
  if (here.plaza.exists) {
    const PathCanvas plaza{
        .chunk = chunk, .west = canvas.west, .south = canvas.south, .width = kTerrainPlazaCells};
    plaza.Stamp(here.centre.x, here.centre.y);
  }
}

// Pose le repère tiré contre la place de son morceau, dans le quart de son site.
void PlaceBesidePlaza(TerrainChunk& chunk, std::span<const TerrainPieceRule> pieces, Draw draw,
                      const TerrainPlaza& plaza) {
  if (draw.piece_index >= pieces.size() || !draw.corner.fits) return;
  const FootprintSize size = FootprintSizeOf(pieces[draw.piece_index], draw.quarter);
  constexpr std::int32_t kCentre = kTerrainChunkCells / 2;
  draw.corner.east = plaza.site_east ? kCentre + kTerrainSiteOffsetCells
                                     : kCentre - kTerrainSiteOffsetCells - size.width + 1;
  draw.corner.north = plaza.site_north ? kCentre + kTerrainSiteOffsetCells
                                       : kCentre - kTerrainSiteOffsetCells - size.height + 1;
  Place(chunk, pieces, draw, 0, 0);
}

// LA GÉOMÉTRIE DE LA PLACE, prouvée pour la plus large route permise. Le site
// laisse libre la bordure de son morceau ; la place ne touche pas le site.
constexpr std::int32_t kPlazaCentre = kTerrainChunkCells / 2;
static_assert(kPlazaCentre + kTerrainSiteOffsetCells + kTerrainLandmarkSiteCells <=
              kTerrainChunkCells - kTerrainBorderCells);
static_assert(kPlazaCentre - kTerrainSiteOffsetCells - kTerrainLandmarkSiteCells + 1 >=
              kTerrainBorderCells);
static_assert(kPlazaCentre + DiscLow(kTerrainPlazaCells) >= kTerrainBorderCells &&
              kPlazaCentre + DiscLow(kTerrainPlazaCells) + kTerrainPlazaCells <=
                  kTerrainChunkCells - kTerrainBorderCells);
static_assert(DiscLow(kTerrainPlazaCells) * 2 + kTerrainPlazaCells - 1 == 0,
              "la place est un disque centré sur son carrefour");
static_assert(!InDisc(kTerrainPlazaCells, kTerrainSiteOffsetCells, kTerrainSiteOffsetCells));
// Le trait d'une route sur l'axe de la place ne déborde pas vers le site.
static_assert(-DiscLow(kMostPlazaPathWidthCells) < kTerrainSiteOffsetCells &&
              DiscLow(kMostPlazaPathWidthCells) + kMostPlazaPathWidthCells - 1 <
                  kTerrainSiteOffsetCells);
// Une porte dépasse le site, et reste à moins d'un demi-morceau du centre.
static_assert(TerrainPlazaGateCells(kMostPlazaPathWidthCells) + DiscLow(kMostPlazaPathWidthCells) >=
              kTerrainSiteOffsetCells + kTerrainLandmarkSiteCells);
static_assert(-TerrainPlazaGateCells(kMostPlazaPathWidthCells) + DiscLow(kMostPlazaPathWidthCells) +
                  kMostPlazaPathWidthCells - 1 <
              -(kTerrainSiteOffsetCells + kTerrainLandmarkSiteCells - 1));
static_assert(TerrainPlazaGateCells(kMostPlazaPathWidthCells) < kTerrainChunkCells / 2);
static_assert(TerrainPlazaGateCells(kMostPlazaPathWidthCells + 1) >= kTerrainChunkCells / 2);

}  // namespace

std::int64_t TerrainPathCellOf(const TerrainRule& rule, std::int64_t chunk) noexcept {
  if (rule.path_period_chunks <= 0) return 0;
  return FloorDivide(chunk, rule.path_period_chunks);
}

TerrainCell TerrainPathCrossing(const TerrainRule& rule, std::uint64_t run_seed,
                                std::int64_t cell_x, std::int64_t cell_y) {
  return CrossingOf(rule, run_seed, cell_x, cell_y).centre;
}

TerrainPlaza TerrainPathPlaza(const TerrainRule& rule, std::uint64_t run_seed, std::int64_t cell_x,
                              std::int64_t cell_y) {
  return CrossingOf(rule, run_seed, cell_x, cell_y).plaza;
}

bool TerrainChunk::Passable(std::int32_t local_x, std::int32_t local_y) const noexcept {
  if (local_x < 0 || local_y < 0 || local_x >= kTerrainChunkCells ||
      local_y >= kTerrainChunkCells) {
    return false;
  }
  return passable[LocalCell(local_x, local_y)] != 0U;
}

bool TerrainChunk::OnPath(std::int32_t local_x, std::int32_t local_y) const noexcept {
  if (local_x < 0 || local_y < 0 || local_x >= kTerrainChunkCells ||
      local_y >= kTerrainChunkCells) {
    return false;
  }
  return path[LocalCell(local_x, local_y)] != 0U;
}

FootprintSize FootprintSizeOf(const TerrainPieceRule& piece, std::uint8_t quarter) noexcept {
  const auto height = static_cast<std::int32_t>(piece.footprint.size());
  const auto width =
      piece.footprint.empty() ? 0 : static_cast<std::int32_t>(piece.footprint.front().size());
  if (quarter % 2 == 1) return FootprintSize{.width = height, .height = width};
  return FootprintSize{.width = width, .height = height};
}

bool FootprintCovers(const TerrainPieceRule& piece, std::uint8_t quarter, std::int32_t east,
                     std::int32_t north) noexcept {
  const FootprintSize drawn = FootprintSizeOf(piece, 0);
  // La case du dessin d'origine (colonne, rangée depuis le sud) qui vient en
  // (east, north) après le quart de tour : la rotation inverse.
  std::int32_t column = east;
  std::int32_t row = north;
  switch (quarter % kTerrainQuarterTurns) {
    case 1:
      column = north;
      row = drawn.height - 1 - east;
      break;
    case 2:
      column = drawn.width - 1 - east;
      row = drawn.height - 1 - north;
      break;
    case 3:
      column = drawn.width - 1 - north;
      row = east;
      break;
    default:
      break;
  }
  if (column < 0 || row < 0 || row >= drawn.height) return false;
  const std::string_view line = piece.footprint[static_cast<std::size_t>(drawn.height - 1 - row)];
  if (static_cast<std::size_t>(column) >= line.size()) return false;
  return line[static_cast<std::size_t>(column)] == kBlockedCell;
}

std::int32_t TerrainChunkOf(std::int32_t cell) noexcept {
  return static_cast<std::int32_t>(FloorDivide(cell, kTerrainChunkCells));
}

std::uint64_t TerrainChunkSeed(std::uint64_t run_seed, std::int32_t chunk_x,
                               std::int32_t chunk_y) noexcept {
  return Mix(run_seed, kTerrainSalt, chunk_x, chunk_y);
}

std::uint32_t TerrainDensityPerMille(const TerrainRule& rule, std::uint64_t run_seed,
                                     std::int64_t cell_x, std::int64_t cell_y) {
  const std::int64_t period_cells =
      std::int64_t{std::max(rule.density_period_chunks, 1)} * kTerrainChunkCells;
  const std::int64_t node_x = FloorDivide(cell_x, period_cells);
  const std::int64_t node_y = FloorDivide(cell_y, period_cells);
  // Les poids de l'interpolation bilinéaire : entiers, positifs, de somme
  // period_cells². La plus grande période permise couvre le monde : la somme
  // vaut alors 2^32, qui ne tient PAS en 32 bits, et la somme pondérée des
  // densités, au plus kTerrainPerMille × 2^32, se calcule en 64 bits.
  static_assert(std::uint64_t{kTerrainWorldCells} * std::uint64_t{kTerrainWorldCells} <=
                    std::numeric_limits<std::uint64_t>::max() / std::uint64_t{kTerrainPerMille},
                "la somme pondérée des densités doit tenir en 64 bits");
  const auto east = static_cast<std::uint64_t>(cell_x - node_x * period_cells);
  const auto north = static_cast<std::uint64_t>(cell_y - node_y * period_cells);
  const auto period = static_cast<std::uint64_t>(period_cells);
  const std::uint64_t west = period - east;
  const std::uint64_t south = period - north;
  const std::uint64_t blended = DensityNode(rule, run_seed, node_x, node_y) * west * south +
                                DensityNode(rule, run_seed, node_x + 1, node_y) * east * south +
                                DensityNode(rule, run_seed, node_x, node_y + 1) * west * north +
                                DensityNode(rule, run_seed, node_x + 1, node_y + 1) * east * north;
  return static_cast<std::uint32_t>(blended / (period * period));
}

TerrainChunk GenerateTerrainChunk(const TerrainRule& rule, std::uint64_t run_seed,
                                  std::int32_t chunk_x, std::int32_t chunk_y) {
  TerrainChunk chunk;
  chunk.chunk_x = chunk_x;
  chunk.chunk_y = chunk_y;
  chunk.passable.fill(1U);
  const std::int64_t chunk_west = std::int64_t{chunk_x} * kTerrainChunkCells;
  const std::int64_t chunk_south = std::int64_t{chunk_y} * kTerrainChunkCells;
  const bool landmark_chunk = CarriesLandmark(rule, run_seed, chunk_x, chunk_y);
  // Les chemins d'abord : ils décident où une pièce ordinaire ne se pose pas.
  PaintPaths(rule, run_seed, chunk);
  const std::vector<std::uint32_t> ordinary = WeightsOf(rule.pieces, false);
  const std::vector<std::uint32_t> landmarks = WeightsOf(rule.pieces, true);
  core::Pcg32 random(TerrainChunkSeed(run_seed, chunk_x, chunk_y), kTerrainStream);
  for (std::int32_t slot_y = 0; slot_y < kTerrainSlotsPerSide; ++slot_y) {
    for (std::int32_t slot_x = 0; slot_x < kTerrainSlotsPerSide; ++slot_x) {
      const std::int32_t slot_east = slot_x * kTerrainSlotCells;
      const std::int32_t slot_north = slot_y * kTerrainSlotCells;
      const std::int64_t west = chunk_west + slot_east;
      const std::int64_t south = chunk_south + slot_north;
      const std::uint32_t density =
          TerrainDensityPerMille(rule, run_seed, west + kSlotCentreCells, south + kSlotCentreCells);
      const bool occupied = random.Chance(density, kTerrainPerMille);
      const Draw draw = DrawPiece(random, rule.pieces, ordinary, kTerrainSlotCells);
      const bool cleared = DistanceFromStart(west, south, west + kTerrainSlotCells - 1,
                                             south + kTerrainSlotCells - 1) < rule.clearing_cells;
      if (occupied && !cleared && !landmark_chunk) {
        Place(chunk, rule.pieces, draw, slot_east, slot_north);
      }
    }
  }
  // Avec des chemins, le repère se cale contre sa place : il tient dans son site.
  if (HasPaths(rule)) {
    const Draw landmark = DrawPiece(random, rule.pieces, landmarks,
                                    kTerrainLandmarkSiteCells + 2 * kTerrainBorderCells);
    if (landmark_chunk) {
      PlaceBesidePlaza(chunk, rule.pieces, landmark,
                       PlazaIn(run_seed, TerrainCell{.x = chunk_x, .y = chunk_y}));
    }
    return chunk;
  }
  const Draw landmark = DrawPiece(random, rule.pieces, landmarks, kTerrainChunkCells);
  if (landmark_chunk) Place(chunk, rule.pieces, landmark, 0, 0);
  return chunk;
}

bool TerrainPassableAt(const TerrainRule& rule, std::uint64_t run_seed, std::int32_t x,
                       std::int32_t y) {
  if (x < 0 || y < 0 || x >= kTerrainWorldCells || y >= kTerrainWorldCells) return false;
  const std::int32_t chunk_x = TerrainChunkOf(x);
  const std::int32_t chunk_y = TerrainChunkOf(y);
  return GenerateTerrainChunk(rule, run_seed, chunk_x, chunk_y)
      .Passable(x - chunk_x * kTerrainChunkCells, y - chunk_y * kTerrainChunkCells);
}

}  // namespace ro::sim
