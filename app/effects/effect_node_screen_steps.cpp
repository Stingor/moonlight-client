// LE LISSAGE DES QUADS D'ÉCRAN de la troupe : voir `EffectNodeSteps`.

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include "ro/render/effect_frame_beat.h"
#include "ro/render/effect_renderer.h"
#include "ro/render/effect_screen_quad.h"
#include "ro/render/effect_step_blend.h"

#include "effects/effect_node_steps.h"
#include "effects/effect_node_view.h"

namespace moonclient {
namespace {

// LA TROUPE ET LES NŒUDS D'ÉCRAN COMPTENT LEURS NUMÉROS À PART : le dernier bit
// de la clé dit d'où vient le nœud.
constexpr std::uint64_t kPooledKeyBit = 0;
constexpr std::uint64_t kScreenNodeKeyBit = 1;

std::uint64_t KeyOf(std::uint64_t serial, std::uint64_t origin_bit) {
  return (serial << 1U) | origin_bit;
}

}  // namespace

EffectNodeSteps::ScreenOutputs::ScreenOutputs(EffectNodeSteps& steps, const EffectNodesInPlay& view,
                                              const ro::render::PooledEffectNode& node)
    : view_(&view) {
  if (steps.enabled_) {
    steps_ = &steps;
    steps.OpenScreen(view, KeyOf(node.pool_serial, kPooledKeyBit), node.anchor_ground_position,
                     ro::render::EffectBeatsEveryFrame(node.sown_by_effect_id));
  }
}

EffectNodeSteps::ScreenOutputs::ScreenOutputs(EffectNodeSteps& steps, const EffectNodesInPlay& view,
                                              const EffectScreenNodeInPlay& node)
    : view_(&view) {
  // UN NŒUD SANS NUMÉRO est né depuis le dernier battement : rien d'avant.
  if (steps.enabled_ && node.serial != 0) {
    steps_ = &steps;
    steps.OpenScreen(view, KeyOf(node.serial, kScreenNodeKeyBit), node.owner_position,
                     node.beats_every_frame);
  }
}

EffectNodeSteps::ScreenOutputs::~ScreenOutputs() {
  if (steps_ != nullptr) {
    steps_->CloseScreen(*view_);
  }
}

void EffectNodeSteps::OpenScreen(const EffectNodesInPlay& view, std::uint64_t key, glm::vec3 bearer,
                                 bool beats_every_frame) {
  ScreenRecording().push_back(ScreenSpan{.key = key,
                                         .bearer_ground_position = bearer,
                                         .beats_every_frame = beats_every_frame,
                                         .first_lot = view.screen_quad_lots_.size(),
                                         .end_lot = view.screen_quad_lots_.size()});
}

void EffectNodeSteps::CloseScreen(const EffectNodesInPlay& view) {
  ScreenRecording().back().end_lot = view.screen_quad_lots_.size();
}

void EffectNodeSteps::IdentifyScreenNodes(EffectNodesInPlay& view) {
  for (EffectScreenNodeInPlay& node : view.screen_nodes_) {
    if (node.serial == 0) {
      node.serial = ++last_screen_serial_;
    }
  }
}

void EffectNodeSteps::SmoothScreen(EffectNodesInPlay& view, const ro::render::Camera& camera,
                                   float viewport_width, float viewport_height, bool dark_veil) {
  if (composing_previous_) {
    return;
  }
  if (!enabled_ || !has_previous_) {
    screen_spans_.clear();
    return;
  }
  std::vector<std::vector<ro::render::EffectScreenQuad>> lots;
  std::vector<ro::render::DrawnScreenQuads> drawn;
  const auto exchange = [&] {
    view.screen_quad_lots_.swap(lots);
    view.screen_quads_.swap(drawn);
    std::swap(view.pool_, previous_pool_);
    view.screen_nodes_.swap(previous_screen_nodes_);
  };
  exchange();
  previous_screen_spans_.clear();
  composing_previous_ = true;
  static_cast<void>(view.ScreenQuads(camera, viewport_width, viewport_height, dark_veil));
  composing_previous_ = false;
  exchange();

  // LE PORTEUR SE PROJETTE À L'IMAGE, aux deux places : le pas qu'il a fait
  // entre les deux, vu de l'écran.
  const auto on_screen = [&](glm::vec3 ground) {
    return ro::render::EffectScreenPositionOf(camera, ground, viewport_width, viewport_height);
  };
  std::ranges::sort(previous_screen_spans_, {}, &ScreenSpan::key);
  for (const ScreenSpan& now : screen_spans_) {
    const auto before =
        std::ranges::lower_bound(previous_screen_spans_, now.key, {}, &ScreenSpan::key);
    if (now.beats_every_frame || before == previous_screen_spans_.end() || before->key != now.key ||
        now.end_lot - now.first_lot != before->end_lot - before->first_lot) {
      continue;
    }
    const std::optional<glm::vec2> here = on_screen(now.bearer_ground_position);
    const std::optional<glm::vec2> there = on_screen(before->bearer_ground_position);
    const glm::vec2 shift = here.has_value() && there.has_value() ? *here - *there : glm::vec2{};
    for (std::size_t rank = 0; rank < now.end_lot - now.first_lot; ++rank) {
      if (view.screen_quads_[now.first_lot + rank].texture_path !=
          drawn[before->first_lot + rank].texture_path) {
        continue;
      }
      static_cast<void>(
          ro::render::BlendEffectScreenQuads(lots[before->first_lot + rank], shift, alpha_,
                                             view.screen_quad_lots_[now.first_lot + rank]));
    }
  }
  screen_spans_.clear();
}

}  // namespace moonclient
