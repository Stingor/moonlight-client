// LE MARCHAND : la cinquième classe du catalogue. Il frappe de près, et ses
// invocations frappent pour lui : des plantes posées qui mordent, un homoncule
// qui le suit. Il soigne le plus blessé, et ses cartes majorent les zeny, gèlent
// ce qui le touche et renforcent ses invocations. Rien de ce qu'il apporte ne
// tire quoi que ce soit pour une classe qui ne l'a pas.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_news.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"

#include "support/survivors_fixture.h"

namespace {

using ro::sim::SurvivorsRun;

constexpr std::uint16_t kMerchantClassId = 5;
constexpr std::uint16_t kPlainsArenaId = 2;  // la prairie générée
constexpr std::uint64_t kSeed = 20261108;

constexpr std::uint16_t kMammoniteId = 42;
constexpr std::uint16_t kBioCannibalizeId = 232;
constexpr std::uint16_t kPotionPitcherId = 231;
constexpr std::uint16_t kMoonlightId = 8009;
constexpr std::uint16_t kCapriceId = 8013;
constexpr std::uint16_t kCastlingId = 8005;
constexpr std::uint16_t kSbr44Id = 8012;

constexpr std::uint16_t kMandragoraClass = 1020;
constexpr std::uint16_t kFilirClass = 6003;

// Un monstre de pierre, lent et neutre.
constexpr std::uint16_t kSteadyFoeId = 1040;
constexpr std::int32_t kSteadyFoeHp = 1000000;

// Assez de pas pour qu'un allié né frappe ce qui est à sa portée.
constexpr std::uint32_t kFewSteps = 10;
// Assez pour qu'un homoncule rejoigne un monstre de sa laisse.
constexpr std::uint32_t kChaseSteps = 100;

[[nodiscard]] ro::sim::SurvivorsStart MerchantStart() {
  return ro::sim::SurvivorsStart{.seed = kSeed,
                                 .class_id = kMerchantClassId,
                                 .arena_id = kPlainsArenaId,
                                 .first_skill_id = kMammoniteId};
}

// Un Marchand sans skill ni monstre.
[[nodiscard]] SurvivorsRun BareMerchant(const ro::sim::SurvivorsRules& rules) {
  auto run = SurvivorsRun::Start(rules, MerchantStart());
  REQUIRE(run.has_value());
  ro::sim::SurvivorsField& field = run->field_for_tests();
  field.hero.skills.clear();
  field.foes.clear();
  field.plants.clear();
  return std::move(*run);
}

void Hold(ro::sim::SurvivorsField& field, std::uint16_t skill_id, std::uint8_t level = 1) {
  field.hero.skills.push_back(
      ro::sim::OwnedSkill{.skill_id = skill_id, .level = level, .ready_step = field.step});
}

// Le héros ne monte ni ne tombe pendant ces pas, et aucun monstre ne naît.
void StepQuietly(SurvivorsRun& run, std::uint32_t steps) {
  ro::sim::SurvivorsField& field = run.field_for_tests();
  for (std::uint32_t done = 0; done < steps; ++done) {
    field.hero.exp = 0;
    field.gems.clear();
    field.wave_spawn_step = field.step + 1;
    std::erase_if(field.foes,
                  [](const ro::sim::SurvivorsFoe& foe) { return foe.max_hp != kSteadyFoeHp; });
    run.Step({});
  }
}

// Un monstre robuste et immobile, à `offset` du héros.
[[nodiscard]] std::uint32_t PlaceSteadyFoe(ro::sim::SurvivorsField& field,
                                           ro::sim::FieldPoint offset) {
  const std::uint32_t serial =
      ro::test_support::PlaceFoe(field, kSteadyFoeId, field.hero.position + offset);
  REQUIRE(serial != 0U);
  ro::sim::SurvivorsFoe& foe = field.foes.back();
  foe.units_per_step = 0;
  foe.hp = kSteadyFoeHp;
  foe.max_hp = kSteadyFoeHp;
  foe.touch_ready_step = std::numeric_limits<std::uint32_t>::max();
  return serial;
}

[[nodiscard]] std::uint32_t HitsOn(const ro::sim::SurvivorsNews& news, std::uint32_t foe_serial,
                                   std::uint16_t skill_id) {
  return static_cast<std::uint32_t>(
      std::ranges::count_if(news.hits, [&](const ro::sim::SurvivorsHit& hit) {
        return hit.foe_serial == foe_serial && hit.skill_id == skill_id;
      }));
}

[[nodiscard]] std::size_t CastsOf(const ro::sim::SurvivorsNews& news, ro::sim::CastKind kind) {
  return static_cast<std::size_t>(std::ranges::count_if(
      news.casts, [&](const ro::sim::SurvivorsCast& cast) { return cast.kind == kind; }));
}

}  // namespace

