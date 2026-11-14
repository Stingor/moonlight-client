#include "ro/sim/survivors_run.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/error.h"
#include "ro/core/pcg32.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_journal.h"
#include "ro/sim/survivors_news.h"
#include "ro/sim/survivors_play.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_state.h"
#include "ro/sim/walk_map.h"

#include "support/survivors_fixture.h"

using ro::sim::CellCenter;
using ro::sim::CellOf;
using ro::sim::SurvivorsPhase;
using ro::sim::SurvivorsRun;
using ro::sim::SurvivorsStart;
using ro::sim::WalkMap;
using ro::test_support::Heading;
using ro::test_support::PillarArena;
using ro::test_support::TinyRules;
using ro::test_support::TinyStart;

TEST_CASE("Partie survivors : le départ", "[sim][survivors]") {
  auto run = SurvivorsRun::Start(TinyRules(), PillarArena(64), TinyStart(42));
  REQUIRE(run.has_value());
  const ro::sim::SurvivorsField& field = run->field();
  CHECK(field.step == 0U);
  CHECK(field.phase == SurvivorsPhase::kRunning);
  CHECK(field.hero.position == CellCenter(32, 32));
  CHECK(field.hero.previous_position == field.hero.position);
  CHECK(field.ground.Passable(CellOf(field.hero.position.x), CellOf(field.hero.position.y)));
  REQUIRE(field.hero.skills.size() == 1);
  CHECK(field.hero.skills[0].skill_id == ro::test_support::kTinyStrikeId);
  CHECK(field.hero.skills[0].level == 1);
  CHECK(field.hero.skills[0].ready_step == 0U);
  CHECK(field.hero.hp == 100);
  CHECK(field.hero.max_hp == 100);
  CHECK(field.tally.hp_left == 100);
  // Les quatre flux sont dérivés de la graine, un par usage.
  CHECK(field.spawn_random == ro::core::Pcg32(42, ro::sim::kSpawnStream));
  CHECK(field.offer_random == ro::core::Pcg32(42, ro::sim::kOfferStream));
  CHECK(field.combat_random == ro::core::Pcg32(42, ro::sim::kCombatStream));
  CHECK(field.loot_random == ro::core::Pcg32(42, ro::sim::kLootStream));
  CHECK(run->elapsed_ms() == 0U);
}

TEST_CASE("Partie survivors : Start refuse ce qui ne se joue pas", "[sim][survivors]") {
  const auto refused = [](const SurvivorsStart& start, WalkMap arena) {
    const auto run = SurvivorsRun::Start(TinyRules(), std::move(arena), start);
    return !run.has_value() && run.error().code == ro::core::ErrorCode::kInvalidArgument;
  };
  SurvivorsStart start = TinyStart(1);
  start.class_id = 77;
  CHECK(refused(start, PillarArena(32)));
  start = TinyStart(1);
  start.arena_id = 77;
  CHECK(refused(start, PillarArena(32)));
  start = TinyStart(1);
  start.first_skill_id = 77;
  CHECK(refused(start, PillarArena(32)));
  // Un bonus de loadout au-delà de ±kMostStatBonus ; à la borne, il se joue.
  for (const std::int32_t bonus : {ro::sim::kMostStatBonus + 1, -ro::sim::kMostStatBonus - 1}) {
    start = TinyStart(1);
    ro::test_support::SetBonus(start, ro::sim::SurvivorsStat::kWalkPct, bonus);
    CHECK(refused(start, PillarArena(32)));
  }
  start = TinyStart(1);
  ro::test_support::SetBonus(start, ro::sim::SurvivorsStat::kWalkPct, ro::sim::kMostStatBonus);
  CHECK(SurvivorsRun::Start(TinyRules(), PillarArena(32), start).has_value());
  CHECK(refused(TinyStart(1), WalkMap()));
  CHECK(refused(TinyStart(1), PillarArena(ro::sim::kMostArenaSideCells + 1)));
  // Rien de praticable à portée du départ.
  CHECK(refused(TinyStart(1), WalkMap(40, 40, std::vector<std::uint8_t>(1600, 0))));

  // Un skill connu mais hors des réserves de toutes les classes.
  ro::sim::SurvivorsRules narrow = TinyRules();
  constexpr std::array<std::uint16_t, 1> kOnlyStrike{ro::test_support::kTinyStrikeId};
  std::array<ro::sim::ClassRule, 1> classes{TinyRules().classes[0]};
  classes[0].starter_skills = kOnlyStrike;
  classes[0].skill_pool = kOnlyStrike;
  narrow.classes = classes;
  start = TinyStart(1);
  start.first_skill_id = ro::test_support::kTinyOrbitId;
  CHECK_FALSE(SurvivorsRun::Start(narrow, PillarArena(32), start).has_value());
}

