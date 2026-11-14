// UN COUP REÇU PAR UNE UNITÉ AU SOL : un bloc d'Ice Wall frappé à l'épée
// montre son chiffre à sa case, de la couleur et de l'allure d'un monstre, et
// le garde là quand le serveur retire le bloc avant que le coup ne touche.

#include <array>
#include <cstdint>
#include <optional>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/actor.h"
#include "ro/sim/combat_notice.h"
#include "ro/sim/strike_log.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world.h"

#include "world/hit_popup_look.h"
#include "world/hit_popup_rules.h"
#include "world/struck_place.h"

namespace {

constexpr std::uint32_t kIceWallUnit = 141;
constexpr std::uint32_t kIceBlockId = 880'001;
constexpr std::uint32_t kPlayerId = 2'000'001;
constexpr ro::sim::CellPosition kBlockCell{.x = 153, .y = 97};
constexpr ro::sim::CellPosition kPlayerCell{.x = 150, .y = 97};
constexpr std::uint32_t kStruckAtMs = 10'000;
constexpr std::uint32_t kShownAtMs = 10'400;
constexpr std::int32_t kSwordDamage = 312;

[[nodiscard]] ro::sim::World WorldWithBlock() {
  ro::sim::World world;
  ro::sim::Actor player;
  player.id = kPlayerId;
  player.kind = ro::sim::ActorKind::kPlayer;
  player.position = kPlayerCell;
  world.Spawn(player);
  world.set_player_id(kPlayerId);
  world.PlaceSkillUnit(ro::sim::SkillUnit{
      .id = kIceBlockId, .creator_id = kPlayerId, .cell = kBlockCell, .unit_id = kIceWallUnit});
  return world;
}

[[nodiscard]] ro::sim::Strike SwordOnBlock() {
  return ro::sim::Strike{.attacker = kPlayerId,
                         .target = kIceBlockId,
                         .started_ms = kStruckAtMs,
                         .flinches = true,
                         .damage = kSwordDamage};
}

}  // namespace

TEST_CASE("Unité frappée : le chiffre d'un bloc d'Ice Wall naît à la case du bloc",
          "[app][degats][ground_unit]") {
  ro::sim::World world = WorldWithBlock();
  ro::sim::WalkPlanner planner{nullptr};
  REQUIRE(world.RecordStrike(SwordOnBlock()));

  // Le coup est au journal, et sa cible a une place : la case du bloc, où le
  // client pose le chiffre — le point de la case, et non un acteur introuvable.
  ro::sim::StrikeCursor cursor;
  const auto taken = cursor.Take(world.strikes());
  REQUIRE(taken.size() == 1);
  REQUIRE(taken.front().strike.target == kIceBlockId);
  const std::optional<ro::sim::FractionalCell> struck =
      moonclient::StruckCellAt(world, planner, kIceBlockId, kShownAtMs);
  REQUIRE(struck.has_value());
  CHECK(struck->x == static_cast<float>(kBlockCell.x));
  CHECK(struck->y == static_cast<float>(kBlockCell.y));

  // Le porteur du chiffre : la case du bloc, et l'angle nul d'une unité que
  // rien ne tourne — la dérive part donc du côté que cet angle choisit.
  const std::optional<moonclient::PopupCarrierPlace> carrier =
      moonclient::PopupCarrierAt(world, planner, kIceBlockId, kShownAtMs, std::nullopt);
  REQUIRE(carrier.has_value());
  CHECK(carrier->cell.x == static_cast<float>(kBlockCell.x));
  CHECK(carrier->cell.y == static_cast<float>(kBlockCell.y));
  CHECK(carrier->client_body_degrees == moonclient::kGroundUnitBodyDegrees);
  CHECK(moonclient::BurstDriftInWorld(carrier->client_body_degrees) ==
        moonclient::BurstDriftInWorld(0.0F));

  // Le chiffre paraît, de la valeur du coup et de la couleur de MES coups sur
  // une créature : le bloc n'est pas un joueur.
  const moonclient::StrikeSides sides = moonclient::SidesOfStrike(world, SwordOnBlock());
  CHECK_FALSE(sides.target_is_player);
  CHECK(sides.attacker_is_self);
  const moonclient::HitPopupLook look{};
  const moonclient::StrikePlan plan = moonclient::PlanStrike(SwordOnBlock(), sides, look);
  REQUIRE(plan.shown);
  CHECK_FALSE(plan.mark.has_value());
  CHECK(moonclient::StrikeValueAt(SwordOnBlock(), plan, 0) ==
        static_cast<std::uint32_t>(kSwordDamage));
}

