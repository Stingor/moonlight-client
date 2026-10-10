#include "world/struck_place.h"

#include "ro/sim/world.h"

#include "world/hit_popup_rules.h"

namespace moonclient {

StrikeSides SidesOfStrike(const ro::sim::World& world, const ro::sim::Strike& strike) {
  // Qui est « moi » : l'acteur du joueur, ou personne tant que le serveur ne
  // l'a pas dit — un coup sans attaquant ne doit pas passer pour le mien.
  const std::uint32_t self_id = world.player_id();
  const bool self_known = self_id != 0;
  const ro::sim::Actor* const struck = world.Find(strike.target);
  // UNE UNITÉ AU SOL N'EST PAS UN ACTEUR : le serveur met l'identifiant de
  // l'unité qui frappe dans le paquet, et non celui du lanceur. Elle se
  // reconnaît donc à ce que son identifiant vit chez les unités, jamais chez
  // les acteurs — et son coup ne se compte pas comme les autres.
  //
  // CE QU'EST L'ATTAQUANT se lit ici, et nulle part ailleurs : le mot
  // « Miss » naît sur lui, et sans son acteur il n'a pas de place où naître.
  const ro::sim::Actor* attacker = world.Find(strike.attacker);
  const bool target_is_self = self_known && strike.target == self_id;
  return StrikeSides{
      .target_is_player = struck != nullptr && struck->kind == ro::sim::ActorKind::kPlayer,
      .target_is_self = target_is_self,
      .attacker_is_self = self_known && strike.attacker == self_id,
      .attacker_is_known = attacker != nullptr,
      .attacker_is_player = attacker != nullptr && attacker->kind == ro::sim::ActorKind::kPlayer,
      .attacker_is_ground_unit = StrikeByGroundUnit(world, strike),
      .attacker_job = attacker != nullptr ? attacker->look.job : 0U,
      .target_carries_miss_guard =
          target_is_self && struck != nullptr && CarriesMissGuard(*struck)};
}

bool StrikeByGroundUnit(const ro::sim::World& world, const ro::sim::Strike& strike) {
  return strike.by_ground_unit || world.skill_units().contains(strike.attacker);
}

const ro::sim::SkillUnit* StruckUnit(const ro::sim::World& world, std::uint32_t struck_id) {
  const auto found = world.skill_units().find(struck_id);
  return found != world.skill_units().end() ? &found->second : nullptr;
}

ro::sim::FractionalCell CellOfUnit(ro::sim::CellPosition cell) noexcept {
  return ro::sim::FractionalCell{static_cast<float>(cell.x), static_cast<float>(cell.y)};
}

std::optional<ro::sim::FractionalCell> StruckCellAt(const ro::sim::World& world,
                                                    ro::sim::WalkPlanner& planner,
                                                    std::uint32_t struck_id, std::uint32_t at_ms) {
  if (const ro::sim::Actor* actor = world.Find(struck_id); actor != nullptr) {
    return ro::sim::InterpolatedCell(*actor, planner.PathFor(*actor), at_ms);
  }
  if (const ro::sim::SkillUnit* unit = StruckUnit(world, struck_id); unit != nullptr) {
    return CellOfUnit(unit->cell);
  }
  return std::nullopt;
}

std::optional<ro::sim::FractionalCell> StrikeTargetCellAt(const ro::sim::World& world,
                                                          ro::sim::WalkPlanner& planner,
                                                          const ro::sim::Strike& strike,
                                                          std::uint32_t at_ms) {
  if (std::optional<ro::sim::FractionalCell> cell =
          StruckCellAt(world, planner, strike.target, at_ms);
      cell.has_value()) {
    return cell;
  }
  if (strike.struck_unit_cell.has_value()) {
    return CellOfUnit(*strike.struck_unit_cell);
  }
  return std::nullopt;
}

std::optional<PopupCarrierPlace> PopupCarrierAt(
    const ro::sim::World& world, ro::sim::WalkPlanner& planner, std::uint32_t carrier_id,
    std::uint32_t now_ms, const std::optional<ro::sim::CellPosition>& kept_unit_cell) {
  if (const ro::sim::Actor* actor = world.Find(carrier_id); actor != nullptr) {
    const ro::sim::WalkPath& path = planner.PathFor(*actor);
    return PopupCarrierPlace{
        .cell = ro::sim::InterpolatedCell(*actor, path, now_ms),
        .client_body_degrees = ClientBodyDegrees(*actor, ro::sim::FacingAt(*actor, path, now_ms))};
  }
  if (const ro::sim::SkillUnit* unit = StruckUnit(world, carrier_id); unit != nullptr) {
    return PopupCarrierPlace{.cell = CellOfUnit(unit->cell),
                             .client_body_degrees = kGroundUnitBodyDegrees};
  }
  if (kept_unit_cell.has_value()) {
    return PopupCarrierPlace{.cell = CellOfUnit(*kept_unit_cell),
                             .client_body_degrees = kGroundUnitBodyDegrees};
  }
  return std::nullopt;
}

}  // namespace moonclient
