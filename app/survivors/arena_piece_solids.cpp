#include "survivors/arena_piece_solids.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numeric>
#include <utility>

#include <glm/glm.hpp>

#include "ro/render/terrain_mesh.h"

namespace moonclient {
namespace {

// Une case vaut la moitié d'un cube.
constexpr float kCellUnits = ro::render::kWorldUnitsPerCube / kGroundCellsPerCube;
constexpr float kHalf = 0.5F;
constexpr std::int32_t kSamplesPerCell = kSolidSamplesPerSide * kSolidSamplesPerSide;
constexpr float kSampleStep = 1.0F / static_cast<float>(kSolidSamplesPerSide);
// Le demi-pas de point : la portée d'un bord de contour.
constexpr float kEdgeReachCells = kHalf * kSampleStep;
// Deux faces dont les boîtes sont à moins de ceci (unités du maillage) font un
// même bloc.
constexpr float kTouchUnits = 0.5F;
// Sous cette aire (cases²), un contour est une face debout : il ne couvre que
// par ses bords.
constexpr float kFlatAreaCells = 1.0e-4F;
constexpr std::size_t kTriangleCorners = 3;
constexpr std::int32_t kFullTurnDegrees = 360;
// Un point du maillage se pose à n'importe quelle hauteur : la pose ne tourne
// que sous l'axe vertical, x et z n'en dépendent pas.
constexpr float kAnyHeight = 0.0F;

[[nodiscard]] float Cross(glm::vec2 origin, glm::vec2 a, glm::vec2 b) noexcept {
  return (a.x - origin.x) * (b.y - origin.y) - (a.y - origin.y) * (b.x - origin.x);
}

[[nodiscard]] float DistanceToSegment(glm::vec2 point, glm::vec2 a, glm::vec2 b) noexcept {
  const glm::vec2 along = b - a;
  const float length_squared = glm::dot(along, along);
  const float t = length_squared > 0.0F
                      ? std::clamp(glm::dot(point - a, along) / length_squared, 0.0F, 1.0F)
                      : 0.0F;
  return glm::length(point - (a + along * t));
}

// Vrai si `point` tombe dans le contour convexe `outline` (cases) ; un contour
// sans aire (une face debout) couvre à moins de `kEdgeReachCells` de lui.
[[nodiscard]] bool Covers(std::span<const glm::vec2> outline, float area, glm::vec2 point) {
  const std::size_t count = outline.size();
  if (std::abs(area) < kFlatAreaCells) {
    for (std::size_t corner = 0; corner < count; ++corner) {
      if (DistanceToSegment(point, outline[corner], outline[(corner + 1) % count]) <=
          kEdgeReachCells) {
        return true;
      }
    }
    return false;
  }
  for (std::size_t corner = 0; corner < count; ++corner) {
    const float side = Cross(outline[corner], outline[(corner + 1) % count], point);
    if ((area > 0.0F && side < 0.0F) || (area < 0.0F && side > 0.0F)) return false;
  }
  return true;
}

// L'aire signée d'un contour.
[[nodiscard]] float SignedArea(std::span<const glm::vec2> outline) {
  float twice = 0.0F;
  for (std::size_t corner = 0; corner < outline.size(); ++corner) {
    const glm::vec2 a = outline[corner];
    const glm::vec2 b = outline[(corner + 1) % outline.size()];
    twice += a.x * b.y - b.x * a.y;
  }
  return twice * kHalf;
}

// Le triangle coupé au plan y = `top` : ce qui passe dessous.
[[nodiscard]] std::vector<glm::vec3> ClipBelow(const std::array<glm::vec3, 3>& triangle,
                                               float top) {
  std::vector<glm::vec3> kept;
  for (std::size_t corner = 0; corner < kTriangleCorners; ++corner) {
    const glm::vec3 a = triangle[corner];
    const glm::vec3 b = triangle[(corner + 1) % kTriangleCorners];
    const bool a_in = a.y <= top;
    const bool b_in = b.y <= top;
    if (a_in) kept.push_back(a);
    if (a_in != b_in) kept.push_back(a + (b - a) * ((top - a.y) / (b.y - a.y)));
  }
  return kept;
}

// L'enveloppe convexe (chaîne monotone), sens trigonométrique.
[[nodiscard]] std::vector<SolidPoint> Hull(std::vector<SolidPoint> points) {
  std::sort(points.begin(), points.end());
  points.erase(std::unique(points.begin(), points.end()), points.end());
  if (points.size() < kTriangleCorners) return points;
  const auto turn = [](const SolidPoint& o, const SolidPoint& a, const SolidPoint& b) {
    return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0]);
  };
  std::vector<SolidPoint> hull(2 * points.size());
  std::size_t size = 0;
  for (const SolidPoint& point : points) {
    while (size >= 2 && turn(hull[size - 2], hull[size - 1], point) <= 0.0F) --size;
    hull[size++] = point;
  }
  const std::size_t lower = size + 1;
  for (std::size_t rank = points.size() - 1; rank-- > 0;) {
    while (size >= lower && turn(hull[size - 2], hull[size - 1], points[rank]) <= 0.0F) --size;
    hull[size++] = points[rank];
  }
  hull.resize(size - 1);
  return hull;
}

