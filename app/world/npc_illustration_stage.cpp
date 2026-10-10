#include "world/npc_illustration_stage.h"

#include <string_view>

#include <imgui.h>

#include "ro/vfs/npc_illustration_paths.h"

namespace moonclient {
namespace {

[[nodiscard]] ro::ui::IllustrationAnchor AnchorOf(ro::sim::IllustrationPlace place) {
  switch (place) {
    case ro::sim::IllustrationPlace::kBottomLeft:
      return ro::ui::IllustrationAnchor::kBottomLeft;
    case ro::sim::IllustrationPlace::kBottomRight:
      return ro::ui::IllustrationAnchor::kBottomRight;
    case ro::sim::IllustrationPlace::kCenteredWindow:
      return ro::ui::IllustrationAnchor::kCenteredWindow;
    case ro::sim::IllustrationPlace::kCenteredFloating:
      return ro::ui::IllustrationAnchor::kCenteredFloating;
    case ro::sim::IllustrationPlace::kBottomMiddle:
      break;
  }
  return ro::ui::IllustrationAnchor::kBottomMiddle;
}

}  // namespace

std::optional<ro::ui::NpcIllustrationView> NpcIllustrationStage::Stage(
    ro::render::GameImages* images) {
  const std::optional<ro::sim::NpcIllustration>& shown = slot_.shown();
  if (!shown.has_value()) {
    dismissed_.reset();
    return std::nullopt;
  }
  if (shown == dismissed_ || images == nullptr) {
    return std::nullopt;
  }
  if (shown != searched_) {
    searched_ = shown;
    found_path_.clear();
    for (const std::string& path : ro::vfs::NpcIllustrationPaths(shown->image)) {
      if (images->Get(path).usable()) {
        found_path_ = path;
        break;
      }
    }
  }
  if (found_path_.empty()) {
    return std::nullopt;
  }
  const ro::render::IconTexture& texture = images->Get(found_path_);
  return ro::ui::NpcIllustrationView{
      .image = ro::ui::SkinImage{.id = static_cast<ImTextureID>(texture.texture.name()),
                                 .width = static_cast<float>(texture.width_px),
                                 .height = static_cast<float>(texture.height_px)},
      .anchor = AnchorOf(shown->place)};
}

void NpcIllustrationStage::Apply(const ro::ui::NpcIllustrationAction& asked) {
  if (asked.closed) {
    dismissed_ = slot_.shown();
  }
}

}  // namespace moonclient
