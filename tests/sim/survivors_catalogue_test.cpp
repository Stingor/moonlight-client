#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/error.h"
#include "ro/sim/survivors_content.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_terrain.h"

using ro::sim::BossMoveKind;
using ro::sim::Cells;
using ro::sim::CellTenths;
using ro::sim::EventKind;
using ro::sim::FoeMove;
using ro::sim::FoeRank;
using ro::sim::FoeRule;
using ro::sim::FoeSize;
using ro::sim::PickupKind;
using ro::sim::SkillArchetype;
using ro::sim::SkillLevel;
using ro::sim::SurvivorsElement;
using ro::sim::SurvivorsRules;
using ro::sim::SurvivorsSkillRule;
using ro::sim::TargetRule;

namespace {

// LE CATALOGUE DU RULESET 2, recopié du cahier du lot 2 : une faute de frappe
// dans la table livrée se voit ici, avant que la partie en or ne la fige. Les
// skills et cartes du lot 1 sont épinglés dans survivors_rules_test.cpp.

[[nodiscard]] const SurvivorsRules& Shipped() { return ro::sim::ShippedSurvivorsRules(); }

struct PinnedFoe {
  std::uint16_t foe_id;
  std::uint8_t ro_level;
  FoeRank rank;
  FoeSize size;
  SurvivorsElement element;
  std::int32_t hp;
  std::uint32_t walk_ms;
  std::int32_t touch_damage;
  std::uint32_t touch_ms;
  std::uint32_t exp;
  FoeMove move;
};

using enum FoeRank;
using enum FoeSize;
using enum SurvivorsElement;
using enum FoeMove;

// Les 45 espèces, dans l'ordre de la table livrée.
constexpr std::array<PinnedFoe, 45> kPinnedFoes{{
    {1063, 3, kCommon, kSmall, kNeutral, 11, 200, 3, 1456, 1, kChase},
    {1007, 2, kCommon, kSmall, kEarth, 22, 400, 4, 1672, 1, kChase},
    {1002, 1, kCommon, kMedium, kWater, 15, 400, 4, 1872, 1, kChase},
    {1113, 3, kCommon, kMedium, kFire, 28, 400, 6, 1372, 2, kChase},
    {1011, 4, kCommon, kSmall, kWind, 14, 200, 4, 1076, 1, kChase},
    {1010, 4, kCommon, kMedium, kEarth, 28, 200, 6, 1672, 2, kChase},
    {1008, 2, kCommon, kSmall, kEarth, 60, 0, 0, 0, 2, kStill},
    {1052, 9, kCommon, kMedium, kEarth, 30, 200, 7, 1864, 2, kChase},
    {1009, 5, kCommon, kMedium, kWind, 24, 150, 6, 1148, 2, kChase},
    {1167, 7, kCommon, kSmall, kEarth, 20, 400, 5, 1624, 2, kChase},
    {1004, 8, kCommon, kSmall, kWind, 20, 150, 6, 1292, 2, kChase},
    {1005, 8, kCommon, kSmall, kDark, 18, 150, 5, 1276, 2, kFlyer},
    {1051, 6, kCommon, kSmall, kNeutral, 22, 150, 6, 1288, 2, kThief},
    {1014, 16, kCommon, kMedium, kWater, 40, 200, 8, 1872, 3, kChase},
    {1031, 14, kCommon, kMedium, kPoison, 36, 300, 8, 1672, 3, kChase},
    {1056, 18, kCommon, kSmall, kEarth, 36, 200, 6, 1576, 3, kChase},
    {1076, 10, kCommon, kMedium, kUndead, 34, 200, 7, 2228, 3, kChase},
    {1015, 15, kCommon, kMedium, kUndead, 60, 400, 8, 2612, 4, kChase},
    {1077, 19, kCommon, kMedium, kPoison, 48, 200, 6, 1672, 4, kChase},
    {1013, 25, kCommon, kMedium, kEarth, 40, 200, 7, 1054, 5, kChase},
    {1057, 21, kCommon, kSmall, kEarth, 35, 200, 7, 1054, 4, kChase},
    {1023, 24, kCommon, kMedium, kEarth, 50, 200, 7, 1864, 6, kChase},
    {1166, 26, kCommon, kLarge, kEarth, 100, 150, 7, 1960, 10, kCharger},
    {1122, 25, kCommon, kMedium, kWind, 60, 100, 12, 1120, 6, kChase},
    {1139, 26, kCommon, kMedium, kEarth, 65, 200, 8, 1528, 8, kChase},
    {1152, 28, kCommon, kMedium, kUndead, 45, 200, 12, 2420, 7, kChase},
    {1016, 31, kCommon, kMedium, kUndead, 40, 300, 0, 0, 8, kRanged},
    {1026, 30, kCommon, kMedium, kUndead, 50, 200, 10, 2468, 8, kChase},
    {1040, 25, kCommon, kLarge, kNeutral, 150, 300, 10, 1608, 15, kChase},
    {1060, 25, kCommon, kLarge, kEarth, 110, 300, 12, 1260, 12, kChase},
    {1035, 42, kCommon, kSmall, kWind, 45, 150, 8, 676, 8, kFlyer},
    {1041, 37, kCommon, kMedium, kUndead, 85, 300, 12, 1772, 12, kChase},
    {1036, 40, kCommon, kMedium, kUndead, 90, 250, 14, 2456, 12, kChase},
    {1102, 44, kCommon, kMedium, kDark, 70, 100, 8, 1504, 12, kBlinker},
    {1170, 33, kCommon, kMedium, kWater, 75, 250, 12, 2112, 12, kChase},
    {1179, 34, kCommon, kSmall, kGhost, 40, 150, 12, 1960, 10, kFlyer},
    {1061, 49, kCommon, kLarge, kGhost, 125, 150, 11, 1816, 18, kCharger},
    {1192, 53, kCommon, kLarge, kUndead, 150, 300, 22, 1816, 20, kChase},
    {1117, 58, kCommon, kLarge, kUndead, 170, 300, 24, 2276, 22, kChase},
    {1101, 50, kCommon, kSmall, kDark, 400, 100, 15, 868, 5, kChase},
    {1090, 2, kMiniBoss, kMedium, kWater, 600, 300, 12, 1072, 20, kChase},
    {1093, 6, kMiniBoss, kMedium, kNeutral, 900, 200, 11, 1456, 30, kChase},
    {1091, 8, kMiniBoss, kSmall, kWind, 1200, 100, 12, 1076, 40, kFlyer},
    {1092, 24, kMiniBoss, kMedium, kEarth, 2400, 150, 11, 1048, 60, kChase},
    {1039, 81, kBoss, kLarge, kDark, 11000, 100, 35, 768, 0, kChase},
}};

[[nodiscard]] const FoeRule& Foe(std::uint16_t foe_id) {
  const FoeRule* rule = ro::sim::FindFoe(Shipped(), foe_id);
  REQUIRE(rule != nullptr);
  return *rule;
}

[[nodiscard]] const SurvivorsSkillRule& Skill(std::uint16_t skill_id) {
  const SurvivorsSkillRule* rule = ro::sim::FindSkill(Shipped(), skill_id);
  REQUIRE(rule != nullptr);
  return *rule;
}

}  // namespace

