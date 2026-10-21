#include "world/scene_graphics.h"

namespace moonclient {

ro::render::TerrainStages TerrainStagesOf(const GraphicsOptions& graphics, bool lightmap_hidden) {
  ro::render::TerrainStages stages;
  stages.cold_light = graphics.cold_light();
  const bool lightmaps = graphics.lightmaps() && !lightmap_hidden;
  stages.shadowmap = lightmaps;
  stages.lightmap = lightmaps;
  return stages;
}

bool SunShadowsApply(const GraphicsOptions& graphics, const ro::render::TerrainStages& stages,
                     bool indoor) {
  return graphics.sun_shadows() && stages.shadowmap && !stages.flat_color.has_value() && !indoor;
}

ro::render::ModelClockBlend ModelClockBlendOf(const GraphicsOptions& graphics) {
  return graphics.frame_interpolation() ? ro::render::ModelClockBlend::kBetweenSteps
                                        : ro::render::ModelClockBlend::kWholeSteps;
}

ActorLighting ActorLightingOf(const GraphicsOptions& graphics,
                              const ro::render::GroundLight* ground,
                              const AilmentTints* ailment_tints) {
  return ActorLighting{.ground = graphics.lightmaps() ? ground : nullptr,
                       .cold = graphics.cold_light(),
                       .ailment_tints = ailment_tints,
                       .element_glow = graphics.element_glow()};
}

void TimeComposedScene(const GraphicsOptions& graphics, ComposedScene& scene) {
  scene.corpse_fade_ms = graphics.corpse_fade_ms();
  scene.appear_fade_ms = graphics.appear_fade_ms();
  scene.leave_fade_ms = graphics.leave_fade_ms();
  scene.readable_motion = ReadableMotion{
      .skill_gesture_floor_ms = static_cast<float>(graphics.skill_gesture_floor_ms()),
      .cast_spans_bar = graphics.cast_spans_bar()};
}

ro::render::SpinBlurFrame SpinBlurOfFrame(ro::render::CameraSpinMeter& meter,
                                          const GraphicsOptions& graphics,
                                          const ro::render::Camera& camera,
                                          float shake_pitch_degrees, bool dragged,
                                          std::uint32_t now_ms, float aspect_ratio) {
  const ro::render::CameraTurnRate rate = meter.Measure(
      camera.yaw_degrees, camera.pitch_degrees - shake_pitch_degrees, dragged, now_ms);
  return ro::render::SpinBlurFrameOf(graphics.spin_blur(), rate, camera, aspect_ratio);
}

}  // namespace moonclient
