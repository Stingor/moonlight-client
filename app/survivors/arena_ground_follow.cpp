#include "survivors/arena_ground_follow.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

#include "ro/render/terrain_mesh.h"

namespace moonclient {
namespace {

constexpr float kHalfWindowCells = static_cast<float>(kGroundWindowCells) / 2.0F;
// Le coin le plus à l'est (ou au nord) où la fenêtre tient encore dans le
// monde.
constexpr std::int32_t kLastOriginCells = ro::sim::kTerrainWorldCells - kGroundWindowCells;

// LE COIN SUR UN AXE : avancé du nombre de pas qui ramène l'écart à au plus un
// pas, du côté où le héros marchait.
[[nodiscard]] std::int32_t FollowingAxis(std::int32_t origin_cells, float offset_cells) {
  const auto step = static_cast<float>(kGroundRecentreCells);
  std::int32_t steps = 0;
  if (offset_cells > step) {
    steps = static_cast<std::int32_t>(std::floor((offset_cells - step) / step)) + 1;
  } else if (offset_cells < -step) {
    steps = -(static_cast<std::int32_t>(std::floor((-offset_cells - step) / step)) + 1);
  }
  const std::int32_t moved = origin_cells + steps * kGroundRecentreCells;
  return std::clamp(moved, std::int32_t{0}, kLastOriginCells);
}

// Le pas d'avance sur un axe : vers le côté où le héros a fait la moitié du
// chemin, ou aucun.
[[nodiscard]] std::int32_t AheadAxis(float offset_cells) {
  const auto half = static_cast<float>(kGroundAheadCells);
  if (offset_cells > half) return kGroundRecentreCells;
  if (offset_cells < -half) return -kGroundRecentreCells;
  return 0;
}

// LA FENÊTRE D'AVANCE PRÉPARÉE SERT ENCORE sur un axe tant que le héros reste
// du côté où elle avance ; sur un axe où elle n'avance pas, tant qu'il n'a pas
// fait la moitié du chemin. Une fenêtre à plus d'un pas n'est pas d'avance.
[[nodiscard]] bool AheadAxisStillServes(std::int32_t step_cells, float offset_cells) {
  if (std::abs(step_cells) > kGroundRecentreCells) return false;
  if (step_cells > 0) return offset_cells > 0.0F;
  if (step_cells < 0) return offset_cells < 0.0F;
  return std::abs(offset_cells) <= static_cast<float>(kGroundAheadCells);
}

}  // namespace

glm::vec2 GroundOffsetFromCentre(GroundWindowOrigin origin, glm::vec2 hero_cell) {
  return hero_cell - glm::vec2(static_cast<float>(origin.x_cells) + kHalfWindowCells,
                               static_cast<float>(origin.y_cells) + kHalfWindowCells);
}

GroundWindowOrigin GroundWindowFollowing(GroundWindowOrigin shown, glm::vec2 hero_cell) {
  const glm::vec2 offset = GroundOffsetFromCentre(shown, hero_cell);
  return GroundWindowOrigin{.x_cells = FollowingAxis(shown.x_cells, offset.x),
                            .y_cells = FollowingAxis(shown.y_cells, offset.y)};
}

std::optional<GroundWindowOrigin> GroundWindowAhead(GroundWindowOrigin shown, glm::vec2 hero_cell) {
  const glm::vec2 offset = GroundOffsetFromCentre(shown, hero_cell);
  const std::int32_t east = AheadAxis(offset.x);
  const std::int32_t north = AheadAxis(offset.y);
  if (east == 0 && north == 0) return std::nullopt;
  const GroundWindowOrigin ahead{
      .x_cells = std::clamp(shown.x_cells + east, std::int32_t{0}, kLastOriginCells),
      .y_cells = std::clamp(shown.y_cells + north, std::int32_t{0}, kLastOriginCells)};
  if (ahead == shown) return std::nullopt;
  return ahead;
}

bool GroundRecentreForced(GroundWindowOrigin shown, glm::vec2 hero_cell) {
  const glm::vec2 offset = GroundOffsetFromCentre(shown, hero_cell);
  const auto force = static_cast<float>(kGroundForceCells);
  return std::abs(offset.x) > force || std::abs(offset.y) > force;
}

glm::vec3 GroundWorldShift(GroundWindowOrigin from, GroundWindowOrigin to) {
  // Une case du monde se compte `to - from` cases plus près du coin nouveau :
  // son x recule d'autant, son z — le nord vers les z décroissants — avance.
  const auto east_cells = static_cast<float>(to.x_cells - from.x_cells);
  const auto north_cells = static_cast<float>(to.y_cells - from.y_cells);
  return glm::vec3(-east_cells * ro::render::kWorldUnitsPerCell, 0.0F,
                   north_cells * ro::render::kWorldUnitsPerCell);
}

GroundPreparedFate GroundPreparedFateOf(GroundWindowOrigin shown, GroundWindowOrigin prepared,
                                        glm::vec2 hero_cell, bool uploaded,
                                        bool shifts_without_trace) {
  const GroundWindowOrigin wanted = GroundWindowFollowing(shown, hero_cell);
  if (wanted != shown) {
    if (prepared != wanted) return GroundPreparedFate::kDrop;
    if (!uploaded) return GroundPreparedFate::kKeep;
    return shifts_without_trace || GroundRecentreForced(shown, hero_cell)
               ? GroundPreparedFate::kMount
               : GroundPreparedFate::kKeep;
  }
  return GroundAheadStillServes(shown, prepared, hero_cell) ? GroundPreparedFate::kKeep
                                                            : GroundPreparedFate::kDrop;
}

std::optional<float> GroundDescentReachOf(bool generated) noexcept {
  if (!generated) return std::nullopt;
  return static_cast<float>(kGroundDescentReachCells);
}

bool MountedArenaServes(const MountedArena& mounted, std::uint16_t arena_id,
                        std::uint64_t run_seed) noexcept {
  if (mounted.arena_id != arena_id) return false;
  return !mounted.generated ||
         (mounted.run_seed == run_seed && mounted.shown_origin == mounted.start_origin);
}

bool GroundAheadStillServes(GroundWindowOrigin shown, GroundWindowOrigin prepared,
                            glm::vec2 hero_cell) {
  if (prepared == shown) return false;
  if (prepared == GroundWindowAhead(shown, hero_cell)) return true;
  const glm::vec2 offset = GroundOffsetFromCentre(shown, hero_cell);
  return AheadAxisStillServes(prepared.x_cells - shown.x_cells, offset.x) &&
         AheadAxisStillServes(prepared.y_cells - shown.y_cells, offset.y);
}

}  // namespace moonclient