TEST_CASE("Partie survivors : un départ bloqué se recale", "[sim][survivors]") {
  // Le centre de 13 × 13 est (6, 6) : un pilier de PillarArena.
  auto run = SurvivorsRun::Start(TinyRules(), PillarArena(13), TinyStart(3));
  REQUIRE(run.has_value());
  const ro::sim::FieldPoint at = run->field().hero.position;
  CHECK(run->field().ground.Passable(CellOf(at.x), CellOf(at.y)));
  CHECK_FALSE(at == CellCenter(6, 6));
}

TEST_CASE("Partie survivors : pas, abandon et état d'essai", "[sim][survivors]") {
  auto run = SurvivorsRun::Start(TinyRules(), PillarArena(64), TinyStart(5));
  REQUIRE(run.has_value());
  for (int step = 0; step < 10; ++step) run->Step({});
  CHECK(run->field().step == 10U);
  CHECK(run->tally().survived_steps == 10U);
  CHECK(run->elapsed_ms() == 200U);

  run->field_for_tests().hero.hp = 7;
  run->Step({});
  CHECK(run->tally().hp_left == 7);

  CHECK_FALSE(run->Answer({}));
  run->Abandon();
  CHECK(run->field().phase == SurvivorsPhase::kOver);
  CHECK(run->tally().outcome == ro::sim::SurvivorsOutcome::kAbandoned);
  run->Step({});
  CHECK(run->field().step == 11U);
  (void)run->TakeNews();
}

namespace {

using ro::sim::FieldPoint;
using ro::sim::JournalEntry;
using ro::sim::JournalKind;
using ro::sim::SurvivorsField;
using ro::sim::SurvivorsInput;
using ro::sim::SurvivorsOutcome;
using ro::sim::SurvivorsPlay;
using ro::test_support::kTinyLunaticId;
using ro::test_support::PlaceFoe;
using ro::test_support::QuietRules;

// Une partie silencieuse au centre d'une arène de 64 cases.
[[nodiscard]] SurvivorsRun QuietRun(std::uint64_t seed) {
  auto run = SurvivorsRun::Start(QuietRules(), PillarArena(64), TinyStart(seed));
  REQUIRE(run.has_value());
  return std::move(*run);
}

}  // namespace

TEST_CASE("Partie survivors : hors kRunning, un pas ne bouge rien", "[sim][survivors]") {
  SurvivorsRun run = QuietRun(7);
  SurvivorsField& field = run.field_for_tests();
  field.hero.skills.clear();
  const FieldPoint hero = field.hero.position;
  (void)PlaceFoe(field, kTinyLunaticId, hero + FieldPoint{ro::sim::Cells(5), 0});
  field.phase = SurvivorsPhase::kChoosing;
  const FieldPoint foe = field.foes[0].position;

  run.Step(Heading(0));
  CHECK(run.field().step == 0U);
  CHECK(run.field().hero.position == hero);
  CHECK(run.field().foes[0].position == foe);
  CHECK(run.tally().survived_steps == 0U);
}

TEST_CASE("Partie survivors : le contact et son intervalle", "[sim][survivors]") {
  // Sans skill : aucune frappe ne vient tuer le monstre posé sur le héros.
  SurvivorsRun run = QuietRun(8);
  SurvivorsField& field = run.field_for_tests();
  field.hero.skills.clear();
  const FieldPoint hero = field.hero.position;
  // Sur le héros, mais à PV nuls : il ne frappe pas.
  (void)PlaceFoe(field, kTinyLunaticId, hero);
  field.foes[0].hp = 0;
  run.Step({});
  CHECK(run.field().hero.hp == run.field().hero.max_hp);
  CHECK(run.TakeNews().hero_hurt == 0);

  // Le même monstre vivant frappe : 3 PV, puis rien avant son intervalle.
  SurvivorsRun alive = QuietRun(8);
  alive.field_for_tests().hero.skills.clear();
  (void)PlaceFoe(alive.field_for_tests(), kTinyLunaticId, hero);
  alive.Step({});
  CHECK(alive.field().hero.hp == alive.field().hero.max_hp - 3);
  CHECK(alive.TakeNews().hero_hurt == 3);
  // 1 456 ms : 73 pas d'attente, comptés du pas du contact.
  for (std::uint32_t step = 1; step < ro::sim::StepsOf(1456); ++step) alive.Step({});
  CHECK(alive.field().hero.hp == alive.field().hero.max_hp - 3);
  alive.Step({});
  CHECK(alive.field().hero.hp == alive.field().hero.max_hp - 6);
}

