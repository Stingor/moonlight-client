#include "world/companion_zones_view.h"

#include <optional>

#include "ro/render/ground_quad.h"
#include "ro/render/skill_area.h"
#include "ro/sim/movement.h"
#include "ro/sim/world.h"

namespace moonclient {

void CompanionZonesView::Compose(const CompanionZonesFrame& frame) {
  lots_.clear();
  quads_.clear();
  paints_.clear();
  const bool window_open = frame.window.open;
  if (!CompanionZonesShown(window_open, frame.kept_shown) || frame.stage == nullptr ||
      !frame.stage->gat.has_value()) {
    return;
  }
  const ro::sim::Actor* owner = frame.world.player();
  const ro::sim::AiCompanion tab = frame.window.companion == ro::ui::kCompanionAiMercenary
                                       ? ro::sim::AiCompanion::kMercenary
                                       : ro::sim::AiCompanion::kHomunculus;
  const std::optional<ro::sim::AiCompanion> companion =
      ZonesCompanion(tab, frame.companions.present(ro::sim::AiCompanion::kHomunculus),
                     frame.companions.present(ro::sim::AiCompanion::kMercenary));
  if (owner == nullptr || !companion.has_value()) {
    return;
  }
  // 🔴 LES DISTANCES QUE LA FENÊTRE MONTRE, quand elle est ouverte sur ce
  // compagnon et réglable : un curseur tiré se voit au sol avant que l'IA ne
  // l'applique, et le profil édité peut ne pas être celui qui agit. Sinon,
  // celles que son IA applique.
  const bool window_shows = window_open && frame.view.editable && companion == tab &&
                            frame.view.companion == frame.window.companion;
  const CompanionZoneRadii radii = window_shows
                                       ? ZoneRadiiOf(frame.view.base)
                                       : ZoneRadiiOf(frame.companions.settings_of(*companion));
  // LA CASE ET LA MARCHE DU MAÎTRE, celles que l'IA lit : sa case entière, et
  // non sa position glissée d'une case à l'autre.
  const CompanionZonesPlan plan = PlanCompanionZones(CompanionZonesAsked{
      .radii = radii,
      .owner_walking = ro::sim::IsWalking(*owner, frame.now_ms),
      .hovered = window_open ? frame.answer.hovered_zone : ro::ui::CompanionAiZoneSlider::kNone,
      .held_cell = frame.companions.brain_of(*companion).held_cell()});
  PaintCompanionZones(plan, ro::sim::StandingCell(*owner), paints_);
  for (const ZoneCellPaint& paint : paints_) {
    if (const std::optional<ro::render::GroundQuad> quad = ro::render::ComposeGroundCellQuad(
            frame.stage->scene.gnd, *frame.stage->gat, paint.cell_x, paint.cell_y, paint.color);
        quad.has_value()) {
      quads_.push_back(*quad);
    }
  }
  if (quads_.empty()) {
    return;
  }
  if (!tile_.has_value()) {
    tile_ = ro::render::ComposeSkillAreaTile();
    texture_path_ =
        std::string(ro::render::SkillAreaTexturePath(ro::render::SkillAreaPattern::kSolid));
  }
  lots_.push_back(ro::render::DrawnGroundQuads{quads_, &texture_path_, &*tile_});
}

}  // namespace moonclient
