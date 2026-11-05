#include "survivors/survivors_minimap_marks.h"

#include <array>
#include <cstddef>

#include "ro/sim/survivors_geometry.h"

#include "survivors/survivors_gems.h"

namespace moonclient {
namespace {

using ro::ui::SurvivorsMinimapMark;

// L'écart d'un point au héros, en cases continues.
[[nodiscard]] ImVec2 OffsetOf(ro::sim::FieldPoint point, ro::sim::FieldPoint hero) noexcept {
  const auto per_cell = static_cast<float>(ro::sim::kSubunitsPerCell);
  return ImVec2(static_cast<float>(point.x - hero.x) / per_cell,
                static_cast<float>(point.y - hero.y) / per_cell);
}

[[nodiscard]] SurvivorsMinimapMark MarkOf(ro::sim::PickupKind kind) noexcept {
  switch (kind) {
    case ro::sim::PickupKind::kChest:
      return SurvivorsMinimapMark::kChest;
    case ro::sim::PickupKind::kCard:
      return SurvivorsMinimapMark::kCard;
    case ro::sim::PickupKind::kMeat:
    case ro::sim::PickupKind::kGold:
    case ro::sim::PickupKind::kManual:
    case ro::sim::PickupKind::kHaste:
    case ro::sim::PickupKind::kHolyWater:
    case ro::sim::PickupKind::kMagnetAll:
      return SurvivorsMinimapMark::kItem;
  }
  return SurvivorsMinimapMark::kItem;
}

// L'ordre de peinture : chaque marque après celles qui la précèdent ici.
constexpr std::array<SurvivorsMinimapMark, 8> kPaintOrder{
    SurvivorsMinimapMark::kPlant, SurvivorsMinimapMark::kItem,     SurvivorsMinimapMark::kCard,
    SurvivorsMinimapMark::kChest, SurvivorsMinimapMark::kEmperium, SurvivorsMinimapMark::kMiniBoss,
    SurvivorsMinimapMark::kBoss,  SurvivorsMinimapMark::kAlly,
};

}  // namespace

void SurvivorsMinimapDotsOf(const ro::sim::SurvivorsField& field,
                            const ro::sim::SurvivorsRules& rules,
                            std::vector<ro::ui::SurvivorsMinimapDot>& into) {
  into.clear();
  const ro::sim::FieldPoint hero = field.hero.position;
  const auto add = [&into, hero](ro::sim::FieldPoint point, SurvivorsMinimapMark mark) {
    into.push_back(
        ro::ui::SurvivorsMinimapDot{.offset_cells = OffsetOf(point, hero), .mark = mark});
  };
  for (const SurvivorsMinimapMark wanted : kPaintOrder) {
    switch (wanted) {
      case SurvivorsMinimapMark::kAlly:
        for (std::size_t index = 0; index < ro::sim::HeroCount(field); ++index) {
          const ro::sim::SurvivorsHero& ally = ro::sim::HeroAt(field, index);
          if (index != field.stage && !ally.down) add(ally.position, wanted);
        }
        break;
      case SurvivorsMinimapMark::kPlant:
        for (const ro::sim::SurvivorsPlant& plant : field.plants) add(plant.position, wanted);
        break;
      case SurvivorsMinimapMark::kItem:
      case SurvivorsMinimapMark::kCard:
      case SurvivorsMinimapMark::kChest:
        for (const ro::sim::SurvivorsPickup& pickup : field.pickups) {
          if (MarkOf(pickup.kind) == wanted) add(pickup.position, wanted);
        }
        break;
      case SurvivorsMinimapMark::kEmperium:
        for (const ro::sim::SurvivorsGem& gem : field.gems) {
          if (gem.exp >= kEmperiumFromExp) add(gem.position, wanted);
        }
        break;
      case SurvivorsMinimapMark::kMiniBoss:
      case SurvivorsMinimapMark::kBoss:
        for (const ro::sim::SurvivorsFoe& foe : field.foes) {
          const ro::sim::FoeRule* rule = ro::sim::FindFoe(rules, foe.foe_id);
          if (rule == nullptr || foe.hp <= 0) continue;
          const bool boss = rule->rank == ro::sim::FoeRank::kBoss;
          const bool mini_boss = rule->rank == ro::sim::FoeRank::kMiniBoss;
          if ((wanted == SurvivorsMinimapMark::kBoss && boss) ||
              (wanted == SurvivorsMinimapMark::kMiniBoss && mini_boss)) {
            add(foe.position, wanted);
          }
        }
        break;
    }
  }
}

}  // namespace moonclient