TEST_CASE("Catalogue survivors : le ruleset 8, compte par compte", "[sim][survivors]") {
  const SurvivorsRules& shipped = Shipped();
  CHECK(shipped.ruleset_version == 10);
  CHECK(shipped.run_ms == 900000U);
  const ro::core::Result<void> checked = ro::sim::CheckSurvivorsRules(shipped);
  INFO((checked ? std::string_view{} : std::string_view{checked.error().detail}));
  CHECK(checked.has_value());

  // Le Sorcier en tête — la partie classée et le défi prennent la première
  // classe —, puis le Chevalier, l'Assassin, le Chasseur et l'Alchimiste, hors
  // classement : chacun porte sa 2e classe dès le départ, puis sa transcendante
  // une fois rené (High Wizard, Lord Knight, Assassin Cross, Sniper, Creator).
  REQUIRE(shipped.classes.size() == 5U);
  CHECK(shipped.classes[0].skill_pool.size() == 13U);
  CHECK(shipped.classes[0].card_pool.size() == 16U);
  CHECK(shipped.classes[0].evolved_job_id == 9);
  CHECK(shipped.classes[0].reborn_job_id == 4010);
  CHECK(shipped.classes[1].job_id == 7);
  CHECK(shipped.classes[1].skill_pool.size() == 9U);
  CHECK(shipped.classes[1].card_pool.size() == 16U);
  CHECK(shipped.classes[1].evolved_job_id == 7);
  CHECK(shipped.classes[1].reborn_job_id == 4008);
  CHECK(shipped.classes[2].job_id == 12);
  CHECK(shipped.classes[2].skill_pool.size() == 10U);
  CHECK(shipped.classes[2].card_pool.size() == 17U);
  CHECK(shipped.classes[2].evolved_job_id == 12);
  CHECK(shipped.classes[2].reborn_job_id == 4013);
  CHECK(shipped.classes[3].job_id == 11);
  CHECK(shipped.classes[3].skill_pool.size() == 9U);
  CHECK(shipped.classes[3].card_pool.size() == 16U);
  CHECK(shipped.classes[3].lent_cards.size() == 6U);
  CHECK(shipped.classes[3].evolved_job_id == 11);
  CHECK(shipped.classes[3].reborn_job_id == 4012);
  CHECK_FALSE(shipped.classes[3].free_play_only);
  CHECK(shipped.classes[4].job_id == 18);
  CHECK(shipped.classes[4].skill_pool.size() == 13U);
  CHECK(shipped.classes[4].card_pool.size() == 16U);
  CHECK(shipped.classes[4].lent_cards.size() == 6U);
  CHECK(shipped.classes[4].evolved_job_id == 18);
  CHECK(shipped.classes[4].reborn_job_id == 4019);
  CHECK_FALSE(shipped.classes[4].free_play_only);
  CHECK(shipped.skills.size() == 101U);
  CHECK(shipped.cards.size() == 30U);
  CHECK(shipped.evolutions.size() == 42U);
  // La prairie, le désert de Morroc, les neiges de Lutie, puis le labo.
  CHECK(shipped.foes.size() == 113U);
  CHECK(shipped.waves.size() == 18U);
  CHECK(shipped.events.size() == 8U);
  REQUIRE(shipped.bosses.size() == 5U);
  // Un seul boss final, le dernier.
  CHECK(std::ranges::count_if(shipped.bosses,
                              [](const ro::sim::BossRule& boss) { return boss.final; }) == 1);
  CHECK(shipped.bosses.back().final);

  // Quatre arènes, toutes des biomes générés, sans carte, toutes classées :
  // la prairie, puis le désert et la neige, chacun avec son contenu, et le
  // labo, avec son contenu et ses MVP tirés. Les plantes de la prairie
  // s'éveillent et se perdent par morceau.
  REQUIRE(shipped.arenas.size() == 4U);
  for (const ro::sim::ArenaRule& arena : shipped.arenas) {
    CHECK(std::string_view{arena.map_name}.empty());
    CHECK(arena.terrain_id != 0);
    CHECK(arena.ranked);
  }
  CHECK(shipped.arenas[0].ranked);
  CHECK(ro::sim::FirstRankedArena(shipped) == &shipped.arenas[0]);
  CHECK(shipped.arenas[0].arena_id == 2);
  CHECK(std::string_view{shipped.arenas[0].key} == "midgard_plains");
  CHECK_FALSE(shipped.arenas[0].start.has_value());
  // La prairie joue le contenu des règles ; le désert, le sien.
  CHECK(shipped.arenas[0].waves.empty());
  const ro::sim::ArenaRule& desert = shipped.arenas[1];
  CHECK(desert.arena_id == 3);
  CHECK(std::string_view{desert.key} == "morroc_desert");
  CHECK(desert.waves.size() == 18U);
  CHECK(desert.events.size() == 7U);
  REQUIRE(desert.bosses.size() == 5U);
  CHECK(desert.bosses.back().final);
  CHECK(desert.bosses.back().foe_id == 1157);  // Pharaoh
  const ro::sim::FoeRule* pharaoh = ro::sim::FindFoe(shipped, 1157);
  REQUIRE(pharaoh != nullptr);
  CHECK(pharaoh->armor_pct == 40);
  CHECK(ro::sim::ContentOf(shipped, shipped.arenas[0]).waves.data() == shipped.waves.data());
  CHECK(ro::sim::ContentOf(shipped, desert).waves.data() == desert.waves.data());
  REQUIRE(shipped.terrains.size() == 4U);
  const ro::sim::ArenaRule& lab = shipped.arenas[3];
  CHECK(lab.arena_id == 5);
  CHECK(std::string_view{lab.key} == "lighthalzen_biolab");
  CHECK(ro::sim::ContentOf(shipped, lab).waves.data() == lab.waves.data());
  CHECK(lab.events.size() == 7U);
  // LE MVP TIRÉ : deux jeux de boss, que la graine choisit ; chacun finit
  // par son MVP, et les cinq mini-boss sont les mêmes.
  REQUIRE(lab.boss_draws.size() == 2U);
  CHECK(ro::sim::ContentOf(shipped, lab, 0).bosses.back().foe_id == 1646);  // Lord Knight
  CHECK(ro::sim::ContentOf(shipped, lab, 1).bosses.back().foe_id == 1651);  // High Wizard
  CHECK(ro::sim::ContentOf(shipped, lab, 7).bosses.data() == lab.boss_draws[1].data());
  for (const std::span<const ro::sim::BossRule> bosses : lab.boss_draws) {
    REQUIRE(bosses.size() == 6U);
    CHECK(bosses.back().final);
    CHECK(bosses.front().foe_id == 1681);  // Gemini-S58
  }
  // Le désert et la neige, sans tirage, jouent toujours leurs boss.
  CHECK(ro::sim::ContentOf(shipped, desert, 1).bosses.data() == desert.bosses.data());
  // Deux ruptures de cuves, sur la cuve du terrain du labo.
  CHECK(std::ranges::count_if(lab.events, [](const ro::sim::EventRule& event) {
          return event.kind == ro::sim::EventKind::kTanks && event.piece_id == 1;
        }) == 2);
  // Le labo est le seul terrain à salles, et n'a pas de chemins.
  const ro::sim::TerrainRule& lab_terrain = shipped.terrains[3];
  CHECK(lab_terrain.terrain_id == lab.terrain_id);
  CHECK(std::string_view{lab_terrain.key} == "lighthalzen_biolab");
  CHECK(lab_terrain.rooms);
  CHECK(lab_terrain.door_cells == 8);
  CHECK(lab_terrain.path_period_chunks == 0);
  for (std::size_t terrain = 0; terrain < 3U; ++terrain)
    CHECK_FALSE(shipped.terrains[terrain].rooms);
  CHECK(shipped.terrains[2].terrain_id == shipped.arenas[2].terrain_id);
  CHECK(std::string_view{shipped.terrains[2].key} == "lutie_snowfield");
  CHECK(shipped.terrains[1].terrain_id == desert.terrain_id);
  CHECK(std::string_view{shipped.terrains[1].key} == "morroc_desert");
  CHECK(shipped.terrains[1].path_period_chunks == 0);
  const ro::sim::TerrainRule& plains = shipped.terrains[0];
  CHECK(plains.terrain_id == shipped.arenas[0].terrain_id);
  CHECK(std::string_view{plains.key} == "midgard_plains");
  CHECK(plains.pieces.size() == 7U);
  CHECK(std::ranges::count_if(plains.pieces, [](const ro::sim::TerrainPieceRule& piece) {
          return piece.landmark;
        }) == 3);
  // Le fourré se traverse ; toute autre pièce bloque son emprise.
  for (const ro::sim::TerrainPieceRule& piece : plains.pieces) {
    CHECK(piece.walkable == (std::string_view{piece.key} == "thicket"));
  }
  // La prairie est sillonnée de chemins.
  CHECK(plains.path_period_chunks > 0);
  CHECK(plains.path_width_cells > 0);
  // Les plantes naissent au fil des tués, sur les deux arènes.
  CHECK(shipped.tuning.plant_kills_per_minute == 10U);
  CHECK(shipped.tuning.plant_start_count == 5);
  CHECK(shipped.tuning.plant_start_cells == 12);
  CHECK(shipped.tuning.most_live_plants == 0);
  CHECK(shipped.tuning.plant_forget_units == ro::sim::Cells(48));

  // Chaque Id de vague et d'événement est un commun du bestiaire.
  for (const ro::sim::WaveRule& wave : shipped.waves) {
    for (const std::uint16_t foe_id : wave.foe_ids) {
      INFO("vague de " << wave.from_ms << " ms, monstre " << foe_id);
      CHECK(Foe(foe_id).rank == kCommon);
    }
  }
  for (const ro::sim::EventRule& event : shipped.events) CHECK(Foe(event.foe_id).rank == kCommon);

  // Un évolué n'entre dans aucune réserve ; chaque texte est à traduire.
  for (const ro::sim::EvolutionRule& evolution : shipped.evolutions) {
    CHECK(std::ranges::find(shipped.classes[0].skill_pool, evolution.evolved_skill_id) ==
          shipped.classes[0].skill_pool.end());
    CHECK(Skill(evolution.evolved_skill_id).levels.size() == 1U);
  }
  for (const SurvivorsSkillRule& skill : shipped.skills) {
    CHECK_FALSE(std::string_view{skill.description}.empty());
  }
  for (const ro::sim::CardRule& card : shipped.cards) {
    CHECK_FALSE(std::string_view{card.description}.empty());
  }
}

