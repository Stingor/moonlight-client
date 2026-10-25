#include "survivors/arena_scene.h"

#include <algorithm>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "ro/core/log.h"

#include "survivors/arena_night.h"

namespace moonclient {
namespace {

// LA CHAUFFE SE PEINT À CETTE DISTANCE DU HÉROS, en cases : hors du cadre de
// la caméra figée au nord à son plus grand recul, d'un côté comme de l'autre.
constexpr float kWarmAwayCells = 60.0F;

// L'horloge de présentation, ramenée à celle des renderers. Une partie ne dure
// pas quarante-neuf jours : la borne ne sert qu'à ne jamais boucler.
[[nodiscard]] std::uint32_t RendererMs(std::uint64_t presentation_ms) {
  return static_cast<std::uint32_t>(
      std::min<std::uint64_t>(presentation_ms, std::numeric_limits<std::uint32_t>::max()));
}

}  // namespace

ro::core::Result<void> ArenaScene::Open(ro::vfs::Vfs& vfs, const ro::sim::ArenaRule& arena,
                                        const MapStageRead& read) {
  const std::string_view map_name = arena.map_name;
  if (map_name.empty()) {
    return ro::core::Fail(ro::core::ErrorCode::kInvalidArgument,
                          "arène " + std::string(arena.key) + " : aucune carte nommée");
  }
  if (auto opened = lit_.Open(vfs, map_name, read); !opened) {
    return ro::core::Fail(opened.error().code, "arène " + std::string(map_name) + " : " +
                                                   ro::core::Describe(opened.error()));
  }
  lit_.set_sky_hour_imposed(true);
  ro::core::LogInfo("arène : {} montée", lit_.stage().name);
  return {};
}

LitFrame ArenaScene::Begin(const GraphicsOptions& graphics, std::uint64_t presentation_ms) {
  return lit_.Begin(graphics, graphics.fog(), RendererMs(presentation_ms));
}

void ArenaScene::Paint(LitFrame& frame, const ro::render::Camera& camera, float aspect_ratio,
                       const GraphicsOptions& graphics, std::uint64_t presentation_ms,
                       std::uint32_t now_ms, const HordeComposer& horde,
                       const ArenaOverlays& overlays) {
  const LitActors actors{.bodies = horde.drawn(), .sun_casters = horde.sun_casters()};
  painting_effects_ = overlays.effects;
  lit_.Paint(frame, camera, aspect_ratio, graphics, ArenaSkyHour(presentation_ms),
             RendererMs(presentation_ms), now_ms, actors, *this);
  painting_effects_ = nullptr;
  if (overlays.effects != nullptr) {
    overlays.effects->DrawAfterBodies(lit_.sprites(), camera, aspect_ratio, overlays.viewport_width,
                                      overlays.viewport_height, frame.fog,
                                      RendererMs(presentation_ms));
  }
  if (overlays.numbers != nullptr) overlays.numbers->Draw(lit_.sprites(), camera, aspect_ratio);
}

void ArenaScene::Warm(HordeComposer& horde, ArenaEffects& effects,
                      std::span<const std::uint16_t> species,
                      std::span<const std::uint16_t> skill_ids, glm::vec2 hero_cell,
                      const ro::render::Camera& camera, float aspect_ratio, float viewport_width,
                      float viewport_height, std::uint32_t now_ms) {
  // HORS CHAMP : au nord du héros, au-delà du bord haut de l'image, ou au sud,
  // derrière la caméra, quand le nord sort de la carte.
  const bool north_fits =
      !lit_.stage().gat.has_value() ||
      hero_cell.y + kWarmAwayCells < static_cast<float>(lit_.stage().gat->height());
  const glm::vec2 far_cell =
      hero_cell + glm::vec2(0.0F, north_fits ? kWarmAwayCells : -kWarmAwayCells);
  std::vector<HordePose> poses;
  poses.reserve(species.size());
  for (std::size_t index = 0; index < species.size(); ++index) {
    poses.push_back(HordePose{.kind = HordeBodyKind::kFoe,
                              .sim_id = static_cast<std::uint32_t>(index),
                              .look_id = species[index],
                              .motion = ro::render::Motion::kWalk,
                              .cell = far_cell});
  }
  horde.Compose(
      poses, [this](const HordePose& pose) { return GroundAtCell(pose.cell); }, HordeShadows{});
  if (!horde.drawn().empty()) lit_.sprites().Draw(camera, horde.drawn(), aspect_ratio);
  effects.Warm(skill_ids, GroundAtCell(far_cell), lit_.sprites(), camera, aspect_ratio,
               viewport_width, viewport_height, now_ms);
}

glm::vec3 ArenaScene::GroundAt(ro::sim::FieldPoint point) const {
  return GroundAtCell(
      CellOfField(glm::vec2(static_cast<float>(point.x), static_cast<float>(point.y))));
}

glm::vec3 ArenaScene::GroundAtCell(glm::vec2 cell) const {
  const std::array<float, 3> ground = GroundPositionAt(lit_.stage(), cell.x, cell.y);
  return glm::vec3(ground[0], ground[1], ground[2]);
}

void ArenaScene::DrawBehindBodies(const ro::render::Camera& camera, float aspect_ratio,
                                  const LitFrame& frame) {
  if (painting_effects_ != nullptr) {
    painting_effects_->DrawBehindBodies(camera, aspect_ratio, frame.fog, lit_.sprites());
  }
}

void ArenaScene::DrawOverWater(const ro::render::Camera& /*camera*/, float /*aspect_ratio*/,
                               const LitFrame& /*frame*/) {}

}  // namespace moonclient
