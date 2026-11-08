// SAISIR UN MODÈLE PAR SON CORPS, et le glisser : voir `BodyDragKeys`.

#include "world/map_editor_grab.h"

#include <cmath>

#include "ro/render/map_effect_math.h"

#include "world/map_editor_session.h"

namespace moonclient {
namespace {

// Un tour complet, en degrés.
constexpr float kFullTurnDegrees = 360.0F;
// La part d'un pas en deçà de laquelle un angle est tenu pour un multiple : un
// angle saisi au chiffre près ne tombe pas toujours juste.
constexpr float kStepTolerance = 1e-3F;

}  // namespace

std::optional<glm::vec3> HorizontalHit(const ro::render::ScreenRay& ray, float world_height) {
  const float rise = ray.direction.y;
  const float run = glm::length(glm::vec2(ray.direction.x, ray.direction.z));
  if (std::abs(rise) <= kGrazingRaySlope * run) {
    return std::nullopt;
  }
  const float travel = (world_height - ray.origin.y) / rise;
  if (travel <= 0.0F) {
    return std::nullopt;
  }
  return ray.origin + ray.direction * travel;
}

ro::ui::map_editor::Place BodyDragged(const BodyAnchor& anchor, BodyDragKeys keys,
                                      glm::vec2 pointer,
                                      const std::optional<glm::vec3>& native_hit) {
  ro::ui::map_editor::Place placed = anchor.place;
  if (keys.tilt || keys.turn) {
    const glm::vec2 shift = pointer - anchor.pointer;
    if (keys.turn) {
      placed.rotation[kTurnAxis] += shift.x * kGizmoDegreesPerPixel;
    }
    if (keys.tilt) {
      placed.rotation[kTiltAxis] += shift.y * kGizmoDegreesPerPixel;
    }
    return placed;
  }
  if (anchor.native_hit.has_value() && native_hit.has_value()) {
    placed.position[0] += native_hit->x - anchor.native_hit->x;
    placed.position[2] += native_hit->z - anchor.native_hit->z;
  }
  return placed;
}

float SteppedAngle(float degrees, float step_degrees) {
  if (step_degrees == 0.0F) {
    return degrees;
  }
  const float step = std::abs(step_degrees);
  const float steps = degrees / step;
  const float reached = step_degrees > 0.0F ? (std::floor(steps + kStepTolerance) + 1.0F) * step
                                            : (std::ceil(steps - kStepTolerance) - 1.0F) * step;
  return std::remainder(reached, kFullTurnDegrees);
}

namespace {

// Le point du plan du glissé sous `ray`, dans le repère du RSW.
std::optional<glm::vec3> NativeHitOf(const ro::render::MapEffectFrame& map_frame,
                                     float world_height, const ro::render::ScreenRay& ray) {
  const std::optional<glm::vec3> hit = HorizontalHit(ray, world_height);
  return hit.has_value() ? std::optional(map_frame.ToNative(*hit)) : std::nullopt;
}

}  // namespace

void MapEditorSession::GrabModelBody(std::size_t model_index, glm::vec2 pointer,
                                     const ro::render::ScreenRay& ray,
                                     const ro::render::MapScene& scene) {
  const bool body_tool = tool_ == ro::ui::MapEditorTool::kMove ||
                         tool_ == ro::ui::MapEditorTool::kRotate ||
                         tool_ == ro::ui::MapEditorTool::kScale;
  if (!active_ || !body_tool || selection_.model_index != model_index ||
      model_index >= scene.rsw.models().size()) {
    return;
  }
  const ro::formats::RswModel& placed = scene.rsw.models()[model_index];
  // ⚠ UNE POSE MASQUÉE NE SE DÉSIGNE PAS : celle qu'on saisit se voit.
  const ro::ui::map_editor::Place start{.position = placed.position,
                                        .rotation = placed.rotation,
                                        .scale = placed.scale,
                                        .hidden = false,
                                        .released = false};
  const ro::render::MapEffectFrame map_frame{scene.gnd.width(), scene.gnd.height()};
  const float world_height =
      map_frame.ToWorld(glm::vec3(placed.position[0], placed.position[1], placed.position[2])).y;
  body_drag_ =
      BodyDrag{.map_frame = map_frame,
               .world_height = world_height,
               .from = pointer,
               .start = start,
               .last = start,
               .anchor = BodyAnchor{.place = start,
                                    .pointer = pointer,
                                    .native_hit = NativeHitOf(map_frame, world_height, ray)}};
}

void MapEditorSession::DragBodyTo(glm::vec2 pointer, const ro::render::ScreenRay& ray,
                                  BodyDragKeys keys) {
  if (!body_drag_.has_value()) {
    return;
  }
  BodyDrag& body = *body_drag_;
  if (!body.started) {
    if (glm::length(pointer - body.from) < kBodyDragDeadZonePixels) {
      return;
    }
    body.started = true;
  }
  const std::optional<glm::vec3> native_hit = NativeHitOf(body.map_frame, body.world_height, ray);
  // UNE TOUCHE PRISE OU LÂCHÉE réancre le geste là où il en est ; un plan
  // qu'on ne voyait pas à l'ancrage se rattrape dès qu'il reparaît.
  const bool reshaped = keys.tilt != body.keys.tilt || keys.turn != body.keys.turn;
  const bool sliding = !keys.tilt && !keys.turn;
  if (reshaped || (sliding && !body.anchor.native_hit.has_value())) {
    body.anchor = BodyAnchor{.place = body.last, .pointer = pointer, .native_hit = native_hit};
  }
  body.keys = keys;
  ro::ui::map_editor::Place placed = BodyDragged(body.anchor, keys, pointer, native_hit);
  if (keys.snap) {
    placed = Snapped(ro::ui::MapEditorTool::kMove, body.start, placed);
    placed = Snapped(ro::ui::MapEditorTool::kRotate, body.start, placed);
  }
  placed.released = false;
  body.last = placed;
  queued_.emplace_back(placed);
}

bool MapEditorSession::ReleaseBody() {
  if (!body_drag_.has_value()) {
    return false;
  }
  // UN CLIC SANS GLISSÉ n'a fait que désigner : il n'entre pas dans l'historique.
  if (body_drag_->started) {
    ro::ui::map_editor::Place last = body_drag_->last;
    last.released = true;
    queued_.emplace_back(last);
  }
  body_drag_.reset();
  return true;
}

}  // namespace moonclient
