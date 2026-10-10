#include "ro/render/grey_world.h"

#include <algorithm>
#include <cmath>

#include "ro/render/map_effect_math.h"
#include "ro/render/terrain_mesh.h"

namespace ro::render {
namespace {

// Le carreau plein n'est pas un fichier : voir `GreyWorldTexturePath`.
constexpr std::string_view kSolidTexture = "<aplat>\\greyworld";

constexpr float kPercentFull = 100.0F;
constexpr float kChannelFull = 255.0F;
// Un cube du .gnd couvre deux cellules de la grille sur chaque axe.
constexpr std::int32_t kCellsPerCubeSide = 2;
constexpr std::size_t kPixelChannels = 4;
constexpr std::uint8_t kOpaque = 255;
// L'alpha d'une couleur empaquetée, dans l'ordre de `SkillAreaColorBytes`.
constexpr std::size_t kAlphaByte = 3;

// La famille d'une cellule, ou rien hors de la grille : le bord de la carte
// est un changement de terrain comme un autre, et le contour le garde.
[[nodiscard]] std::optional<CellFamily> FamilyAt(const formats::GatMap& gat, std::int32_t x,
                                                 std::int32_t y) {
  if (!gat.InBounds(x, y)) {
    return std::nullopt;
  }
  return CellFamilyOf(gat.at(x, y).type);
}

[[nodiscard]] bool BordersOtherTerrain(const formats::GatMap& gat, std::int32_t x, std::int32_t y,
                                       CellFamily family) {
  const std::optional<CellFamily> own = family;
  return FamilyAt(gat, x - 1, y) != own || FamilyAt(gat, x + 1, y) != own ||
         FamilyAt(gat, x, y - 1) != own || FamilyAt(gat, x, y + 1) != own;
}

}  // namespace

CellFamily CellFamilyOf(std::int32_t file_type) noexcept {
  const auto type =
      static_cast<std::int32_t>(static_cast<std::uint32_t>(file_type) & ~formats::kGatWaterFlag);
  if (type == static_cast<std::int32_t>(formats::GatCellType::kBlocked)) {
    return CellFamily::kWall;
  }
  if (type == static_cast<std::int32_t>(formats::GatCellType::kBlockedSnipable)) {
    return CellFamily::kSnipeable;
  }
  return CellFamily::kWalkable;
}

bool GreyWorldHidesModels(const GreyWorldLook& look) noexcept {
  return look.enabled && (look.hide_models || look.flatten);
}

bool GreyWorldHidesLightmap(const GreyWorldLook& look) noexcept {
  return GreyWorldHidesModels(look) && look.hide_lightmap;
}

bool GreyWorldHidesMapEffects(const GreyWorldLook& look) noexcept {
  return look.enabled && look.hide_map_effects;
}

bool GreyWorldShowsGrid(const GreyWorldLook& look) noexcept { return look.enabled && look.grid; }

bool GreyWorldPaintsGround(const GreyWorldLook& look) noexcept {
  return look.enabled && look.flat_ground;
}

bool GreyWorldFlattens(const GreyWorldLook& look) noexcept { return look.enabled && look.flatten; }

std::array<float, 3> GreyWorldGroundColor(const GreyWorldLook& look) noexcept {
  const std::array<std::uint8_t, 4> bytes = SkillAreaColorBytes(look.ground_color);
  return {static_cast<float>(bytes[0]) / kChannelFull, static_cast<float>(bytes[1]) / kChannelFull,
          static_cast<float>(bytes[2]) / kChannelFull};
}

float GreyWorldCellFill(const GreyWorldLook& look) noexcept {
  if (!GreyWorldIsPainted(GreyWorldTexturePath(look.pattern))) {
    return 1.0F;
  }
  const float gap =
      std::clamp(look.gap_percent, kSmallestGreyWorldGapPercent, kLargestGreyWorldGapPercent);
  return 1.0F - gap / kPercentFull;
}

std::string_view GreyWorldTexturePath(SkillAreaPattern pattern) noexcept {
  // ⚠ UN RANG INCONNU — un fichier de réglages écrit à la main — PREND LE
  // CARREAU PLEIN, le défaut, plutôt qu'un chemin qui ne se lirait pas.
  switch (pattern) {
    case SkillAreaPattern::kCorners:
    case SkillAreaPattern::kFrame:
      return SkillAreaTexturePath(pattern);
    case SkillAreaPattern::kSolid:
      break;
  }
  return kSolidTexture;
}

bool GreyWorldIsPainted(std::string_view texture_path) noexcept {
  return texture_path == kSolidTexture;
}

formats::Image ComposeGreyWorldTile() {
  formats::Image tile;
  tile.width = 1;
  tile.height = 1;
  tile.pixels.assign(kPixelChannels, kOpaque);
  return tile;
}

bool GroundHasSurface(const formats::GndMap& gnd, std::int32_t cell_x,
                      std::int32_t cell_y) noexcept {
  if (cell_x < 0 || cell_y < 0) {
    return false;
  }
  const std::int32_t cube_x = cell_x / kCellsPerCubeSide;
  const std::int32_t cube_y = cell_y / kCellsPerCubeSide;
  if (cube_x >= gnd.width() || cube_y >= gnd.height()) {
    return false;
  }
  return gnd.at(cube_x, cube_y).tile_up >= 0;
}

void ComposeGreyWorldCells(const formats::GndMap& gnd, const formats::GatMap& gat,
                           std::int32_t center_x, std::int32_t center_y, const GreyWorldLook& look,
                           std::vector<GroundQuad>& quads) {
  quads.clear();
  const std::int32_t radius =
      std::clamp(look.radius_cells, kSmallestGreyWorldRadiusCells, kLargestGreyWorldRadiusCells);
  const std::int32_t first_x = std::max(center_x - radius, 0);
  const std::int32_t first_y = std::max(center_y - radius, 0);
  const std::int32_t last_x = std::min(center_x + radius, gat.width() - 1);
  const std::int32_t last_y = std::min(center_y + radius, gat.height() - 1);
  const float fill = GreyWorldCellFill(look);
  const std::array<std::uint8_t, 4> walk = SkillAreaColorBytes(look.walk_color);
  const std::array<std::uint8_t, 4> wall = SkillAreaColorBytes(look.wall_color);
  const std::array<std::uint8_t, 4> snipe = SkillAreaColorBytes(look.snipe_color);
  // 🔴 UN FILTRE QUI EFFACERAIT TOUT NE PROUVERAIT RIEN : un sol sans aucune face
  // du dessus est un fichier mal lu, pas une carte sans sol. On dessine alors
  // tout plutôt que de faire disparaître le quadrillage en silence.
  const bool ground_known = std::ranges::any_of(
      gnd.cubes(), [](const formats::GndCube& cube) { return cube.tile_up >= 0; });

  for (std::int32_t y = first_y; y <= last_y; ++y) {
    for (std::int32_t x = first_x; x <= last_x; ++x) {
      // Le test le moins cher d'abord : pas de sol, rien à poser.
      if (ground_known && !GroundHasSurface(gnd, x, y)) {
        continue;
      }
      const CellFamily family = CellFamilyOf(gat.at(x, y).type);
      if (look.fill == GreyWorldFill::kOutline && !BordersOtherTerrain(gat, x, y, family)) {
        continue;
      }
      const std::array<std::uint8_t, 4>& color = family == CellFamily::kWall        ? wall
                                                 : family == CellFamily::kSnipeable ? snipe
                                                                                    : walk;
      // Une famille transparente : le joueur l'a éteinte, rien à soumettre.
      if (color[kAlphaByte] == 0) {
        continue;
      }
      if (std::optional<GroundQuad> quad = ComposeGroundCellQuad(gnd, gat, x, y, color, fill)) {
        quads.push_back(*quad);
      }
    }
  }
}

std::optional<float> FlatReliefLevel(const formats::GndMap& gnd) {
  double sum = 0.0;
  std::size_t corners = 0;
  for (const formats::GndCube& cube : gnd.cubes()) {
    if (cube.tile_up < 0) {
      continue;
    }
    for (const float height : cube.corner_height) {
      sum += static_cast<double>(height);
    }
    corners += cube.corner_height.size();
  }
  if (corners == 0) {
    return std::nullopt;
  }
  return static_cast<float>(sum / static_cast<double>(corners));
}

void ReseatMapEffects(formats::RswScene& rsw, const formats::GndMap& ground, float level) {
  const MapEffectFrame frame{ground.width(), ground.height()};
  const std::size_t effect_count = rsw.effects().size();
  for (std::size_t index = 0; index < effect_count; ++index) {
    std::array<float, 3> position = rsw.effects()[index].position;
    const glm::vec3 world = frame.ToWorld(glm::vec3(position[0], position[1], position[2]));
    const std::array<float, 2> cell = FractionalCellAt(ground, world.x, world.z);
    if (!GroundHasSurface(ground, static_cast<std::int32_t>(std::lround(cell[0])),
                          static_cast<std::int32_t>(std::lround(cell[1])))) {
      continue;
    }
    // Le fichier compte la hauteur vers le BAS, le monde vers le haut : le sol
    // sous l'objet, dans l'unité du fichier, est l'opposé de son altitude.
    const float ground_below = -GroundHeightAt(ground, world.x, world.z);
    position[1] += level - ground_below;
    rsw.MoveEffect(index, position);
  }
}

}  // namespace ro::render