TEST_CASE("Partie survivors : un monstre tué ce pas ne frappe pas", "[sim][survivors]") {
  SurvivorsRun run = QuietRun(18);
  SurvivorsField& field = run.field_for_tests();
  field.hero.skills.clear();
  // Un PV, sur le héros, et une frappe qui l'atteint dans ce même pas : la
  // frappe et l'enterrement passent avant le contact.
  const std::uint32_t serial = PlaceFoe(field, kTinyLunaticId, field.hero.position);
  field.foes[0].hp = 1;
  field.strikes.push_back(ro::sim::SurvivorsStrike{.foe_serial = serial,
                                                   .land_step = field.step,
                                                   .base_damage = 10,
                                                   .skill_id = ro::test_support::kTinyStrikeId,
                                                   .element = ro::sim::SurvivorsElement::kFire});
  run.Step({});
  CHECK(run.field().strikes.empty());
  CHECK(run.TakeNews().hero_hurt == 0);
  CHECK(run.field().hero.hp == run.field().hero.max_hp);
  CHECK(run.field().foes.empty());
  CHECK(run.tally().kills == 1U);
}

TEST_CASE("Partie survivors : un monstre sans dégâts de contact ne blesse pas",
          "[sim][survivors]") {
  std::array<ro::sim::FoeRule, ro::test_support::kTinyFoes.size()> foes =
      ro::test_support::kTinyFoes;
  for (ro::sim::FoeRule& foe : foes) {
    if (foe.foe_id == kTinyLunaticId) foe.touch_damage = 0;
  }
  ro::sim::SurvivorsRules rules = QuietRules();
  rules.foes = foes;
  auto started = SurvivorsRun::Start(rules, PillarArena(64), TinyStart(19));
  REQUIRE(started.has_value());
  SurvivorsRun run = std::move(*started);
  SurvivorsField& field = run.field_for_tests();
  field.hero.skills.clear();
  (void)PlaceFoe(field, kTinyLunaticId, field.hero.position);
  for (int step = 0; step < 10; ++step) run.Step({});
  CHECK(run.TakeNews().hero_hurt == 0);
  CHECK(run.field().hero.hp == run.field().hero.max_hp);
}

TEST_CASE("Partie survivors : l'armure retranche le contact, au moins un PV", "[sim][survivors]") {
  // Un seul contact d'un Lunatic (3 PV) contre un héros à 50 PV.
  const auto hurt = [](std::int32_t armor) {
    SurvivorsRun run = QuietRun(9);
    SurvivorsField& field = run.field_for_tests();
    field.hero.skills.clear();
    field.hero.stats.values[static_cast<std::size_t>(ro::sim::SurvivorsStat::kArmor)] = armor;
    field.hero.hp = 50;
    (void)PlaceFoe(field, kTinyLunaticId, field.hero.position);
    run.Step({});
    CHECK(run.field().phase == SurvivorsPhase::kRunning);
    return std::pair{run.field().hero.hp, run.TakeNews().hero_hurt};
  };
  // L'armure se retranche : 3 − 1.
  CHECK(hurt(1) == std::pair{48, 2});
  // Plus forte que le contact : il reste le minimum d'un PV.
  CHECK(hurt(10) == std::pair{49, 1});
  // Sans armure, le contact entier.
  CHECK(hurt(0) == std::pair{47, 3});
}

TEST_CASE("Partie survivors : l'armure et la chute du héros", "[sim][survivors]") {
  SurvivorsRun run = QuietRun(9);
  SurvivorsField& field = run.field_for_tests();
  field.hero.skills.clear();
  // Une armure plus forte que le contact laisse un PV de dégâts.
  field.hero.stats.values[static_cast<std::size_t>(ro::sim::SurvivorsStat::kArmor)] = 10;
  // Un PV par minute : une réserve restée nulle prouve que Regenerate n'a pas
  // tourné.
  field.hero.stats.values[static_cast<std::size_t>(ro::sim::SurvivorsStat::kRegenPerMinute)] = 1;
  field.hero.hp = 1;
  (void)PlaceFoe(field, kTinyLunaticId, field.hero.position);
  run.Step({});
  CHECK(run.field().phase == SurvivorsPhase::kOver);
  CHECK(run.tally().outcome == SurvivorsOutcome::kFell);
  CHECK(run.field().hero.hp == 0);
  // La suite du pas est sautée : la régénération ne tourne pas.
  CHECK(run.field().hero.regen_carry == 0U);
  CHECK(run.field().step == 1U);
}

TEST_CASE("Partie survivors : la régénération par minute", "[sim][survivors]") {
  SurvivorsRun run = QuietRun(10);
  SurvivorsField& field = run.field_for_tests();
  // 100 PV par minute : 3 000 pas par minute, un PV tous les 30 pas.
  field.hero.stats.values[static_cast<std::size_t>(ro::sim::SurvivorsStat::kRegenPerMinute)] = 100;
  field.hero.hp = 50;
  for (int step = 0; step < 29; ++step) run.Step({});
  CHECK(run.field().hero.hp == 50);
  run.Step({});
  CHECK(run.field().hero.hp == 51);
  for (int step = 0; step < 30 * 60; ++step) run.Step({});
  CHECK(run.field().hero.hp == run.field().hero.max_hp);
}

