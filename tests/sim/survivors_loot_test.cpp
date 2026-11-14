#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/pcg32.h"
#include "ro/sim/survivors_content.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_news.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"
#include "ro/sim/walk_map.h"

#include "support/survivors_content_fixture.h"
#include "support/survivors_fixture.h"

using ro::sim::CellOf;
using ro::sim::Cells;
using ro::sim::ChestGainKind;
using ro::sim::FieldPoint;
using ro::sim::FindFoe;
using ro::sim::OfferKind;
using ro::sim::OwnedCard;
using ro::sim::OwnedSkill;
using ro::sim::PickupKind;
using ro::sim::SurvivorsChestNews;
using ro::sim::SurvivorsField;
using ro::sim::SurvivorsFoe;
using ro::sim::SurvivorsGem;
using ro::sim::SurvivorsNews;
using ro::sim::SurvivorsPhase;
using ro::sim::SurvivorsPickup;
using ro::sim::SurvivorsPlant;
using ro::sim::SurvivorsRun;
using ro::sim::SurvivorsStart;
using ro::sim::SurvivorsStat;
using ro::sim::WalkMap;
using ro::test_support::ContentTables;
using ro::test_support::Heading;
using ro::test_support::kContentBaphometId;
using ro::test_support::kContentChestItemId;
using ro::test_support::kContentEvolvedId;
using ro::test_support::kTinyCardId;
using ro::test_support::kTinyHomingId;
using ro::test_support::kTinyLunaticId;
using ro::test_support::kTinyOrbitId;
using ro::test_support::kTinyStrikeId;
using ro::test_support::PillarArena;
using ro::test_support::PlaceFoe;
using ro::test_support::SetBonus;
using ro::test_support::StepStill;
using ro::test_support::TinyStart;

namespace {

constexpr std::int32_t kArenaSideCells = 64;
constexpr std::uint32_t kChestDraws = ro::sim::kChestDrawBound;
constexpr std::uint8_t kStrikeMaxLevel = 2;  // Fire Bolt des règles minuscules
constexpr std::uint8_t kCardMaxLevel = 2;
constexpr std::uint32_t kEvolutionStep = 30000 / ro::sim::kSurvivorsStepMs;
constexpr std::size_t kWolfIndex = 10;  // dans kContentFoes
constexpr std::uint16_t kRedPlantId = 1078;
constexpr std::uint16_t kMeatItemId = 517;
constexpr std::uint16_t kNoLevelUpLevel = 1000;

// Les règles de contenu sans vague, événement, boss ni élite. Les plantes y
// restent, pour que leurs objets aient une règle ; StartRun vide le champ que
// le départ en pose, et un essai y met les siennes.
struct LootContent : ContentTables {
  LootContent() {
    rules.waves = ro::test_support::kQuietWaves;
    rules.events = {};
    rules.bosses = {};
    rules.elites = {};
  }
};

[[nodiscard]] WalkMap OpenArena() {
  const auto side = static_cast<std::size_t>(kArenaSideCells);
  return WalkMap(kArenaSideCells, kArenaSideCells, std::vector<std::uint8_t>(side * side, 1U));
}

// Une partie sur une arène ouverte, sans plante, au héros sans skill.
[[nodiscard]] SurvivorsRun StartRun(const ro::sim::SurvivorsRules& rules,
                                    SurvivorsStart start = TinyStart(1)) {
  auto run = SurvivorsRun::Start(rules, OpenArena(), start);
  REQUIRE(run.has_value());
  run->field_for_tests().plants.clear();
  run->field_for_tests().hero.skills.clear();
  return std::move(*run);
}

void Drop(SurvivorsField& field, PickupKind kind, std::uint16_t item_id, FieldPoint at) {
  field.pickups.push_back(SurvivorsPickup{
      .serial = field.next_serial++, .kind = kind, .item_id = item_id, .position = at});
}

void DropChest(SurvivorsField& field) {
  Drop(field, PickupKind::kChest, kContentChestItemId, field.hero.position);
}

// Le coffre ouvert au pas suivant, ramassé sur place.
[[nodiscard]] SurvivorsChestNews OpenOne(SurvivorsRun& run) {
  DropChest(run.field_for_tests());
  StepStill(run);
  SurvivorsNews news = run.TakeNews();
  REQUIRE(news.chests.size() == 1U);
  return news.chests.front();
}

// Le tirage qu'un coffre ferait au départ de `start`.
[[nodiscard]] std::uint32_t FirstChestDraw(const ro::sim::SurvivorsRules& rules,
                                           SurvivorsStart start = TinyStart(1)) {
  SurvivorsRun run = StartRun(rules, start);
  ro::core::Pcg32 copy = run.field().loot_random;
  return copy.Below(kChestDraws);
}

[[nodiscard]] FieldPoint East(std::int32_t units) { return FieldPoint{units, 0}; }

}  // namespace

