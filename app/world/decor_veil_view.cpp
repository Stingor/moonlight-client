#include "world/decor_veil_view.h"

#include <array>
#include <span>

#include "ro/render/model_sight.h"
#include "ro/sim/movement.h"

namespace moonclient {

void DecorVeilView::Advance(const DecorVeilFrame& frame) {
  if (!frame.enabled || frame.stage == nullptr || frame.camera == nullptr) {
    // ÉTEINTE, L'OPTION REND TOUT PLEIN d'un coup : un voile figé à mi-course
    // resterait translucide jusqu'à ce qu'on la rallume.
    if (enabled_) {
      veils_.Clear();
    }
    enabled_ = false;
    last_ms_.reset();
    return;
  }
  const MapStage& stage = *frame.stage;
  const Prepared now{.map_name = stage.name,
                     .models = stage.scene.models.data(),
                     .model_count = stage.scene.models.size()};
  if (!prepared_.has_value() || prepared_->map_name != now.map_name ||
      prepared_->models != now.models || prepared_->model_count != now.model_count) {
    veils_.Prepare(stage.scene, stage.gat.has_value() ? &*stage.gat : nullptr);
    prepared_ = now;
  } else if (!enabled_) {
    veils_.Clear();
  }
  enabled_ = true;
  const std::uint32_t elapsed_ms =
      last_ms_.has_value() && frame.now_ms > *last_ms_ ? frame.now_ms - *last_ms_ : 0;
  last_ms_ = frame.now_ms;

  const ro::render::ClientFrame client = ro::render::ClientFrameOf(stage.scene.gnd);
  std::optional<ro::render::SightSegment> segment;
  const ro::sim::Actor* player = frame.world != nullptr ? frame.world->player() : nullptr;
  if (player != nullptr && frame.planner != nullptr) {
    const ro::sim::FractionalCell cell =
        ro::sim::InterpolatedCell(*player, frame.planner->PathFor(*player), frame.now_ms);
    const std::array<float, 3> ground = GroundPositionAt(stage, cell.x, cell.y);
    segment =
        ro::render::PlayerSightSegment(*frame.camera, frame.aspect_ratio, frame.width, frame.height,
                                       glm::vec3(ground[0], ground[1], ground[2]), client);
  }
  veils_.Advance(static_cast<float>(elapsed_ms), segment,
                 ro::render::SightFrustumOf(*frame.camera, frame.aspect_ratio, client));
}

void DecorVeilView::Draw(const ro::render::ModelRenderer& models, const ro::render::Camera& camera,
                         const MapStage& stage, const ro::render::SceneLight& light,
                         float aspect_ratio, const ro::render::ModelClock* clock,
                         const ro::render::SceneFog& fog) const {
  const std::span<const ro::render::VeiledPose> veiled =
      enabled_ ? veils_.veiled() : std::span<const ro::render::VeiledPose>();
  models.DrawVeiled(camera, light, aspect_ratio, stage.scene.models, veiled, clock, fog);
}

}  // namespace moonclient