struct Box {
  SolidPoint low{};
  SolidPoint high{};
};

[[nodiscard]] Box BoxOf(const std::vector<SolidPoint>& points) {
  Box box{.low = points.front(), .high = points.front()};
  for (const SolidPoint& point : points) {
    for (std::size_t axis = 0; axis < 2; ++axis) {
      box.low[axis] = std::min(box.low[axis], point[axis]);
      box.high[axis] = std::max(box.high[axis], point[axis]);
    }
  }
  return box;
}

[[nodiscard]] bool Touch(const Box& a, const Box& b) noexcept {
  for (std::size_t axis = 0; axis < 2; ++axis) {
    if (a.low[axis] > b.high[axis] + kTouchUnits || b.low[axis] > a.high[axis] + kTouchUnits) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] std::size_t Root(std::vector<std::size_t>& parent, std::size_t node) {
  while (parent[node] != node) {
    parent[node] = parent[parent[node]];
    node = parent[node];
  }
  return node;
}

// La matrice du décor d'un modèle relevé, sans carte : la grille de cubes est
// vide, si bien qu'un point du monde se lit en cases par `CellOf`.
[[nodiscard]] glm::mat4 SolidMatrix(const ModelSolid& solid, const PlannedPose& pose) {
  ro::render::ModelMesh bounds;
  bounds.bounds_min = solid.bounds_min;
  bounds.bounds_max = solid.bounds_max;
  return ro::render::ModelInstanceMatrix(PosePlacement(pose, 0.0F, 0, 0), 0, 0, bounds,
                                         solid.rsm_version);
}

// Un point du monde, sur une carte sans cube, en cases : l'inverse de
// `PosePlacement` posé par `ModelInstanceMatrix`.
[[nodiscard]] glm::vec2 CellOf(const glm::vec4& world) noexcept {
  return {world.x / kCellUnits, (ro::render::kWorldUnitsPerCube - world.z) / kCellUnits};
}

}  // namespace

const ModelSolid* FindModelSolid(std::span<const ModelSolid> solids,
                                 const GroundModelChoice& choice) {
  for (const ModelSolid& solid : solids) {
    if (solid.model == choice.model && solid.scale == choice.scale &&
        solid.sink_units == choice.sink_units) {
      return &solid;
    }
  }
  return nullptr;
}