TEST_CASE("Catalogue survivors : les 45 espèces et leurs comportements", "[sim][survivors]") {
  const SurvivorsRules& shipped = Shipped();
  // Les 45 monstres de la prairie, en tête.
  REQUIRE(shipped.foes.size() >= kPinnedFoes.size());
  for (std::size_t index = 0; index < kPinnedFoes.size(); ++index) {
    const PinnedFoe& pinned = kPinnedFoes[index];
    const FoeRule& foe = shipped.foes[index];
    INFO("monstre " << pinned.foe_id);
    CHECK(foe.foe_id == pinned.foe_id);
    CHECK(foe.ro_level == pinned.ro_level);
    CHECK(foe.rank == pinned.rank);
    CHECK(foe.size == pinned.size);
    CHECK(foe.element == pinned.element);
    CHECK(foe.hp == pinned.hp);
    CHECK(foe.ro_walk_ms_per_cell == pinned.walk_ms);
    CHECK(foe.touch_damage == pinned.touch_damage);
    CHECK(foe.touch_interval_ms == pinned.touch_ms);
    CHECK(foe.exp == pinned.exp);
    CHECK(foe.behaviour.move == pinned.move);
  }

  // Les paramètres de comportement du cahier.
  const auto& poporing = Foe(1031).behaviour;
  CHECK(poporing.puddle_damage == 2);
  CHECK(poporing.puddle_radius_units == CellTenths(15));
  CHECK(poporing.puddle_ms == 4000U);
  CHECK(poporing.puddle_period_ms == 1000U);
  const auto& smokie = Foe(1056).behaviour;
  CHECK(smokie.traits == ro::sim::kFoeHides);
  CHECK(smokie.period_ms == 6000U);
  CHECK(smokie.active_ms == 1000U);
  CHECK(Foe(1013).behaviour.pack == 6);
  CHECK(Foe(1040).behaviour.traits == ro::sim::kFoeKnockbackImmune);
  for (const std::uint16_t charger_id : {std::uint16_t{1166}, std::uint16_t{1061}}) {
    const auto& charger = Foe(charger_id).behaviour;
    CHECK(charger.period_ms == 5000U);
    CHECK(charger.telegraph_ms == 500U);
    CHECK(charger.reach_units == Cells(6));
    CHECK(charger.dash_units_per_s == Cells(16));
    // Quatre cases à seize cases par seconde.
    CHECK(charger.active_ms == 250U);
  }
  const auto& archer = Foe(1016).behaviour;
  CHECK(archer.reach_units == Cells(6));
  CHECK(archer.telegraph_ms == 500U);
  CHECK(archer.period_ms == 2864U);
  CHECK(archer.shot_damage == 6);
  CHECK(archer.shot_units_per_s == Cells(10));
  CHECK(archer.shot_radius_units == CellTenths(3));
  const auto& bathory = Foe(1102).behaviour;
  CHECK(bathory.telegraph_ms == 300U);
  CHECK(bathory.period_ms == 4000U);
  CHECK(bathory.reach_units == Cells(3));
  CHECK(Foe(1005).behaviour.traits == ro::sim::kFoeOverHorde);
  CHECK(Foe(1035).behaviour.traits == ro::sim::kFoeOverHorde);
  CHECK(Foe(1179).behaviour.traits == (ro::sim::kFoeOverHorde | ro::sim::kFoePassesWalls));
  CHECK(Foe(1039).behaviour.traits == (ro::sim::kFoeKnockbackImmune | ro::sim::kFoeFreezeImmune));
}

