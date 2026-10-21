#include "ro/render/model_clock.h"

#include "ro/render/model_pose.h"
#include "ro/render/tick_blend.h"

namespace ro::render {
namespace {

// L'image d'après pour un groupe : un cran pour la première famille, l'horloge
// pour la seconde. `start_ms` se reporte quand la boucle de la seconde repart.
float NextFrame(const ModelRig& rig, const ModelPlaybackGroup& playback, float frame,
                std::uint32_t& start_ms, std::uint32_t now_ms) {
  if (!rig.second_family()) {
    return static_cast<float>(NextFirstFamilyFrame(static_cast<std::int32_t>(frame),
                                                   playback.animation_type,
                                                   playback.animation_speed, rig.animation_length));
  }
  const SecondFamilyPlayed played =
      SecondFamilyPlayedAt(now_ms - start_ms, playback.animation_type, playback.animation_speed,
                           SecondFamilyDurationMs(rig.animation_length, rig.frame_rate));
  if (played.restart) {
    start_ms = now_ms;
    return 0.0F;
  }
  return SecondFamilyFrameAt(played.played_ms, rig.frame_rate);
}

// L'image où prendre la pose : entre celle du pas précédent et celle du pas
// courant, sauf quand la boucle vient de repartir.
float ShownBetween(float previous_frame, float frame, float weight) {
  if (frame < previous_frame) {
    return frame;
  }
  return BlendScalar(previous_frame, frame, weight);
}

}  // namespace

void ModelClock::Start(const std::vector<ModelInstances>& models, std::uint32_t now_ms) {
  groups_.clear();
  groups_.resize(models.size());
  for (std::size_t model = 0; model < models.size(); ++model) {
    const ModelInstances& instances = models[model];
    if (instances.rig.empty()) {
      continue;
    }
    const Group rest{.frame = 0.0F,
                     .start_ms = now_ms,
                     .pose = PoseRig(instances.rig, 0.0F),
                     .texture_motions = RigTextureMotions(instances.rig, 0.0F)};
    groups_[model].assign(instances.animated.size(), rest);
  }
  origin_ms_ = now_ms;
  steps_seen_ = 0;
  started_ = true;
}

void ModelClock::Advance(const std::vector<ModelInstances>& models, std::uint32_t now_ms,
                         ModelClockBlend blend) {
  if (!started_ || models.size() != groups_.size()) {
    return;
  }
  const bool blended = blend == ModelClockBlend::kBetweenSteps;
  // Soustraction non signée : l'horloge du système peut repasser par zéro.
  const std::uint32_t since_origin_ms = now_ms - origin_ms_;
  const std::uint64_t steps = since_origin_ms / kModelStepMs;
  const bool stepped = steps > steps_seen_;
  if (!stepped && !blended && !blend_shown_) {
    return;
  }
  if (stepped) {
    steps_seen_ = steps;
  }
  const float weight = TickBlendWeight(since_origin_ms % kModelStepMs, kModelStepMs, blended);
  for (std::size_t model = 0; model < models.size(); ++model) {
    const ModelInstances& instances = models[model];
    std::vector<Group>& groups = groups_[model];
    for (std::size_t index = 0; index < groups.size() && index < instances.animated.size();
         ++index) {
      Group& group = groups[index];
      if (stepped) {
        group.previous_frame = group.frame;
        group.frame = NextFrame(instances.rig, instances.animated[index], group.frame,
                                group.start_ms, now_ms);
      }
      const float shown = ShownBetween(group.previous_frame, group.frame, weight);
      if (shown == group.shown_frame) {
        continue;
      }
      group.shown_frame = shown;
      group.pose = PoseRig(instances.rig, shown);
      group.texture_motions = RigTextureMotions(instances.rig, shown);
    }
  }
  blend_shown_ = blended;
}

void ModelClock::Hold(const std::vector<ModelInstances>& models, std::size_t model_index,
                      std::size_t group_index, float frame) {
  if (model_index >= models.size() || models[model_index].rig.empty()) {
    return;
  }
  if (groups_.size() < models.size()) {
    groups_.resize(models.size());
  }
  std::vector<Group>& groups = groups_[model_index];
  if (groups.size() <= group_index) {
    groups.resize(group_index + 1);
  }
  Group& group = groups[group_index];
  if (!group.pose.empty() && group.frame == frame) {
    return;
  }
  const ModelRig& rig = models[model_index].rig;
  group.frame = frame;
  group.previous_frame = frame;
  group.shown_frame = frame;
  group.pose = PoseRig(rig, frame);
  group.texture_motions = RigTextureMotions(rig, frame);
}

const ModelClock::Group* ModelClock::Find(std::size_t model_index, std::size_t group_index) const {
  if (model_index >= groups_.size() || group_index >= groups_[model_index].size()) {
    return nullptr;
  }
  return &groups_[model_index][group_index];
}

float ModelClock::Frame(std::size_t model_index, std::size_t group_index) const {
  const Group* group = Find(model_index, group_index);
  return group != nullptr ? group->frame : 0.0F;
}

float ModelClock::ShownFrame(std::size_t model_index, std::size_t group_index) const {
  const Group* group = Find(model_index, group_index);
  return group != nullptr ? group->shown_frame : 0.0F;
}

std::span<const glm::mat4> ModelClock::Pose(std::size_t model_index,
                                            std::size_t group_index) const {
  const Group* group = Find(model_index, group_index);
  return group != nullptr ? std::span<const glm::mat4>(group->pose) : std::span<const glm::mat4>();
}

std::span<const TextureMotion> ModelClock::TextureMotions(std::size_t model_index,
                                                          std::size_t group_index) const {
  const Group* group = Find(model_index, group_index);
  return group != nullptr ? std::span<const TextureMotion>(group->texture_motions)
                          : std::span<const TextureMotion>();
}

}  // namespace ro::render