TEST_CASE("Marchand survivors : sa classe, ses corps, hors classement", "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  const ro::sim::ClassRule* merchant = ro::sim::FindClass(rules, kMerchantClassId);
  REQUIRE(merchant != nullptr);
  CHECK(std::string_view(merchant->key) == "merchant");
  CHECK(merchant->attack_gesture);
  CHECK(merchant->free_play_only);
  // Il ramasse le plus loin du mode.
  for (const ro::sim::ClassRule& other : rules.classes) {
    if (other.class_id != kMerchantClassId) CHECK(other.magnet_units < merchant->magnet_units);
  }
  ro::sim::SurvivorsHero hero;
  CHECK(ro::sim::HeroJobOf(*merchant, hero) == 18);
  hero.evolutions = 1;
  CHECK(ro::sim::HeroJobOf(*merchant, hero) == 18);
  hero.level = merchant->reborn_level;
  CHECK(ro::sim::HeroJobOf(*merchant, hero) == 4019);
}

TEST_CASE("Marchand survivors : les plantes naissent, mordent, puis partent", "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  SurvivorsRun run = BareMerchant(rules);
  ro::sim::SurvivorsField& field = run.field_for_tests();
  Hold(field, kBioCannibalizeId);
  // La plante naît à une case au cap du héros, le nord : le monstre à deux
  // cases au nord est à sa portée.
  field.hero.facing = 0;
  const std::uint32_t serial = PlaceSteadyFoe(field, {0, ro::sim::Cells(2)});
  (void)run.TakeNews();
  StepQuietly(run, kFewSteps);
  const ro::sim::SurvivorsNews news = run.TakeNews();
  REQUIRE(field.summons.size() == 1U);
  const ro::sim::SurvivorsSummon plant = field.summons.front();
  CHECK(plant.summon_class == kMandragoraClass);
  CHECK(plant.skill_id == kBioCannibalizeId);
  CHECK(plant.expires_step > field.step);
  CHECK(CastsOf(news, ro::sim::CastKind::kSummonOn) == 1U);
  CHECK(HitsOn(news, serial, kBioCannibalizeId) > 0U);
  CHECK(CastsOf(news, ro::sim::CastKind::kSummonStrike) == HitsOn(news, serial, kBioCannibalizeId));
  // Une plante ne marche pas.
  CHECK(field.summons.front().position == plant.position);

  // Sa vie finie, elle part ; une autre la remplace à la recharge.
  StepQuietly(run, plant.expires_step - field.step + 1);
  CHECK(std::ranges::none_of(field.summons, [&](const ro::sim::SurvivorsSummon& summon) {
    return summon.serial == plant.serial;
  }));
  CHECK(CastsOf(run.TakeNews(), ro::sim::CastKind::kSummonOff) >= 1U);
}

