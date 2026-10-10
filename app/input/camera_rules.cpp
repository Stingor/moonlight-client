#include "input/camera_rules.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <vector>

namespace moonclient {
namespace {

constexpr float kZoomNearest = 200.0F;
constexpr float kZoomFarthestOutdoor = 500.0F;
constexpr float kZoomFarthestExpanded = 1000.0F;
constexpr float kZoomFarthestIndoor = 300.0F;

// Plancher : centre − écart. Plafond : écart + décalage, sans Ctrl ou Ctrl tenu.
constexpr float kAltitudeCenter = -45.0F;
constexpr float kAltitudeSpanOutdoor = 65.0F;
constexpr float kAltitudeSpanIndoor = 10.0F;
constexpr float kCeilingOffset = -70.0F;
constexpr float kCeilingOffsetCtrl = -45.0F;

constexpr float kYawPerPixel = 1.0F;
constexpr float kVerticalPerPixel = 0.5F;
constexpr float kZoomPerWheelEvent = 24.3F;
constexpr float kAltitudePerWheelEvent = 5.0F;

constexpr float kResetZoom = 300.0F;
constexpr float kResetAltitude = -50.0F;
constexpr float kResetYaw = 0.0F;

constexpr float kIndoorYaw = 45.0F;
constexpr float kIndoorYawSpan = 20.0F;

constexpr float kFullTurn = 360.0F;

// Même sans bornes : au zénith, la matrice de vue dégénère.
constexpr float kFreeAltitude = ro::render::kMaxPitchDegrees;

// La caméra la plus haute, bornée. Le plancher du jeu dehors (−110) va au-delà.
constexpr float kLowestAltitude = -70.0F;

constexpr float kFirstAltitude = -ro::render::kGameElevationDegrees;

// `nom#` pour les cartes intérieures, `nom#neuf nombres#` pour les bornes propres.
constexpr char kFieldEnd = '#';
constexpr std::string_view kCommentStart = "//";
constexpr std::string_view kMapExtension = ".rsw";
constexpr std::size_t kViewpointNumbers = 9;

constexpr std::size_t kInstanceAtLeast = 4;
constexpr std::size_t kInstanceBack = 4;
constexpr std::size_t kInstanceCut = 3;

[[nodiscard]] float Clamped(float value, float floor, float ceiling) {
  // Le plancher, puis le plafond : quand ils se croisent, le plafond gagne.
  if (floor > value) {
    value = floor;
  }
  if (value > ceiling) {
    value = ceiling;
  }
  return value;
}

[[nodiscard]] float Turned(float yaw_degrees) {
  const float turned = std::fmod(yaw_degrees, kFullTurn);
  return turned < 0.0F ? turned + kFullTurn : turned;
}

[[nodiscard]] std::string_view Trimmed(std::string_view text) {
  const auto blank = [](char letter) {
    return letter == ' ' || letter == '\t' || letter == '\r' || letter == '\n';
  };
  while (!text.empty() && blank(text.front())) {
    text.remove_prefix(1);
  }
  while (!text.empty() && blank(text.back())) {
    text.remove_suffix(1);
  }
  return text;
}

// Les champs d'une ligne, sans ce qui suit un commentaire.
[[nodiscard]] std::vector<std::string_view> FieldsOf(std::string_view line) {
  if (const std::size_t comment = line.find(kCommentStart); comment != std::string_view::npos) {
    line = line.substr(0, comment);
  }
  std::vector<std::string_view> fields;
  std::size_t start = 0;
  for (std::size_t at = 0; at < line.size(); ++at) {
    if (line[at] == kFieldEnd) {
      fields.push_back(Trimmed(line.substr(start, at - start)));
      start = at + 1;
    }
  }
  return fields;
}

template <typename Visit>
void ForEachLine(std::string_view text, Visit visit) {
  while (!text.empty()) {
    const std::size_t end = text.find('\n');
    visit(text.substr(0, end));
    if (end == std::string_view::npos) {
      break;
    }
    text.remove_prefix(end + 1);
  }
}

[[nodiscard]] std::optional<std::int16_t> NumberOf(std::string_view field) {
  std::int16_t value = 0;
  const char* const end = field.data() + field.size();
  const auto [stop, error] = std::from_chars(field.data(), end, value);
  if (error != std::errc() || stop != end) {
    return std::nullopt;
  }
  return value;
}

[[nodiscard]] std::optional<ViewpointRow> RowOf(const std::vector<std::string_view>& fields) {
  if (fields.size() < kViewpointNumbers + 1) {
    return std::nullopt;
  }
  std::array<std::int16_t, kViewpointNumbers> numbers{};
  for (std::size_t rank = 0; rank < kViewpointNumbers; ++rank) {
    const std::optional<std::int16_t> number = NumberOf(fields[rank + 1]);
    if (!number.has_value()) {
      return std::nullopt;
    }
    numbers[rank] = *number;
  }
  return ViewpointRow{.range = numbers[0],
                      .scope = numbers[1],
                      .range_on_entry = numbers[2],
                      .rotation_from = numbers[3],
                      .rotation_to = numbers[4],
                      .rotation_on_entry = numbers[5],
                      .altitude_from = numbers[6],
                      .altitude_to = numbers[7],
                      .altitude_on_entry = numbers[8]};
}

[[nodiscard]] float LowerAltitudeBound(const CameraLimits& limits) {
  return limits.bounded ? limits.altitude_floor : -kFreeAltitude;
}

// Ctrl élargit le plafond, sauf sur une carte à bornes propres ou au-delà de
// `ctrl_zoom_limit`.
[[nodiscard]] float UpperAltitudeBound(const CameraLimits& limits, const NativeView& view,
                                       bool ctrl_held) {
  if (!limits.bounded) {
    return kFreeAltitude;
  }
  if (!limits.rules.viewpoint.has_value() && ctrl_held && limits.ctrl_zoom_limit >= view.zoom) {
    return limits.altitude_ceiling_ctrl;
  }
  return limits.altitude_ceiling;
}

[[nodiscard]] float ZoomClamped(float zoom, const CameraLimits& limits) {
  if (!limits.bounded) {
    return Clamped(zoom, ro::render::kMinCameraDistance, ro::render::kMaxCameraDistance);
  }
  return Clamped(zoom, limits.zoom_nearest, limits.zoom_farthest);
}

[[nodiscard]] float YawDragged(float yaw, float dx_px, const CameraLimits& limits) {
  const float wanted = yaw + dx_px * kYawPerPixel;
  if (!limits.bounded) {
    return wanted;
  }
  if (limits.rules.viewpoint.has_value()) {
    const ViewpointRow& row = *limits.rules.viewpoint;
    return std::min(
        static_cast<float>(row.rotation_to),
        std::max(static_cast<float>(row.rotation_from), Turned(yaw) + dx_px * kYawPerPixel));
  }
  if (limits.rules.indoor) {
    return std::min(kIndoorYaw + kIndoorYawSpan,
                    std::max(kIndoorYaw - kIndoorYawSpan, Turned(yaw) + dx_px * kYawPerPixel));
  }
  return wanted;
}

[[nodiscard]] NativeView EntryViewOf(const ViewpointRow& row) {
  return NativeView{.zoom = static_cast<float>(row.range_on_entry),
                    .altitude_degrees = static_cast<float>(row.altitude_on_entry),
                    .yaw_degrees = static_cast<float>(row.rotation_on_entry)};
}

[[nodiscard]] bool ResetsAllowed(const CameraLimits& limits) {
  return !limits.bounded || !limits.rules.viewpoint.has_value();
}

}  // namespace

NativeView NativeViewOf(const ro::render::Camera& camera) {
  return NativeView{.zoom = camera.distance,
                    .altitude_degrees = -camera.pitch_degrees,
                    .yaw_degrees = -camera.yaw_degrees};
}

void ApplyNativeView(ro::render::Camera& camera, const NativeView& view) {
  camera.distance = view.zoom;
  camera.pitch_degrees = -view.altitude_degrees;
  camera.yaw_degrees = -view.yaw_degrees;
}

std::string CameraTableKey(std::string_view map_name) {
  std::string key(map_name);
  for (char& letter : key) {
    if (letter >= 'A' && letter <= 'Z') {
      letter = static_cast<char>(letter - 'A' + 'a');
    }
  }
  if (const std::size_t at = key.rfind('@'); at != std::string::npos && at >= kInstanceAtLeast) {
    key.erase(at - kInstanceBack, kInstanceCut);
  }
  if (!key.ends_with(kMapExtension)) {
    key += kMapExtension;
  }
  return key;
}

MapCameraTables MapCameraTables::Parse(std::string_view indoor_text,
                                       std::string_view viewpoint_text) {
  MapCameraTables tables;
  ForEachLine(indoor_text, [&tables](std::string_view line) {
    for (const std::string_view field : FieldsOf(line)) {
      if (!field.empty()) {
        tables.indoor_.insert(CameraTableKey(field));
      }
    }
  });
  ForEachLine(viewpoint_text, [&tables](std::string_view line) {
    const std::vector<std::string_view> fields = FieldsOf(line);
    if (fields.empty() || fields.front().empty()) {
      return;
    }
    if (const std::optional<ViewpointRow> row = RowOf(fields); row.has_value()) {
      tables.viewpoints_.insert_or_assign(CameraTableKey(fields.front()), *row);
    }
  });
  return tables;
}

MapCameraRules MapCameraTables::RulesFor(std::string_view map_name) const {
  const std::string key = CameraTableKey(map_name);
  MapCameraRules rules;
  rules.indoor = indoor_.contains(key);
  if (const auto found = viewpoints_.find(key); found != viewpoints_.end()) {
    rules.viewpoint = found->second;
  }
  return rules;
}

CameraLimits LimitsFor(const MapCameraRules& rules, bool expanded_zoom, bool bounded) {
  CameraLimits limits;
  limits.bounded = bounded;
  limits.rules = rules;
  limits.ctrl_zoom_limit = expanded_zoom ? kZoomFarthestExpanded : kZoomFarthestOutdoor;
  const float span = rules.indoor ? kAltitudeSpanIndoor : kAltitudeSpanOutdoor;
  limits.zoom_nearest = kZoomNearest;
  limits.zoom_farthest = rules.indoor ? kZoomFarthestIndoor : limits.ctrl_zoom_limit;
  limits.altitude_floor = std::max(kAltitudeCenter - span, kLowestAltitude);
  limits.altitude_ceiling = span + kCeilingOffset;
  limits.altitude_ceiling_ctrl = span + kCeilingOffsetCtrl;
  if (rules.viewpoint.has_value()) {
    const ViewpointRow& row = *rules.viewpoint;
    limits.zoom_nearest = row.range;
    limits.zoom_farthest = static_cast<float>(row.range + row.scope);
    limits.altitude_floor = row.altitude_to;
    limits.altitude_ceiling = row.altitude_from;
    limits.altitude_ceiling_ctrl = row.altitude_from;
  }
  return limits;
}

void DragCamera(ro::render::Camera& camera, const CameraDrag& drag, const CameraLimits& limits,
                bool ctrl_held) {
  NativeView view = NativeViewOf(camera);
  if (drag.orbit) {
    view.yaw_degrees = YawDragged(view.yaw_degrees, drag.dx_px, limits);
  }
  if (drag.tilt) {
    view.altitude_degrees =
        Clamped(view.altitude_degrees - drag.dy_px * kVerticalPerPixel, LowerAltitudeBound(limits),
                UpperAltitudeBound(limits, view, ctrl_held));
  }
  if (drag.zoom) {
    view.zoom = ZoomClamped(view.zoom - drag.dy_px * kVerticalPerPixel, limits);
  }
  ApplyNativeView(camera, view);
}

void WheelZoomCamera(ro::render::Camera& camera, bool forward, const CameraLimits& limits) {
  NativeView view = NativeViewOf(camera);
  view.zoom = ZoomClamped(view.zoom + (forward ? kZoomPerWheelEvent : -kZoomPerWheelEvent), limits);
  ApplyNativeView(camera, view);
}

void WheelTiltCamera(ro::render::Camera& camera, bool forward, const CameraLimits& limits,
                     bool ctrl_held) {
  NativeView view = NativeViewOf(camera);
  view.altitude_degrees =
      Clamped(view.altitude_degrees + (forward ? kAltitudePerWheelEvent : -kAltitudePerWheelEvent),
              LowerAltitudeBound(limits), UpperAltitudeBound(limits, view, ctrl_held));
  ApplyNativeView(camera, view);
}

bool ResetCameraZoom(ro::render::Camera& camera, const CameraLimits& limits) {
  if (!ResetsAllowed(limits)) {
    return false;
  }
  NativeView view = NativeViewOf(camera);
  view.zoom = kResetZoom;
  ApplyNativeView(camera, view);
  return true;
}

bool ResetCameraTilt(ro::render::Camera& camera, const CameraLimits& limits) {
  if (!ResetsAllowed(limits)) {
    return false;
  }
  NativeView view = NativeViewOf(camera);
  view.altitude_degrees = kResetAltitude;
  ApplyNativeView(camera, view);
  return true;
}

bool ResetCameraYaw(ro::render::Camera& camera, const CameraLimits& limits) {
  if (!ResetsAllowed(limits) || limits.rules.indoor) {
    return false;
  }
  NativeView view = NativeViewOf(camera);
  view.yaw_degrees = kResetYaw;
  ApplyNativeView(camera, view);
  return true;
}

void ResetCamera(ro::render::Camera& camera, const CameraLimits& limits) {
  NativeView view = NativeViewOf(camera);
  if (limits.rules.viewpoint.has_value()) {
    view = EntryViewOf(*limits.rules.viewpoint);
  } else {
    view = NativeView{.zoom = kResetZoom,
                      .altitude_degrees = kResetAltitude,
                      .yaw_degrees = limits.rules.indoor ? kIndoorYaw : kResetYaw};
  }
  ApplyNativeView(camera, view);
}

CameraMemory::CameraMemory()
    : outdoor{.zoom = kNativeDefaultZoom, .altitude_degrees = kFirstAltitude},
      indoor{.zoom = kNativeDefaultZoom, .altitude_degrees = kFirstAltitude} {}

void RememberCamera(CameraMemory& memory, const ro::render::Camera& camera,
                    const CameraLimits& leaving) {
  NativeView& kept = leaving.rules.indoor ? memory.indoor : memory.outdoor;
  kept = NativeViewOf(camera);
  memory.left_indoor = leaving.rules.indoor;
}

void ArriveWithCamera(ro::render::Camera& camera, const CameraMemory& memory,
                      const CameraLimits& arriving) {
  // Dehors, le lacet est gardé — sauf en sortant d'un intérieur, dont le lacet
  // imposé resterait sinon figé sur la carte suivante : on reprend alors celui
  // qu'on avait dehors avant d'entrer.
  NativeView view = NativeViewOf(camera);
  const NativeView& kept = arriving.rules.indoor ? memory.indoor : memory.outdoor;
  view.zoom = Clamped(kept.zoom, kZoomNearest,
                      arriving.rules.indoor ? kZoomFarthestIndoor : arriving.ctrl_zoom_limit);
  view.altitude_degrees =
      Clamped(kept.altitude_degrees, arriving.altitude_floor, arriving.altitude_ceiling_ctrl);
  if (arriving.rules.indoor) {
    view.yaw_degrees = kIndoorYaw;
  } else if (memory.left_indoor) {
    view.yaw_degrees = memory.outdoor.yaw_degrees;
  }
  if (arriving.rules.viewpoint.has_value()) {
    view = EntryViewOf(*arriving.rules.viewpoint);
  }
  ApplyNativeView(camera, view);
}

}  // namespace moonclient
