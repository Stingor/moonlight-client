#include "ro/render/hat_effect_anchor.h"

#include <algorithm>
#include <cmath>
#include <span>

#include "ro/render/actor_scale.h"
#include "ro/render/effect_renderer.h"
#include "ro/render/sprite_catalog.h"

namespace ro::render {
namespace {

// Une plage de métiers, bornes comprises.
struct JobRange {
  std::uint32_t first = 0;
  std::uint32_t last = 0;
};

// Les corps montés des premières classes : chevalier et croisé sur leur Peco,
// leurs formes transcendantes et bébés.
constexpr std::array<JobRange, 6> kMountedFirstClasses = {
    JobRange{13, 13},     JobRange{21, 21},     JobRange{4014, 4014},
    JobRange{4022, 4022}, JobRange{4036, 4036}, JobRange{4044, 4044}};
// Les montures des troisièmes classes et de leurs bébés, puis celles des
// quatrièmes.
constexpr std::array<JobRange, 3> kMountedLaterClasses = {
    JobRange{4080, 4095}, JobRange{4109, 4112}, JobRange{4278, 4281}};

constexpr std::int32_t kFirstClassMountSize = 3;
constexpr std::int32_t kLaterClassMountSize = 5;

[[nodiscard]] bool Within(std::span<const JobRange> ranges, std::uint32_t job) {
  return std::ranges::any_of(
      ranges, [job](const JobRange& range) { return job >= range.first && job <= range.last; });
}

// Le décalage à droite que le chemin du fichier donne d'office.
[[nodiscard]] float NamedShift(std::string_view file) {
  for (const HatAnchorNamedShift& shift : kHatAnchorNamedShifts) {
    if (file.find(shift.name) != std::string_view::npos) {
      return shift.right_units;
    }
  }
  return 0.0F;
}

// La hauteur et le décalage, en unités de fiche, devenus une ancre.
[[nodiscard]] HatEffectAnchor Anchor(std::int32_t height_units, float right_units,
                                     float body_scale) {
  const float right = right_units * body_scale;
  const float height = static_cast<float>(height_units) * body_scale;
  HatEffectAnchor anchor;
  if (right < 0.0F && height < 0.0F) {
    anchor.lift_pixels = kEffectAnchorLiftPixels;
    return anchor;
  }
  anchor.view_offset = glm::vec2(right, -height);
  return anchor;
}

}  // namespace

std::int32_t HatAnchorJobSizeClass(std::uint32_t job) {
  if (Within(kMountedFirstClasses, job)) {
    return kFirstClassMountSize;
  }
  if (Within(kMountedLaterClasses, job)) {
    return kLaterClassMountSize;
  }
  return 0;
}

HatEffectAnchor HatStrAnchor(const HatEffect& record, std::string_view file,
                             const HatEffectBearer& bearer) {
  const bool fixed_base = record.ignore_riding || record.attached_head;
  std::int32_t height =
      kHatAnchorBaseHeight + (fixed_base ? 0 : HatAnchorJobSizeClass(bearer.job)) +
      static_cast<std::int32_t>(std::lround(record.offset)) + kHatAnchorHeightCorrection;
  if (record.attached_head && IsDoramJob(bearer.job)) {
    height += kHatAnchorDoramHeadCorrection;
  }
  float body_scale = BodyScaleFor(bearer.job);
  if (record.adjust_position_when_shrunk) {
    body_scale *= bearer.shrink_scale;
  }
  HatEffectAnchor anchor = Anchor(height, NamedShift(file) + record.offset_x, body_scale);
  anchor.scale = record.adjust_size_when_shrunk ? bearer.shrink_scale : 1.0F;
  return anchor;
}

HatEffectAnchor HatConcreteStrAnchor(std::string_view file, const HatEffectBearer& bearer) {
  const std::int32_t height =
      kHatAnchorBaseHeight + HatAnchorJobSizeClass(bearer.job) + kHatAnchorHeightCorrection;
  return Anchor(height, NamedShift(file), BodyScaleFor(bearer.job));
}

bool HatIdIsReadAsStr(std::uint32_t effect_id) {
  return std::ranges::find(kHatIdsReadAsStr, effect_id) != kHatIdsReadAsStr.end();
}

}  // namespace ro::render