TEST_CASE("Marchand survivors : jamais plus de plantes que le niveau n'en tient",
          "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  SurvivorsRun run = BareMerchant(rules);
  ro::sim::SurvivorsField& field = run.field_for_tests();
  Hold(field, kBioCannibalizeId, 2);
  const ro::sim::SkillLevel& level = ro::sim::FindSkill(rules, kBioCannibalizeId)->levels[1];
  REQUIRE(level.amount == 2U);
  // Trois recharges : deux plantes, pas trois.
  StepQuietly(run, 3 * ro::sim::StepsOf(level.cooldown_ms) + kFewSteps);
  CHECK(field.summons.size() == 2U);
  // La plante quitte la partie avec son skill.
  field.hero.skills.clear();
  StepQuietly(run, 1);
  CHECK(field.summons.empty());
}

TEST_CASE("Marchand survivors : l'homoncule suit le héros et fond sur sa laisse",
          "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  SurvivorsRun run = BareMerchant(rules);
  ro::sim::SurvivorsField& field = run.field_for_tests();
  Hold(field, kMoonlightId);
  // Hors de sa laisse : il ne quitte pas le héros.
  const std::uint32_t far = PlaceSteadyFoe(field, {ro::sim::Cells(12), 0});
  (void)run.TakeNews();
  StepQuietly(run, kChaseSteps);
  REQUIRE(field.summons.size() == 1U);
  CHECK(field.summons.front().summon_class == kFilirClass);
  CHECK(HitsOn(run.TakeNews(), far, kMoonlightId) == 0U);
  const std::int64_t heel = ro::sim::Cells(3);
  CHECK(ro::sim::DistanceSquared(field.summons.front().position, field.hero.position) <=
        heel * heel);

  // Dans sa laisse : il y vole, et frappe.
  const std::uint32_t near = PlaceSteadyFoe(field, {ro::sim::Cells(-5), 0});
  StepQuietly(run, kChaseSteps);
  CHECK(HitsOn(run.TakeNews(), near, kMoonlightId) > 0U);
  // Il ne meurt pas : toujours le même.
  CHECK(field.summons.size() == 1U);
  CHECK(field.summons.front().expires_step == 0U);
}

TEST_CASE("Marchand survivors : le bonus de quantité ne double pas l'homoncule",
          "[sim][survivors]") {
  constexpr std::int32_t kExtraAmount = 2;
  ro::sim::SurvivorsStart start = MerchantStart();
  ro::test_support::SetBonus(start, ro::sim::SurvivorsStat::kAmount, kExtraAmount);
  auto started = SurvivorsRun::Start(ro::sim::ShippedSurvivorsRules(), start);
  REQUIRE(started.has_value());
  SurvivorsRun run = std::move(*started);
  ro::sim::SurvivorsField& field = run.field_for_tests();
  field.hero.skills.clear();
  field.foes.clear();
  field.plants.clear();
  Hold(field, kMoonlightId);
  StepQuietly(run, kChaseSteps);
  REQUIRE(field.hero.stats.Of(ro::sim::SurvivorsStat::kAmount) == kExtraAmount);
  CHECK(field.summons.size() == 1U);
}

TEST_CASE("Marchand survivors : un seul homoncule, même évolué", "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  for (const std::uint16_t held : {kMoonlightId, kSbr44Id}) {
    SurvivorsRun run = BareMerchant(rules);
    ro::sim::SurvivorsField& field = run.field_for_tests();
    Hold(field, held);
    field.offers = {ro::sim::SurvivorsOffer{
        .kind = ro::sim::OfferKind::kNewSkill, .id = kCapriceId, .next_level = 1}};
    field.phase = ro::sim::SurvivorsPhase::kChoosing;
    CHECK_FALSE(run.Answer(ro::sim::SurvivorsAnswer{.offer_index = 0}));
    CHECK(field.hero.skills.size() == 1U);
  }
  // Sans homoncule, il se prend.
  SurvivorsRun run = BareMerchant(rules);
  ro::sim::SurvivorsField& field = run.field_for_tests();
  Hold(field, kMammoniteId);
  field.offers = {ro::sim::SurvivorsOffer{
      .kind = ro::sim::OfferKind::kNewSkill, .id = kCapriceId, .next_level = 1}};
  field.phase = ro::sim::SurvivorsPhase::kChoosing;
  CHECK(run.Answer(ro::sim::SurvivorsAnswer{.offer_index = 0}));
}