TEST_CASE("Butin survivors : un coffre donne 1, 3 ou 5 gains aux seuils exacts du tirage",
          "[sim][survivors]") {
  LootContent tables;
  const std::uint32_t draw = FirstChestDraw(tables.rules);
  const auto gains_with = [&](std::uint32_t quintuple, std::uint32_t triple,
                              SurvivorsStart start = TinyStart(1)) {
    tables.rules.chests.quintuple_per_10000 = quintuple;
    tables.rules.chests.triple_per_10000 = triple;
    SurvivorsRun run = StartRun(tables.rules, start);
    run.field_for_tests().hero.skills.push_back(OwnedSkill{.skill_id = kTinyStrikeId});
    return OpenOne(run).gain_count;
  };
  // Un seuil est strict : un tirage égal au seuil n'y est pas.
  CHECK(gains_with(draw, draw) == 1U);
  CHECK(gains_with(draw, draw + 1) == 3U);
  CHECK(gains_with(draw + 1, draw + 1) == 5U);

  SECTION("la chance majore les seuils de (100 + chance) %") {
    SurvivorsStart lucky = TinyStart(1);
    SetBonus(lucky, SurvivorsStat::kLuckPct, 100);
    REQUIRE(FirstChestDraw(tables.rules, lucky) == draw);
    // Le seuil doublé dépasse le tirage, le seuil seul non.
    const std::uint32_t halved = draw / 2 + 1;
    REQUIRE(halved <= draw);
    CHECK(gains_with(halved, halved, lucky) == 5U);
    CHECK(gains_with(halved, halved) == 1U);
  }
}

TEST_CASE("Butin survivors : les gains d'un coffre montent ce qui est possédé",
          "[sim][survivors]") {
  LootContent tables;
  tables.rules.chests.triple_per_10000 = 0;
  tables.rules.chests.quintuple_per_10000 = 0;

  SECTION("une montée tirée parmi le skill et la carte possédés") {
    SurvivorsRun run = StartRun(tables.rules);
    SurvivorsField& field = run.field_for_tests();
    field.hero.skills.push_back(OwnedSkill{.skill_id = kTinyStrikeId});
    field.hero.cards.push_back(OwnedCard{.card_item_id = kTinyCardId});
    ro::core::Pcg32 expected = field.loot_random;
    (void)expected.Below(kChestDraws);
    constexpr std::array<std::uint32_t, 2> kEqual{100, 100};
    const std::size_t picked = expected.PickWeighted(kEqual);
    const SurvivorsChestNews news = OpenOne(run);
    CHECK(run.field().loot_random == expected);
    REQUIRE(news.gain_count == 1U);
    if (picked == 0) {
      CHECK(news.gains[0] == ro::sim::SurvivorsChestGain{.kind = ChestGainKind::kSkillLevel,
                                                         .id = kTinyStrikeId,
                                                         .level = kStrikeMaxLevel});
      CHECK(run.field().hero.skills[0].level == kStrikeMaxLevel);
    } else {
      CHECK(news.gains[0] == ro::sim::SurvivorsChestGain{.kind = ChestGainKind::kCardLevel,
                                                         .id = kTinyCardId,
                                                         .level = kCardMaxLevel});
      CHECK(run.field().hero.cards[0].level == kCardMaxLevel);
    }
    CHECK(run.tally().chests_opened == 1U);
  }

  SECTION("sans candidat, des zenys, sans tirer") {
    SurvivorsRun run = StartRun(tables.rules);
    SurvivorsField& field = run.field_for_tests();
    field.hero.skills.push_back(OwnedSkill{.skill_id = kTinyStrikeId, .level = kStrikeMaxLevel});
    ro::core::Pcg32 expected = field.loot_random;
    (void)expected.Below(kChestDraws);
    const std::uint32_t zeny = field.hero.zeny;
    const SurvivorsChestNews news = OpenOne(run);
    CHECK(run.field().loot_random == expected);
    REQUIRE(news.gain_count == 1U);
    CHECK(news.gains[0].kind == ChestGainKind::kZeny);
    CHECK(run.field().hero.zeny == zeny + tables.rules.tuning.zeny_offer);
  }

  SECTION("un coffre ne donne jamais un skill neuf") {
    tables.rules.chests.triple_per_10000 = kChestDraws;
    SurvivorsRun run = StartRun(tables.rules);
    const SurvivorsChestNews news = OpenOne(run);
    REQUIRE(news.gain_count == 3U);
    CHECK(run.field().hero.skills.empty());
    CHECK(run.field().hero.cards.empty());
  }
}

