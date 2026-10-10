#include "world/talkbox_bubbles.h"

#include <optional>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/actor_pick.h"
#include "ro/sim/world.h"

namespace moonclient {

std::vector<ro::ui::ChatBubbleView> TalkboxBubblesOnScreen(
    const ro::render::Camera& camera, float aspect_ratio, std::span<const GroundUnitPick> speakers,
    const ro::sim::World& world, std::uint32_t now_ms, float viewport_width, float viewport_height,
    const ro::ui::ChatBubbleLook& look) {
  std::vector<ro::ui::ChatBubbleView> bubbles;
  for (const GroundUnitPick& speaker : speakers) {
    const auto unit = world.skill_units().find(speaker.id);
    if (unit == world.skill_units().end() || unit->second.said.empty()) {
      continue;
    }
    const std::string_view said = unit->second.said;
    const std::uint32_t since_ms = unit->second.said_since_ms;
    const float elapsed_ms = now_ms > since_ms ? static_cast<float>(now_ms - since_ms) : 0.0F;
    if (elapsed_ms >= ro::ui::ChatBubbleLifeMs(look, said)) {
      continue;
    }
    const std::optional<glm::vec2> foot = ro::render::ProjectToScreen(
        camera, aspect_ratio, speaker.ground, viewport_width, viewport_height);
    if (!foot.has_value()) {
      continue;
    }
    bubbles.push_back(ro::ui::ChatBubbleView{
        .head = ImVec2(foot->x, foot->y - viewport_height * kTalkboxBubbleLiftOn480 /
                                              kTalkboxBubbleReferenceHeight),
        .text = said,
        .elapsed_ms = elapsed_ms,
        .text_color = kTalkboxBubbleColor});
  }
  return bubbles;
}

}  // namespace moonclient
