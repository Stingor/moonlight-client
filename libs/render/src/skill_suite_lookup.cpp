#include <algorithm>
#include <cmath>
#include <tuple>

#include <glm/trigonometric.hpp>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_node.h"
#include "ro/render/skill_suite_table.h"

namespace ro::render {

std::span<const SkillSuiteStep> SkillSuiteOf(std::uint16_t skill_id, SkillSuiteMoment moment) {
  // Une PLAGE : la table range les étapes d'une compétence et d'un moment
  // côte à côte, dans l'ordre du client.
  const auto key = [](const SkillSuiteStep& step) {
    return std::tuple{step.skill_id, step.moment};
  };
  return core::RowsWithKey(SkillSuiteTable(), std::tuple{skill_id, moment}, key);
}

std::optional<glm::vec3> SkillSuiteAimPoint(const SkillSuiteStep& step,
                                            std::optional<glm::vec3> other_end, glm::vec3 target,
                                            std::optional<glm::vec3> caster,
                                            std::uint32_t& generator) {
  switch (step.aim) {
    case SkillSuiteAim::kOtherEnd:
      return other_end;
    case SkillSuiteAim::kTarget:
      return target;
    case SkillSuiteAim::kCaster:
      return caster;
    case SkillSuiteAim::kCasterRing: {
      if (!caster.has_value()) {
        return std::nullopt;
      }
      const std::uint32_t draw = step.aim_arc_span_degrees > 0
                                     ? NextEffectNodeDraw(generator) %
                                           static_cast<std::uint32_t>(step.aim_arc_span_degrees)
                                     : 0U;
      const float degrees =
          glm::radians(static_cast<float>(step.aim_arc_first_degrees + static_cast<int>(draw)));
      // Le sinus s'ajoute à la profondeur du client, qui est l'opposée de la
      // nôtre.
      return *caster + glm::vec3(std::cos(degrees) * step.aim_ring_radius, 0.0F,
                                 -std::sin(degrees) * step.aim_ring_radius);
    }
    case SkillSuiteAim::kBeyondTarget:
      // Sans lanceur, pas de sens du tir : l'autre bout, comme par défaut.
      if (!caster.has_value()) {
        return other_end;
      }
      return target + (target - *caster);
  }
  return other_end;
}

SkillSuiteScatter SkillSuiteDrawScatter(const SkillSuiteStep& step, std::uint32_t& generator) {
  SkillSuiteScatter drawn;
  if (step.scatter_radius_max == 0 || step.scatter_angle_step_degrees == 0) {
    return drawn;
  }
  // La distance se tire sous le rayon maximal plus un, et se relève au minimal.
  const auto radius = static_cast<float>(std::max<std::uint32_t>(
      NextEffectNodeDraw(generator) % (static_cast<std::uint32_t>(step.scatter_radius_max) + 1U),
      step.scatter_radius_min));
  constexpr std::uint32_t kDegreesPerTurn = 360;
  const std::uint32_t crans = kDegreesPerTurn / step.scatter_angle_step_degrees;
  const float degrees = glm::radians(static_cast<float>((NextEffectNodeDraw(generator) % crans) *
                                                        step.scatter_angle_step_degrees));
  // Le sinus va à la profondeur du client, qui est l'opposée de la nôtre.
  drawn.offset = glm::vec3(std::cos(degrees) * radius, 0.0F, -std::sin(degrees) * radius);
  if (step.delay_steps > 0) {
    drawn.delay_ms = static_cast<std::uint32_t>(step.delay_step_ms) *
                     (NextEffectNodeDraw(generator) % step.delay_steps);
  }
  return drawn;
}

}  // namespace ro::render