TEST_CASE("Butin survivors : l'évolution passe en tête, après son heure, carte et niveau requis",
          "[sim][survivors]") {
  LootContent tables;
  tables.rules.chests.triple_per_10000 = 0;
  tables.rules.chests.quintuple_per_10000 = 0;
  // Fire Bolt au plus haut niveau, Hornet Card possédée, au pas `step`.
  const auto ready_at = [&](std::uint32_t step, std::uint8_t level, bool card) {
    SurvivorsRun run = StartRun(tables.rules);
    SurvivorsField& field = run.field_for_tests();
    field.step = step;
    field.hero.skills.push_back(OwnedSkill{.skill_id = kTinyHomingId});
    field.hero.skills.push_back(OwnedSkill{.skill_id = kTinyStrikeId, .level = level});
    if (card) field.hero.cards.push_back(OwnedCard{.card_item_id = kTinyCardId});
    return run;
  };

  SECTION("à l'heure : l'emplacement garde son index et prend l'évolué") {
    SurvivorsRun run = ready_at(kEvolutionStep, kStrikeMaxLevel, true);
    DropChest(run.field_for_tests());
    StepStill(run);
    const SurvivorsNews news = run.TakeNews();
    REQUIRE(news.chests.size() == 1U);
    REQUIRE(news.chests[0].gain_count == 1U);
    CHECK(news.chests[0].gains[0] == ro::sim::SurvivorsChestGain{.kind = ChestGainKind::kEvolution,
                                                                 .id = kContentEvolvedId,
                                                                 .level = 1});
    const SurvivorsField& field = run.field();
    REQUIRE(field.hero.skills.size() == 2U);
    CHECK(field.hero.skills[0].skill_id == kTinyHomingId);
    CHECK(field.hero.skills[1].skill_id == kContentEvolvedId);
    CHECK(field.hero.skills[1].level == 1U);
    CHECK(field.hero.skills[1].ready_step == kEvolutionStep);
    // Les skills ont déjà tiré au pas du coffre : sa phase part au suivant.
    CHECK(field.hero.skills[1].phase_step == kEvolutionStep + 1U);
    CHECK(field.hero.evolutions == 1U);
    CHECK(run.tally().evolutions == 1U);
    REQUIRE(news.evolutions.size() == 1U);
    CHECK(news.evolutions[0].slot == 1U);
    CHECK(news.evolutions[0].from_skill_id == kTinyStrikeId);
    CHECK(news.evolutions[0].to_skill_id == kContentEvolvedId);
  }

  SECTION("un pas trop tôt : une montée, pas d'évolution") {
    SurvivorsRun run = ready_at(kEvolutionStep - 1, kStrikeMaxLevel, true);
    const SurvivorsChestNews news = OpenOne(run);
    REQUIRE(news.gain_count == 1U);
    CHECK(news.gains[0].kind != ChestGainKind::kEvolution);
    CHECK(run.field().hero.skills[1].skill_id == kTinyStrikeId);
    CHECK(run.tally().evolutions == 0U);
  }

  SECTION("sans la carte, ou sous le plus haut niveau : rien") {
    SurvivorsRun no_card = ready_at(kEvolutionStep, kStrikeMaxLevel, false);
    CHECK(OpenOne(no_card).gains[0].kind != ChestGainKind::kEvolution);
    SurvivorsRun low = ready_at(kEvolutionStep, 1, true);
    CHECK(OpenOne(low).gains[0].kind != ChestGainKind::kEvolution);
    CHECK(low.field().hero.skills[1].skill_id == kTinyStrikeId);
  }
}

