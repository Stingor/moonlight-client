#include "ro/render/navi_trail.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <numbers>

namespace ro::render {
namespace {

// L'angle d'un huitième de tour, en radians.
constexpr double kOctantRadians = std::numbers::pi / 4.0;
// La direction 0 regarde vers −Y (le sud), soit un quart de tour avant l'axe des x ;
// les suivantes tournent dans le sens HORAIRE, comme celles des acteurs.
constexpr double kDirectionZeroRadians = -std::numbers::pi / 2.0;

constexpr std::array<std::uint8_t, 4> kNaviTrailColor(std::uint8_t alpha) {
  return {0xFF, 0xFF, 0xFF, alpha};
}

}  // namespace

std::optional<std::uint8_t> NaviTrailDirectionOf(std::int32_t delta_x, std::int32_t delta_y) {
  if (delta_x == 0 && delta_y == 0) {
    return std::nullopt;
  }
  const double angle = std::atan2(static_cast<double>(delta_y), static_cast<double>(delta_x));
  const auto octant =
      static_cast<std::int32_t>(std::lround((kDirectionZeroRadians - angle) / kOctantRadians));
  const auto directions = static_cast<std::int32_t>(kNaviTrailDirections);
  return static_cast<std::uint8_t>(((octant % directions) + directions) % directions);
}

std::vector<NaviTrailMark> ComposeNaviTrail(std::span<const glm::ivec2> path_cells,
                                            std::uint32_t now_ms, std::size_t reach_cells) {
  std::vector<NaviTrailMark> marks;
  if (path_cells.size() < 2 || reach_cells == 0) {
    return marks;
  }
  const std::size_t last = path_cells.size() - 1;
  const std::uint32_t wave_phase = (now_ms / kNaviTrailWaveStepMs) % kNaviTrailWaveRanks;
  const std::size_t farthest = std::min(last - 1, reach_cells - 1);
  // La première case porteuse est celle dont la distance à la cible, en cases,
  // est impaire : la cible elle-même n'en porte pas, et la parité ne change pas
  // quand le joueur avance sur la trace.
  const std::size_t first = (last % kNaviTrailCellsPerMark == 1) ? 0 : 1;
  for (std::size_t rank = first; rank <= farthest; rank += kNaviTrailCellsPerMark) {
    const glm::ivec2 here = path_cells[rank];
    const glm::ivec2 next = path_cells[rank + 1];
    const std::optional<std::uint8_t> direction =
        NaviTrailDirectionOf(next.x - here.x, next.y - here.y);
    if (!direction.has_value()) {
      continue;
    }
    // Le rang de la flèche, compté depuis la cible. La vague éclaire un rang
    // qui DÉCROÎT avec le temps : elle court du joueur vers la cible.
    const auto from_goal = static_cast<std::uint32_t>((last - 1 - rank) / kNaviTrailCellsPerMark);
    const bool lit = (from_goal + wave_phase + 1) % kNaviTrailWaveRanks == 0;
    marks.push_back(NaviTrailMark{.cell_x = here.x,
                                  .cell_y = here.y,
                                  .direction = *direction,
                                  .alpha = lit ? kNaviTrailWaveAlpha : kNaviTrailAlpha});
  }
  return marks;
}

std::optional<GroundQuad> ComposeNaviTrailQuad(const formats::GndMap& gnd,
                                               const formats::GatMap& gat,
                                               const NaviTrailMark& mark) {
  std::optional<GroundQuad> quad =
      ComposeGroundCellQuad(gnd, gat, mark.cell_x, mark.cell_y, kNaviTrailColor(mark.alpha));
  if (!quad.has_value()) {
    return std::nullopt;
  }
  for (glm::vec2& uv : quad->texture_uv) {
    uv.y = 1.0F - uv.y;
  }
  return quad;
}

std::string NaviTrailTexturePath(int icon_set, std::uint8_t direction) {
  const int set = std::clamp(icon_set, kNaviTrailFirstIconSet, kNaviTrailLastIconSet);
  std::string path(kNaviTrailTextureFolder);
  path += std::format("navi_grid{}_{}.tga", set, static_cast<int>(direction));
  return path;
}

}  // namespace ro::render