TEST_CASE("Catalogue survivors : les vagues et les événements", "[sim][survivors]") {
  const SurvivorsRules& shipped = Shipped();
  struct PinnedWave {
    std::uint32_t from_ms;
    std::uint16_t floor;
    std::uint32_t interval_ms;
    std::uint8_t batch;
    std::size_t foes;
  };
  constexpr std::array<PinnedWave, 18> kPinnedWaves{{
      {0, 15, 1000, 2, 3},
      {60000, 30, 1000, 3, 4},
      {120000, 45, 900, 3, 4},
      {180000, 60, 900, 4, 4},
      {240000, 75, 800, 4, 4},
      {300000, 90, 800, 5, 4},
      {360000, 110, 700, 5, 4},
      {420000, 80, 700, 6, 3},
      {440000, 130, 700, 6, 3},
      {480000, 150, 600, 6, 4},
      {540000, 170, 600, 7, 4},
      {600000, 190, 500, 7, 4},
      {660000, 160, 500, 8, 4},
      {680000, 210, 500, 8, 4},
      {720000, 230, 450, 8, 4},
      {780000, 260, 400, 9, 5},
      {840000, 300, 350, 10, 4},
      {890000, 0, 1000, 0, 4},
  }};
  REQUIRE(shipped.waves.size() == kPinnedWaves.size());
  for (std::size_t index = 0; index < kPinnedWaves.size(); ++index) {
    const PinnedWave& pinned = kPinnedWaves[index];
    const ro::sim::WaveRule& wave = shipped.waves[index];
    INFO("vague de " << pinned.from_ms << " ms");
    CHECK(wave.from_ms == pinned.from_ms);
    CHECK(wave.floor == pinned.floor);
    CHECK(wave.interval_ms == pinned.interval_ms);
    CHECK(wave.batch == pinned.batch);
    CHECK(wave.foe_ids.size() == pinned.foes);
  }
  // La vague de 4:00 : Willow, Rocker, Condor, Savage Babe.
  constexpr std::array<std::uint16_t, 4> kWave4Foes{1010, 1052, 1009, 1167};
  constexpr std::array<std::uint32_t, 4> kWave4Weights{30, 25, 25, 20};
  CHECK(std::ranges::equal(shipped.waves[4].foe_ids, kWave4Foes));
  CHECK(std::ranges::equal(shipped.waves[4].weights, kWave4Weights));

  constexpr std::array<ro::sim::EventRule, 8> kPinnedEvents{{
      {90000, EventKind::kSwarm, 1011, 24, 0},
      {150000, EventKind::kRing, 1008, 28, Cells(6)},
      {270000, EventKind::kSwarm, 1004, 30, 0},
      {340000, EventKind::kGroup, 1051, 6, 0},
      {390000, EventKind::kRing, 1014, 12, Cells(7)},
      {510000, EventKind::kSwarm, 1005, 40, 0},
      {630000, EventKind::kRaid, 1122, 18, 0},
      {810000, EventKind::kRing, 1036, 30, Cells(8)},
  }};
  REQUIRE(shipped.events.size() == kPinnedEvents.size());
  for (std::size_t index = 0; index < kPinnedEvents.size(); ++index) {
    const ro::sim::EventRule& pinned = kPinnedEvents[index];
    const ro::sim::EventRule& event = shipped.events[index];
    INFO("événement de " << pinned.at_ms << " ms");
    CHECK(event.at_ms == pinned.at_ms);
    CHECK(event.kind == pinned.kind);
    CHECK(event.foe_id == pinned.foe_id);
    CHECK(event.count == pinned.count);
    CHECK(event.radius_units == pinned.radius_units);
  }
}