TEST_CASE("Butin survivors : un skill dont l'évolué est possédé n'est plus offert",
          "[sim][survivors]") {
  LootContent tables;
  tables.rules.caps.offers = ro::sim::kMostOffers;
  const auto offered_ids = [&] {
    SurvivorsRun run = StartRun(tables.rules);
    SurvivorsField& field = run.field_for_tests();
    field.hero.skills.push_back(OwnedSkill{.skill_id = kContentEvolvedId});
    field.pending_level_ups = 1;
    StepStill(run);
    REQUIRE(run.field().phase == SurvivorsPhase::kChoosing);
    std::vector<std::uint16_t> ids;
    for (const ro::sim::SurvivorsOffer& offer : run.field().offers) ids.push_back(offer.id);
    return ids;
  };
  const auto has = [](const std::vector<std::uint16_t>& ids, std::uint16_t id) {
    return std::find(ids.begin(), ids.end(), id) != ids.end();
  };
  // Quatre offres au plus : Soul Strike, Sight et la carte restent.
  const std::vector<std::uint16_t> with_evolution = offered_ids();
  CHECK_FALSE(has(with_evolution, kTinyStrikeId));
  CHECK(has(with_evolution, kTinyHomingId));
  CHECK(has(with_evolution, kTinyOrbitId));
  CHECK(has(with_evolution, kTinyCardId));
  // Témoin : sans règle d'évolution, Fire Bolt revient parmi les offres.
  tables.rules.evolutions = {};
  CHECK(has(offered_ids(), kTinyStrikeId));
}

TEST_CASE("Butin survivors : les plantes, une au plus par bloc, jamais sur un mur",
          "[sim][survivors]") {
  constexpr std::int32_t kSide = 160;
  constexpr std::int32_t kBlock = 12;
  constexpr std::uint32_t kCellsPerPlant = 150;
  const ro::sim::SurvivorsRules& rules = ro::test_support::ContentRules();
  REQUIRE(rules.tuning.plant_block_cells == kBlock);
  REQUIRE(rules.tuning.plant_cells_per_plant == kCellsPerPlant);
  const auto plants_of = [&](std::uint64_t seed) {
    auto run = SurvivorsRun::Start(rules, PillarArena(kSide), TinyStart(seed));
    REQUIRE(run.has_value());
    return run->field().plants;
  };
  const std::vector<SurvivorsPlant> plants = plants_of(1);
  const WalkMap arena = PillarArena(kSide);

  // La densité : chaque bloc porte une plante avec la probabilité (cases
  // praticables) / 150 ; le compte reste à quatre écarts types de l'attente.
  double expected = 0;
  double variance = 0;
  std::vector<int> per_block;
  const std::int32_t blocks_per_side = (kSide + kBlock - 1) / kBlock;
  per_block.assign(static_cast<std::size_t>(blocks_per_side * blocks_per_side), 0);
  for (std::int32_t block_y = 0; block_y < kSide; block_y += kBlock) {
    for (std::int32_t block_x = 0; block_x < kSide; block_x += kBlock) {
      int open = 0;
      for (std::int32_t y = block_y; y < std::min(block_y + kBlock, kSide); ++y) {
        for (std::int32_t x = block_x; x < std::min(block_x + kBlock, kSide); ++x) {
          if (arena.Passable(x, y)) ++open;
        }
      }
      const double chance = std::min(1.0, open / static_cast<double>(kCellsPerPlant));
      expected += chance;
      variance += chance * (1 - chance);
    }
  }
  const double count = static_cast<double>(plants.size());
  CHECK(std::abs(count - expected) <= 4 * std::sqrt(variance) + 1);
  CHECK(plants.size() > 100U);

  for (const SurvivorsPlant& plant : plants) {
    const std::int32_t cell_x = CellOf(plant.position.x);
    const std::int32_t cell_y = CellOf(plant.position.y);
    CHECK(arena.Passable(cell_x, cell_y));
    CHECK(plant.position == ro::sim::CellCenter(cell_x, cell_y));
    const std::size_t block =
        static_cast<std::size_t>(cell_y / kBlock * blocks_per_side + cell_x / kBlock);
    ++per_block[block];
    CHECK(std::any_of(rules.plants.begin(), rules.plants.end(),
                      [&](const ro::sim::PlantRule& rule) { return rule.foe_id == plant.foe_id; }));
  }
  CHECK(*std::max_element(per_block.begin(), per_block.end()) <= 1);

  // Même graine, même champ ; une autre graine, un autre.
  const auto same = [](const std::vector<SurvivorsPlant>& a, const std::vector<SurvivorsPlant>& b) {
    return std::equal(a.begin(), a.end(), b.begin(), b.end(),
                      [](const SurvivorsPlant& left, const SurvivorsPlant& right) {
                        return left.foe_id == right.foe_id && left.position == right.position;
                      });
  };
  CHECK(same(plants, plants_of(1)));
  CHECK_FALSE(same(plants, plants_of(2)));
}