TEST_CASE("Partie survivors : la fin à run_ms", "[sim][survivors]") {
  SurvivorsRun run = QuietRun(11);
  const std::uint32_t last_step = ro::test_support::kTinyRunMs / ro::sim::kSurvivorsStepMs;
  for (std::uint32_t step = 1; step < last_step; ++step) run.Step({});
  CHECK(run.field().phase == SurvivorsPhase::kRunning);
  run.Step({});
  CHECK(run.field().phase == SurvivorsPhase::kOver);
  CHECK(run.tally().outcome == SurvivorsOutcome::kSurvived);
  CHECK(run.tally().survived_steps == last_step);
  run.Step({});
  CHECK(run.field().step == last_step);
}

TEST_CASE("Partie survivors : abandon pendant un choix", "[sim][survivors]") {
  SurvivorsRun run = QuietRun(12);
  run.Step({});
  run.field_for_tests().phase = SurvivorsPhase::kChoosing;
  run.Abandon();
  CHECK(run.field().phase == SurvivorsPhase::kOver);
  CHECK(run.tally().outcome == SurvivorsOutcome::kAbandoned);
  // Une partie finie ne change plus d'issue.
  run.Abandon();
  CHECK(run.tally().outcome == SurvivorsOutcome::kAbandoned);
}

TEST_CASE("Partie survivors : le bilan d'un abandon suit les PV du choix", "[sim][survivors]") {
  SurvivorsRun run = QuietRun(12);
  run.Step({});
  // Deux montées attendues, un soin offert : le héros se soigne, puis le
  // second choix s'ouvre, et il abandonne.
  SurvivorsField& field = run.field_for_tests();
  field.hero.hp = 40;
  field.pending_level_ups = 2;
  field.phase = SurvivorsPhase::kChoosing;
  field.offers = {ro::sim::SurvivorsOffer{.kind = ro::sim::OfferKind::kHeal}};
  REQUIRE(run.tally().hp_left == ro::test_support::kTinyClasses[0].max_hp);
  REQUIRE(run.Answer({.kind = ro::sim::AnswerKind::kPick, .offer_index = 0}));
  REQUIRE(run.field().phase == SurvivorsPhase::kChoosing);
  REQUIRE(run.field().hero.hp == 70);
  run.Abandon();
  CHECK(run.tally().outcome == SurvivorsOutcome::kAbandoned);
  CHECK(run.tally().hp_left == 70);
  CHECK(run.tally().hp_left == run.field().hero.hp);
  CHECK(run.tally().survived_steps == 1U);
}

TEST_CASE("Horloge survivors : les pas dus et le rattrapage plafonné", "[sim][survivors]") {
  SurvivorsPlay play(QuietRun(13));
  CHECK(play.blend_per_mille() == 0U);
  CHECK(play.Advance(50, {}) == 2U);
  CHECK(play.run().field().step == 2U);
  CHECK(play.blend_per_mille() == 500U);
  CHECK(play.Advance(10, {}) == 1U);
  CHECK(play.blend_per_mille() == 0U);
  CHECK(play.Advance(19, {}) == 0U);
  CHECK(play.blend_per_mille() == 950U);
  CHECK(play.Advance(1, {}) == 1U);
  CHECK(play.run().field().step == 4U);

  // Une image figée : au plus kMostCatchUpSteps pas, et le dû au-delà perdu,
  // sauf la fraction d'un pas.
  CHECK(play.Advance(1013, {}) == ro::sim::kMostCatchUpSteps);
  CHECK(play.run().field().step == 4U + ro::sim::kMostCatchUpSteps);
  CHECK(play.blend_per_mille() == 650U);
  CHECK(play.Advance(7, {}) == 1U);
  CHECK(play.blend_per_mille() == 0U);
  // Pile au plafond, rien n'est perdu.
  CHECK(play.Advance(ro::sim::kMostCatchUpSteps * ro::sim::kSurvivorsStepMs + 5, {}) ==
        ro::sim::kMostCatchUpSteps);
  CHECK(play.blend_per_mille() == 250U);
  CHECK(play.journal().final_step == play.run().field().step);
}