std::vector<SolidFace> SolidFacesOf(const ro::render::ModelMesh& mesh, std::int32_t rsm_version,
                                    float scale, float sink_units, float height_units) {
  std::vector<SolidFace> faces;
  if (mesh.vertices.size() < kTriangleCorners) return faces;
  ModelSolid frame{.scale = scale,
                   .rsm_version = rsm_version,
                   .bounds_min = mesh.bounds_min,
                   .bounds_max = mesh.bounds_max};
  const glm::mat4 placed = SolidMatrix(frame, PlannedPose{.scale = scale});
  const glm::mat4 back = glm::inverse(placed);
  float base = std::numeric_limits<float>::max();
  std::vector<glm::vec3> world;
  world.reserve(mesh.vertices.size());
  for (const ro::render::ModelVertex& vertex : mesh.vertices) {
    const glm::vec4 point =
        placed * glm::vec4(vertex.position[0], vertex.position[1], vertex.position[2], 1.0F);
    world.emplace_back(point);
    base = std::min(base, point.y);
  }
  // Le sol est à l'enfoncement au-dessus du point le plus bas.
  const float ground = base + sink_units * scale;
  const float top = ground + height_units;
  for (std::size_t first = 0; first + kTriangleCorners <= world.size(); first += kTriangleCorners) {
    const std::vector<glm::vec3> kept =
        ClipBelow({world[first], world[first + 1], world[first + 2]}, top);
    if (kept.empty()) continue;
    SolidFace face{.lowest_units = std::numeric_limits<float>::max()};
    face.points.reserve(kept.size());
    for (const glm::vec3& point : kept) {
      const glm::vec4 local = back * glm::vec4(point, 1.0F);
      face.points.push_back({local.x, local.z});
      face.lowest_units = std::min(face.lowest_units, point.y - ground);
    }
    faces.push_back(std::move(face));
  }
  return faces;
}

namespace {

// UN BLOC : des faces qui se touchent, et le plus bas d'entre elles.
struct SolidBlock {
  std::vector<std::size_t> faces;
  float lowest_units = std::numeric_limits<float>::max();
};

// Les blocs de `faces`, dans l'ordre de leur première face.
[[nodiscard]] std::vector<SolidBlock> BlocksOf(const std::vector<SolidFace>& faces) {
  std::vector<Box> boxes;
  boxes.reserve(faces.size());
  for (const SolidFace& face : faces) boxes.push_back(BoxOf(face.points));
  std::vector<std::size_t> parent(faces.size());
  std::iota(parent.begin(), parent.end(), std::size_t{0});
  for (std::size_t later = 1; later < faces.size(); ++later) {
    for (std::size_t earlier = 0; earlier < later; ++earlier) {
      if (Touch(boxes[earlier], boxes[later])) {
        parent[Root(parent, later)] = Root(parent, earlier);
      }
    }
  }
  std::vector<std::size_t> block_roots;
  std::vector<SolidBlock> blocks;
  for (std::size_t face = 0; face < faces.size(); ++face) {
    const std::size_t root = Root(parent, face);
    const auto found = std::find(block_roots.begin(), block_roots.end(), root);
    const auto rank = static_cast<std::size_t>(found - block_roots.begin());
    if (found == block_roots.end()) {
      block_roots.push_back(root);
      blocks.emplace_back();
    }
    blocks[rank].faces.push_back(face);
    blocks[rank].lowest_units = std::min(blocks[rank].lowest_units, faces[face].lowest_units);
  }
  return blocks;
}

[[nodiscard]] bool Grounded(const SolidBlock& block) noexcept {
  return block.lowest_units <= kSolidContactUnits;
}

}  // namespace

std::vector<std::vector<SolidPoint>> SolidHullsOf(const std::vector<SolidFace>& faces) {
  std::vector<std::vector<SolidPoint>> hulls;
  for (const SolidBlock& block : BlocksOf(faces)) {
    if (!Grounded(block)) continue;
    std::vector<SolidPoint> points;
    for (const std::size_t face : block.faces) {
      points.insert(points.end(), faces[face].points.begin(), faces[face].points.end());
    }
    hulls.push_back(Hull(std::move(points)));
  }
  return hulls;
}