TEST_CASE("Butin survivors : ce qui casse une plante, et ce qui ne la casse pas",
          "[sim][survivors]") {
  LootContent tables;
  SurvivorsRun run = StartRun(tables.rules);
  SurvivorsField& field = run.field_for_tests();
  const auto plant_at = [&](FieldPoint at) {
    field.plants.push_back(
        SurvivorsPlant{.serial = field.next_serial++, .foe_id = kRedPlantId, .position = at});
  };

  SECTION("le contact du héros : l'objet tombe, et se ramasse au même pas") {
    plant_at(field.hero.position);
    StepStill(run);
    CHECK(run.field().plants.empty());
    CHECK(run.field().pickups.empty());
    const SurvivorsNews news = run.TakeNews();
    REQUIRE(news.pickups_taken.size() == 1U);
    CHECK(news.pickups_taken[0].kind == PickupKind::kMeat);
    CHECK(news.pickups_taken[0].item_id == kMeatItemId);
  }

  SECTION("un éclat la casse, et son objet reste sur sa case") {
    const FieldPoint at = field.hero.position + East(Cells(6));
    plant_at(at);
    field.blasts.push_back(ro::sim::SurvivorsBlast{
        .center = at, .land_step = field.step, .damage = 10, .radius_units = Cells(1)});
    StepStill(run);
    CHECK(run.field().plants.empty());
    REQUIRE(run.field().pickups.size() == 1U);
    CHECK(run.field().pickups[0].kind == PickupKind::kMeat);
    CHECK(run.field().pickups[0].position == at);
  }

  SECTION("une frappe ne la casse pas") {
    const FieldPoint at = field.hero.position + East(Cells(6));
    plant_at(at);
    const std::uint32_t serial = PlaceFoe(field, kTinyLunaticId, at);
    field.foes.back().units_per_step = 0;
    field.strikes.push_back(ro::sim::SurvivorsStrike{
        .foe_serial = serial, .land_step = field.step, .base_damage = 100});
    StepStill(run);
    CHECK(run.tally().kills == 1U);
    CHECK(run.field().plants.size() == 1U);
    CHECK(run.field().pickups.empty());
  }
}

TEST_CASE("Butin survivors : l'effet de chaque objet", "[sim][survivors]") {
  LootContent tables;
  SurvivorsRun run = StartRun(tables.rules);
  SurvivorsField& field = run.field_for_tests();
  const auto drop_here = [&](PickupKind kind) {
    const ro::sim::PickupRule* rule = ro::sim::FindPickup(tables.rules, kind);
    REQUIRE(rule != nullptr);
    Drop(field, kind, rule->item_id, field.hero.position);
  };

  SECTION("Meat soigne, Gold enrichit") {
    field.hero.hp = 50;
    drop_here(PickupKind::kMeat);
    drop_here(PickupKind::kGold);
    StepStill(run);
    CHECK(run.field().hero.hp == 80);
    CHECK(run.field().hero.zeny == 10U);
    CHECK(run.TakeNews().pickups_taken.size() == 2U);
  }

  SECTION("Battle Manual double l'XP, puis s'éteint") {
    // Un niveau assez haut pour qu'aucune gemme n'ouvre de choix.
    field.hero.level = kNoLevelUpLevel;
    const auto gain_of_gem = [&] {
      const std::uint32_t before = run.tally().exp_total;
      run.field_for_tests().gems.push_back(
          SurvivorsGem{.position = run.field().hero.position,
                       .previous_position = run.field().hero.position,
                       .exp = 4});
      StepStill(run);
      return run.tally().exp_total - before;
    };
    CHECK(gain_of_gem() == 4U);
    drop_here(PickupKind::kManual);
    StepStill(run);
    CHECK(gain_of_gem() == 8U);
    StepStill(run, 30000 / ro::sim::kSurvivorsStepMs);
    CHECK(gain_of_gem() == 4U);
  }

  SECTION("Speed Potion majore la marche de 50 %, puis s'éteint") {
    const auto stride = [&] {
      run.Step(Heading(0));
      return ro::sim::IntegerSqrt(static_cast<std::uint64_t>(
          ro::sim::DistanceSquared(run.field().hero.position, run.field().hero.previous_position)));
    };
    const std::uint32_t normal = stride();
    CHECK(normal == static_cast<std::uint32_t>(ro::sim::UnitsPerStep(150)));
    drop_here(PickupKind::kHaste);
    StepStill(run);
    CHECK(stride() == normal * 150 / 100);
    StepStill(run, 10000 / ro::sim::kSurvivorsStepMs);
    CHECK(stride() == normal);
  }

  SECTION("Holy Water tue les communs à moins de douze cases, jamais un boss") {
    (void)PlaceFoe(field, kTinyLunaticId, field.hero.position + East(Cells(5)));
    const std::uint32_t far =
        PlaceFoe(field, kTinyLunaticId, field.hero.position + East(Cells(13)));
    const std::uint32_t boss =
        PlaceFoe(field, kContentBaphometId, field.hero.position + East(-Cells(5)));
    for (SurvivorsFoe& foe : field.foes) foe.units_per_step = 0;
    field.hero.hp = field.hero.max_hp;
    drop_here(PickupKind::kHolyWater);
    StepStill(run);
    CHECK(run.tally().kills == 1U);
    REQUIRE(run.field().foes.size() == 2U);
    CHECK(run.field().foes[0].serial == far);
    CHECK(run.field().foes[1].serial == boss);
    // Le tué laisse sa gemme.
    CHECK(run.field().gems.size() == 1U);
  }

  SECTION("Bubble Gum saisit toutes les gemmes") {
    for (std::int32_t cells = 8; cells <= 20; cells += 4) {
      const FieldPoint at = field.hero.position + East(Cells(cells));
      field.gems.push_back(SurvivorsGem{.position = at, .previous_position = at, .exp = 1});
    }
    drop_here(PickupKind::kMagnetAll);
    StepStill(run);
    REQUIRE(run.field().gems.size() == 4U);
    for (const SurvivorsGem& gem : run.field().gems) CHECK(gem.pull_units > 0);
  }
}

