#include "survivors/arena_ground_relief.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>

namespace moonclient {
namespace {

// ── LE MÉLANGE ENTIER des graines visuelles : un pas du finaliseur SplitMix64
// par composante, aux constantes publiées.
constexpr std::uint64_t kGoldenGamma = 0x9E37'79B9'7F4A'7C15ULL;
constexpr std::uint64_t kMixFirst = 0xBF58'476D'1CE4'E5B9ULL;
constexpr std::uint64_t kMixSecond = 0x94D0'49BB'1331'11EBULL;
constexpr unsigned kMixShiftFirst = 30;
constexpr unsigned kMixShiftSecond = 27;
constexpr unsigned kMixShiftLast = 31;
constexpr double kUnitScale = 1.0 / 18446744073709551616.0;  // 2^-64

// Les sels des deux ondulations, en ASCII.
constexpr std::uint64_t kHillSalt = 0x4849'4C4C'5300'0000ULL;   // « HILLS »
constexpr std::uint64_t kSwellSalt = 0x5357'454C'4C00'0000ULL;  // « SWELL »

[[nodiscard]] double Lattice(std::uint64_t seed, std::uint64_t salt, std::int64_t x,
                             std::int64_t y) noexcept {
  return static_cast<double>(VisualMix(seed, salt, x, y)) * kUnitScale;
}

[[nodiscard]] double Smooth(double t) noexcept { return t * t * (3.0 - 2.0 * t); }
// Sa dérivée.
[[nodiscard]] double SmoothSlope(double t) noexcept { return 6.0 * t * (1.0 - t); }
// Les directions des nœuds, d'un huitième de tour en huitième de tour.
constexpr double kDiagonal = std::numbers::sqrt2 / 2.0;
constexpr std::array<glm::dvec2, kGradientDirections> kGradients = {
    glm::dvec2(1.0, 0.0),  glm::dvec2(kDiagonal, kDiagonal),
    glm::dvec2(0.0, 1.0),  glm::dvec2(-kDiagonal, kDiagonal),
    glm::dvec2(-1.0, 0.0), glm::dvec2(-kDiagonal, -kDiagonal),
    glm::dvec2(0.0, -1.0), glm::dvec2(kDiagonal, -kDiagonal)};

// Le centre d'une case, compté depuis son coin.
constexpr double kHalfCell = 0.5;

// La distance, en cases, entre deux rectangles de terrasse ; nulle s'ils se
// touchent.
[[nodiscard]] double DistanceBetweenPads(const GroundReliefPad& one, const GroundReliefPad& other) {
  const double dx = std::max({one.west - other.east, 0.0, other.west - one.east});
  const double dy = std::max({one.south - other.north, 0.0, other.south - one.north});
  return std::hypot(dx, dy);
}

// Vrai quand la bride de borne `bound` ≥ 0 laisse l'écart `gap` tel quel : en
// deçà du genou. Une bride qui ne touche rien ne réécrit pas la hauteur.
[[nodiscard]] bool BridleKeeps(double gap, double bound) {
  return std::abs(gap) <= kReliefPadKneeStart * bound;
}

// LA BRIDE de l'écart `gap` sous la borne `bound` ≥ 0, au-delà du genou : un
// genou parabolique qui part de `kReliefPadKneeStart` fois la borne et la
// rejoint, à pente nulle, en (2 − kReliefPadKneeStart) fois la borne.
[[nodiscard]] double BridleGap(double gap, double bound) {
  if (bound <= 0.0) return 0.0;
  const double knee_end = 2.0 - kReliefPadKneeStart;
  const double share = std::abs(gap) / bound;
  double kept = 1.0;
  if (share < knee_end) {
    const double past = share - kReliefPadKneeStart;
    kept = share - past * past / (2.0 * (knee_end - kReliefPadKneeStart));
  }
  return std::copysign(kept * bound, gap);
}

// ── OÙ SE TROUVENT LES TERRASSES.
//
// Une terrasse agit à `kReliefPadReachCells` de son bord, et déborde de son
// morceau d'au plus `kReliefPadOverhangCells` : les terrasses qui agissent sur
// un rectangle sont dans les morceaux à `kPadActingRingChunks` au plus de lui.
constexpr std::int32_t kPadActingRingChunks = 2;
static_assert(kReliefPadReachCells + kReliefPadOverhangCells <=
                  static_cast<double>(kPadActingRingChunks * ro::sim::kTerrainChunkCells),
              "une terrasse hors du tour agirait sur le rectangle");
// LE GROUPE D'UNE TERRASSE FUSIONNÉE se lit entier : deux terrasses à moins de
// `kReliefPadFuseCells` sont dans des morceaux voisins (côté ou coin), et un
// morceau à repère est seul dans son carré de repères. Un carré d'au moins
// `kReliefLeastLandmarkPeriodChunks` de côté a ses morceaux de coin et ses
// morceaux de bord distincts : un morceau de bord ne touche que le carré d'en
// face, un morceau de coin les trois carrés de son coin. Un groupe tient donc
// dans les quatre carrés d'un coin, à deux morceaux au plus de ce coin : dans
// quatre morceaux de côté, si bien que chaque terrasse du groupe est à
// `kReliefPadFusedSpanChunks` morceaux au plus des autres.
constexpr std::int32_t kPadReadRingChunks = kPadActingRingChunks + kReliefPadFusedSpanChunks;

// UNE PLACE, son site entier et sa marge : la place est un disque de
// `kTerrainPlazaCells` autour de la case du centre ; le site, un carré de
// `kTerrainLandmarkSiteCells` dont le coin le plus proche du centre s'écarte de
// `kTerrainSiteOffsetCells` cases de la case du centre.
constexpr double kPlazaRadiusCells = static_cast<double>(ro::sim::kTerrainPlazaCells) / 2.0;
constexpr double kSiteOffsetCells = ro::sim::kTerrainSiteOffsetCells;
constexpr double kSiteCells = ro::sim::kTerrainLandmarkSiteCells;
// Le centre de la place est celui de son morceau : la terrasse y tient.
constexpr double kPlazaFromChunkCells = static_cast<double>(ro::sim::kTerrainChunkCells) / 2.0;
static_assert(kPlazaFromChunkCells + kSiteOffsetCells + kSiteCells + kReliefPadMarginCells <=
              static_cast<double>(ro::sim::kTerrainChunkCells) + kReliefPadOverhangCells);
static_assert(kPlazaFromChunkCells - kSiteOffsetCells + 1.0 - kSiteCells - kReliefPadMarginCells >=
              -kReliefPadOverhangCells);

[[nodiscard]] GroundReliefPad Widened(GroundReliefPad pad) {
  pad.west -= kReliefPadMarginCells;
  pad.south -= kReliefPadMarginCells;
  pad.east += kReliefPadMarginCells;
  pad.north += kReliefPadMarginCells;
  return pad;
}

// LA TERRASSE D'UNE PLACE : le rectangle de son disque et de son site.
[[nodiscard]] GroundReliefPad PadOfPlaza(const ro::sim::TerrainPlaza& plaza) {
  const auto centre_x = static_cast<double>(plaza.centre.x);
  const auto centre_y = static_cast<double>(plaza.centre.y);
  const double site_west = plaza.site_east ? centre_x + kSiteOffsetCells
                                           : centre_x - kSiteOffsetCells + 1.0 - kSiteCells;
  const double site_south = plaza.site_north ? centre_y + kSiteOffsetCells
                                             : centre_y - kSiteOffsetCells + 1.0 - kSiteCells;
  const double disc_west = centre_x + kHalfCell - kPlazaRadiusCells;
  const double disc_south = centre_y + kHalfCell - kPlazaRadiusCells;
  const double disc_east = centre_x + kHalfCell + kPlazaRadiusCells;
  const double disc_north = centre_y + kHalfCell + kPlazaRadiusCells;
  return Widened({.west = std::min(disc_west, site_west),
                  .south = std::min(disc_south, site_south),
                  .east = std::max(disc_east, site_west + kSiteCells),
                  .north = std::max(disc_north, site_south + kSiteCells)});
}

// LA TERRASSE D'UNE PIÈCE posée sans chemin : son emprise.
[[nodiscard]] GroundReliefPad PadOfPiece(const ro::sim::TerrainPieceRule& piece,
                                         const ro::sim::PlacedPiece& placed) {
  const ro::sim::FootprintSize size = ro::sim::FootprintSizeOf(piece, placed.quarter);
  return Widened({.west = static_cast<double>(placed.x),
                  .south = static_cast<double>(placed.y),
                  .east = static_cast<double>(placed.x + size.width),
                  .north = static_cast<double>(placed.y + size.height)});
}

// Les morceaux lus, bornes comprises.
struct ChunkSpan {
  std::int64_t first_x = 0;
  std::int64_t first_y = 0;
  std::int64_t last_x = 0;
  std::int64_t last_y = 0;
};

// Les terrasses des morceaux `span`, sans hauteur.
[[nodiscard]] std::vector<GroundReliefPad> PadsInChunks(const ro::sim::TerrainRule& rule,
                                                        std::uint64_t run_seed,
                                                        const ChunkSpan& span) {
  std::vector<GroundReliefPad> pads;
  if (rule.landmark_period_chunks <= 0) return pads;
  if (rule.path_period_chunks > 0 && rule.path_width_cells > 0) {
    // Une place par cellule de chemins au plus, au centre de son morceau.
    for (std::int64_t cell_y = ro::sim::TerrainPathCellOf(rule, span.first_y);
         cell_y <= ro::sim::TerrainPathCellOf(rule, span.last_y); ++cell_y) {
      for (std::int64_t cell_x = ro::sim::TerrainPathCellOf(rule, span.first_x);
           cell_x <= ro::sim::TerrainPathCellOf(rule, span.last_x); ++cell_x) {
        const ro::sim::TerrainPlaza plaza =
            ro::sim::TerrainPathPlaza(rule, run_seed, cell_x, cell_y);
        if (plaza.exists) pads.push_back(PadOfPlaza(plaza));
      }
    }
    return pads;
  }
  for (std::int64_t chunk_y = span.first_y; chunk_y <= span.last_y; ++chunk_y) {
    for (std::int64_t chunk_x = span.first_x; chunk_x <= span.last_x; ++chunk_x) {
      const ro::sim::TerrainChunk chunk = ro::sim::GenerateTerrainChunk(
          rule, run_seed, static_cast<std::int32_t>(chunk_x), static_cast<std::int32_t>(chunk_y));
      for (const ro::sim::PlacedPiece& placed : chunk.pieces) {
        for (const ro::sim::TerrainPieceRule& piece : rule.pieces) {
          if (piece.piece_id == placed.piece_id && piece.landmark) {
            pads.push_back(PadOfPiece(piece, placed));
          }
        }
      }
    }
  }
  return pads;
}

// LA FUSION : les groupes de terrasses à moins de `kReliefPadFuseCells` de
// proche en proche prennent la hauteur des collines au centre du rectangle
// qui les englobe, bornée à `kReliefPadMostUnits`.
void FusePads(std::vector<GroundReliefPad>& pads, std::uint64_t run_seed) {
  std::vector<std::size_t> group(pads.size());
  for (std::size_t rank = 0; rank < pads.size(); ++rank) group[rank] = rank;
  const auto root = [&group](std::size_t rank) {
    while (group[rank] != rank) rank = group[rank];
    return rank;
  };
  for (std::size_t one = 0; one < pads.size(); ++one) {
    for (std::size_t other = one + 1; other < pads.size(); ++other) {
      if (DistanceBetweenPads(pads[one], pads[other]) < kReliefPadFuseCells) {
        group[root(other)] = root(one);
      }
    }
  }
  for (std::size_t leader = 0; leader < pads.size(); ++leader) {
    if (root(leader) != leader) continue;
    GroundReliefPad bounds = pads[leader];
    for (std::size_t rank = 0; rank < pads.size(); ++rank) {
      if (root(rank) != leader) continue;
      bounds.west = std::min(bounds.west, pads[rank].west);
      bounds.south = std::min(bounds.south, pads[rank].south);
      bounds.east = std::max(bounds.east, pads[rank].east);
      bounds.north = std::max(bounds.north, pads[rank].north);
    }
    const double height = std::clamp(GroundHillsAt(run_seed, (bounds.west + bounds.east) / 2.0,
                                                   (bounds.south + bounds.north) / 2.0),
                                     -kReliefPadMostUnits, kReliefPadMostUnits);
    for (std::size_t rank = 0; rank < pads.size(); ++rank) {
      if (root(rank) == leader) pads[rank].height = height;
    }
  }
}

// L'éclairement d'une surface, canal par canal, sous l'incidence `incidence` :
// l'ambiante, plus la diffuse que le soleil y pose. Sans la saturation du
// nuanceur, qui rendrait toute pente au soleil égale au plat.
[[nodiscard]] glm::vec3 GroundLightUnder(float incidence, const GroundPlanLight& light) {
  return light.ambient + std::max(incidence, 0.0F) * light.diffuse;
}

// La plus faible lumière du sol plat que l'on divise encore.
constexpr float kDarkestFlatLight = 1.0e-4F;

}  // namespace

std::uint64_t VisualFinalize(std::uint64_t value) noexcept {
  value = (value ^ (value >> kMixShiftFirst)) * kMixFirst;
  value = (value ^ (value >> kMixShiftSecond)) * kMixSecond;
  return value ^ (value >> kMixShiftLast);
}

std::uint64_t VisualMix(std::uint64_t seed, std::uint64_t salt, std::int64_t x,
                        std::int64_t y) noexcept {
  std::uint64_t value = VisualFinalize(seed + kGoldenGamma);
  value = VisualFinalize(value ^ (salt + kGoldenGamma));
  value = VisualFinalize(value ^ (static_cast<std::uint64_t>(x) + kGoldenGamma));
  return VisualFinalize(value ^ (static_cast<std::uint64_t>(y) + kGoldenGamma));
}

double VisualValueNoise(std::uint64_t seed, std::uint64_t salt, double east, double north,
                        double period_cells) noexcept {
  const double x = east / period_cells;
  const double y = north / period_cells;
  const double x0 = std::floor(x);
  const double y0 = std::floor(y);
  const auto ix = static_cast<std::int64_t>(x0);
  const auto iy = static_cast<std::int64_t>(y0);
  const double tx = Smooth(x - x0);
  const double ty = Smooth(y - y0);
  const double south = std::lerp(Lattice(seed, salt, ix, iy), Lattice(seed, salt, ix + 1, iy), tx);
  const double north_edge =
      std::lerp(Lattice(seed, salt, ix, iy + 1), Lattice(seed, salt, ix + 1, iy + 1), tx);
  return std::lerp(south, north_edge, ty);
}

GradientCell GradientNoiseIn(const std::array<std::uint32_t, 4>& directions, double tx,
                             double ty) noexcept {
  const double fade_x = Smooth(tx);
  const double fade_y = Smooth(ty);
  const double slope_x = SmoothSlope(tx);
  const double slope_y = SmoothSlope(ty);
  GradientCell cell;
  for (std::size_t corner = 0; corner < directions.size(); ++corner) {
    const double east_side = static_cast<double>(corner % 2);
    const double north_side = static_cast<double>(corner / 2);
    const glm::dvec2& gradient = kGradients[directions[corner] % kGradientDirections];
    const double gradient_x = gradient.x;
    const double gradient_y = gradient.y;
    const double dot = gradient_x * (tx - east_side) + gradient_y * (ty - north_side);
    const double weight_x = east_side != 0.0 ? fade_x : 1.0 - fade_x;
    const double weight_y = north_side != 0.0 ? fade_y : 1.0 - fade_y;
    const double turn_x = east_side != 0.0 ? slope_x : -slope_x;
    const double turn_y = north_side != 0.0 ? slope_y : -slope_y;
    cell.value += weight_x * weight_y * dot;
    cell.along_east += turn_x * weight_y * dot + weight_x * weight_y * gradient_x;
    cell.along_north += weight_x * turn_y * dot + weight_x * weight_y * gradient_y;
  }
  return cell;
}

double VisualGradientNoise(std::uint64_t seed, std::uint64_t salt, double east, double north,
                           double period_cells) noexcept {
  const double x = east / period_cells;
  const double y = north / period_cells;
  const double x0 = std::floor(x);
  const double y0 = std::floor(y);
  const auto ix = static_cast<std::int64_t>(x0);
  const auto iy = static_cast<std::int64_t>(y0);
  const auto direction = [seed, salt](std::int64_t node_x, std::int64_t node_y) {
    return static_cast<std::uint32_t>(VisualMix(seed, salt, node_x, node_y) % kGradientDirections);
  };
  return GradientNoiseIn({direction(ix, iy), direction(ix + 1, iy), direction(ix, iy + 1),
                          direction(ix + 1, iy + 1)},
                         x - x0, y - y0)
      .value;
}

double GroundHillsAt(std::uint64_t run_seed, double world_east, double world_north) noexcept {
  const double hill =
      VisualGradientNoise(run_seed, kHillSalt, world_east, world_north, kReliefHillPeriodCells);
  const double swell =
      VisualGradientNoise(run_seed, kSwellSalt, world_east, world_north, kReliefSwellPeriodCells);
  // Centré sur zéro : une moitié creuse, l'autre bombe.
  return (kReliefHillUnits * hill + kReliefSwellUnits * swell) / kGradientNoiseMost;
}

std::vector<GroundReliefPad> GroundReliefPadsAround(const ro::sim::TerrainRule& rule,
                                                    std::uint64_t run_seed, double west,
                                                    double south, double east, double north) {
  const auto chunk_of = [](double cell) {
    return static_cast<std::int64_t>(
        ro::sim::TerrainChunkOf(static_cast<std::int32_t>(std::floor(cell))));
  };
  const ChunkSpan span{.first_x = chunk_of(west) - kPadReadRingChunks,
                       .first_y = chunk_of(south) - kPadReadRingChunks,
                       .last_x = chunk_of(east) + kPadReadRingChunks,
                       .last_y = chunk_of(north) + kPadReadRingChunks};
  std::vector<GroundReliefPad> pads = PadsInChunks(rule, run_seed, span);
  FusePads(pads, run_seed);
  // Seules restent celles qui agissent : leur groupe a été lu entier.
  const GroundReliefPad area{.west = west, .south = south, .east = east, .north = north};
  std::erase_if(pads, [&area](const GroundReliefPad& pad) {
    return DistanceBetweenPads(pad, area) > kReliefPadReachCells;
  });
  std::ranges::sort(pads, [](const GroundReliefPad& left, const GroundReliefPad& right) {
    return left.west != right.west ? left.west < right.west : left.south < right.south;
  });
  return pads;
}

float GroundReliefAt(std::uint64_t run_seed, double world_east, double world_north,
                     std::span<const GroundReliefPad> pads) {
  double height = GroundHillsAt(run_seed, world_east, world_north);
  for (const GroundReliefPad& pad : pads) {
    // Au-delà de sa portée, une bride ne touche rien : inutile de la calculer.
    const double dx = std::max({pad.west - world_east, 0.0, world_east - pad.east});
    const double dy = std::max({pad.south - world_north, 0.0, world_north - pad.north});
    if (dx > kReliefPadReachCells || dy > kReliefPadReachCells) continue;
    const double bound = ReliefBridleBoundUnits(std::sqrt(dx * dx + dy * dy));
    const double gap = height - pad.height;
    if (BridleKeeps(gap, bound)) continue;
    height = pad.height + BridleGap(gap, bound);
  }
  return static_cast<float>(height);
}

glm::dvec2 GroundReliefSlopeAt(std::uint64_t run_seed, double world_east, double world_north,
                               std::span<const GroundReliefPad> pads) {
  const double probe = kReliefSlopeProbeCells;
  const double across = 2.0 * probe * kGroundReliefCellUnits;
  const double east = GroundReliefAt(run_seed, world_east + probe, world_north, pads);
  const double west = GroundReliefAt(run_seed, world_east - probe, world_north, pads);
  const double north = GroundReliefAt(run_seed, world_east, world_north + probe, pads);
  const double south = GroundReliefAt(run_seed, world_east, world_north - probe, pads);
  return {(east - west) / across, (north - south) / across};
}

glm::vec3 GroundNormalOfSlope(glm::dvec2 slope) {
  // La hauteur du fichier descend : le monde monte de −pente. Sa normale est
  // (−∂y/∂x, 1, −∂y/∂z), le sud vers les z croissants.
  return glm::normalize(glm::vec3(static_cast<float>(slope.x), 1.0F, static_cast<float>(-slope.y)));
}

glm::vec3 GroundCellNormal(const std::array<float, 4>& corner_heights) {
  const double side = 2.0 * kGroundReliefCellUnits;
  const double south_west = corner_heights[0];
  const double south_east = corner_heights[1];
  const double north_west = corner_heights[2];
  const double north_east = corner_heights[3];
  return GroundNormalOfSlope({(south_east + north_east - south_west - north_west) / side,
                              (north_west + north_east - south_west - south_east) / side});
}

glm::vec3 GroundReliefNormalAt(std::uint64_t run_seed, double world_east, double world_north,
                               std::span<const GroundReliefPad> pads) {
  return GroundNormalOfSlope(GroundReliefSlopeAt(run_seed, world_east, world_north, pads));
}

float GroundSlopeLight(glm::vec3 normal, const GroundPlanLight& light) {
  const glm::vec3 toward_sun = glm::normalize(light.toward_sun);
  const glm::vec3 flat = GroundLightUnder(toward_sun.y, light);
  const glm::vec3 sloped = GroundLightUnder(glm::dot(glm::normalize(normal), toward_sun), light);
  float kept = 0.0F;
  const glm::length_t channels = glm::vec3::length();
  for (glm::length_t channel = 0; channel < channels; ++channel) {
    kept += sloped[channel] / std::max(flat[channel], kDarkestFlatLight);
  }
  return kept / static_cast<float>(channels);
}

float GroundSlopeShade(glm::vec3 normal, const GroundPlanLight& light) {
  const float marked = 1.0F + kReliefLightGain * (GroundSlopeLight(normal, light) - 1.0F);
  return std::clamp(kGroundFlatShade * marked, kDarkestSlopeShade, 1.0F);
}

float GroundSlopeActorShade(glm::vec3 normal, const GroundPlanLight& light) {
  return std::min(GroundSlopeShade(normal, light) / kGroundFlatShade, 1.0F);
}

}  // namespace moonclient
