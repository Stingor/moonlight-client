#include "ro/render/ground_unit_pose.h"

#include "ro/render/model_pose.h"
#include "ro/render/second_family_pose.h"

namespace ro::render {
namespace {

// Le temps de pose, le même pour les deux familles : une image pour la
// première, des millisecondes pour la seconde.
constexpr std::int32_t kRestTime = 3;
// La vitesse que reçoit tout modèle d'unité.
constexpr float kPlaySpeed = 1.0F;

}  // namespace

float GroundUnitRestFrame(const ModelRig& rig) {
  if (!rig.second_family()) {
    return static_cast<float>(kRestTime);
  }
  return SecondFamilyFrameAt(static_cast<std::uint32_t>(kRestTime), rig.frame_rate);
}

GroundUnitPlay GroundUnitAtRest(const ModelRig& rig) {
  return GroundUnitPlay{.counted_frame = 0, .frame = GroundUnitRestFrame(rig)};
}

void StepGroundUnit(const ModelRig& rig, bool sprung, std::uint32_t built_ms, std::uint32_t now_ms,
                    GroundUnitPlay& play) {
  if (!sprung) {
    return;
  }
  if (!rig.second_family()) {
    play.counted_frame =
        NextFirstFamilyFrame(play.counted_frame, kModelPlayOnce, kPlaySpeed, rig.animation_length);
    play.frame = static_cast<float>(play.counted_frame);
    return;
  }
  // Soustraction non signée : l'horloge du système peut repasser par zéro.
  const SecondFamilyPlayed played =
      SecondFamilyPlayedAt(now_ms - built_ms, kModelPlayOnce, kPlaySpeed,
                           SecondFamilyDurationMs(rig.animation_length, rig.frame_rate));
  play.frame = SecondFamilyFrameAt(played.played_ms, rig.frame_rate);
}

bool GroundUnitRestsDeployed(std::uint32_t unit_id) noexcept { return unit_id == kAnkleSnareUnit; }

GroundUnitPlay GroundUnitDeployed(const ModelRig& rig) {
  if (!rig.second_family()) {
    const std::int32_t last_frame = rig.animation_length > 0 ? rig.animation_length - 1 : 0;
    return GroundUnitPlay{.counted_frame = last_frame, .frame = static_cast<float>(last_frame)};
  }
  // La dernière milliseconde jouée, comme un piège joué jusqu'au bout.
  const double duration_ms = SecondFamilyDurationMs(rig.animation_length, rig.frame_rate);
  const auto last_ms = duration_ms > 1.0 ? static_cast<std::uint32_t>(duration_ms - 1.0) : 0U;
  return GroundUnitPlay{.counted_frame = 0, .frame = SecondFamilyFrameAt(last_ms, rig.frame_rate)};
}

void StepDeployedGroundUnit(const ModelRig& rig, bool sprung, GroundUnitPlay& play) {
  play = sprung ? GroundUnitAtRest(rig) : GroundUnitDeployed(rig);
}

}  // namespace ro::render
