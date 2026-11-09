#include "survivors/arena_trap_models.h"

#include <algorithm>

#include <glm/common.hpp>

#include "ro/core/log.h"
#include "ro/render/scene_light.h"

#include "survivors/horde_poses.h"
#include "survivors/survivors_looks.h"

namespace moonclient {
namespace {

// Les numéros des unités de l'arène : celui de la zone, au-delà de ceux que
// le monde privé donne à ses acteurs (le héros, une orbe par rang).
constexpr std::uint32_t kFirstTrapUnitId = 0x40000000U;

[[nodiscard]] std::uint32_t TrapUnitIdOf(const ro::sim::SurvivorsZone& zone) noexcept {
  return kFirstTrapUnitId + zone.serial;
}

}  // namespace

ArenaTrapModels::ArenaTrapModels(ro::vfs::Vfs& vfs) : units_(vfs) {
  if (auto renderer = ro::render::ModelRenderer::Create(); renderer) {
    renderer_ = std::move(*renderer);
  } else {
    ro::core::LogWarning("arène : les pièges posés ne se peignent pas — {}",
                         ro::core::Describe(renderer.error()));
  }
}

void ArenaTrapModels::Follow(ro::sim::World& world, std::span<const ro::sim::SurvivorsZone> zones,
                             const MapStage& stage, glm::ivec2 origin_cells, std::uint32_t now_ms) {
  now_ms_ = now_ms;
  std::vector<std::uint32_t> armed;
  for (const ro::sim::SurvivorsZone& zone : zones) {
    const std::optional<std::uint32_t> unit_id = TrapUnitOf(zone.skill_id);
    if (!unit_id.has_value()) continue;
    const std::uint32_t id = TrapUnitIdOf(zone);
    armed.push_back(id);
    if (std::ranges::find(placed_ids_, id) != placed_ids_.end()) continue;
    const glm::vec2 cell =
        CellOfField(glm::vec2(static_cast<float>(zone.from.x), static_cast<float>(zone.from.y))) -
        glm::vec2(origin_cells);
    // Un entier y est le CENTRE d'une case : le piège prend la plus proche.
    const glm::vec2 nearest = glm::round(cell);
    if (nearest.x < 0.0F || nearest.y < 0.0F) continue;
    world.PlaceSkillUnit(ro::sim::SkillUnit{
        .id = id,
        .cell = ro::sim::CellPosition{.x = static_cast<std::uint16_t>(nearest.x),
                                      .y = static_cast<std::uint16_t>(nearest.y)},
        .unit_id = *unit_id,
        .placed_ms = now_ms,
        .look_since_ms = now_ms});
    placed_ids_.push_back(id);
  }
  std::erase_if(placed_ids_, [&world, &armed](std::uint32_t id) {
    if (std::ranges::find(armed, id) != armed.end()) return false;
    static_cast<void>(world.RemoveSkillUnit(id));
    return true;
  });
  const std::vector<ro::render::ModelInstances>* changed = units_.Changed(stage, world);
  if (changed == nullptr || !renderer_.has_value()) return;
  if (auto uploaded = renderer_->Upload(*changed); !uploaded) {
    ro::core::LogWarning("arène : les pièges posés ne se téléversent pas — {}",
                         ro::core::Describe(uploaded.error()));
  }
}

void ArenaTrapModels::Forget(ro::sim::World& world) {
  for (const std::uint32_t id : placed_ids_) static_cast<void>(world.RemoveSkillUnit(id));
  placed_ids_.clear();
  units_.ForgetPlacement();
}

void ArenaTrapModels::Draw(const ro::sim::World& world, const ro::render::Camera& camera,
                           float aspect_ratio, const ro::render::SceneFog& fog) {
  if (!renderer_.has_value() || placed_ids_.empty()) return;
  renderer_->Draw(camera, ro::render::kGroundUnitModelLight, aspect_ratio,
                  ro::render::ModelStages(), &units_.Advance(world, now_ms_), fog);
}

}  // namespace moonclient