TEST_CASE("Unité frappée : le coup qui brise le bloc garde sa case après le retrait",
          "[app][degats][ground_unit]") {
  ro::sim::World world = WorldWithBlock();
  ro::sim::WalkPlanner planner{nullptr};
  REQUIRE(world.RecordStrike(SwordOnBlock()));
  // Le serveur retire le bloc brisé aussitôt, bien avant que l'épée ne touche.
  REQUIRE(world.RemoveSkillUnit(kIceBlockId));

  CHECK_FALSE(moonclient::StruckCellAt(world, planner, kIceBlockId, kShownAtMs).has_value());
  CHECK_FALSE(moonclient::PopupCarrierAt(world, planner, kIceBlockId, kShownAtMs, std::nullopt)
                  .has_value());
  // Le coup au journal porte la case du bloc, relevée à son arrivée : c'est
  // elle qui tient lieu du bloc disparu.
  ro::sim::StrikeCursor cursor;
  const auto taken = cursor.Take(world.strikes());
  REQUIRE(taken.size() == 1);
  const ro::sim::Strike& recorded = taken.front().strike;
  REQUIRE(recorded.struck_unit_cell.has_value());
  CHECK(recorded.struck_unit_cell->x == kBlockCell.x);
  CHECK(recorded.struck_unit_cell->y == kBlockCell.y);
  const std::optional<ro::sim::FractionalCell> target =
      moonclient::StrikeTargetCellAt(world, planner, recorded, kShownAtMs);
  REQUIRE(target.has_value());
  CHECK(target->x == static_cast<float>(kBlockCell.x));
  CHECK(target->y == static_cast<float>(kBlockCell.y));
  const std::optional<moonclient::PopupCarrierPlace> kept = moonclient::PopupCarrierAt(
      world, planner, kIceBlockId, kShownAtMs, recorded.struck_unit_cell);
  REQUIRE(kept.has_value());
  CHECK(kept->cell.x == static_cast<float>(kBlockCell.x));
  CHECK(kept->cell.y == static_cast<float>(kBlockCell.y));
}

TEST_CASE("Unité frappée : un acteur frappé garde sa place de marche", "[app][degats]") {
  ro::sim::World world = WorldWithBlock();
  ro::sim::WalkPlanner planner{nullptr};
  // Un acteur l'emporte sur une case gardée : il est là, et il se lit là où il
  // se tient.
  const std::optional<moonclient::PopupCarrierPlace> self =
      moonclient::PopupCarrierAt(world, planner, kPlayerId, kShownAtMs, kBlockCell);
  REQUIRE(self.has_value());
  CHECK(self->cell.x == static_cast<float>(kPlayerCell.x));
  CHECK(self->cell.y == static_cast<float>(kPlayerCell.y));
  CHECK(moonclient::StruckUnit(world, kPlayerId) == nullptr);
  CHECK(moonclient::StruckUnit(world, kIceBlockId) != nullptr);
}

TEST_CASE("Unité qui frappe : la météore garde son chiffre après le retrait de son unité",
          "[app][degats][ground_unit]") {
  // Le serveur porte chaque météore par une unité au sol, et la retire dans le
  // même envoi que son coup : quand on décide du chiffre, elle n'est déjà plus
  // posée. Le coup doit rester celui d'une unité au sol.
  constexpr std::uint32_t kMeteorUnitId = 880'101;
  constexpr std::uint32_t kMonsterId = 110'000'001;
  constexpr std::uint16_t kMeteorStorm = 83;
  constexpr std::uint8_t kMultiHit = 8;
  constexpr std::uint16_t kMeteorHits = 7;
  constexpr std::int32_t kMeteorDamage = 7000;
  ro::sim::World world = WorldWithBlock();
  ro::sim::Actor monster;
  monster.id = kMonsterId;
  monster.kind = ro::sim::ActorKind::kMonster;
  monster.position = kBlockCell;
  world.Spawn(monster);
  world.PlaceSkillUnit(ro::sim::SkillUnit{.id = kMeteorUnitId,
                                          .creator_id = kPlayerId,
                                          .cell = kBlockCell,
                                          .unit_id = ro::sim::kSilentSkillUnit});
  REQUIRE(world.RecordStrike(ro::sim::Strike{.attacker = kMeteorUnitId,
                                             .target = kMonsterId,
                                             .started_ms = kStruckAtMs,
                                             .damage = kMeteorDamage,
                                             .hits = kMeteorHits,
                                             .type = kMultiHit,
                                             .skill_id = kMeteorStorm}));
  REQUIRE(world.RemoveSkillUnit(kMeteorUnitId));

  ro::sim::StrikeCursor cursor;
  const auto taken = cursor.Take(world.strikes());
  REQUIRE(taken.size() == 1);
  const ro::sim::Strike& recorded = taken.front().strike;
  CHECK(recorded.by_ground_unit);
  CHECK(moonclient::StrikeByGroundUnit(world, recorded));
  const moonclient::StrikeSides sides = moonclient::SidesOfStrike(world, recorded);
  CHECK(sides.attacker_is_ground_unit);
  const moonclient::StrikePlan plan =
      moonclient::PlanStrike(recorded, sides, moonclient::HitPopupLook{});
  REQUIRE(plan.shown);
  CHECK(plan.count == kMeteorHits);
  CHECK(moonclient::StrikeValueAt(recorded, plan, 0) ==
        static_cast<std::uint32_t>(kMeteorDamage / kMeteorHits));
}
