#include "game/companion_ai_zones.h"

#include <array>

#include "ro/sim/companion_sight.h"

namespace moonclient {
namespace {

using ro::ui::CompanionAiZoneSlider;

// L'ORIGINE D'UN DÉCALAGE, au milieu de la plage d'une case : les fonctions de
// l'IA prennent deux cases, et un décalage se mesure ainsi depuis un maître
// assez loin de zéro pour que tout voisin reste représentable.
constexpr int kOffsetOrigin = 0x8000;

[[nodiscard]] ro::sim::CellPosition OffsetCell(int dx_cells, int dy_cells) noexcept {
  return ro::sim::CellPosition{.x = static_cast<std::uint16_t>(kOffsetOrigin + dx_cells),
                               .y = static_cast<std::uint16_t>(kOffsetOrigin + dy_cells)};
}

// Vrai quand une case de la zone touche l'extérieur à `edge_cells` près, le
// long d'un axe. ⚠ LES AXES SUFFISENT : le disque et le carré sont convexes et
// symétriques, si bien qu'une case dont aucun voisin d'axe à cette distance
// ne sort est à l'intérieur.
[[nodiscard]] bool OnEdge(const CompanionZone& zone, CompanionZoneShape shape, int dx_cells,
                          int dy_cells, int edge_cells) noexcept {
  const int radius = zone.radius_cells;
  return !ZoneHolds(shape, radius, dx_cells + edge_cells, dy_cells) ||
         !ZoneHolds(shape, radius, dx_cells - edge_cells, dy_cells) ||
         !ZoneHolds(shape, radius, dx_cells, dy_cells + edge_cells) ||
         !ZoneHolds(shape, radius, dx_cells, dy_cells - edge_cells);
}

}  // namespace

CompanionZoneShape ShapeOfZone(CompanionZoneKind kind) noexcept {
  return kind == CompanionZoneKind::kEngage ? CompanionZoneShape::kSquare
                                            : CompanionZoneShape::kDisc;
}

bool ZoneHolds(CompanionZoneShape shape, int radius_cells, int dx_cells, int dy_cells) noexcept {
  const ro::sim::CellPosition owner = OffsetCell(0, 0);
  const ro::sim::CellPosition cell = OffsetCell(dx_cells, dy_cells);
  return shape == CompanionZoneShape::kSquare
             ? ro::sim::WithinSquareCells(owner, cell, radius_cells)
             : ro::sim::WithinCells(owner, cell, radius_cells);
}

CompanionZoneRadii ZoneRadiiOf(const ro::sim::CompanionAiSettings& settings) noexcept {
  return CompanionZoneRadii{.follow_cells = settings.follow_distance_cells,
                            .leash_still_cells = settings.leash_still_cells,
                            .leash_walking_cells = settings.leash_walking_cells,
                            .engage_still_cells = settings.engage_still_cells,
                            .engage_walking_cells = settings.engage_walking_cells};
}

CompanionZoneRadii ZoneRadiiOf(const ro::ui::CompanionBaseFields& fields) noexcept {
  return CompanionZoneRadii{.follow_cells = fields.follow_cells,
                            .leash_still_cells = fields.leash_still_cells,
                            .leash_walking_cells = fields.leash_walking_cells,
                            .engage_still_cells = fields.engage_still_cells,
                            .engage_walking_cells = fields.engage_walking_cells};
}

CompanionZonesPlan PlanCompanionZones(const CompanionZonesAsked& asked) {
  const CompanionZoneRadii& radii = asked.radii;
  CompanionZonesPlan plan;
  // « AUCUNE » ne se dessine pas : un éloignement ou un engagement nul ne
  // borne rien. Le suivi, lui, a toujours sa zone, fût-elle la seule case du
  // maître.
  const auto push = [&plan](CompanionZoneKind kind, int radius_cells, bool emphasized) {
    if (radius_cells < 0 || (kind != CompanionZoneKind::kFollow && radius_cells == 0)) {
      return;
    }
    plan.zones.push_back(
        CompanionZone{.kind = kind, .radius_cells = radius_cells, .emphasized = emphasized});
  };
  switch (asked.hovered) {
    case CompanionAiZoneSlider::kFollow:
      push(CompanionZoneKind::kFollow, radii.follow_cells, true);
      return plan;
    case CompanionAiZoneSlider::kLeashStill:
      push(CompanionZoneKind::kLeash, radii.leash_still_cells, true);
      return plan;
    case CompanionAiZoneSlider::kLeashWalking:
      push(CompanionZoneKind::kLeash, radii.leash_walking_cells, true);
      return plan;
    case CompanionAiZoneSlider::kEngageStill:
      push(CompanionZoneKind::kEngage, radii.engage_still_cells, true);
      return plan;
    case CompanionAiZoneSlider::kEngageWalking:
      push(CompanionZoneKind::kEngage, radii.engage_walking_cells, true);
      return plan;
    case CompanionAiZoneSlider::kNone:
      break;
  }
  push(CompanionZoneKind::kFollow, radii.follow_cells, false);
  push(CompanionZoneKind::kLeash,
       asked.owner_walking ? radii.leash_walking_cells : radii.leash_still_cells, false);
  push(CompanionZoneKind::kEngage,
       asked.owner_walking ? radii.engage_walking_cells : radii.engage_still_cells, false);
  plan.held_cell = asked.held_cell;
  return plan;
}

std::optional<ro::sim::AiCompanion> ZonesCompanion(ro::sim::AiCompanion tab,
                                                   bool homunculus_present,
                                                   bool mercenary_present) noexcept {
  const auto present = [homunculus_present, mercenary_present](ro::sim::AiCompanion companion) {
    return companion == ro::sim::AiCompanion::kHomunculus ? homunculus_present : mercenary_present;
  };
  if (present(tab)) {
    return tab;
  }
  const ro::sim::AiCompanion other = tab == ro::sim::AiCompanion::kHomunculus
                                         ? ro::sim::AiCompanion::kMercenary
                                         : ro::sim::AiCompanion::kHomunculus;
  if (present(other)) {
    return other;
  }
  return std::nullopt;
}

std::array<std::uint8_t, 4> ZoneColorOf(const CompanionZone& zone) noexcept {
  std::array<std::uint8_t, 4> color = kFollowZoneColor;
  if (zone.kind == CompanionZoneKind::kLeash) {
    color = kLeashZoneColor;
  } else if (zone.kind == CompanionZoneKind::kEngage) {
    color = kEngageZoneColor;
  }
  if (zone.emphasized) {
    color.back() = kEmphasizedZoneAlpha;
  }
  return color;
}

void PaintCompanionZones(const CompanionZonesPlan& plan, ro::sim::CellPosition owner_cell,
                         std::vector<ZoneCellPaint>& into) {
  const auto owner_x = static_cast<std::int32_t>(owner_cell.x);
  const auto owner_y = static_cast<std::int32_t>(owner_cell.y);
  for (const CompanionZone& zone : plan.zones) {
    const CompanionZoneShape shape = ShapeOfZone(zone.kind);
    const std::array<std::uint8_t, 4> color = ZoneColorOf(zone);
    const int edge_cells = zone.emphasized ? kEmphasizedZoneEdgeCells : kZoneEdgeCells;
    // LE CARRÉ DU RAYON CONTIENT LES DEUX FORMES.
    const int radius = zone.radius_cells;
    for (int dy = -radius; dy <= radius; ++dy) {
      for (int dx = -radius; dx <= radius; ++dx) {
        if (ZoneHolds(shape, radius, dx, dy) && OnEdge(zone, shape, dx, dy, edge_cells)) {
          into.push_back(
              ZoneCellPaint{.cell_x = owner_x + dx, .cell_y = owner_y + dy, .color = color});
        }
      }
    }
  }
  if (plan.held_cell.has_value()) {
    into.push_back(ZoneCellPaint{.cell_x = static_cast<std::int32_t>(plan.held_cell->x),
                                 .cell_y = static_cast<std::int32_t>(plan.held_cell->y),
                                 .color = kHeldCellColor});
  }
}

}  // namespace moonclient