TEST_CASE("Horloge survivors : le cap s'inscrit quand il change", "[sim][survivors]") {
  SurvivorsPlay play(QuietRun(14));
  CHECK(play.Advance(60, Heading(3)) == 3U);
  CHECK(play.Advance(40, Heading(3)) == 2U);
  // 67 se ramène à 3 : pas de nouvelle entrée.
  CHECK(play.Advance(20, Heading(67)) == 1U);
  CHECK(play.Advance(20, Heading(70)) == 1U);
  CHECK(play.Advance(40, {}) == 2U);
  // Un tour sans pas dû n'inscrit rien, même si le cap change.
  CHECK(play.Advance(5, Heading(9)) == 0U);
  CHECK(play.Advance(15, {}) == 1U);
  const std::vector<JournalEntry> expected{
      {.step = 0, .kind = JournalKind::kHeading, .value = 3},
      {.step = 6, .kind = JournalKind::kHeading, .value = 6},
      {.step = 7, .kind = JournalKind::kHeading, .value = ro::sim::kNoHeading},
  };
  CHECK(play.journal().entries == expected);
  CHECK(play.journal().final_step == 10U);
  CHECK(play.run().field().hero.facing == 6);
}

TEST_CASE("Horloge survivors : rien hors kRunning", "[sim][survivors]") {
  SurvivorsRun choosing = QuietRun(15);
  choosing.field_for_tests().phase = SurvivorsPhase::kChoosing;
  SurvivorsPlay play(std::move(choosing));
  CHECK(play.Advance(100, Heading(5)) == 0U);
  CHECK(play.run().field().step == 0U);
  CHECK(play.journal().entries.empty());
  CHECK(play.blend_per_mille() == 0U);
  // Une réponse refusée ne s'inscrit pas.
  CHECK_FALSE(play.Answer({.kind = ro::sim::AnswerKind::kPick, .offer_index = 9}));
  CHECK(play.journal().entries.empty());

  // L'abandon s'inscrit au pas courant et le fige ; après lui, plus rien.
  play.Abandon();
  REQUIRE(play.journal().entries.size() == 1);
  CHECK(play.journal().entries[0] ==
        JournalEntry{.step = 0, .kind = JournalKind::kAbandon, .value = 0});
  CHECK(play.run().tally().outcome == SurvivorsOutcome::kAbandoned);
  play.Abandon();
  CHECK(play.journal().entries.size() == 1);
  CHECK(play.Advance(100, Heading(5)) == 0U);
  CHECK(play.journal().entries.size() == 1);
  CHECK(play.journal().final_step == 0U);
}

TEST_CASE("Horloge survivors : un choix ouvert arrête la boucle", "[sim][survivors]") {
  SurvivorsRun run = QuietRun(16);
  run.field_for_tests().pending_level_ups = 1;
  SurvivorsPlay play(std::move(run));
  CHECK(play.Advance(95, {}) == 1U);
  CHECK(play.run().field().phase == SurvivorsPhase::kChoosing);
  CHECK(play.run().field().step == 1U);
  CHECK(play.blend_per_mille() == 0U);
  CHECK(play.journal().final_step == 1U);
  CHECK(play.Advance(100, {}) == 0U);
}

TEST_CASE("Horloge survivors : la partie finie arrête la boucle", "[sim][survivors]") {
  SurvivorsPlay play(QuietRun(17));
  const std::uint32_t last_step = ro::test_support::kTinyRunMs / ro::sim::kSurvivorsStepMs;
  std::uint32_t total = 0;
  for (std::uint32_t turn = 0; turn < last_step; ++turn) {
    total += play.Advance(ro::sim::kMostCatchUpSteps * ro::sim::kSurvivorsStepMs, {});
  }
  CHECK(total == last_step);
  CHECK(play.run().tally().outcome == SurvivorsOutcome::kSurvived);
  CHECK(play.journal().final_step == last_step);
  CHECK(play.blend_per_mille() == 0U);
}

// ── La méta dans la partie ──────────────────────────────────────────────────

namespace {

// La Cursed Water des essais : par rang, PV +20 % et contact +10 %.
constexpr std::array<ro::sim::CursedWaterRank, ro::sim::kMostCursedWater + 1> kTestCursedWater{{
    {.foe_hp_pct = 0, .touch_pct = 0},
    {.foe_hp_pct = 20, .touch_pct = 10},
    {.foe_hp_pct = 40, .touch_pct = 20},
    {.foe_hp_pct = 60, .touch_pct = 30},
    {.foe_hp_pct = 80, .touch_pct = 40},
    {.foe_hp_pct = 100, .touch_pct = 50},
}};

// Des règles, minuscules ou silencieuses, qui connaissent la Cursed Water.
[[nodiscard]] ro::sim::SurvivorsRules Cursed(const ro::sim::SurvivorsRules& base) {
  ro::sim::SurvivorsRules rules = base;
  rules.cursed_water = kTestCursedWater;
  return rules;
}

[[nodiscard]] const ro::sim::SurvivorsRules& CursedQuietRules() {
  static const ro::sim::SurvivorsRules kRules = Cursed(QuietRules());
  return kRules;
}

[[nodiscard]] const ro::sim::SurvivorsRules& CursedTinyRules() {
  static const ro::sim::SurvivorsRules kRules = Cursed(TinyRules());
  return kRules;
}

[[nodiscard]] SurvivorsRun MetaRun(const ro::sim::SurvivorsRules& rules,
                                   const SurvivorsStart& start) {
  auto started = SurvivorsRun::Start(rules, PillarArena(64), start);
  REQUIRE(started.has_value());
  return std::move(*started);
}

}  // namespace