std::vector<SolidFace> GroundedFacesOf(const std::vector<SolidFace>& faces,
                                       std::vector<SolidFace>* airborne) {
  std::vector<std::uint8_t> kept(faces.size(), 0);
  for (const SolidBlock& block : BlocksOf(faces)) {
    if (!Grounded(block)) continue;
    for (const std::size_t face : block.faces) kept[face] = 1;
  }
  std::vector<SolidFace> grounded;
  for (std::size_t face = 0; face < faces.size(); ++face) {
    if (kept[face] != 0) {
      grounded.push_back(faces[face]);
    } else if (airborne != nullptr) {
      airborne->push_back(faces[face]);
    }
  }
  return grounded;
}

ro::formats::RswModel PosePlacement(const PlannedPose& pose, float ground_units,
                                    std::int32_t gnd_width, std::int32_t gnd_height) {
  const float half_width = static_cast<float>(gnd_width) * ro::render::kWorldUnitsPerCube * kHalf;
  const float half_depth = static_cast<float>(gnd_height) * ro::render::kWorldUnitsPerCube * kHalf;
  ro::formats::RswModel model;
  // Le RSW compte la hauteur vers le bas ; un modèle enfoncé descend sous le sol.
  model.position = {pose.east * kCellUnits - half_width, pose.sink_units - ground_units,
                    pose.north * kCellUnits - half_depth};
  model.rotation = {0.0F, pose.yaw_degrees, 0.0F};
  model.scale = {pose.scale, pose.scale, pose.scale};
  return model;
}

SolidGrid SolidGrid::Around(ro::sim::FootprintSize size, std::int32_t margin) {
  SolidGrid grid{.west = -margin,
                 .south = -margin,
                 .width = size.width + 2 * margin,
                 .height = size.height + 2 * margin};
  grid.samples.assign(static_cast<std::size_t>(grid.width) * static_cast<std::size_t>(grid.height),
                      0U);
  return grid;
}

bool SolidGrid::Inside(std::int32_t east, std::int32_t north) const noexcept {
  return east >= west && north >= south && east < west + width && north < south + height;
}

std::int32_t SolidGrid::SamplesAt(std::int32_t east, std::int32_t north) const noexcept {
  if (!Inside(east, north)) return 0;
  const auto index = static_cast<std::size_t>(north - south) * static_cast<std::size_t>(width) +
                     static_cast<std::size_t>(east - west);
  return std::popcount(samples[index]);
}

bool SolidGrid::Solid(std::int32_t east, std::int32_t north) const noexcept {
  return SamplesAt(east, north) >= kSolidLeastSamples;
}

std::vector<std::vector<glm::vec2>> PlacedSolidOutlines(const ModelSolid& solid,
                                                        const PlannedPose& pose) {
  const glm::mat4 placed = SolidMatrix(solid, pose);
  std::vector<std::vector<glm::vec2>> outlines;
  outlines.reserve(solid.outlines.size());
  for (const SolidOutline& outline : solid.outlines) {
    if (outline.points.empty()) continue;
    std::vector<glm::vec2>& cells = outlines.emplace_back();
    for (const SolidPoint& point : outline.points) {
      cells.push_back(CellOf(placed * glm::vec4(point[0], kAnyHeight, point[1], 1.0F)));
    }
  }
  return outlines;
}

