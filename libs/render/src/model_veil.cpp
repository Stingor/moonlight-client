#include "ro/render/model_veil.h"

#include <algorithm>
#include <cmath>

namespace ro::render {
namespace {

// AU-DELÀ, LE RETARD NE SE RATTRAPE PLUS image par image : en soixante-huit
// images, n'importe quel voile a rejoint l'état où le segment le tient, et
// compter plus ne changerait rien — sauf le temps passé à compter après une
// longue pause.
constexpr std::size_t kMostFramesPerAdvance = 128;

}  // namespace

void StepVeil(ModelVeil& veil) {
  if (veil.marked) {
    if (veil.opacity > kVeilFloor) {
      veil.opacity = static_cast<std::uint8_t>(veil.opacity - kVeilStep);
    } else {
      veil.marked = false;
    }
    return;
  }
  if (veil.opacity < kVeilOpaque) {
    veil.opacity = static_cast<std::uint8_t>(veil.opacity + kVeilStep);
  }
}

void ModelVeils::Prepare(const MapScene& scene, const formats::GatMap* gat) {
  poses_.clear();
  boxes_.clear();
  const ClientFrame frame = ClientFrameOf(scene.gnd);
  opacities_.assign(scene.models.size(), {});
  for (std::size_t model_index = 0; model_index < scene.models.size(); ++model_index) {
    const ModelInstances& model = scene.models[model_index];
    std::size_t slot = 0;
    for (std::size_t index = 0; index < model.transforms.size(); ++index) {
      poses_.push_back(Pose{.model_index = model_index, .index = index, .slot = slot++});
      boxes_.push_back(SightBoxOf(model.transforms[index], model.mesh, frame));
    }
    for (std::size_t group = 0; group < model.animated.size(); ++group) {
      const std::vector<glm::mat4>& transforms = model.animated[group].transforms;
      for (std::size_t index = 0; index < transforms.size(); ++index) {
        poses_.push_back(
            Pose{.model_index = model_index, .group = group, .index = index, .slot = slot++});
        boxes_.push_back(SightBoxOf(transforms[index], model.mesh, frame));
      }
    }
    opacities_[model_index].assign(slot, kVeilOpaque);
  }
  tree_ = SightQuadTree::Build(scene.rsw, scene.gnd, gat, boxes_);
  Clear();
}

void ModelVeils::Clear() {
  veils_.assign(poses_.size(), ModelVeil());
  pending_ms_ = 0.0F;
  sides_.assign(poses_.size(), FrustumSide::kOutside);
  Publish(sides_);
}

void ModelVeils::Advance(float elapsed_ms, const std::optional<SightSegment>& segment,
                         const SightFrustum& frustum) {
  pending_ms_ += std::max(elapsed_ms, 0.0F);
  auto frames = static_cast<std::size_t>(std::floor(pending_ms_ / kVeilFrameMs));
  if (frames > kMostFramesPerAdvance) {
    frames = kMostFramesPerAdvance;
    pending_ms_ = 0.0F;
  } else {
    pending_ms_ -= static_cast<float>(frames) * kVeilFrameMs;
  }
  // La vue ne bouge pas entre deux images du même appel.
  sides_.resize(boxes_.size());
  for (std::size_t rank = 0; rank < boxes_.size(); ++rank) {
    sides_[rank] = BoxAgainstFrustum(frustum, boxes_[rank]);
  }
  for (std::size_t frame = 0; frame < frames; ++frame) {
    // LE SEGMENT MARQUE D'ABORD, puis chaque pose dessinée avance d'un cran :
    // c'est l'ordre du client dans son image.
    if (segment.has_value()) {
      crossed_.clear();
      tree_.CollectAlong(segment->origin, segment->direction, crossed_);
      for (const std::uint32_t item : crossed_) {
        if (SegmentMeetsBox(*segment, boxes_[item])) {
          veils_[item].marked = true;
        }
      }
    }
    for (std::size_t rank = 0; rank < veils_.size(); ++rank) {
      if (sides_[rank] != FrustumSide::kOutside) {
        StepVeil(veils_[rank]);
      }
    }
  }
  Publish(sides_);
}

void ModelVeils::Publish(std::span<const FrustumSide> sides) {
  veiled_.clear();
  for (std::size_t rank = 0; rank < poses_.size(); ++rank) {
    const Pose& pose = poses_[rank];
    const std::uint8_t opacity = veils_[rank].opacity;
    opacities_[pose.model_index][pose.slot] = opacity;
    if (opacity < kVeilOpaque && sides[rank] != FrustumSide::kOutside) {
      veiled_.push_back(VeiledPose{.model_index = pose.model_index,
                                   .group = pose.group,
                                   .index = pose.index,
                                   .opacity = opacity,
                                   .across = sides[rank] == FrustumSide::kAcross});
    }
  }
}

}  // namespace ro::render