TEST_CASE("Catalogue survivors : les skills neufs, les évolutions et les cartes neuves",
          "[sim][survivors]") {
  // Lightning Bolt rebondit des niveaux 4 à 7 une fois, au niveau 8 deux fois.
  constexpr std::array<std::uint8_t, 8> kBounces{0, 0, 0, 1, 1, 1, 1, 2};
  const SurvivorsSkillRule& lightning = Skill(20);
  REQUIRE(lightning.levels.size() == kBounces.size());
  for (std::size_t index = 0; index < kBounces.size(); ++index) {
    CHECK(lightning.levels[index].bounces == kBounces[index]);
    if (kBounces[index] == 0) continue;
    CHECK(lightning.levels[index].bounce_pct == 60);
    CHECK(lightning.levels[index].bounce_reach_units == Cells(3));
  }

  const SurvivorsSkillRule& napalm = Skill(11);
  CHECK(napalm.archetype == SkillArchetype::kClusterBlast);
  CHECK(napalm.target == TargetRule::kDensest);
  CHECK(napalm.element == kGhost);
  REQUIRE(napalm.levels.size() == 8U);
  const SkillLevel& napalm_top = napalm.levels.back();
  CHECK(napalm_top.amount == 3);
  CHECK(napalm_top.damage == 48);
  CHECK(napalm_top.cooldown_ms == 1000U);
  CHECK(napalm_top.area_radius_units == CellTenths(22));
  CHECK(napalm_top.splash_pct == 60);
  CHECK(napalm_top.interval_ms == 150U);
  CHECK(napalm_top.reach_units == Cells(8));

  const SurvivorsSkillRule& wall = Skill(18);
  CHECK(wall.archetype == SkillArchetype::kWall);
  CHECK(wall.element == kFire);
  REQUIRE(wall.levels.size() == 8U);
  const SkillLevel& wall_first = wall.levels.front();
  CHECK(wall_first.amount == 1);
  CHECK(wall_first.damage == 12);
  CHECK(wall_first.zone_hits == 5);
  CHECK(wall_first.length_units == Cells(3));
  CHECK(wall_first.cooldown_ms == 4500U);
  CHECK(wall_first.reach_units == Cells(2));
  CHECK(wall_first.knockback_units == Cells(2));
  CHECK(wall_first.rehit_ms == 400U);
  CHECK(wall_first.life_ms == 4000U);
  CHECK(wall.levels.back().amount == 4);
  CHECK(wall.levels.back().zone_hits == 10);

  const SurvivorsSkillRule& diver = Skill(15);
  CHECK(diver.archetype == SkillArchetype::kLinear);
  CHECK(diver.element == kWater);
  REQUIRE(diver.levels.size() == 8U);
  const SkillLevel& diver_top = diver.levels.back();
  CHECK(diver_top.amount == 4);
  CHECK(diver_top.damage == 40);
  CHECK(diver_top.freeze_ms == 2500U);
  CHECK(diver_top.pierce == 3);
  CHECK(diver_top.speed_units_per_s == Cells(12));
  CHECK(diver_top.reach_units == Cells(9));
  CHECK(diver_top.hit_radius_units == CellTenths(4));

  // Les évolutions : skill de base, carte (ou second skill d'une union),
  // évolué, archétype.
  struct PinnedEvolution {
    std::uint16_t skill_id;
    std::uint16_t card_item_id;
    std::uint16_t evolved_skill_id;
    SkillArchetype archetype;
    std::uint16_t partner_skill_id = 0;
  };
  constexpr std::array<PinnedEvolution, 42> kPinnedEvolutions{{
      {19, 4019, 83, SkillArchetype::kClusterBlast},
      {10, 4009, 81, SkillArchetype::kOrbit},
      {18, 4072, 80, SkillArchetype::kRing},
      {15, 4197, 88, SkillArchetype::kNova},
      {14, 4266, 89, SkillArchetype::kStorm},
      {20, 4179, 84, SkillArchetype::kLinear},
      {21, 4147, 85, SkillArchetype::kStorm},
      {90, 4003, 91, SkillArchetype::kStorm},
      {13, 0, 400, SkillArchetype::kHoming, 11},
      {17, 4022, 2211, SkillArchetype::kClusterBlast},
      {86, 0, 2204, SkillArchetype::kStorm, 14},
      // Le Chevalier : Bash, Pierce, Spear Stab, Magnum Break, Endure, Two-Hand
      // Quicken, et l'union de Bowling Bash et de Brandish Spear.
      {5, 4072, 398, SkillArchetype::kTargetStrike},
      {56, 4019, 397, SkillArchetype::kLinear},
      {58, 4266, 399, SkillArchetype::kCone},
      {7, 4179, 355, SkillArchetype::kNova},
      {8, 4003, 357, SkillArchetype::kStance},
      {60, 4183, 359, SkillArchetype::kStance},
      {62, 0, 2006, SkillArchetype::kStorm, 57},
      // Le Voleur : Double Attack, Grimtooth, Venom Dust, Enchant Poison, Hiding,
      // Throw Stone, et l'union d'Envenom et de Sprinkle Sand.
      {48, 4019, 136, SkillArchetype::kTargetStrike},
      {137, 4179, 379, SkillArchetype::kLinear},
      {140, 4022, 141, SkillArchetype::kClusterBlast},
      {138, 4183, 378, SkillArchetype::kStance},
      {51, 4266, 135, SkillArchetype::kReflex},
      {152, 4016, 1004, SkillArchetype::kLinear},
      {52, 0, 406, SkillArchetype::kNova, 149},
      // L'Archer : Double Strafe, Arrow Shower, Blitz Beat, Improve Concentration,
      // Ankle Snare, Land Mine, et l'union de Charge Arrow et de Skid Trap.
      {46, 4094, 499, SkillArchetype::kLinear},
      {47, 4051, 382, SkillArchetype::kLinear},
      {129, 4115, 381, SkillArchetype::kTargetStrike},
      {45, 4021, 380, SkillArchetype::kStance},
      {117, 4010, 123, SkillArchetype::kRing},
      {116, 4121, 122, SkillArchetype::kRing},
      {148, 0, 1009, SkillArchetype::kLinear, 115},
      // Le Marchand : l'union d'Acid Terror et de Demonstration, Potion Pitcher,
      // Chemical Protection, Bio Cannibalize, Mammonite, Cart Revolution, puis
      // ses quatre homoncules, que la seule carte Sasquatch fait évoluer.
      {230, 0, 490, SkillArchetype::kTargetStrike, 229},
      {231, 4036, 478, SkillArchetype::kMend},
      {236, 4417, 479, SkillArchetype::kStance},
      {232, 4293, 491, SkillArchetype::kSummon},
      {42, 4196, 2477, SkillArchetype::kClusterBlast},
      {153, 4043, 2476, SkillArchetype::kOrbit},
      {8009, 4216, 8012, SkillArchetype::kSummon},
      {8013, 4216, 8016, SkillArchetype::kSummon},
      {8001, 4216, 8004, SkillArchetype::kSummon},
      {8005, 4216, 8008, SkillArchetype::kSummon},
  }};
  REQUIRE(Shipped().evolutions.size() == kPinnedEvolutions.size());
  for (std::size_t index = 0; index < kPinnedEvolutions.size(); ++index) {
    const PinnedEvolution& pinned = kPinnedEvolutions[index];
    const ro::sim::EvolutionRule& evolution = Shipped().evolutions[index];
    CHECK(evolution.skill_id == pinned.skill_id);
    CHECK(evolution.card_item_id == pinned.card_item_id);
    CHECK(evolution.evolved_skill_id == pinned.evolved_skill_id);
    CHECK(evolution.partner_skill_id == pinned.partner_skill_id);
    CHECK(Skill(pinned.evolved_skill_id).archetype == pinned.archetype);
  }
  const SkillLevel& meteor = Skill(83).levels.front();
  CHECK(meteor.amount == 5);
  CHECK(meteor.damage == 32);
  CHECK(meteor.area_radius_units == CellTenths(25));
  CHECK(meteor.splash_pct == 100);
  const SkillLevel& rasher = Skill(81).levels.front();
  CHECK(rasher.amount == 6);
  CHECK(rasher.damage == 22);
  CHECK(rasher.rest_ms == 0U);
  CHECK(rasher.knockback_units == Cells(2));
  const SkillLevel& pillar = Skill(80).levels.front();
  CHECK(pillar.amount == 8);
  CHECK(pillar.damage == 25);
  CHECK(pillar.area_radius_units == Cells(1));
  CHECK(pillar.rehit_ms == 1000U);
  const SkillLevel& nova = Skill(88).levels.front();
  CHECK(nova.damage == 30);
  CHECK(nova.area_radius_units == Cells(4));
  CHECK(nova.freeze_ms == 2500U);

  struct PinnedCard {
    std::uint16_t card_item_id;
    ro::sim::SurvivorsStat stat;
    std::array<std::int32_t, 5> levels;
    std::size_t level_count;
  };
  constexpr std::array<PinnedCard, 4> kPinnedCards{{
      {4022, ro::sim::SurvivorsStat::kRegenPerMinute, {18, 36, 54, 72, 90}, 5},
      {4016, ro::sim::SurvivorsStat::kMagnetPct, {40, 80, 120, 160, 200}, 5},
      {4072, ro::sim::SurvivorsStat::kArmor, {1, 2, 3, 4, 5}, 5},
      {4197, ro::sim::SurvivorsStat::kAmount, {1, 2, 0, 0, 0}, 2},
  }};
  for (const PinnedCard& pinned : kPinnedCards) {
    INFO("carte " << pinned.card_item_id);
    const ro::sim::CardRule* card = ro::sim::FindCard(Shipped(), pinned.card_item_id);
    REQUIRE(card != nullptr);
    CHECK(card->stat == pinned.stat);
    CHECK(std::ranges::equal(
        card->per_level, std::span<const std::int32_t>{pinned.levels}.first(pinned.level_count)));
  }
}