TEST_CASE("Partie survivors : Start refuse une méta qui ne se joue pas", "[sim][survivors]") {
  const auto starts = [](const ro::sim::SurvivorsRules& rules, const SurvivorsStart& start) {
    return SurvivorsRun::Start(rules, PillarArena(32), start).has_value();
  };
  SurvivorsStart start = TinyStart(1);
  start.cursed_water = ro::sim::kMostCursedWater;
  CHECK(starts(CursedQuietRules(), start));
  // Des règles sans Cursed Water n'en jouent aucun rang.
  CHECK_FALSE(starts(QuietRules(), start));
  start.cursed_water = ro::sim::kMostCursedWater + 1;
  CHECK_FALSE(starts(CursedQuietRules(), start));

  // Les verrous : croissants, dans une réserve, jamais le premier skill.
  start = TinyStart(1);
  start.locked_ids = {ro::test_support::kTinyOrbitId, ro::test_support::kTinyCardId};
  CHECK(starts(QuietRules(), start));
  start.locked_ids = {ro::test_support::kTinyCardId, ro::test_support::kTinyOrbitId};
  CHECK_FALSE(starts(QuietRules(), start));
  start.locked_ids = {ro::test_support::kTinyOrbitId, ro::test_support::kTinyOrbitId};
  CHECK_FALSE(starts(QuietRules(), start));
  start.locked_ids = {ro::test_support::kTinyPoringId};
  CHECK_FALSE(starts(QuietRules(), start));
  start.locked_ids = {ro::test_support::kTinyStrikeId};
  CHECK_FALSE(starts(QuietRules(), start));
  start.locked_ids.clear();
  for (std::uint16_t id = 1; id <= ro::sim::kMostLockedIds + 1; ++id)
    start.locked_ids.push_back(id);
  CHECK_FALSE(starts(QuietRules(), start));

  // Le départ neutre : rien de tout cela.
  CHECK(ro::sim::IsNeutralStart(TinyStart(1)));
  start = TinyStart(1);
  start.lives = 1;
  CHECK_FALSE(ro::sim::IsNeutralStart(start));
  start = TinyStart(1);
  start.cursed_water = 1;
  CHECK_FALSE(ro::sim::IsNeutralStart(start));
  start = TinyStart(1);
  start.locked_ids = {ro::test_support::kTinyCardId};
  CHECK_FALSE(ro::sim::IsNeutralStart(start));
}

TEST_CASE("Partie survivors : une vie de réserve relève le héros, la dernière chute est finale",
          "[sim][survivors]") {
  SurvivorsStart start = TinyStart(9);
  start.lives = 1;
  SurvivorsRun run = MetaRun(QuietRules(), start);
  SurvivorsField& field = run.field_for_tests();
  field.hero.skills.clear();
  field.hero.hp = 1;
  const FieldPoint hero = field.hero.position;
  (void)PlaceFoe(field, kTinyLunaticId, hero);
  const std::uint32_t near =
      PlaceFoe(field, kTinyLunaticId, hero + FieldPoint{ro::sim::Cells(2), 0});
  const std::uint32_t far =
      PlaceFoe(field, kTinyLunaticId, hero + FieldPoint{ro::sim::Cells(6), 0});
  for (ro::sim::SurvivorsFoe& foe : field.foes) foe.units_per_step = 0;
  run.Step({});

  // Relevé à 50 % de ses PV max, la partie continue, sans chute au bilan.
  CHECK(run.field().phase == SurvivorsPhase::kRunning);
  CHECK(run.tally().outcome == SurvivorsOutcome::kRunning);
  CHECK(run.field().hero.hp == run.field().hero.max_hp / 2);
  CHECK(run.field().lives_left == 0);
  const ro::sim::SurvivorsNews news = run.TakeNews();
  REQUIRE(news.revives.size() == 1U);
  // Le contact a d'abord poussé le héros : il se relève là où il est.
  const FieldPoint revived_at = run.field().hero.position;
  CHECK(news.revives[0].at == revived_at);
  CHECK(news.revives[0].lives_left == 0);
  // L'onde écarte le voisin jusqu'à quatre cases du héros, pas le lointain.
  for (const ro::sim::SurvivorsFoe& foe : run.field().foes) {
    const auto distance = static_cast<std::int32_t>(ro::sim::IntegerSqrt(
        static_cast<std::uint64_t>(ro::sim::DistanceSquared(foe.position, revived_at))));
    if (foe.serial == near) {
      CHECK(foe.knock_left_units == ro::sim::Cells(ro::sim::kRevivePushCells) - distance);
      CHECK(foe.knock_heading == ro::sim::HeadingOf(foe.position - revived_at, 0));
    }
    if (foe.serial == far) CHECK(foe.knock_left_units == 0);
  }

  // Trois secondes sans un coup, contacts compris.
  const std::int32_t revived_hp = run.field().hero.hp;
  for (std::uint32_t step = 0; step < ro::sim::StepsOf(ro::sim::kReviveInvulnerableMs); ++step) {
    for (ro::sim::SurvivorsFoe& foe : run.field_for_tests().foes) foe.position = hero;
    run.Step({});
  }
  CHECK(run.field().hero.hp == revived_hp);
  CHECK(run.TakeNews().hero_hurt == 0);

  // Sans vie : la chute est finale.
  run.field_for_tests().hero.hp = 1;
  for (int step = 0; step < 200 && run.field().phase == SurvivorsPhase::kRunning; ++step) {
    for (ro::sim::SurvivorsFoe& foe : run.field_for_tests().foes) foe.position = hero;
    run.Step({});
  }
  CHECK(run.tally().outcome == SurvivorsOutcome::kFell);
  CHECK(run.TakeNews().revives.empty());
}

