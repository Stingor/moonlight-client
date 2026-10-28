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

// La distance, en cases, du point au rectangle de la terrasse ; nulle dedans.
[[nodiscard]] double DistanceToPad(const GroundReliefPad& pad, double east, double north) {
  const double dx = std::max({pad.west - east, 0.0, east - pad.east});
  const double dy = std::max({pad.south - north, 0.0, north - pad.north});
  return std::hypot(dx, dy);
}

// LA DISTANCE ARRONDIE : une parabole sur `kReliefPadRoundCells`, puis la
// distance moins sa demi-largeur. Elle ne monte jamais plus vite que la
// distance, et sa pente part de zéro au bord de la terrasse.
[[nodiscard]] double RoundedDistance(double distance) {
  if (distance < kReliefPadRoundCells) {
    return distance * distance / (2.0 * kReliefPadRoundCells);
  }
  return distance - kReliefPadRoundCells / 2.0;
}

// LA BRIDE de l'écart `gap` sous la borne `bound` ≥ 0 : l'écart tel quel
// jusqu'à `kReliefPadKneeStart` fois la borne, puis un genou parabolique qui
// rejoint la borne, à pente nulle, en (2 − kReliefPadKneeStart) fois la borne.
[[nodiscard]] double BridleGap(double gap, double bound) {
  if (bound <= 0.0) return 0.0;
  const double knee_end = 2.0 - kReliefPadKneeStart;
  const double share = std::abs(gap) / bound;
  // En deçà du genou, l'écart reste tel quel, au bit près : une terrasse
  // lointaine ne change rien, que la fenêtre la lise ou non.
  if (share <= kReliefPadKneeStart) return gap;
  double kept = 1.0;
  if (share < knee_end) {
    const double past = share - kReliefPadKneeStart;
    kept = share - past * past / (2.0 * (knee_end - kReliefPadKneeStart));
  }
  return std::copysign(kept * bound, gap);
}

// L'éclairement d'une surface, canal par canal, sous l'incidence `incidence` :
// l'ambiante, plus la diffuse que le soleil y pose. Sans la saturation du
// nuanceur, qui rendrait toute pente au soleil égale au plat.
[[nodiscard]] glm::vec3 GroundLightUnder(float incidence, const GroundPlanLight& light) {
  return light.ambient + std::max(incidence, 0.0F) * light.diffuse;
}

// Le centre d'une case, compté depuis son coin.
constexpr double kHalfCell = 0.5;
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

std::optional<GroundReliefPad> GroundReliefPadOf(const ro::sim::TerrainRule& rule,
                                                 std::uint64_t run_seed,
                                                 const ro::sim::TerrainPieceRule& piece,
                                                 const ro::sim::PlacedPiece& placed) {
  if (!piece.landmark) return std::nullopt;
  const ro::sim::FootprintSize size = ro::sim::FootprintSizeOf(piece, placed.quarter);
  GroundReliefPad pad{.west = static_cast<double>(placed.x),
                      .south = static_cast<double>(placed.y),
                      .east = static_cast<double>(placed.x + size.width),
                      .north = static_cast<double>(placed.y + size.height)};
  // LA PLACE DU REPÈRE, quand la règle a des chemins : celle de sa cellule.
  const ro::sim::TerrainPlaza plaza = ro::sim::TerrainPathPlaza(
      rule, run_seed, ro::sim::TerrainPathCellOf(rule, ro::sim::TerrainChunkOf(placed.x)),
      ro::sim::TerrainPathCellOf(rule, ro::sim::TerrainChunkOf(placed.y)));
  if (plaza.exists) {
    const double radius = static_cast<double>(ro::sim::kTerrainPlazaCells) / 2.0;
    const double centre_east = static_cast<double>(plaza.centre.x) + kHalfCell;
    const double centre_north = static_cast<double>(plaza.centre.y) + kHalfCell;
    pad.west = std::min(pad.west, centre_east - radius);
    pad.east = std::max(pad.east, centre_east + radius);
    pad.south = std::min(pad.south, centre_north - radius);
    pad.north = std::max(pad.north, centre_north + radius);
  }
  pad.west -= kReliefPadMarginCells;
  pad.south -= kReliefPadMarginCells;
  pad.east += kReliefPadMarginCells;
  pad.north += kReliefPadMarginCells;
  pad.height = GroundHillsAt(run_seed, (pad.west + pad.east) / 2.0, (pad.south + pad.north) / 2.0);
  return pad;
}

void SortReliefPads(std::vector<GroundReliefPad>& pads) {
  std::ranges::sort(pads, [](const GroundReliefPad& left, const GroundReliefPad& right) {
    return left.west != right.west ? left.west < right.west : left.south < right.south;
  });
}

float GroundReliefAt(std::uint64_t run_seed, double world_east, double world_north,
                     std::span<const GroundReliefPad> pads) {
  double height = GroundHillsAt(run_seed, world_east, world_north);
  for (const GroundReliefPad& pad : pads) {
    const double bound = kGroundAxisSlopeCap *
                         RoundedDistance(DistanceToPad(pad, world_east, world_north)) *
                         kGroundReliefCellUnits;
    height = pad.height + BridleGap(height - pad.height, bound);
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