TEST_CASE("Catalogue survivors : les boss et leurs coups", "[sim][survivors]") {
  const SurvivorsRules& shipped = Shipped();
  struct PinnedBoss {
    std::uint32_t at_ms;
    std::uint16_t foe_id;
    std::uint32_t walk_ms;
    std::uint16_t escort_foe_id;
    std::uint8_t escort_count;
    std::size_t moves;
  };
  constexpr std::array<PinnedBoss, 5> kPinnedBosses{{
      {180000, 1090, 300, 0, 0, 1},
      {300000, 1093, 200, 1063, 8, 1},
      {420000, 1091, 100, 0, 0, 1},
      {540000, 1092, 180, 1013, 4, 1},
      {900000, 1039, 115, 0, 0, 6},
  }};
  REQUIRE(shipped.bosses.size() == kPinnedBosses.size());
  for (std::size_t index = 0; index < kPinnedBosses.size(); ++index) {
    const PinnedBoss& pinned = kPinnedBosses[index];
    const ro::sim::BossRule& boss = shipped.bosses[index];
    INFO("boss " << pinned.foe_id);
    CHECK(boss.at_ms == pinned.at_ms);
    CHECK(boss.foe_id == pinned.foe_id);
    CHECK(boss.walk_ms_per_cell == pinned.walk_ms);
    CHECK(boss.escort_foe_id == pinned.escort_foe_id);
    CHECK(boss.escort_count == pinned.escort_count);
    CHECK(boss.moves.size() == pinned.moves);
  }
  const ro::sim::BossMoveRule& summon = shipped.bosses[0].moves[0];
  CHECK(summon.kind == BossMoveKind::kSummon);
  CHECK(summon.foe_id == 1002);
  CHECK(summon.count == 6);
  CHECK(summon.period_ms == 6000U);
  const ro::sim::BossMoveRule& leap = shipped.bosses[1].moves[0];
  CHECK(leap.kind == BossMoveKind::kLeap);
  CHECK(leap.reach_units == Cells(4));
  CHECK(leap.telegraph_ms == 400U);
  CHECK(leap.period_ms == 5000U);
  const ro::sim::BossMoveRule& flee = shipped.bosses[2].moves[0];
  CHECK(flee.kind == BossMoveKind::kFlee);
  CHECK(flee.effect_ms == 1000U);
  const ro::sim::BossMoveRule& howl = shipped.bosses[3].moves[0];
  CHECK(howl.kind == BossMoveKind::kHowl);
  CHECK(howl.foe_id == 1013);
  CHECK(howl.boost_pct == 30);
  CHECK(howl.effect_ms == 4000U);
  CHECK(howl.period_ms == 10000U);

  // Baphomet : coup, phase de PV, période, compte.
  struct PinnedMove {
    BossMoveKind kind;
    std::uint8_t from_hp_pct;
    std::uint8_t to_hp_pct;
    std::uint32_t period_ms;
    std::uint8_t count;
  };
  constexpr std::array<PinnedMove, 6> kPinnedBaphomet{{
      {BossMoveKind::kCharge, 100, 60, 8000, 0},
      {BossMoveKind::kSummon, 100, 25, 12000, 4},
      {BossMoveKind::kQuake, 60, 25, 6000, 3},
      {BossMoveKind::kCharge, 25, 0, 8000, 0},
      {BossMoveKind::kSummon, 25, 0, 12000, 8},
      {BossMoveKind::kQuake, 25, 0, 4000, 3},
  }};
  const ro::sim::BossRule& baphomet = shipped.bosses.back();
  CHECK(baphomet.final);
  REQUIRE(baphomet.moves.size() == kPinnedBaphomet.size());
  for (std::size_t index = 0; index < kPinnedBaphomet.size(); ++index) {
    const PinnedMove& pinned = kPinnedBaphomet[index];
    const ro::sim::BossMoveRule& move = baphomet.moves[index];
    INFO("coup " << index);
    CHECK(move.kind == pinned.kind);
    CHECK(move.from_hp_pct == pinned.from_hp_pct);
    CHECK(move.to_hp_pct == pinned.to_hp_pct);
    CHECK(move.period_ms == pinned.period_ms);
    CHECK(move.count == pinned.count);
    if (move.kind == BossMoveKind::kCharge) {
      CHECK(move.telegraph_ms == 800U);
      CHECK(move.reach_units == Cells(6));
    }
    if (move.kind == BossMoveKind::kSummon) CHECK(move.foe_id == 1101);
    if (move.kind == BossMoveKind::kQuake) {
      CHECK(move.telegraph_ms == 1000U);
      CHECK(move.radius_units == CellTenths(25));
      CHECK(move.damage == 40);
    }
  }
}

