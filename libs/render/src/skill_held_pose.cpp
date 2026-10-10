#include "ro/render/skill_held_pose.h"

#include <algorithm>

namespace ro::render {

namespace {

// Les poses d'une compétence, annonce et impact, côte à côte dans la table.
std::span<const SkillHeldPose> PosesOf(std::uint16_t skill_id) {
  const std::span<const SkillHeldPose> table = SkillHeldPoseTable();
  const auto trouvees = std::ranges::equal_range(table, skill_id, {}, &SkillHeldPose::skill_id);
  return {trouvees.begin(), trouvees.end()};
}

}  // namespace

const SkillHeldPose* SkillHeldPoseOf(std::uint16_t skill_id) {
  for (const SkillHeldPose& pose : PosesOf(skill_id)) {
    if (pose.damage_types == 0) {
      return &pose;
    }
  }
  return nullptr;
}

const SkillHeldPose* SkillImpactPoseOf(std::uint16_t skill_id, std::uint8_t damage_type) {
  constexpr std::uint8_t kDamageTypeBits = 16;
  if (damage_type >= kDamageTypeBits) {
    return nullptr;
  }
  for (const SkillHeldPose& pose : PosesOf(skill_id)) {
    if ((pose.damage_types >> damage_type) & 1U) {
      return &pose;
    }
  }
  return nullptr;
}

std::uint32_t ImpactPoseHoldMs(const SkillHeldPose& pose, std::uint32_t struck_ms,
                               std::uint32_t combo_delay_ms, std::uint32_t combo_delay_since_ms) {
  if (!pose.hold_from_combo_delay) {
    return pose.hold_ms;
  }
  const std::uint32_t annonce_ms = combo_delay_since_ms + combo_delay_ms;
  const std::uint32_t at_strike_ms = struck_ms + combo_delay_ms;
  const std::uint32_t echeance_ms =
      static_cast<std::int32_t>(annonce_ms - at_strike_ms) > 0 ? annonce_ms : at_strike_ms;
  return echeance_ms - struck_ms;
}

std::optional<HeldPoseFrame> HeldPoseFrameAt(const SkillHeldPose& pose, std::uint32_t elapsed_ms,
                                             std::uint32_t tick_ms) {
  if (tick_ms == 0 || pose.period_ticks == 0 || pose.last_frame < pose.first_frame) {
    return std::nullopt;
  }
  if (pose.timed && elapsed_ms >= pose.hold_ms) {
    return std::nullopt;
  }
  // L'image avance d'un cran toutes les `period_ticks` avances : à la k-ième,
  // la plage en a franchi k / période.
  const std::uint32_t advance = elapsed_ms / tick_ms;
  const std::uint32_t crans = advance / pose.period_ticks;
  const std::uint32_t plage = static_cast<std::uint32_t>(pose.last_frame - pose.first_frame) + 1U;
  if (pose.loops) {
    return HeldPoseFrame{pose.play_rank,
                         static_cast<std::uint8_t>(pose.first_frame + crans % plage)};
  }
  if (crans < plage) {
    return HeldPoseFrame{pose.play_rank, static_cast<std::uint8_t>(pose.first_frame + crans)};
  }
  // Tenue : jusqu'à l'échéance quand elle est minutée, une seule avance sinon.
  if (!pose.timed && advance > plage * pose.period_ticks) {
    return std::nullopt;
  }
  return HeldPoseFrame{pose.hold_rank, pose.hold_frame};
}

}  // namespace ro::render
