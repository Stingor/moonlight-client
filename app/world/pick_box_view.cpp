#include "world/pick_box_view.h"

#include <cstdint>

#include "ro/render/actor_pick.h"
#include "ro/sim/actor.h"
#include "ro/sim/world.h"

#include "world/ground_unit_pick.h"

namespace moonclient {
namespace {

// Le numéro de classe qu'un objet au sol montre dans sa zone : le client n'y
// met pas l'objet, mais cette constante.
constexpr std::uint32_t kGroundItemPickJob = 32003;

// Un numéro d'acteur que le serveur ne peut pas viser : négatif, lu signé.
bool NegativeId(std::uint32_t id) { return static_cast<std::int32_t>(id) < 0; }

ro::ui::PickCategory CategoryOf(const ro::sim::Actor& actor) {
  if (actor.kind == ro::sim::ActorKind::kItem) {
    return ro::ui::PickCategory::kGroundItem;
  }
  switch (actor.familiar) {
    case ro::sim::Familiar::kPet:
      return ro::ui::PickCategory::kPet;
    case ro::sim::Familiar::kHomunculus:
    case ro::sim::Familiar::kMercenary:
    case ro::sim::Familiar::kElemental:
      return ro::ui::PickCategory::kCompanion;
    case ro::sim::Familiar::kNone:
      break;
  }
  return ro::ui::PickCategory::kActor;
}

}  // namespace

void ApplyPickTuning(const ro::ui::PickTuning& tuning, std::span<ro::render::DrawnActor> drawn,
                     std::span<const std::uint32_t> ids, const ro::sim::World& world) {
  for (std::size_t rank = 0; rank < drawn.size() && rank < ids.size(); ++rank) {
    ro::render::DrawnActor& one = drawn[rank];
    const ro::sim::Actor* actor = world.Find(ids[rank]);
    const bool item = actor != nullptr && actor->kind == ro::sim::ActorKind::kItem;
    one.pick_floor_pixels = item ? tuning.ground_floor_pixels : tuning.actor_floor_pixels;
    one.pick_share = NegativeId(ids[rank]) ? tuning.negative_id_share : 1.0F;
  }
}

ro::ui::PickBoxesView PickBoxesOnScreen(const ro::render::Camera& camera, float aspect_ratio,
                                        const PickScene& scene, const ro::sim::World& world,
                                        std::optional<std::size_t> hovered_rank, glm::vec2 mouse_px,
                                        float viewport_width, float viewport_height) {
  ro::ui::PickBoxesView view;
  view.mouse = mouse_px;
  const ro::sim::Actor* hovered_actor = nullptr;
  for (std::size_t rank = 0; rank < scene.drawn.size() && rank < scene.ids.size(); ++rank) {
    const std::optional<ro::render::ScreenRect> rect = ro::render::ActorScreenRect(
        camera, aspect_ratio, scene.drawn[rank], viewport_width, viewport_height);
    const ro::sim::Actor* actor = world.Find(scene.ids[rank]);
    if (!rect.has_value() || actor == nullptr) {
      continue;
    }
    if (hovered_rank.has_value() && *hovered_rank == rank) {
      view.hovered = view.boxes.size();
      hovered_actor = actor;
    }
    const ro::ui::PickCategory category = CategoryOf(*actor);
    view.boxes.push_back(
        ro::ui::PickBoxView{.min = rect->min,
                            .max = rect->max,
                            .id = actor->id,
                            .job = category == ro::ui::PickCategory::kGroundItem
                                       ? kGroundItemPickJob
                                       : static_cast<std::uint32_t>(actor->look.job),
                            .category = category,
                            .own = actor->id == world.player_id()});
  }
  const std::optional<std::uint32_t> unit_hovered =
      PickGroundUnit(camera, aspect_ratio, scene.ground_units, mouse_px.x, mouse_px.y,
                     scene.ground_unit_floor_pixels, viewport_width, viewport_height);
  const bool unit_wins = unit_hovered.has_value() && GroundUnitOutranks(hovered_actor);
  for (const GroundUnitPick& pick : scene.ground_units) {
    const std::optional<ro::render::ScreenRect> rect = ro::render::GroundUnitScreenRect(
        camera, aspect_ratio, pick.ground, scene.ground_unit_floor_pixels, viewport_width,
        viewport_height);
    if (!rect.has_value()) {
      continue;
    }
    if (unit_wins && *unit_hovered == pick.id) {
      view.hovered = view.boxes.size();
    }
    view.boxes.push_back(ro::ui::PickBoxView{.min = rect->min,
                                             .max = rect->max,
                                             .id = pick.id,
                                             .job = pick.unit_id,
                                             .category = ro::ui::PickCategory::kSkillUnit});
  }
  return view;
}

}  // namespace moonclient