void CoverModelSolid(const ModelSolid& solid, const PlannedPose& pose, SolidGrid& grid) {
  for (const std::vector<glm::vec2>& cells : PlacedSolidOutlines(solid, pose)) {
    const float area = SignedArea(cells);
    glm::vec2 low = cells.front();
    glm::vec2 high = cells.front();
    for (const glm::vec2& cell : cells) {
      low = glm::min(low, cell);
      high = glm::max(high, cell);
    }
    const auto first_x = static_cast<std::int32_t>(std::floor(low.x - kEdgeReachCells));
    const auto last_x = static_cast<std::int32_t>(std::floor(high.x + kEdgeReachCells));
    const auto first_y = static_cast<std::int32_t>(std::floor(low.y - kEdgeReachCells));
    const auto last_y = static_cast<std::int32_t>(std::floor(high.y + kEdgeReachCells));
    for (std::int32_t y = first_y; y <= last_y; ++y) {
      for (std::int32_t x = first_x; x <= last_x; ++x) {
        if (!grid.Inside(x, y)) continue;
        std::uint16_t mask = 0;
        for (std::int32_t row = 0; row < kSolidSamplesPerSide; ++row) {
          for (std::int32_t column = 0; column < kSolidSamplesPerSide; ++column) {
            const glm::vec2 sample(
                static_cast<float>(x) + (static_cast<float>(column) + kHalf) * kSampleStep,
                static_cast<float>(y) + (static_cast<float>(row) + kHalf) * kSampleStep);
            if (Covers(cells, area, sample)) {
              mask = static_cast<std::uint16_t>(
                  mask | (1U << static_cast<unsigned>(row * kSolidSamplesPerSide + column)));
            }
          }
        }
        const auto index =
            static_cast<std::size_t>(y - grid.south) * static_cast<std::size_t>(grid.width) +
            static_cast<std::size_t>(x - grid.west);
        grid.samples[index] = static_cast<std::uint16_t>(grid.samples[index] | mask);
      }
    }
  }
}

std::vector<PlannedPose> SlotSurveyPoses(const PieceModelSlot& slot,
                                         const GroundModelChoice& choice,
                                         ro::sim::FootprintSize drawn, std::uint8_t quarter) {
  std::vector<PlannedPose> poses;
  if (!slot.any_yaw) {
    poses.push_back(PieceSlotPose(slot, choice, drawn, quarter, 0.0F, 1.0F));
    return poses;
  }
  for (const float jitter : {1.0F - kPieceScaleJitter, 1.0F, 1.0F + kPieceScaleJitter}) {
    for (std::int32_t yaw = 0; yaw < kFullTurnDegrees; yaw += kSolidYawStepDegrees) {
      poses.push_back(PieceSlotPose(slot, choice, drawn, quarter, static_cast<float>(yaw), jitter));
    }
  }
  return poses;
}

SolidGrid PieceSolidGrid(const PieceLook& look, const ro::sim::TerrainPieceRule& rule,
                         std::uint8_t quarter, std::span<const ModelSolid> solids,
                         std::int32_t margin, std::vector<std::string_view>& missing) {
  const ro::sim::FootprintSize drawn = ro::sim::FootprintSizeOf(rule, 0);
  SolidGrid grid = SolidGrid::Around(ro::sim::FootprintSizeOf(rule, quarter), margin);
  for (const PieceModelSlot& slot : look.slots) {
    for (const GroundModelChoice& choice : slot.models) {
      const ModelSolid* solid = FindModelSolid(solids, choice);
      if (solid == nullptr) {
        if (std::find(missing.begin(), missing.end(), choice.model) == missing.end()) {
          missing.push_back(choice.model);
        }
        continue;
      }
      for (const PlannedPose& pose : SlotSurveyPoses(slot, choice, drawn, quarter)) {
        CoverModelSolid(*solid, pose, grid);
      }
    }
  }
  return grid;
}

FootprintAudit AuditFootprint(const SolidGrid& grid, const ro::sim::TerrainPieceRule& rule,
                              std::uint8_t quarter) {
  FootprintAudit audit;
  for (std::int32_t y = grid.south; y < grid.south + grid.height; ++y) {
    for (std::int32_t x = grid.west; x < grid.west + grid.width; ++x) {
      const bool covered = ro::sim::FootprintCovers(rule, quarter, x, y);
      const bool solid = grid.Solid(x, y);
      if (solid && !covered) ++audit.solid_outside;
      if (!covered && grid.SamplesAt(x, y) > 0) ++audit.touched_outside;
      if (solid && covered) ++audit.matched;
      if (!covered || grid.SamplesAt(x, y) > 0) continue;
      ++audit.empty_inside;
      if (!grid.Solid(x - 1, y) && !grid.Solid(x + 1, y) && !grid.Solid(x, y - 1) &&
          !grid.Solid(x, y + 1)) {
        ++audit.empty_far;
      }
    }
  }
  return audit;
}

}  // namespace moonclient
