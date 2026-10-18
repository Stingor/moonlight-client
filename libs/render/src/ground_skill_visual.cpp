#include "ro/render/ground_skill_visual.h"

#include <algorithm>
#include <cmath>
#include <iterator>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_class_registry.h"

namespace ro::render {
namespace {

// Deux entrées de la même variante : même bascule, même plage de niveaux.
[[nodiscard]] bool SameVariant(const GroundSkillVisualEntry& left,
                               const GroundSkillVisualEntry& right) noexcept {
  return left.setting == right.setting && left.min_level == right.min_level &&
         left.max_level == right.max_level;
}

[[nodiscard]] bool CoversLevel(std::int32_t min_level, std::int32_t max_level,
                               std::int32_t level) noexcept {
  return min_level <= level && level <= max_level;
}

// Les entrées d'une table triée par compétence qui portent cet identifiant.
template <typename Entry>
[[nodiscard]] std::span<const Entry> EntriesOf(std::span<const Entry> table,
                                               std::uint32_t skill_id) {
  return core::RowsWithKey(table, skill_id,
                           [](const Entry& entry) { return std::uint32_t{entry.skill_id}; });
}

}  // namespace

std::span<const GroundSkillVisualEntry> GroundSkillVisualsOf(std::uint32_t skill_id) {
  return EntriesOf(GroundSkillVisualTable(), skill_id);
}

bool GroundSkillLauncherCarries(const GroundSkillVisualEntry& entry) noexcept {
  return entry.bearer == GroundSkillVisualBearer::kLauncher &&
         !ClientRegistersEffectClass(entry.effect_id);
}

bool GroundSkillStepLauncherCarries(const GroundSkillPacketStep& step) noexcept {
  return step.gesture == GroundSkillPacketGesture::kLauncher &&
         !ClientRegistersEffectClass(step.effect_id);
}

namespace {

std::vector<GroundSkillVisualEntry> VisualsToPlayIn(std::span<const GroundSkillVisualEntry> table,
                                                    std::uint32_t skill_id,
                                                    std::uint16_t table_effect_id,
                                                    std::int32_t skill_level,
                                                    bool minimal_effects) {
  const std::span<const GroundSkillVisualEntry> known = EntriesOf(table, skill_id);
  if (known.empty()) {
    if (table_effect_id == 0) {
      return {};
    }
    GroundSkillVisualEntry alone;
    alone.skill_id = static_cast<std::uint16_t>(skill_id);
    alone.effect_id = table_effect_id;
    return {alone};
  }
  // LA VARIANTE DE LA BASCULE ET DU NIVEAU, comme le répartiteur la choisit.
  const auto variant = std::ranges::find_if(known, [&](const GroundSkillVisualEntry& entry) {
    return GroundSkillSettingPlays(entry.setting, minimal_effects) &&
           CoversLevel(entry.min_level, entry.max_level, skill_level);
  });
  if (variant == known.end()) {
    return {};
  }
  std::vector<GroundSkillVisualEntry> played;
  std::ranges::copy_if(known, std::back_inserter(played), [&](const GroundSkillVisualEntry& entry) {
    return SameVariant(entry, *variant);
  });
  return played;
}

std::vector<GroundSkillSound> SoundsToPlayIn(std::span<const GroundSkillSound> table,
                                             std::uint32_t skill_id, std::int32_t skill_level,
                                             bool minimal_effects) {
  std::vector<GroundSkillSound> played;
  std::ranges::copy_if(EntriesOf(table, skill_id), std::back_inserter(played),
                       [&](const GroundSkillSound& sound) {
                         return GroundSkillSettingPlays(sound.setting, minimal_effects) &&
                                CoversLevel(sound.min_level, sound.max_level, skill_level);
                       });
  return played;
}

// Un angle dans une borne, ouverte ou fermée.
[[nodiscard]] bool AtLeast(float angle, float bound, bool inclusive) noexcept {
  return inclusive ? angle >= bound : angle > bound;
}
[[nodiscard]] bool AtMost(float angle, float bound, bool inclusive) noexcept {
  return inclusive ? angle <= bound : angle < bound;
}

}  // namespace

std::vector<GroundSkillVisualEntry> GroundSkillVisualsToPlay(std::uint32_t skill_id,
                                                             std::uint16_t table_effect_id,
                                                             std::int32_t skill_level,
                                                             bool minimal_effects) {
  return VisualsToPlayIn(GroundSkillVisualTable(), skill_id, table_effect_id, skill_level,
                         minimal_effects);
}

std::vector<GroundSkillSound> GroundSkillSoundsToPlay(std::uint32_t skill_id,
                                                      std::int32_t skill_level,
                                                      bool minimal_effects) {
  return SoundsToPlayIn(GroundSkillSoundTable(), skill_id, skill_level, minimal_effects);
}

float GroundSkillViewAngleDegrees(float camera_yaw_degrees, float client_body_degrees) noexcept {
  constexpr float kFullTurnDegrees = 360.0F;
  // Le client tient son lacet dans le tour, et le nôtre tourne dans l'autre sens.
  float client_yaw = std::fmod(-camera_yaw_degrees, kFullTurnDegrees);
  if (client_yaw < 0.0F) {
    client_yaw += kFullTurnDegrees;
  }
  if (client_yaw >= kFullTurnDegrees) {
    client_yaw -= kFullTurnDegrees;
  }
  return client_yaw + client_body_degrees;
}

bool GroundSkillStepFitsViewAngle(const GroundSkillPacketStep& step,
                                  float view_angle_degrees) noexcept {
  return !step.by_view_angle ||
         (AtLeast(view_angle_degrees, step.view_angle_min_degrees, step.view_angle_min_inclusive) &&
          AtMost(view_angle_degrees, step.view_angle_max_degrees, step.view_angle_max_inclusive));
}

GroundSkillPacketStep GroundSkillStepOf(const GroundSkillVisualEntry& entry) {
  GroundSkillPacketStep step;
  step.skill_id = entry.skill_id;
  step.setting = entry.setting;
  step.min_level = entry.min_level;
  step.max_level = entry.max_level;
  switch (entry.bearer) {
    case GroundSkillVisualBearer::kLauncher:
      step.gesture = GroundSkillPacketGesture::kLauncher;
      break;
    case GroundSkillVisualBearer::kCell:
      step.gesture = GroundSkillPacketGesture::kOnCell;
      break;
    case GroundSkillVisualBearer::kCaster:
      step.gesture = GroundSkillPacketGesture::kOnCaster;
      break;
  }
  step.effect_id = entry.effect_id;
  step.launcher_life_ms = entry.launcher_life_ms;
  step.client_height_offset = entry.client_height_offset;
  step.aimed_at_cell = entry.aimed_at_cell;
  step.str_start_delay_ms = entry.str_start_delay_ms;
  step.str_skipped_share = entry.str_skipped_share;
  return step;
}

GroundSkillPacketStep GroundSkillStepOf(const GroundSkillSound& sound) {
  GroundSkillPacketStep step;
  step.skill_id = sound.skill_id;
  step.setting = sound.setting;
  step.min_level = sound.min_level;
  step.max_level = sound.max_level;
  step.gesture = GroundSkillPacketGesture::kSound;
  step.sound = sound.file;
  step.sound_client_position = sound.client_position;
  step.hearing_range = sound.hearing_range;
  step.near_distance = sound.near_distance;
  step.volume = sound.volume;
  return step;
}

GroundSkillPacketTables GeneratedGroundSkillPacketTables() {
  return {.dispatcher = GroundSkillVisualTable(),
          .dispatcher_sounds = GroundSkillSoundTable(),
          .handler = GroundSkillPacketStepTable(),
          .registry_skips = GroundSkillPacketRegistrySkipTable()};
}

GroundSkillPacketPlan PlanGroundSkillPacket(const GroundSkillPacketTables& tables,
                                            std::uint32_t skill_id, std::uint16_t table_effect_id,
                                            std::int32_t skill_level, bool minimal_effects,
                                            GroundSkillPacketCaster caster,
                                            bool has_named_effects) {
  GroundSkillPacketPlan plan;
  const std::span<const GroundSkillPacketStep> handled = EntriesOf(tables.handler, skill_id);
  const bool seen = caster != GroundSkillPacketCaster::kUnseen;
  // Sans métier pour dire le contraire, le client prend le chemin du joueur.
  const GroundSkillPacketCaster path = seen ? caster : GroundSkillPacketCaster::kPlayer;
  if (seen) {
    // LE REPLI ne vaut que pour une compétence que personne d'autre ne connaît.
    const bool fallback = handled.empty() && !has_named_effects;
    for (const GroundSkillSound& sound :
         SoundsToPlayIn(tables.dispatcher_sounds, skill_id, skill_level, minimal_effects)) {
      plan.steps.push_back(GroundSkillStepOf(sound));
    }
    for (const GroundSkillVisualEntry& entry :
         VisualsToPlayIn(tables.dispatcher, skill_id, fallback ? table_effect_id : std::uint16_t{0},
                         skill_level, minimal_effects)) {
      plan.steps.push_back(GroundSkillStepOf(entry));
    }
  }
  for (const GroundSkillPacketStep& step : handled) {
    if (step.caster != path || !GroundSkillSettingPlays(step.setting, minimal_effects) ||
        !CoversLevel(step.min_level, step.max_level, skill_level)) {
      continue;
    }
    if (!seen) {
      // LE REGARD D'UN LANCEUR ABSENT ne se lit pas : le client sort avant.
      if (step.by_view_angle) {
        continue;
      }
      if (step.gesture == GroundSkillPacketGesture::kOnCaster) {
        if (std::ranges::find(kCasterEffectsTheHandlerWaitsFor, step.effect_id) !=
            kCasterEffectsTheHandlerWaitsFor.end()) {
          break;
        }
        continue;
      }
    }
    plan.steps.push_back(step);
  }
  plan.reaches_registry =
      path == GroundSkillPacketCaster::kPlayer &&
      !std::ranges::binary_search(tables.registry_skips, skill_id, {},
                                  [](std::uint16_t id) { return std::uint32_t{id}; });
  return plan;
}

const GroundSkillPose* GroundSkillPoseOf(std::uint32_t skill_id, std::int32_t skill_level) {
  for (const GroundSkillPose& pose : EntriesOf(GroundSkillPoseTable(), skill_id)) {
    if (CoversLevel(pose.min_level, pose.max_level, skill_level)) {
      return &pose;
    }
  }
  return nullptr;
}

bool GroundSkillSendsFalcon(std::uint32_t skill_id) {
  const std::span<const std::uint16_t> table = GroundSkillFalconFlightTable();
  return std::ranges::binary_search(table, skill_id, {},
                                    [](std::uint16_t id) { return std::uint32_t{id}; });
}

std::int32_t StrSkippedFrames(std::int32_t last_frame, float share) noexcept {
  return static_cast<std::int32_t>(static_cast<float>(last_frame) * share);
}

}  // namespace ro::render
