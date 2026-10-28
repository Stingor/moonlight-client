#include "survivors/arena_ground_plan.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <set>
#include <span>
#include <utility>

#include "ro/core/pcg32.h"

#include "survivors/arena_ground_paths.h"

namespace moonclient {
namespace {

using ro::sim::kTerrainChunkCells;

// ── LE MÉLANGE ENTIER des graines visuelles : un pas du finaliseur SplitMix64
// par composante, aux constantes publiées.
constexpr std::uint64_t kGoldenGamma = 0x9E37'79B9'7F4A'7C15ULL;
constexpr std::uint64_t kMixFirst = 0xBF58'476D'1CE4'E5B9ULL;
constexpr std::uint64_t kMixSecond = 0x94D0'49BB'1331'11EBULL;
constexpr unsigned kMixShiftFirst = 30;
constexpr unsigned kMixShiftSecond = 27;
constexpr unsigned kMixShiftLast = 31;

// Les sels visuels, en ASCII : rien de ce qu'ils tirent ne décide une case.
constexpr std::uint64_t kVisualSalt = 0x5649'5355'414C'0000ULL;  // « VISUAL »
constexpr std::uint64_t kReliefSalt = 0x5245'4C49'4546'0000ULL;  // « RELIEF »
constexpr std::uint64_t kTintSalt = 0x5449'4E54'0000'0000ULL;    // « TINT »
constexpr std::uint64_t kPatchSalt = 0x5041'5443'4800'0000ULL;   // « PATCH »
constexpr std::uint64_t kVergeSalt = 0x5645'5247'4500'0000ULL;   // « VERGE »
// Le flux du `Pcg32` visuel d'un morceau.
constexpr std::uint64_t kVisualStream = 0x0000'0056'4953'4955ULL;

[[nodiscard]] std::uint64_t Finalize(std::uint64_t value) noexcept {
  value = (value ^ (value >> kMixShiftFirst)) * kMixFirst;
  value = (value ^ (value >> kMixShiftSecond)) * kMixSecond;
  return value ^ (value >> kMixShiftLast);
}

[[nodiscard]] std::uint64_t Mix(std::uint64_t seed, std::uint64_t salt, std::int64_t x,
                                std::int64_t y) noexcept {
  std::uint64_t value = Finalize(seed + kGoldenGamma);
  value = Finalize(value ^ (salt + kGoldenGamma));
  value = Finalize(value ^ (static_cast<std::uint64_t>(x) + kGoldenGamma));
  return Finalize(value ^ (static_cast<std::uint64_t>(y) + kGoldenGamma));
}

// ── LE BRUIT DE VALEUR : un nœud tous les `period` coins de case, interpolé en
// douceur. Une fonction du coin du monde seul.
constexpr double kUnitScale = 1.0 / 18446744073709551616.0;  // 2^-64

[[nodiscard]] double Lattice(std::uint64_t seed, std::uint64_t salt, std::int64_t x,
                             std::int64_t y) noexcept {
  return static_cast<double>(Mix(seed, salt, x, y)) * kUnitScale;
}

[[nodiscard]] double Smooth(double t) noexcept { return t * t * (3.0 - 2.0 * t); }

// Dans [0, 1).
[[nodiscard]] double ValueNoise(std::uint64_t seed, std::uint64_t salt, double east, double north,
                                double period) noexcept {
  const double x = east / period;
  const double y = north / period;
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

// ── LE RELIEF (Q3) : deux ondulations, l'une ample et lente, l'autre courte.
// Un cube fait dix unités ; un dénivelé de quelques unités ne cache rien sous
// la caméra de l'arène.
constexpr double kReliefSlowPeriodCells = 28.0;
constexpr double kReliefSlowUnits = 5.0;
constexpr double kReliefQuickPeriodCells = 11.0;
constexpr double kReliefQuickUnits = 1.5;
constexpr std::uint64_t kQuickReliefSalt = kReliefSalt + 1;

// ── LE SOL PEINT.
// Le tirage d'un motif : sa chance, son choix et son quart de tour se lisent
// sur trois tranches du même mélange.
constexpr unsigned kPickShift = 16;
constexpr unsigned kTurnShift = 40;
// La teinte : un vert plus sombre mêlé par taches lentes.
constexpr double kTintPeriodCells = 23.0;
constexpr double kTintStrength = 0.55;
constexpr std::array<double, 3> kTintBgr = {150.0, 205.0, 185.0};
constexpr double kFullChannel = 255.0;
constexpr std::uint8_t kOpaque = 255;

// ── L'OMBRE CUITE : un disque sous chaque modèle, décalé à l'opposé du soleil
// d'une part de son rayon, plein jusqu'à `kShadeCore` de ce rayon.
constexpr std::uint8_t kFullDay = 255;
constexpr float kShadeOffset = 0.6F;
constexpr float kShadeCore = 0.55F;
constexpr std::int32_t kLightmapInnerTexels = 6;
constexpr float kHalfTexel = 0.5F;
// Un soleil presque au zénith ne décale pas l'ombre.
constexpr float kLeastHorizontalSun = 1.0e-3F;

// ── LES POSES.
constexpr std::uint32_t kFullTurnDegrees = 360;
constexpr float kQuarterTurnDegrees = 90.0F;
// Un modèle tiré grandit ou rapetisse d'au plus cette part.
constexpr float kScaleJitter = 0.12F;
constexpr float kUnitFromDraw = 1.0F / 4294967296.0F;  // 2^-32
// Un semis se pose dans sa case, loin de son bord.
constexpr float kSeedInset = 0.2F;
constexpr float kSeedSpan = 0.6F;

constexpr std::int32_t kWindowChunks = kGroundWindowCells / kTerrainChunkCells;
// UN TOUR DE MORCEAUX AUTOUR DE LA FENÊTRE se lit aussi : leurs pièces jettent
// de l'ombre et de la terre jusque dans la fenêtre, comme dans la voisine.
constexpr std::int32_t kRingChunks = 1;
// Le plus de choix qu'une place ou un semis départage.
constexpr std::size_t kMostChoices = 16;
constexpr std::size_t kCellCount =
    static_cast<std::size_t>(kGroundWindowCells) * static_cast<std::size_t>(kGroundWindowCells);
constexpr std::size_t kCubeCount =
    static_cast<std::size_t>(kGroundWindowCubes) * static_cast<std::size_t>(kGroundWindowCubes);
constexpr std::int32_t kCornerSide = kGroundWindowCells + 1;
// Les cases lues de la fenêtre et de son tour de morceaux, et le décalage
// d'une case de la fenêtre dans ce carré.
constexpr std::int32_t kRingCells = kRingChunks * kTerrainChunkCells;
constexpr std::int32_t kRingSide = kGroundWindowCells + 2 * kRingCells;
// UN COIN DE CUBE EST DE TERRE quand au moins tant des quatre cases qui le
// touchent sont de chemin.
constexpr std::int32_t kDirtCornerLeastCells = 2;
constexpr std::int32_t kCubeCorners = kGroundWindowCubes + 1;
constexpr std::uint8_t kNoTexture = 0xFF;
constexpr std::uint8_t kQuarterTurns = ro::sim::kTerrainQuarterTurns;
constexpr std::uint8_t kHalfTurn = kQuarterTurns / 2;

[[nodiscard]] float UnitOf(std::uint32_t draw) noexcept {
  return static_cast<float>(draw) * kUnitFromDraw;
}

// Le point (u, v) de l'emprise d'origine, après `quarter` quarts de tour dans
// le sens trigonométrique, compté depuis le coin sud-ouest de l'emprise tournée.
[[nodiscard]] glm::vec2 TurnInFootprint(glm::vec2 point, ro::sim::FootprintSize drawn,
                                        std::uint8_t quarter) noexcept {
  const auto width = static_cast<float>(drawn.width);
  const auto height = static_cast<float>(drawn.height);
  switch (quarter % ro::sim::kTerrainQuarterTurns) {
    case 1:
      return {height - point.y, point.x};
    case 2:
      return {width - point.x, height - point.y};
    case 3:
      return {point.y, width - point.x};
    default:
      return point;
  }
}

// Un tirage pondéré parmi des choix, par un seul `PickWeighted`.
template <typename Choice>
[[nodiscard]] const Choice* Pick(ro::core::Pcg32& random, std::span<const Choice> choices) {
  std::array<std::uint32_t, kMostChoices> weights{};
  const std::size_t count = std::min(choices.size(), weights.size());
  for (std::size_t rank = 0; rank < count; ++rank) weights[rank] = choices[rank].weight;
  const std::size_t picked = random.PickWeighted(std::span(weights.data(), count));
  return picked < count ? &choices[picked] : nullptr;
}

// UN DISQUE D'OMBRE, compté depuis la case du monde `corner` (le coin de sa
// pièce) : la même ombre se calcule au bit près dans toute fenêtre qui la voit.
struct ShadeDisc {
  glm::ivec2 corner{0};
  glm::vec2 center{0.0F};
  float radius = 0.0F;
};

// Ce que la fenêtre se construit en chemin.
class Planner {
 public:
  Planner(const ro::sim::TerrainRule& rule, std::uint64_t run_seed, const BiomeLook& biome,
          GroundWindowOrigin origin, const GroundPlanLight& light)
      : rule_(rule), run_seed_(run_seed), biome_(biome), origin_(origin) {
    const glm::vec2 away(-light.toward_sun.x, light.toward_sun.z);
    if (glm::length(away) > kLeastHorizontalSun) shade_direction_ = glm::normalize(away);
    plan_.origin = origin;
  }

  GroundPlan Build() {
    ReadChunks();
    SowVerges();
    RaiseRelief();
    PaintCubes();
    BakeShade();
    return std::move(plan_);
  }

 private:
  // Les morceaux de la fenêtre, leurs cases, leurs pièces, et les poses qui en
  // découlent, morceau par morceau du sud au nord.
  void ReadChunks() {
    plan_.passable.assign(kCellCount, 0);
    plan_.path.assign(kCellCount, 0);
    const auto ring_area =
        static_cast<std::size_t>(kRingSide) * static_cast<std::size_t>(kRingSide);
    ring_passable_.assign(ring_area, 0);
    ring_path_.assign(ring_area, 0);
    const std::int32_t first_x = ro::sim::TerrainChunkOf(origin_.x_cells);
    const std::int32_t first_y = ro::sim::TerrainChunkOf(origin_.y_cells);
    for (std::int32_t row = -kRingChunks; row < kWindowChunks + kRingChunks; ++row) {
      for (std::int32_t column = -kRingChunks; column < kWindowChunks + kRingChunks; ++column) {
        const ro::sim::TerrainChunk chunk =
            ro::sim::GenerateTerrainChunk(rule_, run_seed_, first_x + column, first_y + row);
        const bool shown = row >= 0 && column >= 0 && row < kWindowChunks && column < kWindowChunks;
        CopyCells(chunk, column, row, shown);
        PoseChunk(chunk, column, row, shown);
      }
    }
  }

  // Les cases du morceau, dans le carré du tour et, s'il se montre, dans la
  // fenêtre.
  void CopyCells(const ro::sim::TerrainChunk& chunk, std::int32_t column, std::int32_t row,
                 bool shown) {
    for (std::int32_t north = 0; north < kTerrainChunkCells; ++north) {
      for (std::int32_t east = 0; east < kTerrainChunkCells; ++east) {
        const std::int32_t x = column * kTerrainChunkCells + east;
        const std::int32_t y = row * kTerrainChunkCells + north;
        const std::uint8_t passable = chunk.Passable(east, north) ? 1U : 0U;
        const std::uint8_t path = chunk.OnPath(east, north) ? 1U : 0U;
        ring_passable_[RingIndex(x, y)] = passable;
        ring_path_[RingIndex(x, y)] = path;
        if (!shown) continue;
        plan_.passable[CellIndex(x, y)] = passable;
        plan_.path[CellIndex(x, y)] = path;
      }
    }
  }

  // Vrai si la case (x, y) de la fenêtre, ou de son tour, est un chemin ; faux
  // au-delà du tour.
  [[nodiscard]] bool PathAt(std::int32_t x, std::int32_t y) const {
    return InRing(x, y) && ring_path_[RingIndex(x, y)] != 0U;
  }
  [[nodiscard]] bool PassableInRing(std::int32_t x, std::int32_t y) const {
    return InRing(x, y) && ring_passable_[RingIndex(x, y)] != 0U;
  }
  [[nodiscard]] static bool InRing(std::int32_t x, std::int32_t y) {
    return x >= -kRingCells && y >= -kRingCells && x < kGroundWindowCells + kRingCells &&
           y < kGroundWindowCells + kRingCells;
  }
  [[nodiscard]] static std::size_t RingIndex(std::int32_t x, std::int32_t y) {
    return static_cast<std::size_t>(y + kRingCells) * static_cast<std::size_t>(kRingSide) +
           static_cast<std::size_t>(x + kRingCells);
  }

  // LE SEMIS DE BORDURE : chaque case libre qui longe un chemin, sans en être
  // et loin de toute case bloquée, tire d'un `Pcg32` propre à la case du monde
  // sa chance, son modèle, son angle et sa place : la même dans toute fenêtre.
  void SowVerges() {
    if (biome_.verge_seeds.empty() || biome_.verge_per_mille == 0) return;
    for (std::int32_t y = 0; y < kGroundWindowCells; ++y) {
      for (std::int32_t x = 0; x < kGroundWindowCells; ++x) {
        if (!AlongPath(x, y)) continue;
        ro::core::Pcg32 random(Mix(run_seed_, kVergeSalt, origin_.x_cells + x, origin_.y_cells + y),
                               kVisualStream);
        // Toujours quatre tirages : la chance, le modèle, l'angle, la place.
        const bool sown = random.Chance(biome_.verge_per_mille, ro::sim::kTerrainPerMille);
        const SeedLook* seed = Pick(random, biome_.verge_seeds);
        const std::uint32_t yaw = random.Below(kFullTurnDegrees);
        const float offset = UnitOf(random.Next());
        if (!sown || seed == nullptr) continue;
        plan_.poses.push_back(
            PlannedPose{.model = seed->model,
                        .east = static_cast<float>(x) + kSeedInset + kSeedSpan * offset,
                        .north = static_cast<float>(y) + kSeedInset + kSeedSpan * (1.0F - offset),
                        .yaw_degrees = static_cast<float>(yaw),
                        .scale = seed->scale,
                        .piece = kSeedPose});
      }
    }
  }

  // Une case libre, hors chemin, qu'un chemin touche par un côté et qu'aucune
  // case bloquée ne touche.
  [[nodiscard]] bool AlongPath(std::int32_t x, std::int32_t y) const {
    if (PathAt(x, y)) return false;
    if (!PathAt(x - 1, y) && !PathAt(x + 1, y) && !PathAt(x, y - 1) && !PathAt(x, y + 1)) {
      return false;
    }
    for (std::int32_t dy = -1; dy <= 1; ++dy) {
      for (std::int32_t dx = -1; dx <= 1; ++dx) {
        if (!PassableInRing(x + dx, y + dy)) return false;
      }
    }
    return true;
  }

  // Les pièces du morceau puis son semis, d'un `Pcg32` propre au morceau : le
  // même quelle que soit la fenêtre qui le montre. Un morceau du tour, qui
  // n'est pas `shown`, ne laisse que l'ombre et la terre de ses pièces.
  void PoseChunk(const ro::sim::TerrainChunk& chunk, std::int32_t column, std::int32_t row,
                 bool shown) {
    ro::core::Pcg32 random(
        Finalize(ro::sim::TerrainChunkSeed(run_seed_, chunk.chunk_x, chunk.chunk_y) ^ kVisualSalt),
        kVisualStream);
    for (const ro::sim::PlacedPiece& placed : chunk.pieces) {
      const ro::sim::TerrainPieceRule* piece_rule = RuleOf(placed.piece_id);
      const PieceLook* look =
          piece_rule != nullptr ? FindPieceLook(biome_, piece_rule->key) : nullptr;
      const auto piece_rank = static_cast<std::uint32_t>(plan_.pieces.size());
      const PlannedPiece planned{.placed = placed, .rule = piece_rule, .look = look};
      ring_pieces_.push_back(planned);
      if (shown) plan_.pieces.push_back(planned);
      if (look == nullptr) continue;
      const ro::sim::FootprintSize drawn = ro::sim::FootprintSizeOf(*piece_rule, 0);
      const glm::vec2 corner(static_cast<float>(placed.x - origin_.x_cells),
                             static_cast<float>(placed.y - origin_.y_cells));
      for (const PieceModelSlot& slot : look->slots) {
        // Toujours trois tirages par place : le choix, l'angle, l'échelle.
        const GroundModelChoice* choice = Pick(random, slot.models);
        const std::uint32_t yaw_draw = random.Below(kFullTurnDegrees);
        const float scale_draw = UnitOf(random.Next());
        if (choice == nullptr) continue;
        const glm::vec2 at =
            corner + TurnInFootprint({slot.east_cells, slot.north_cells}, drawn, placed.quarter);
        const float yaw = slot.any_yaw ? static_cast<float>(yaw_draw)
                                       : PieceModelYawDegrees(placed.quarter, slot.yaw_degrees);
        const float jitter = slot.any_yaw ? 1.0F + kScaleJitter * (2.0F * scale_draw - 1.0F) : 1.0F;
        if (shown) {
          plan_.poses.push_back(PlannedPose{.model = choice->model,
                                            .east = at.x,
                                            .north = at.y,
                                            .yaw_degrees = yaw,
                                            .scale = choice->scale * jitter,
                                            .piece = piece_rank});
        }
        if (choice->shade_cells > 0.0F) {
          const float radius = choice->shade_cells * jitter;
          const glm::vec2 anchor =
              TurnInFootprint({slot.east_cells, slot.north_cells}, drawn, placed.quarter);
          discs_.push_back(ShadeDisc{.corner = {placed.x, placed.y},
                                     .center = anchor + shade_direction_ * (radius * kShadeOffset),
                                     .radius = radius});
        }
      }
    }
    if (shown) SowChunk(random, chunk, column, row);
  }

  // Le semis : toujours `seeds_per_chunk` essais de cinq tirages ; une case
  // bloquée ou voisine d'une case bloquée, et une case de chemin, n'en
  // reçoivent pas.
  void SowChunk(ro::core::Pcg32& random, const ro::sim::TerrainChunk& chunk, std::int32_t column,
                std::int32_t row) {
    const auto side = static_cast<std::uint32_t>(kTerrainChunkCells);
    for (std::uint32_t attempt = 0; attempt < biome_.seeds_per_chunk; ++attempt) {
      const auto east = static_cast<std::int32_t>(random.Below(side));
      const auto north = static_cast<std::int32_t>(random.Below(side));
      const SeedLook* seed = Pick(random, biome_.seeds);
      const std::uint32_t yaw = random.Below(kFullTurnDegrees);
      const float offset = UnitOf(random.Next());
      if (seed == nullptr || !ClearAround(chunk, east, north) || chunk.OnPath(east, north)) {
        continue;
      }
      plan_.poses.push_back(
          PlannedPose{.model = seed->model,
                      .east = static_cast<float>(column * kTerrainChunkCells + east) + kSeedInset +
                              kSeedSpan * offset,
                      .north = static_cast<float>(row * kTerrainChunkCells + north) + kSeedInset +
                               kSeedSpan * (1.0F - offset),
                      .yaw_degrees = static_cast<float>(yaw),
                      .scale = seed->scale,
                      .piece = kSeedPose});
    }
  }

  [[nodiscard]] static bool ClearAround(const ro::sim::TerrainChunk& chunk, std::int32_t east,
                                        std::int32_t north) {
    for (std::int32_t dy = -1; dy <= 1; ++dy) {
      for (std::int32_t dx = -1; dx <= 1; ++dx) {
        const std::int32_t x = east + dx;
        const std::int32_t y = north + dy;
        // Hors du morceau : la bordure de l'emplacement voisin, toujours libre.
        if (x < 0 || y < 0 || x >= kTerrainChunkCells || y >= kTerrainChunkCells) continue;
        if (!chunk.Passable(x, y)) return false;
      }
    }
    return true;
  }

  [[nodiscard]] const ro::sim::TerrainPieceRule* RuleOf(std::uint16_t piece_id) const {
    for (const ro::sim::TerrainPieceRule& piece : rule_.pieces) {
      if (piece.piece_id == piece_id) return &piece;
    }
    return nullptr;
  }

  // Les hauteurs de chaque coin de case, puis celles des cases et des cubes,
  // lues sur la même grille : le sol dessiné et le sol foulé se rejoignent à
  // chaque coin de cube.
  void RaiseRelief() {
    corners_.resize(static_cast<std::size_t>(kCornerSide) * static_cast<std::size_t>(kCornerSide));
    for (std::int32_t y = 0; y < kCornerSide; ++y) {
      for (std::int32_t x = 0; x < kCornerSide; ++x) {
        corners_[static_cast<std::size_t>(y) * kCornerSide + static_cast<std::size_t>(x)] =
            GroundReliefAt(run_seed_, static_cast<double>(origin_.x_cells + x),
                           static_cast<double>(origin_.y_cells + y));
      }
    }
    plan_.cell_heights.resize(kCellCount);
    for (std::int32_t y = 0; y < kGroundWindowCells; ++y) {
      for (std::int32_t x = 0; x < kGroundWindowCells; ++x) {
        plan_.cell_heights[CellIndex(x, y)] = CornersOf(x, y, 1);
      }
    }
  }

  // Les hauteurs, sud-ouest, sud-est, nord-ouest, nord-est, d'un carré de
  // `span` cases dont le coin sud-ouest est (x, y).
  [[nodiscard]] std::array<float, 4> CornersOf(std::int32_t x, std::int32_t y,
                                               std::int32_t span) const {
    const auto at = [this](std::int32_t cx, std::int32_t cy) {
      return corners_[static_cast<std::size_t>(cy) * kCornerSide + static_cast<std::size_t>(cx)];
    };
    return {at(x, y), at(x + span, y), at(x, y + span), at(x + span, y + span)};
  }

  void PaintCubes() {
    const std::set<std::pair<std::int32_t, std::int32_t>> occupied = OccupiedSlots();
    const std::vector<std::uint8_t> dirt = DirtCornersOfCubes();
    ReadPathBlocks();
    // Un emplacement qu'un chemin effleure ne porte pas de tache : leurs
    // lisières ne se raccorderaient pas.
    std::set<std::pair<std::int32_t, std::int32_t>> pathed;
    for (std::int32_t j = 0; j < kGroundWindowCubes; ++j) {
      for (std::int32_t i = 0; i < kGroundWindowCubes; ++i) {
        if (plan_.path_blocks[CubeIndex(i, j)] == 0) continue;
        pathed.emplace(FloorDivide(origin_.x_cells / kGroundCellsPerCube + i, kGroundTextureCubes),
                       FloorDivide(origin_.y_cells / kGroundCellsPerCube + j, kGroundTextureCubes));
      }
    }
    plan_.cubes.resize(kCubeCount);
    for (std::int32_t j = 0; j < kGroundWindowCubes; ++j) {
      for (std::int32_t i = 0; i < kGroundWindowCubes; ++i) {
        PlannedCube& cube = plan_.cubes[CubeIndex(i, j)];
        const std::int32_t world_cube_x = origin_.x_cells / kGroundCellsPerCube + i;
        const std::int32_t world_cube_y = origin_.y_cells / kGroundCellsPerCube + j;
        const std::int32_t slot_x = FloorDivide(world_cube_x, kGroundTextureCubes);
        const std::int32_t slot_y = FloorDivide(world_cube_y, kGroundTextureCubes);
        const auto [texture, turn] = MotifOf(slot_x, slot_y, occupied.contains({slot_x, slot_y}),
                                             pathed.contains({slot_x, slot_y}));
        cube.texture = texture;
        cube.turn = turn;
        cube.quarter_east = static_cast<std::uint8_t>(world_cube_x - slot_x * kGroundTextureCubes);
        cube.quarter_north = static_cast<std::uint8_t>(world_cube_y - slot_y * kGroundTextureCubes);
        PaintPathTile(cube, dirt[CubeIndex(i, j)]);
        cube.corner_height =
            CornersOf(i * kGroundCellsPerCube, j * kGroundCellsPerCube, kGroundCellsPerCube);
        const double east = static_cast<double>(origin_.x_cells + i * kGroundCellsPerCube + 1);
        const double north = static_cast<double>(origin_.y_cells + j * kGroundCellsPerCube + 1);
        const double tint =
            kTintStrength * ValueNoise(run_seed_, kTintSalt, east, north, kTintPeriodCells);
        for (std::size_t channel = 0; channel < kTintBgr.size(); ++channel) {
          cube.color[channel] = static_cast<std::uint8_t>(
              std::lround(std::lerp(kFullChannel, kTintBgr[channel], tint)));
        }
        cube.color[kTintBgr.size()] = kOpaque;
      }
    }
  }

  // LE BLOC DE CHAQUE CUBE, lu dans le tour au bord de la fenêtre.
  void ReadPathBlocks() {
    plan_.path_blocks.assign(kCubeCount, 0);
    for (std::int32_t j = 0; j < kGroundWindowCubes; ++j) {
      for (std::int32_t i = 0; i < kGroundWindowCubes; ++i) {
        std::uint16_t block = 0;
        for (std::int32_t row = 0; row < kPathBlockSide; ++row) {
          for (std::int32_t column = 0; column < kPathBlockSide; ++column) {
            if (!PathAt(i * kGroundCellsPerCube - 1 + column, j * kGroundCellsPerCube - 1 + row)) {
              continue;
            }
            block |= static_cast<std::uint16_t>(1U << (row * kPathBlockSide + column));
          }
        }
        plan_.path_blocks[CubeIndex(i, j)] = block;
      }
    }
  }

  // LES COINS DE TERRE DE CHAQUE CUBE. Le coin (2a, 2b) de la fenêtre, en
  // cases, touche les cases (2a − 1, 2b − 1) à (2a, 2b) ; celles du bord se
  // lisent dans le tour, si bien qu'un coin vaut la même chose dans toute
  // fenêtre.
  [[nodiscard]] std::vector<std::uint8_t> DirtCornersOfCubes() const {
    std::vector<std::uint8_t> corner_dirt(static_cast<std::size_t>(kCubeCorners) * kCubeCorners, 0);
    for (std::int32_t b = 0; b < kCubeCorners; ++b) {
      for (std::int32_t a = 0; a < kCubeCorners; ++a) {
        const std::int32_t x = a * kGroundCellsPerCube;
        const std::int32_t y = b * kGroundCellsPerCube;
        const std::int32_t paths = static_cast<std::int32_t>(PathAt(x - 1, y - 1)) +
                                   static_cast<std::int32_t>(PathAt(x, y - 1)) +
                                   static_cast<std::int32_t>(PathAt(x - 1, y)) +
                                   static_cast<std::int32_t>(PathAt(x, y));
        corner_dirt[static_cast<std::size_t>(b) * kCubeCorners + static_cast<std::size_t>(a)] =
            paths >= kDirtCornerLeastCells ? 1U : 0U;
      }
    }
    const auto at = [&corner_dirt](std::int32_t a, std::int32_t b) {
      return corner_dirt[static_cast<std::size_t>(b) * kCubeCorners +
                         static_cast<std::size_t>(a)] != 0U;
    };
    std::vector<std::uint8_t> cubes(kCubeCount, 0);
    for (std::int32_t j = 0; j < kGroundWindowCubes; ++j) {
      for (std::int32_t i = 0; i < kGroundWindowCubes; ++i) {
        std::uint8_t corners = 0;
        if (at(i, j)) corners |= kDirtSouthWest;
        if (at(i + 1, j)) corners |= kDirtSouthEast;
        if (at(i, j + 1)) corners |= kDirtNorthWest;
        if (at(i + 1, j + 1)) corners |= kDirtNorthEast;
        cubes[CubeIndex(i, j)] = corners;
      }
    }
    return cubes;
  }

  // LE CARREAU DE CHEMIN d'un cube qui a des coins de terre. La terre pleine
  // garde la place du cube dans son motif, comme l'herbe ; un bord glisse le
  // long de sa colonne avec la case du monde, sans demi-tour ; un coin montre
  // son cube. Un biome sans le carreau voulu laisse l'herbe.
  void PaintPathTile(PlannedCube& cube, std::uint8_t dirt_corners) const {
    if (dirt_corners == 0) return;
    const PathTile tile = PathTileOf(dirt_corners);
    const std::uint8_t rank = TextureOfRole(tile.role);
    if (rank == kNoTexture) return;
    const GroundTextureLook& look = biome_.textures[rank];
    cube.texture = rank;
    if (tile.role == GroundTextureRole::kPathFull) {
      cube.turn = 0;
      return;
    }
    std::uint8_t turn = tile.turn;
    TextureCube shown{.u = look.quarter_u, .v = look.quarter_v};
    if (tile.role == GroundTextureRole::kPathEdge) {
      // La terre à l'est ou au nord lit l'autre colonne de la bande, tournée
      // comme la terre à l'ouest ou au sud ; sans autre colonne, elle lit la
      // même, retournée.
      if (turn % kQuarterTurns >= kHalfTurn &&
          look.east_quarter_u != GroundTextureLook::kNoEastColumn) {
        shown.u = look.east_quarter_u;
        turn %= kHalfTurn;
      }
      shown.v = TextureCubeOf(cube.quarter_east, cube.quarter_north, turn).v;
    }
    const MotifCube place = MotifCubeShowing(shown, turn);
    cube.quarter_east = place.quarter_east;
    cube.quarter_north = place.quarter_north;
    cube.turn = turn;
    // Un coin se découpe dans une texture dont les cubes voisins ne le
    // prolongent pas ; un bord glisse le long d'une colonne qui se prolonge.
    cube.inset = tile.role != GroundTextureRole::kPathEdge;
  }

  // Le rang de la première texture du rôle, ou kNoTexture.
  [[nodiscard]] std::uint8_t TextureOfRole(GroundTextureRole role) const {
    for (std::size_t rank = 0; rank < biome_.textures.size() && rank < kNoTexture; ++rank) {
      if (biome_.textures[rank].role == role) return static_cast<std::uint8_t>(rank);
    }
    return kNoTexture;
  }

  [[nodiscard]] static std::size_t CubeIndex(std::int32_t i, std::int32_t j) {
    return static_cast<std::size_t>(j) * kGroundWindowCubes + static_cast<std::size_t>(i);
  }

  // LE MOTIF D'UN EMPLACEMENT DU MONDE et son quart de tour : une tache sur un
  // emplacement qui porte une pièce, à sa chance ; une tache éparse, plus
  // rare, sur un emplacement vide ; l'herbe de fond sinon, et toujours sur un
  // emplacement qu'un chemin effleure. Un seul mélange par emplacement : le
  // même dans toute fenêtre.
  [[nodiscard]] std::pair<std::uint16_t, std::uint8_t> MotifOf(std::int32_t slot_x,
                                                               std::int32_t slot_y, bool occupied,
                                                               bool pathed) const {
    const std::uint64_t mixed = Mix(run_seed_, kPatchSalt, slot_x, slot_y);
    const std::uint32_t chance = occupied ? biome_.patch_per_mille : biome_.scatter_per_mille;
    const GroundTextureRole wanted =
        !pathed && mixed % ro::sim::kTerrainPerMille < chance
            ? (occupied ? GroundTextureRole::kPatch : GroundTextureRole::kScatter)
            : GroundTextureRole::kGrass;
    std::uint64_t total = 0;
    for (const GroundTextureLook& texture : biome_.textures) {
      if (texture.role == wanted) total += texture.weight;
    }
    if (total == 0) return {std::uint16_t{0}, std::uint8_t{0}};
    std::uint64_t pick = (mixed >> kPickShift) % total;
    for (std::size_t rank = 0; rank < biome_.textures.size(); ++rank) {
      const GroundTextureLook& texture = biome_.textures[rank];
      if (texture.role != wanted) continue;
      if (pick < texture.weight) {
        // L'herbe de fond ne tourne pas : elle ne se raccorde qu'à l'endroit.
        const auto turn =
            wanted == GroundTextureRole::kGrass
                ? std::uint8_t{0}
                : static_cast<std::uint8_t>((mixed >> kTurnShift) % ro::sim::kTerrainQuarterTurns);
        return {static_cast<std::uint16_t>(rank), turn};
      }
      pick -= texture.weight;
    }
    return {std::uint16_t{0}, std::uint8_t{0}};
  }

  // Les emplacements du monde qu'une case bloquée d'une pièce touche.
  [[nodiscard]] std::set<std::pair<std::int32_t, std::int32_t>> OccupiedSlots() const {
    std::set<std::pair<std::int32_t, std::int32_t>> occupied;
    for (const PlannedPiece& piece : ring_pieces_) {
      if (piece.rule == nullptr) continue;
      const ro::sim::FootprintSize size =
          ro::sim::FootprintSizeOf(*piece.rule, piece.placed.quarter);
      for (std::int32_t north = 0; north < size.height; ++north) {
        for (std::int32_t east = 0; east < size.width; ++east) {
          if (!ro::sim::FootprintBlocks(*piece.rule, piece.placed.quarter, east, north)) continue;
          occupied.emplace(FloorDivide(piece.placed.x + east, ro::sim::kTerrainSlotCells),
                           FloorDivide(piece.placed.y + north, ro::sim::kTerrainSlotCells));
        }
      }
    }
    return occupied;
  }

  [[nodiscard]] static std::int32_t FloorDivide(std::int32_t value, std::int32_t divisor) {
    const std::int32_t quotient = value / divisor;
    return (value % divisor != 0 && value < 0) ? quotient - 1 : quotient;
  }

  // Les ombres cuites : une lightmap à soi pour chaque cube qu'un disque
  // approche, à un cube près pour que les bords partagés se raccordent ; le plein
  // jour partagé pour les autres.
  void BakeShade() {
    PlannedShade day{};
    day.fill(kFullDay);
    plan_.lightmaps.push_back(day);
    std::vector<std::vector<std::uint32_t>> near(kCubeCount);
    for (std::uint32_t rank = 0; rank < discs_.size(); ++rank) {
      // Le disque dans les cases de la fenêtre, pour trouver ses cubes.
      const ShadeDisc& world_disc = discs_[rank];
      const ShadeDisc disc{.center = glm::vec2(world_disc.corner - Origin()) + world_disc.center,
                           .radius = world_disc.radius};
      const float reach = disc.radius + kGroundCellsPerCube;
      const auto first_i = std::max(
          0, static_cast<std::int32_t>(std::floor((disc.center.x - reach) / kGroundCellsPerCube)));
      const auto last_i = std::min(
          kGroundWindowCubes - 1,
          static_cast<std::int32_t>(std::floor((disc.center.x + reach) / kGroundCellsPerCube)));
      const auto first_j = std::max(
          0, static_cast<std::int32_t>(std::floor((disc.center.y - reach) / kGroundCellsPerCube)));
      const auto last_j = std::min(
          kGroundWindowCubes - 1,
          static_cast<std::int32_t>(std::floor((disc.center.y + reach) / kGroundCellsPerCube)));
      for (std::int32_t j = first_j; j <= last_j; ++j) {
        for (std::int32_t i = first_i; i <= last_i; ++i) {
          near[static_cast<std::size_t>(j) * kGroundWindowCubes + static_cast<std::size_t>(i)]
              .push_back(rank);
        }
      }
    }
    for (std::size_t cube = 0; cube < kCubeCount; ++cube) {
      if (near[cube].empty()) continue;
      const auto i = static_cast<std::int32_t>(cube % kGroundWindowCubes);
      const auto j = static_cast<std::int32_t>(cube / kGroundWindowCubes);
      PlannedShade shade{};
      bool shaded = false;
      for (std::int32_t ty = 0; ty < kGroundLightmapSide; ++ty) {
        for (std::int32_t tx = 0; tx < kGroundLightmapSide; ++tx) {
          // Le texel tx couvre le cube de (tx − 1) / 6 à tx / 6 : son centre,
          // compté depuis le coin du cube dans le monde.
          const glm::ivec2 cube_corner =
              Origin() + glm::ivec2(i * kGroundCellsPerCube, j * kGroundCellsPerCube);
          const glm::vec2 inside(
              (static_cast<float>(tx) - kHalfTexel) * kGroundCellsPerCube / kLightmapInnerTexels,
              (static_cast<float>(ty) - kHalfTexel) * kGroundCellsPerCube / kLightmapInnerTexels);
          float light = 1.0F;
          for (const std::uint32_t rank : near[cube]) {
            const ShadeDisc& disc = discs_[rank];
            light = std::min(light, LightUnder(disc, glm::vec2(cube_corner - disc.corner) + inside -
                                                         disc.center));
          }
          const auto level = static_cast<std::uint8_t>(std::lround(std::lerp(
              static_cast<float>(kGroundShadeLevel), static_cast<float>(kFullDay), light)));
          shade[static_cast<std::size_t>(ty) * kGroundLightmapSide + static_cast<std::size_t>(tx)] =
              level;
          shaded = shaded || level != kFullDay;
        }
      }
      if (!shaded) continue;
      plan_.cubes[cube].lightmap = static_cast<std::uint32_t>(plan_.lightmaps.size());
      plan_.lightmaps.push_back(shade);
    }
  }

  // La lumière qu'un disque laisse à l'écart `from_center` de son centre : 0 en
  // son cœur, 1 au-delà.
  [[nodiscard]] static float LightUnder(const ShadeDisc& disc, glm::vec2 from_center) {
    const float distance = glm::length(from_center);
    const float core = disc.radius * kShadeCore;
    if (distance <= core) return 0.0F;
    if (distance >= disc.radius) return 1.0F;
    const float t = (distance - core) / (disc.radius - core);
    return t * t * (3.0F - 2.0F * t);
  }

  [[nodiscard]] glm::ivec2 Origin() const { return {origin_.x_cells, origin_.y_cells}; }

  [[nodiscard]] static std::size_t CellIndex(std::int32_t x, std::int32_t y) {
    return static_cast<std::size_t>(y) * kGroundWindowCells + static_cast<std::size_t>(x);
  }

  const ro::sim::TerrainRule& rule_;
  std::uint64_t run_seed_ = 0;
  const BiomeLook& biome_;
  GroundWindowOrigin origin_;
  glm::vec2 shade_direction_{0.0F};
  std::vector<float> corners_;
  std::vector<ShadeDisc> discs_;
  // Les pièces de la fenêtre et de son tour : leurs emplacements portent des
  // taches.
  std::vector<PlannedPiece> ring_pieces_;
  // Les cases de la fenêtre et de son tour : praticables, de chemin.
  std::vector<std::uint8_t> ring_passable_;
  std::vector<std::uint8_t> ring_path_;
  GroundPlan plan_;
};

}  // namespace

GroundWindowOrigin GroundWindowAround(std::int32_t x_cells, std::int32_t y_cells) {
  constexpr std::int32_t kChunksBeside = kWindowChunks / 2;
  return GroundWindowOrigin{
      .x_cells = (ro::sim::TerrainChunkOf(x_cells) - kChunksBeside) * kTerrainChunkCells,
      .y_cells = (ro::sim::TerrainChunkOf(y_cells) - kChunksBeside) * kTerrainChunkCells};
}

float PieceModelYawDegrees(std::uint8_t quarter, float slot_yaw_degrees) {
  const auto full_turn = static_cast<float>(kFullTurnDegrees);
  const float turned =
      std::fmod(slot_yaw_degrees - static_cast<float>(quarter % ro::sim::kTerrainQuarterTurns) *
                                       kQuarterTurnDegrees,
                full_turn);
  return turned < 0.0F ? turned + full_turn : turned;
}

float GroundReliefAt(std::uint64_t run_seed, double world_east, double world_north) {
  const double slow =
      ValueNoise(run_seed, kReliefSalt, world_east, world_north, kReliefSlowPeriodCells);
  const double quick =
      ValueNoise(run_seed, kQuickReliefSalt, world_east, world_north, kReliefQuickPeriodCells);
  // Centré sur zéro : une moitié creuse, l'autre bombe.
  return static_cast<float>(kReliefSlowUnits * (2.0 * slow - 1.0) +
                            kReliefQuickUnits * (2.0 * quick - 1.0));
}

GroundPlan PlanGroundWindow(const ro::sim::TerrainRule& rule, std::uint64_t run_seed,
                            const BiomeLook& biome, GroundWindowOrigin origin,
                            const GroundPlanLight& light) {
  return Planner(rule, run_seed, biome, origin, light).Build();
}

}  // namespace moonclient