TEST_CASE("Partie survivors : la Cursed Water durcit les monstres qui naissent et leur contact",
          "[sim][survivors]") {
  for (const std::uint8_t rank : {std::uint8_t{0}, std::uint8_t{3}, ro::sim::kMostCursedWater}) {
    INFO("rang " << int{rank});
    SurvivorsStart start = TinyStart(4);
    start.cursed_water = rank;
    SurvivorsRun run = MetaRun(CursedTinyRules(), start);
    run.Step({});
    REQUIRE_FALSE(run.field().foes.empty());
    // À la première minute, aucune croissance : les PV de la règle, majorés.
    for (const ro::sim::SurvivorsFoe& foe : run.field().foes) {
      const ro::sim::FoeRule* rule = ro::sim::FindFoe(CursedTinyRules(), foe.foe_id);
      REQUIRE(rule != nullptr);
      CHECK(foe.max_hp == rule->hp * (100 + kTestCursedWater[rank].foe_hp_pct) / 100);
    }

    // Le contact d'un Lunatic (3) : 3, 3 × 130 % puis 3 × 150 %, tronqués.
    SurvivorsRun touched = MetaRun(CursedQuietRules(), start);
    SurvivorsField& field = touched.field_for_tests();
    field.hero.skills.clear();
    (void)PlaceFoe(field, kTinyLunaticId, field.hero.position);
    touched.Step({});
    CHECK(touched.TakeNews().hero_hurt == 3 * (100 + kTestCursedWater[rank].touch_pct) / 100);
  }
}

TEST_CASE("Partie survivors : un glaçage compte quand un monstre vivant devient ralenti",
          "[sim][survivors]") {
  SurvivorsRun run = QuietRun(12);
  SurvivorsField& field = run.field_for_tests();
  field.hero.skills.clear();
  const std::uint32_t serial =
      PlaceFoe(field, kTinyLunaticId, field.hero.position + FieldPoint{ro::sim::Cells(6), 0});
  field.foes[0].units_per_step = 0;
  const auto strike = [&](std::int32_t damage) {
    run.field_for_tests().strikes.push_back(
        ro::sim::SurvivorsStrike{.foe_serial = serial,
                                 .land_step = run.field().step,
                                 .base_damage = damage,
                                 .skill_id = ro::test_support::kTinyStrikeId,
                                 .chill_ms = 1000});
    run.Step({});
  };
  strike(1);
  CHECK(run.field().recap.chills == 1U);
  // Une seconde frappe sur le monstre encore ralenti prolonge son glaçage sans
  // le recompter.
  REQUIRE(run.field().foes[0].chill_until_step > run.field().step);
  strike(1);
  CHECK(run.field().recap.chills == 1U);
  // Son glaçage échu, le monstre ralenti de nouveau compte une fois de plus.
  while (run.field().foes[0].chill_until_step > run.field().step) run.Step({});
  strike(1);
  CHECK(run.field().recap.chills == 2U);
  // Le coup qui tue ne glace personne, même hors de tout glaçage en cours.
  while (run.field().foes[0].chill_until_step > run.field().step) run.Step({});
  strike(100);
  CHECK(run.field().foes.empty());
  CHECK(run.field().recap.chills == 2U);
}