TEST_CASE("Marchand survivors : la potion soigne le blessé, et rien sans blessé",
          "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  SurvivorsRun run = BareMerchant(rules);
  ro::sim::SurvivorsField& field = run.field_for_tests();
  Hold(field, kPotionPitcherId);
  (void)run.TakeNews();
  StepQuietly(run, 1);
  CHECK(CastsOf(run.TakeNews(), ro::sim::CastKind::kMend) == 0U);

  const ro::sim::SkillLevel& level = ro::sim::FindSkill(rules, kPotionPitcherId)->levels[0];
  field.hero.hp = field.hero.max_hp / 2;
  const std::int32_t before = field.hero.hp;
  StepQuietly(run, 1);
  CHECK(CastsOf(run.TakeNews(), ro::sim::CastKind::kMend) == 1U);
  CHECK(field.hero.hp - before == field.hero.max_hp * level.heal_pct / 100);
}

TEST_CASE("Marchand survivors : Amistr prend la place du héros en danger", "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  SurvivorsRun run = BareMerchant(rules);
  ro::sim::SurvivorsField& field = run.field_for_tests();
  Hold(field, kCastlingId);
  StepQuietly(run, kFewSteps);
  REQUIRE(field.summons.size() == 1U);
  // Sain, rien ne s'échange.
  const ro::sim::FieldPoint hero_at = field.hero.position;
  StepQuietly(run, 1);
  CHECK(field.hero.position == hero_at);

  const ro::sim::FieldPoint amistr_at = field.summons.front().position;
  REQUIRE(amistr_at != hero_at);
  field.hero.hp = field.hero.max_hp / 10;
  (void)run.TakeNews();
  StepQuietly(run, 1);
  CHECK(field.hero.position == amistr_at);
  CHECK(field.hero.invulnerable_until_step > field.step);
  CHECK(CastsOf(run.TakeNews(), ro::sim::CastKind::kSummonSwap) == 1U);
  // Pas deux fois de suite : l'échange attend sa recharge.
  StepQuietly(run, 1);
  CHECK(field.hero.position == amistr_at);
}

TEST_CASE("Marchand survivors : Marin majore le Gold, Sasquatch gèle ce qui touche",
          "[sim][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  const ro::sim::PickupRule* gold = ro::sim::FindPickup(rules, ro::sim::PickupKind::kGold);
  REQUIRE(gold != nullptr);
  constexpr std::int32_t kZenyPct = 40;
  {
    SurvivorsRun run = BareMerchant(rules);
    ro::sim::SurvivorsField& field = run.field_for_tests();
    field.hero.stats.values[static_cast<std::size_t>(ro::sim::SurvivorsStat::kZenyPct)] = kZenyPct;
    field.pickups.push_back(ro::sim::SurvivorsPickup{
        .serial = 1, .kind = ro::sim::PickupKind::kGold, .position = field.hero.position});
    const std::uint32_t before = field.hero.zeny;
    StepQuietly(run, 1);
    CHECK(field.hero.zeny - before ==
          static_cast<std::uint32_t>(gold->amount + gold->amount * kZenyPct / 100));
  }
  {
    SurvivorsRun run = BareMerchant(rules);
    ro::sim::SurvivorsField& field = run.field_for_tests();
    field.hero.stats
        .values[static_cast<std::size_t>(ro::sim::SurvivorsStat::kFreezeRiposteChancePct)] = 100;
    const std::uint32_t serial = PlaceSteadyFoe(field, {});
    field.foes.back().touch_ready_step = field.step;
    StepQuietly(run, 1);
    const auto found = std::ranges::find(field.foes, serial, &ro::sim::SurvivorsFoe::serial);
    REQUIRE(found != field.foes.end());
    CHECK(found->freeze_until_step > field.step);
  }
}
