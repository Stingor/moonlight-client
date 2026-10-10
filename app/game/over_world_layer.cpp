#include "game/over_world_layer.h"

#include <cmath>
#include <cstdint>
#include <span>

#include "ro/render/inset_paint.h"

#include "effects/effect_node_view.h"

namespace moonclient {
namespace {

[[nodiscard]] std::int32_t Pixels(float value) {
  return static_cast<std::int32_t>(std::lround(value));
}

}  // namespace

void OverWorldLayer::Prepare(const Painters& painters) {
  painters_ = painters;
  game_hud_.clear();
}

void OverWorldLayer::PaintCallback(const ImDrawList* list, const ImDrawCmd* command) {
  static_cast<void>(list);
  if (command->UserCallbackData != nullptr) {
    static_cast<OverWorldLayer*>(command->UserCallbackData)->PaintNow();
  }
}

void OverWorldLayer::PaintNow() {
  // TOUTE L'IMAGE, en pixels : l'interface peut avoir borné la vue et la
  // découpe au dernier élément qu'elle a peint.
  const ImDrawData* frame = ImGui::GetDrawData();
  if (frame == nullptr) {
    return;
  }
  const ro::render::InsetRect whole{
      .left = 0,
      .bottom = 0,
      .width = Pixels(frame->DisplaySize.x * frame->FramebufferScale.x),
      .height = Pixels(frame->DisplaySize.y * frame->FramebufferScale.y)};
  if (whole.width <= 0 || whole.height <= 0) {
    return;
  }
  ro::render::BeginInsetPaint(whole, whole);
  for (const OverWorldPass pass : kOverWorldPasses) {
    switch (pass) {
      case OverWorldPass::kDarkVeil:
        if (painters_.effects != nullptr && painters_.nodes != nullptr &&
            painters_.camera != nullptr) {
          const std::span<const ro::render::DrawnScreenQuads> veil = painters_.nodes->ScreenQuads(
              *painters_.camera, painters_.scene_width, painters_.scene_height, true);
          if (!veil.empty()) {
            painters_.effects->DrawScreenQuads(veil, painters_.scene_width, painters_.scene_height);
          }
        }
        break;
      case OverWorldPass::kGameHud:
        if (painters_.overlay != nullptr && !game_hud_.empty()) {
          painters_.overlay->Draw(game_hud_, painters_.hud_width, painters_.hud_height);
        }
        break;
    }
  }
  ro::render::EndInsetPaint();
}

}  // namespace moonclient