TEST_CASE("Butin survivors : une élite ou un mini-boss lâche un coffre, un commun non",
          "[sim][survivors]") {
  LootContent tables;
  tables.foes[kWolfIndex].rank = ro::sim::FoeRank::kMiniBoss;
  const std::uint16_t wolf_id = tables.foes[kWolfIndex].foe_id;
  const auto chests_after_kill = [&](std::uint16_t foe_id, bool elite) {
    SurvivorsRun run = StartRun(tables.rules);
    SurvivorsField& field = run.field_for_tests();
    const FieldPoint at = field.hero.position + East(Cells(6));
    (void)PlaceFoe(field, foe_id, at);
    field.foes.back().elite = elite;
    field.foes.back().hp = 0;
    field.foes.back().units_per_step = 0;
    StepStill(run);
    std::size_t chests = 0;
    for (const SurvivorsPickup& pickup : run.field().pickups) {
      if (pickup.kind == PickupKind::kChest && pickup.item_id == kContentChestItemId &&
          pickup.position == at) {
        ++chests;
      }
    }
    return chests;
  };
  CHECK(chests_after_kill(kTinyLunaticId, true) == 1U);
  CHECK(chests_after_kill(wolf_id, false) == 1U);
  CHECK(chests_after_kill(kTinyLunaticId, false) == 0U);
  CHECK(FindFoe(tables.rules, kContentBaphometId)->rank == ro::sim::FoeRank::kBoss);
  CHECK(chests_after_kill(kContentBaphometId, false) == 0U);
  // Un objet de coffre nul : aucun coffre.
  tables.rules.chests.item_id = 0;
  tables.rules.elites = {};
  CHECK(chests_after_kill(kTinyLunaticId, true) == 0U);
}

TEST_CASE("Butin survivors : le coffre d'une élite tombe même au plafond des objets",
          "[sim][survivors]") {
  LootContent tables;
  SurvivorsRun run = StartRun(tables.rules);
  SurvivorsField& field = run.field_for_tests();
  const std::uint16_t most_pickups = tables.rules.caps.most_pickups;
  const FieldPoint at = field.hero.position + East(Cells(6));
  while (field.pickups.size() < most_pickups) Drop(field, PickupKind::kMeat, kMeatItemId, at);
  (void)PlaceFoe(field, kTinyLunaticId, at);
  field.foes.back().elite = true;
  field.foes.back().hp = 0;
  field.foes.back().units_per_step = 0;
  StepStill(run);
  REQUIRE(run.field().pickups.size() == std::size_t{most_pickups} + 1U);
  CHECK(run.field().pickups.back().kind == PickupKind::kChest);
  CHECK(run.field().pickups.back().position == at);
}

// ── Les cartes ─────────────────────────────────────────────────────────────

