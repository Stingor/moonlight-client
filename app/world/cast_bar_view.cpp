#include "world/cast_bar_view.h"

#include <algorithm>
#include <cstddef>
#include <optional>

#include "ro/render/skill_impl.h"
#include "ro/sim/world.h"

#include "world/entity_bar_view.h"

namespace moonclient {

bool ShownCastOf(const ro::sim::Actor& actor, std::uint32_t now_ms, ShownCast* into) {
  ShownCast shown;
  if (actor.cast.has_value() &&
      ro::render::SkillImplFor(actor.cast->skill_id).ShowsCastBar(actor.cast->duration_ms)) {
    shown.started_ms = actor.cast->started_ms;
    shown.duration_ms = actor.cast->duration_ms;
    shown.skill_id = actor.cast->skill_id;
  }
  if (actor.gauge.has_value() &&
      (shown.duration_ms == 0 || actor.gauge->started_ms >= shown.started_ms)) {
    shown.started_ms = actor.gauge->started_ms;
    shown.duration_ms = actor.gauge->duration_ms;
    shown.skill_id = 0;
  }
  if (shown.duration_ms == 0 || now_ms < shown.started_ms ||
      now_ms - shown.started_ms >= shown.duration_ms) {
    return false;
  }
  *into = shown;
  return true;
}

bool OwnBarShown(const ro::sim::World& world, std::uint32_t now_ms) {
  const ro::sim::Actor* player = world.player();
  ShownCast shown;
  return player != nullptr && ShownCastOf(*player, now_ms, &shown);
}

ro::ui::StatBarValue OwnCastGauge(const ro::sim::World& world, const ro::sim::SkillTree* skills,
                                  std::uint32_t now_ms) {
  ro::ui::StatBarValue gauge;
  const ro::sim::Actor* player = world.player();
  ShownCast shown;
  if (player == nullptr || !ShownCastOf(*player, now_ms, &shown)) {
    return gauge;
  }
  gauge.known = true;
  gauge.current = now_ms - shown.started_ms;
  gauge.most = shown.duration_ms;
  const std::uint32_t remaining_ms = shown.duration_ms - (now_ms - shown.started_ms);
  gauge.composed = ro::ui::CastSecondsText(remaining_ms);
  if (const ro::sim::SkillNode* node =
          shown.skill_id != 0 && skills != nullptr ? skills->Find(shown.skill_id) : nullptr;
      node != nullptr && !node->display_name.empty()) {
    gauge.composed = node->display_name + "  " + gauge.composed;
  }
  return gauge;
}

std::vector<ro::ui::CastBarView> CastBarsOnScreen(
    const ro::render::Camera& camera, float aspect_ratio,
    std::span<const ro::render::DrawnActor> drawn, std::span<const std::uint32_t> ids,
    const ro::sim::World& world, const ro::sim::PartyRoster& party,
    const ro::sim::SkillTree* skills, std::span<const ro::ui::ChatBubbleView> bubbles,
    std::uint32_t now_ms, float viewport_width, float viewport_height) {
  std::vector<ro::ui::CastBarView> bars;
  for (std::size_t index = 0; index < drawn.size() && index < ids.size(); ++index) {
    const ro::sim::Actor* actor = world.Find(ids[index]);
    ShownCast shown;
    if (actor == nullptr || !ShownCastOf(*actor, now_ms, &shown)) {
      continue;
    }
    const std::optional<glm::vec2> feet = ro::render::ProjectToScreen(
        camera, aspect_ratio, drawn[index].ground_position, viewport_width, viewport_height);
    if (!feet.has_value() || feet->x <= 0.0F || feet->y <= 0.0F || feet->x >= viewport_width ||
        feet->y >= viewport_height) {
      continue;
    }
    // LE HAUT DU SPRITE, celui d'où la bulle de parole monte : la barre du
    // client partage son ancrage et descend de là.
    const std::optional<ro::render::ScreenRect> bounds = ro::render::ActorScreenBounds(
        camera, aspect_ratio, drawn[index], viewport_width, viewport_height);
    ro::ui::CastBarView bar;
    bar.head = ImVec2(feet->x, bounds.has_value() ? bounds->min.y : feet->y);
    bar.kind = EntityBarKindOf(*actor, world, party);
    bar.elapsed_ms = now_ms - shown.started_ms;
    bar.total_ms = shown.duration_ms;
    // ⚠ LE NOM VIENT DU PAQUET D'INCANTATION LUI-MÊME, qui porte le sort : il
    // ne peut pas nommer la barre d'un autre.
    if (const ro::sim::SkillNode* node =
            shown.skill_id != 0 && skills != nullptr ? skills->Find(shown.skill_id) : nullptr;
        node != nullptr) {
      bar.skill_name = node->display_name;
    }
    bar.balloon = std::ranges::any_of(bubbles, [&](const ro::ui::ChatBubbleView& bubble) {
      return bubble.speaker_id == actor->id;
    });
    bars.push_back(bar);
  }
  return bars;
}

}  // namespace moonclient