TEST_CASE("Partie survivors : la carte Phreeoni choisie relève le Chasseur abattu",
          "[sim][survivors]") {
  constexpr std::uint16_t kHunterClassId = 3;
  constexpr std::uint16_t kPlainsArenaId = 2;
  constexpr std::uint16_t kPhreeoniCardId = 4121;
  constexpr std::uint16_t kPoringId = 1002;
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  auto started = SurvivorsRun::Start(
      rules, SurvivorsStart{.seed = 7, .class_id = kHunterClassId, .arena_id = kPlainsArenaId});
  REQUIRE(started.has_value());
  SurvivorsRun run = std::move(*started);
  ro::sim::SurvivorsField& field = run.field_for_tests();
  field.foes.clear();
  field.phase = SurvivorsPhase::kChoosing;
  field.offers = {ro::sim::SurvivorsOffer{
      .kind = ro::sim::OfferKind::kNewCard, .id = kPhreeoniCardId, .next_level = 1}};
  REQUIRE(
      run.Answer(ro::sim::SurvivorsAnswer{.kind = ro::sim::AnswerKind::kPick, .offer_index = 0}));
  CHECK(run.field().lives_left == 1);

  field.hero.hp = 1;
  (void)ro::test_support::PlaceFoe(field, kPoringId, field.hero.position);
  for (int step = 0;
       step < 50 && run.field().phase == SurvivorsPhase::kRunning && run.field().lives_left == 1;
       ++step) {
    run.Step({});
  }
  CHECK(run.field().phase == SurvivorsPhase::kRunning);
  CHECK(run.field().lives_left == 0);
  CHECK(run.field().hero.hp > 0);
}

TEST_CASE("Partie survivors : la relève de Phreeoni survit à l'évolution en Blast Mine",
          "[sim][survivors]") {
  constexpr std::uint16_t kHunterClassId = 3;
  constexpr std::uint16_t kPlainsArenaId = 2;
  constexpr std::uint16_t kPhreeoniCardId = 4121;
  constexpr std::uint16_t kLandMineId = 116;
  constexpr std::uint16_t kBlastMineId = 122;
  constexpr std::uint16_t kPoringId = 1002;
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  auto started = SurvivorsRun::Start(
      rules, SurvivorsStart{.seed = 7, .class_id = kHunterClassId, .arena_id = kPlainsArenaId});
  REQUIRE(started.has_value());
  SurvivorsRun run = std::move(*started);
  ro::sim::SurvivorsField& field = run.field_for_tests();
  field.foes.clear();
  const ro::sim::SurvivorsSkillRule* land_mine = ro::sim::FindSkill(rules, kLandMineId);
  REQUIRE(land_mine != nullptr);
  field.hero.skills.push_back(ro::sim::OwnedSkill{
      .skill_id = kLandMineId, .level = static_cast<std::uint8_t>(land_mine->levels.size())});
  field.phase = SurvivorsPhase::kChoosing;
  field.offers = {ro::sim::SurvivorsOffer{
      .kind = ro::sim::OfferKind::kNewCard, .id = kPhreeoniCardId, .next_level = 1}};
  REQUIRE(
      run.Answer(ro::sim::SurvivorsAnswer{.kind = ro::sim::AnswerKind::kPick, .offer_index = 0}));
  REQUIRE(run.field().lives_left == 1);

  // Le coffre de l'heure des évolutions, sous les pieds du héros.
  field.step = rules.chests.evolution_from_ms / ro::sim::kSurvivorsStepMs;
  field.pickups.push_back(ro::sim::SurvivorsPickup{.serial = field.next_serial++,
                                                   .kind = ro::sim::PickupKind::kChest,
                                                   .item_id = rules.chests.item_id,
                                                   .position = field.hero.position});
  for (int step = 0; step < 3 && field.hero.skills.back().skill_id != kBlastMineId; ++step) {
    field.hero.exp = 0;
    run.Step({});
  }
  REQUIRE(std::ranges::any_of(run.field().hero.skills, [](const ro::sim::OwnedSkill& owned) {
    return owned.skill_id == kBlastMineId;
  }));
  CHECK(run.field().lives_left == 1);

  // Un Poring que les mines ne tuent pas avant qu'il touche.
  field.hero.hp = 1;
  (void)ro::test_support::PlaceFoe(field, kPoringId, field.hero.position);
  field.foes.back().hp = std::numeric_limits<std::int32_t>::max() / 2;
  for (int step = 0;
       step < 50 && run.field().phase == SurvivorsPhase::kRunning && run.field().lives_left == 1;
       ++step) {
    field.hero.exp = 0;
    field.hero.hp = std::min(field.hero.hp, 1);
    run.Step({});
  }
  CHECK(run.field().phase == SurvivorsPhase::kRunning);
  CHECK(run.field().lives_left == 0);
  CHECK(run.field().hero.hp > 0);
}