namespace {

constexpr std::size_t kLunaticIndex = 0;  // dans kContentFoes
// Une cote si grande qu'aucun tirage de ces essais ne la passe : seule la
// garantie d'un mini-boss fait tomber une carte.
constexpr std::uint32_t kNeverOdds = 0xFFFFFFFFU;

// Un monstre `foe_id` tué sur place, à six cases à l'est du héros (ou sur lui).
FieldPoint KillAt(SurvivorsRun& run, std::uint16_t foe_id, FieldPoint offset) {
  SurvivorsField& field = run.field_for_tests();
  const FieldPoint at = field.hero.position + offset;
  (void)PlaceFoe(field, foe_id, at);
  field.foes.back().hp = 0;
  field.foes.back().units_per_step = 0;
  StepStill(run);
  return at;
}

[[nodiscard]] std::size_t CardsOnGround(const SurvivorsField& field) {
  return static_cast<std::size_t>(std::count_if(
      field.pickups.begin(), field.pickups.end(),
      [](const SurvivorsPickup& pickup) { return pickup.kind == PickupKind::kCard; }));
}

}  // namespace

TEST_CASE("Butin survivors : une carte tombe, se ramasse au contact, sans autre effet",
          "[sim][survivors]") {
  LootContent tables;
  tables.foes[kLunaticIndex].card_item_id = kTinyCardId;
  tables.rules.card_drops.common_odds = 1;  // chaque tué lâche sa carte
  SurvivorsRun run = StartRun(tables.rules);
  const std::uint32_t serial_before = run.field().next_serial;
  const FieldPoint at = KillAt(run, kTinyLunaticId, East(Cells(6)));
  REQUIRE(run.field().pickups.size() == 1U);
  const SurvivorsPickup card = run.field().pickups.front();
  CHECK(card.kind == PickupKind::kCard);
  CHECK(card.item_id == kTinyCardId);
  CHECK(card.position == at);
  // Un numéro de carte, à part : le monstre posé a pris le seul numéro de
  // naissance.
  CHECK(card.serial == ro::sim::kFirstCardSerial);
  CHECK(run.field().next_serial == serial_before + 1U);
  CHECK(run.field().next_card_serial == ro::sim::kFirstCardSerial + 1U);
  // La chute a sa nouvelle, où l'arène fait sonner la carte.
  const SurvivorsNews fallen = run.TakeNews();
  REQUIRE(fallen.cards_dropped.size() == 1U);
  CHECK(fallen.cards_dropped[0].card_item_id == kTinyCardId);
  CHECK(fallen.cards_dropped[0].at == at);
  CHECK(fallen.cards_found.empty());

  // Au contact : au récapitulatif et dans une nouvelle de carte, ni dans les
  // cartes du héros ni dans les objets ramassés.
  run.field_for_tests().hero.position = at;
  StepStill(run);
  const SurvivorsNews news = run.TakeNews();
  CHECK(run.field().pickups.empty());
  CHECK(run.field().recap.cards_found == std::vector<std::uint16_t>{kTinyCardId});
  REQUIRE(news.cards_found.size() == 1U);
  CHECK(news.cards_found[0].card_item_id == kTinyCardId);
  CHECK(news.cards_found[0].at == at);
  CHECK(news.pickups_taken.empty());
  CHECK(run.field().hero.cards.empty());
}

TEST_CASE("Butin survivors : la cote d'un commun se tire sur le flux des cartes",
          "[sim][survivors]") {
  constexpr std::uint32_t kOdds = 3;
  constexpr int kKills = 60;
  LootContent tables;
  tables.foes[kLunaticIndex].card_item_id = kTinyCardId;
  tables.rules.card_drops.common_odds = kOdds;
  SurvivorsRun run = StartRun(tables.rules);
  // Le flux des cartes de la graine, rejoué à part : un Chance(1, cote) par
  // tué qui porte une carte, et rien d'autre.
  ro::core::Pcg32 expected = ro::core::Pcg32(1, ro::sim::kCardStream);
  CHECK(run.field().card_random == expected);
  std::size_t wanted = 0;
  for (int kill = 0; kill < kKills; ++kill) {
    if (expected.Chance(1, kOdds)) ++wanted;
    (void)KillAt(run, kTinyLunaticId, East(Cells(6)));
  }
  CHECK(CardsOnGround(run.field()) == wanted);
  CHECK(wanted > 0U);
  CHECK(wanted < static_cast<std::size_t>(kKills));
  // Un monstre sans carte ne tire rien.
  (void)KillAt(run, ro::test_support::kTinyPoringId, East(Cells(6)));
  CHECK(run.field().card_random == expected);
}

