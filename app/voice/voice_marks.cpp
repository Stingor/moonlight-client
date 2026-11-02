#include "voice/voice_marks.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>

#include "ro/sim/world.h"
#include "ro/ui/menu_icons.h"

#include "game/skin_image_of.h"

namespace moonclient {

std::vector<ro::ui::VoiceMarkView> VoiceMarksOnScreen(const ro::render::Camera& camera,
                                                      float aspect_ratio,
                                                      std::span<const ro::render::DrawnActor> drawn,
                                                      std::span<const std::uint32_t> ids,
                                                      const ro::sim::World& world,
                                                      const ro::ui::VoiceOverlayState& voice,
                                                      float viewport_width, float viewport_height) {
  std::vector<ro::ui::VoiceMarkView> marks;
  if (voice.speaking.empty() && voice.self_talking.empty()) {
    return marks;
  }
  for (std::size_t index = 0; index < drawn.size() && index < ids.size(); ++index) {
    const ro::sim::Actor* actor = world.Find(ids[index]);
    // Un PNJ parle aussi : le serveur voix fait dire au bot du chat ses
    // répliques, sous le nom du PNJ.
    const bool may_speak = actor != nullptr && (actor->kind == ro::sim::ActorKind::kPlayer ||
                                                (actor->kind == ro::sim::ActorKind::kNonPlayer &&
                                                 actor->familiar == ro::sim::Familiar::kNone));
    if (!may_speak) {
      continue;
    }
    const bool speaking =
        ids[index] == world.player_id()
            ? !voice.self_talking.empty()
            : std::ranges::find(voice.speaking, actor->name) != voice.speaking.end();
    if (!speaking) {
      continue;
    }
    const std::optional<ro::render::ScreenRect> bounds = ro::render::ActorSpeakerScreenBounds(
        camera, aspect_ratio, drawn[index], viewport_width, viewport_height);
    if (bounds.has_value()) {
      marks.push_back(ro::ui::VoiceMarkView{
          .head = ImVec2((bounds->min.x + bounds->max.x) * 0.5F, bounds->min.y)});
    }
  }
  return marks;
}

ro::ui::SkinImage SpeakingIconImage(ro::render::GameImages* images, SpeakingIcon icon) {
  const std::string_view path = SpeakingIconPath(icon);
  if (images == nullptr || path.empty()) {
    return {};
  }
  return ro::ui::AtOurDensity(SkinImageOf(images->GetOursIfShipped(path)));
}

std::array<ro::ui::SkinImage, static_cast<std::size_t>(SpeakingIcon::kCount)> SpeakingIconImages(
    ro::render::GameImages* images) {
  std::array<ro::ui::SkinImage, static_cast<std::size_t>(SpeakingIcon::kCount)> all{};
  for (std::size_t rank = 0; rank < all.size(); ++rank) {
    all[rank] = SpeakingIconImage(images, static_cast<SpeakingIcon>(rank));
  }
  return all;
}

}  // namespace moonclient