TEST_CASE("Catalogue survivors : élites, coffre, plantes, objets et réglages", "[sim][survivors]") {
  const SurvivorsRules& shipped = Shipped();
  CHECK(shipped.elites.first_ms == 120000U);
  CHECK(shipped.elites.interval_ms == 75000U);
  CHECK(shipped.elites.last_ms == 870000U);
  CHECK(shipped.elites.hp_pct == 1000);
  CHECK(shipped.elites.touch_pct == 150);
  CHECK(shipped.elites.exp_pct == 500);
  CHECK(shipped.chests.item_id == 603);
  CHECK(shipped.chests.evolution_from_ms == 360000U);
  CHECK(shipped.chests.triple_per_10000 == 1200U);
  CHECK(shipped.chests.quintuple_per_10000 == 200U);

  struct PinnedPlant {
    std::uint16_t foe_id;
    PickupKind pickup;
    std::uint32_t weight;
    std::uint16_t item_id;
  };
  constexpr std::array<PinnedPlant, 7> kPinnedPlants{{
      {1078, PickupKind::kMeat, 30, 517},
      {1081, PickupKind::kGold, 10, 969},
      {1080, PickupKind::kHaste, 10, 12016},
      {1079, PickupKind::kManual, 30, 12208},
      {1083, PickupKind::kMagnetAll, 15, 12210},
      {1082, PickupKind::kHolyWater, 5, 523},
      {1084, PickupKind::kBerry, 10, 607},
  }};
  REQUIRE(shipped.plants.size() == kPinnedPlants.size());
  for (std::size_t index = 0; index < kPinnedPlants.size(); ++index) {
    const PinnedPlant& pinned = kPinnedPlants[index];
    INFO("plante " << pinned.foe_id);
    CHECK(shipped.plants[index].foe_id == pinned.foe_id);
    CHECK(shipped.plants[index].pickup == pinned.pickup);
    CHECK(shipped.plants[index].weight == pinned.weight);
    const ro::sim::PickupRule* pickup = ro::sim::FindPickup(shipped, pinned.pickup);
    REQUIRE(pickup != nullptr);
    CHECK(pickup->item_id == pinned.item_id);
  }
  CHECK(ro::sim::FindPickup(shipped, PickupKind::kMeat)->amount == 60);
  CHECK(ro::sim::FindPickup(shipped, PickupKind::kGold)->amount == 25);
  CHECK(ro::sim::FindPickup(shipped, PickupKind::kManual)->amount == 100);
  CHECK(ro::sim::FindPickup(shipped, PickupKind::kManual)->duration_ms == 60000U);
  CHECK(ro::sim::FindPickup(shipped, PickupKind::kHaste)->amount == 50);
  CHECK(ro::sim::FindPickup(shipped, PickupKind::kHaste)->duration_ms == 10000U);
  CHECK(ro::sim::FindPickup(shipped, PickupKind::kHolyWater)->reach_units == Cells(12));
  CHECK(ro::sim::FindPickup(shipped, PickupKind::kChest)->item_id == 603);
  // Seul le Black Mushroom attend les cartes : sans elles, son poids est nul.
  for (const ro::sim::PlantRule& plant : shipped.plants) {
    CHECK(plant.sprouted_by_cards == (plant.foe_id == 1084));
  }

  const ro::sim::SurvivorsTuning& tuning = shipped.tuning;
  CHECK(tuning.boss_hp_growth_pct_per_minute == 0);
  CHECK(tuning.boss_walk_scale_pct == 100);
  CHECK(tuning.knock_units_per_step == CellTenths(2));
  CHECK(tuning.plant_block_cells == 0);
  CHECK(tuning.plant_cells_per_plant == 0U);
  CHECK(tuning.pickup_radius_units == CellTenths(5));
  CHECK(tuning.final_overtime_ms == 300000U);
}