TEST_CASE("Butin survivors : le premier mini-boss d'une espèce lâche sa carte, pas le second",
          "[sim][survivors]") {
  LootContent tables;
  tables.foes[kWolfIndex].rank = ro::sim::FoeRank::kMiniBoss;
  tables.foes[kWolfIndex].card_item_id = kTinyCardId;
  tables.rules.card_drops.common_odds = kNeverOdds;
  const std::uint16_t wolf_id = tables.foes[kWolfIndex].foe_id;
  SurvivorsRun run = StartRun(tables.rules);
  ro::core::Pcg32 expected = run.field().card_random;
  (void)KillAt(run, wolf_id, East(Cells(6)));
  CHECK(CardsOnGround(run.field()) == 1U);
  // La garantie ne tire rien ; le second tire à la cote, et ne la passe pas.
  CHECK(run.field().card_random == expected);
  (void)KillAt(run, wolf_id, East(Cells(6)));
  CHECK(CardsOnGround(run.field()) == 1U);
  CHECK(run.field().card_random != expected);
  CHECK(run.field().card_guarantees_spent == std::vector<std::uint16_t>{wolf_id});
}

TEST_CASE("Butin survivors : une carte ne décale ni le coffre ni le plafond des objets",
          "[sim][survivors]") {
  // Une élite porteuse tuée sur le héros : son coffre s'ouvre au même pas, avec
  // ou sans la carte qui tombe à côté.
  const auto chest_of_elite = [](bool carries_card) {
    LootContent tables;
    if (carries_card) tables.foes[kLunaticIndex].card_item_id = kTinyCardId;
    tables.rules.card_drops.common_odds = 1;
    SurvivorsRun run = StartRun(tables.rules);
    SurvivorsField& field = run.field_for_tests();
    field.hero.skills.push_back(OwnedSkill{.skill_id = kTinyStrikeId});
    (void)PlaceFoe(field, kTinyLunaticId, field.hero.position);
    field.foes.back().elite = true;
    field.foes.back().hp = 0;
    field.foes.back().units_per_step = 0;
    StepStill(run);
    const SurvivorsNews news = run.TakeNews();
    REQUIRE(news.chests.size() == 1U);
    CHECK(news.cards_found.size() == (carries_card ? 1U : 0U));
    return std::pair{news.chests.front().gains, run.field().loot_random};
  };
  CHECK(chest_of_elite(true) == chest_of_elite(false));

  // Une place sous le plafond des objets, deux cartes au sol : l'objet d'une
  // plante tombe encore, la carte ne lui a pas pris sa place.
  LootContent tables;
  tables.foes[kLunaticIndex].card_item_id = kTinyCardId;
  tables.rules.card_drops.common_odds = 1;
  SurvivorsRun run = StartRun(tables.rules);
  SurvivorsField& field = run.field_for_tests();
  const std::uint16_t most_pickups = tables.rules.caps.most_pickups;
  const FieldPoint far = field.hero.position + East(Cells(20));
  while (field.pickups.size() + 1U < most_pickups) Drop(field, PickupKind::kMeat, kMeatItemId, far);
  (void)KillAt(run, kTinyLunaticId, East(Cells(6)));
  (void)KillAt(run, kTinyLunaticId, East(Cells(7)));
  CHECK(CardsOnGround(run.field()) == 2U);
  CHECK(run.field().pickups.size() == std::size_t{most_pickups} + 1U);
  (void)run.TakeNews();
  field.plants.push_back(SurvivorsPlant{
      .serial = field.next_serial++, .foe_id = kRedPlantId, .position = field.hero.position});
  StepStill(run);
  const SurvivorsNews news = run.TakeNews();
  REQUIRE(news.pickups_taken.size() == 1U);
  CHECK(news.pickups_taken[0].kind == PickupKind::kMeat);
}

TEST_CASE("Butin survivors : une orbite montée par un coffre part sur kOrbitOn",
          "[sim][survivors]") {
  LootContent tables;
  tables.rules.chests.triple_per_10000 = 0;
  tables.rules.chests.quintuple_per_10000 = 0;
  SurvivorsRun run = StartRun(tables.rules);
  SurvivorsField& field = run.field_for_tests();
  field.hero.skills.clear();
  field.hero.skills.push_back(OwnedSkill{.skill_id = ro::test_support::kTinyOrbitId, .level = 1});

  const SurvivorsChestNews chest = OpenOne(run);
  REQUIRE(chest.gain_count == 1U);
  REQUIRE(chest.gains[0].id == ro::test_support::kTinyOrbitId);

  StepStill(run);
  const SurvivorsNews news = run.TakeNews();
  CHECK(std::count_if(news.casts.begin(), news.casts.end(), [](const ro::sim::SurvivorsCast& cast) {
          return cast.skill_id == ro::test_support::kTinyOrbitId &&
                 cast.kind == ro::sim::CastKind::kOrbitOn;
        }) == 1);
}
