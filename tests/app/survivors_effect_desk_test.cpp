// LES EFFETS D'UNE PARTIE, lus dans la simulation : un effet en cours suit le
// terme que la partie tient (un second ramassage relance la durée pleine), un
// objet ramassé fait un avis de deux secondes, et la pause dit les cartes, les
// Bénédictions, les charges et les vies avec les chiffres des règles.

#include "game/survivors_effect_desk.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/survivors_content.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_meta.h"
#include "ro/sim/survivors_news.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/walk_map.h"

#include "support/survivors_content_fixture.h"
#include "support/survivors_fixture.h"

using moonclient::PickupNotices;
using moonclient::PickupNoticeShown;
using moonclient::SurvivorsPermanentEffect;
using moonclient::SurvivorsPermanentSource;
using moonclient::SurvivorsTimedEffect;
using ro::sim::PickupKind;
using ro::sim::SurvivorsPickupNews;
using ro::sim::SurvivorsRun;
using ro::sim::SurvivorsStart;
using ro::ui::SurvivorsEffectFigure;

namespace {

constexpr std::int32_t kArenaSideCells = 64;
constexpr std::uint32_t kManualMs = 30'000;
constexpr std::uint32_t kHasteMs = 10'000;
constexpr std::uint32_t kTenSecondsSteps = 10'000 / ro::sim::kSurvivorsStepMs;
constexpr std::uint16_t kManualItemId = 12208;
constexpr std::uint16_t kGoldItemId = 969;
constexpr std::int32_t kBlessingDamagePct = 6;

// Les règles de contenu sans vague, événement, boss ni élite : rien ne vient
// déranger le héros qui ramasse.
struct QuietContent : ro::test_support::ContentTables {
  QuietContent() {
    rules.waves = ro::test_support::kQuietWaves;
    rules.events = {};
    rules.bosses = {};
    rules.elites = {};
  }
};

[[nodiscard]] SurvivorsRun StartRun(const ro::sim::SurvivorsRules& rules,
                                    SurvivorsStart start = ro::test_support::TinyStart(1)) {
  const auto side = static_cast<std::size_t>(kArenaSideCells);
  auto run = SurvivorsRun::Start(rules,
                                 ro::sim::WalkMap(kArenaSideCells, kArenaSideCells,
                                                  std::vector<std::uint8_t>(side * side, 1U)),
                                 start);
  REQUIRE(run.has_value());
  run->field_for_tests().plants.clear();
  run->field_for_tests().hero.skills.clear();
  return std::move(*run);
}

// L'objet de cette sorte posé sous le héros, ramassé au pas suivant.
void PickHere(SurvivorsRun& run, const ro::sim::SurvivorsRules& rules, PickupKind kind) {
  const ro::sim::PickupRule* rule = ro::sim::FindPickup(rules, kind);
  REQUIRE(rule != nullptr);
  ro::sim::SurvivorsField& field = run.field_for_tests();
  field.pickups.push_back(ro::sim::SurvivorsPickup{.serial = field.next_serial++,
                                                   .kind = kind,
                                                   .item_id = rule->item_id,
                                                   .position = field.hero.position});
  ro::test_support::StepStill(run);
}

[[nodiscard]] std::vector<SurvivorsTimedEffect> TimedOf(const SurvivorsRun& run,
                                                        const ro::sim::SurvivorsRules& rules) {
  std::vector<SurvivorsTimedEffect> effects;
  moonclient::TimedEffectsOf(run.field(), rules, effects);
  return effects;
}

}  // namespace

TEST_CASE("Effets survivors : un effet en cours suit la partie, puis s'en va", "[app][survivors]") {
  QuietContent tables;
  SurvivorsRun run = StartRun(tables.rules);
  CHECK(TimedOf(run, tables.rules).empty());

  PickHere(run, tables.rules, PickupKind::kManual);
  std::vector<SurvivorsTimedEffect> effects = TimedOf(run, tables.rules);
  REQUIRE(effects.size() == 1U);
  CHECK(effects[0].kind == PickupKind::kManual);
  CHECK(effects[0].item_id == kManualItemId);
  CHECK(effects[0].full_ms == kManualMs);
  CHECK(effects[0].remaining_ms == kManualMs);

  ro::test_support::StepStill(run, kTenSecondsSteps);
  effects = TimedOf(run, tables.rules);
  REQUIRE(effects.size() == 1U);
  CHECK(effects[0].remaining_ms == kManualMs - kTenSecondsSteps * ro::sim::kSurvivorsStepMs);

  SECTION("un second ramassage relance la durée pleine") {
    PickHere(run, tables.rules, PickupKind::kManual);
    effects = TimedOf(run, tables.rules);
    REQUIRE(effects.size() == 1U);
    CHECK(effects[0].remaining_ms == kManualMs);
  }

  SECTION("à son terme, l'effet quitte la colonne") {
    ro::test_support::StepStill(run, (kManualMs / ro::sim::kSurvivorsStepMs) - kTenSecondsSteps);
    CHECK(TimedOf(run, tables.rules).empty());
  }

  SECTION("deux effets se rangent dans l'ordre des règles") {
    PickHere(run, tables.rules, PickupKind::kHaste);
    effects = TimedOf(run, tables.rules);
    REQUIRE(effects.size() == 2U);
    CHECK(effects[0].kind == PickupKind::kManual);
    CHECK(effects[1].kind == PickupKind::kHaste);
    CHECK(effects[1].remaining_ms == kHasteMs);
  }
}

TEST_CASE("Effets survivors : l'effet d'un objet vient de sa règle", "[app][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  const ro::sim::PickupRule* manual = ro::sim::FindPickup(rules, PickupKind::kManual);
  REQUIRE(manual != nullptr);
  const std::optional<moonclient::SurvivorsPickupFigure> manual_figure =
      moonclient::PickupFigureOf(*manual);
  REQUIRE(manual_figure.has_value());
  CHECK(manual_figure->figure == SurvivorsEffectFigure::kExpPct);
  CHECK(manual_figure->value == manual->amount);

  const ro::sim::PickupRule* water = ro::sim::FindPickup(rules, PickupKind::kHolyWater);
  REQUIRE(water != nullptr);
  const std::optional<moonclient::SurvivorsPickupFigure> water_figure =
      moonclient::PickupFigureOf(*water);
  REQUIRE(water_figure.has_value());
  CHECK(water_figure->figure == SurvivorsEffectFigure::kPurifyCells);
  CHECK(water_figure->value * ro::sim::kSubunitsPerCell == water->reach_units);

  CHECK_FALSE(moonclient::PickupFigureOf({.kind = PickupKind::kChest}).has_value());
  CHECK_FALSE(moonclient::NoticedPickup(PickupKind::kGold));
  CHECK(moonclient::NoticedPickup(PickupKind::kHaste));
}

TEST_CASE("Effets survivors : l'avis d'un ramassage dure deux secondes", "[app][survivors]") {
  PickupNotices notices;
  std::vector<PickupNoticeShown> shown;
  const std::vector<SurvivorsPickupNews> taken = {
      {.kind = PickupKind::kGold, .item_id = kGoldItemId},
      {.kind = PickupKind::kManual, .item_id = kManualItemId}};
  constexpr std::uint64_t kTakenMs = 1'000;
  notices.Notice(taken, kTakenMs);

  // Le Gold ne fait pas d'avis : le compteur des Zeny le dit.
  notices.ShownAt(kTakenMs, shown);
  REQUIRE(shown.size() == 1U);
  CHECK(shown[0].item_id == kManualItemId);
  notices.ShownAt(kTakenMs + moonclient::kPickupNoticeMs / 2, shown);
  REQUIRE(shown.size() == 1U);
  CHECK(shown[0].fraction == 0.5F);
  notices.ShownAt(kTakenMs + moonclient::kPickupNoticeMs, shown);
  CHECK(shown.empty());

  SECTION("au-delà du plafond, le plus ancien laisse sa place") {
    for (std::size_t rank = 0; rank <= moonclient::kMostPickupNotices; ++rank) {
      notices.Notice(std::vector<SurvivorsPickupNews>{{.kind = PickupKind::kMeat}},
                     kTakenMs * 3 + rank);
    }
    CHECK(notices.living() == moonclient::kMostPickupNotices);
  }

  SECTION("une horloge qui recule efface les avis") {
    notices.Notice(taken, kTakenMs * 4);
    notices.ShownAt(kTakenMs, shown);
    CHECK(shown.empty());
  }
}

TEST_CASE("Effets survivors : la pause dit les effets permanents de la partie",
          "[app][survivors]") {
  QuietContent tables;
  std::vector<SurvivorsPermanentEffect> effects;

  SECTION("un départ neutre n'a que ses cartes") {
    SurvivorsRun run = StartRun(tables.rules);
    run.field_for_tests().hero.cards.push_back(
        {.card_item_id = ro::test_support::kTinyCardId, .level = 2});
    moonclient::PermanentEffectsOf(run.field(), tables.rules, effects);
    REQUIRE(effects.size() == 1U);
    CHECK(effects[0].source == SurvivorsPermanentSource::kCard);
    CHECK(effects[0].level == 2);
    CHECK(effects[0].figure == SurvivorsEffectFigure::kDamagePct);
    CHECK(effects[0].value == ro::test_support::kTinyCardLevels[1]);
  }

  SECTION("les cartes du critique et de l'esquive disent leur chance") {
    constexpr std::uint16_t kSmokieCardId = 4044;
    constexpr std::uint16_t kWhisperCardId = 4102;
    constexpr std::uint16_t kThiefClassId = 3;
    constexpr std::uint16_t kPlainsArenaId = 2;
    constexpr std::uint16_t kDoubleAttackId = 48;
    const ro::sim::SurvivorsRules& shipped = ro::sim::ShippedSurvivorsRules();
    auto thief = SurvivorsRun::Start(shipped, SurvivorsStart{.seed = 1,
                                                             .class_id = kThiefClassId,
                                                             .arena_id = kPlainsArenaId,
                                                             .first_skill_id = kDoubleAttackId});
    REQUIRE(thief.has_value());
    thief->field_for_tests().hero.cards.push_back({.card_item_id = kSmokieCardId, .level = 1});
    thief->field_for_tests().hero.cards.push_back({.card_item_id = kWhisperCardId, .level = 1});
    moonclient::PermanentEffectsOf(thief->field(), shipped, effects);
    REQUIRE(effects.size() == 2U);
    CHECK(effects[0].figure == SurvivorsEffectFigure::kCritPct);
    CHECK(effects[1].figure == SurvivorsEffectFigure::kDodgePct);
  }

  SECTION("un départ béni ajoute Bénédictions, charges qui restent et vies") {
    SurvivorsStart start = ro::test_support::TinyStart(1);
    start.loadout.bonus.values[static_cast<std::size_t>(ro::sim::SurvivorsStat::kDamagePct)] =
        kBlessingDamagePct;
    start.loadout.rerolls = 2;
    start.lives = 1;
    SurvivorsRun run = StartRun(tables.rules, start);
    run.field_for_tests().rerolls_left = 1;
    moonclient::PermanentEffectsOf(run.field(), tables.rules, effects);
    REQUIRE(effects.size() == 3U);

    CHECK(effects[0].source == SurvivorsPermanentSource::kBlessing);
    CHECK(effects[0].id == static_cast<std::uint16_t>(ro::sim::Blessing::kBlessing));
    CHECK(effects[0].figure == SurvivorsEffectFigure::kDamagePct);
    CHECK(effects[0].value == kBlessingDamagePct);

    const ro::sim::KafraRule* magnifier =
        ro::sim::FindKafra(ro::sim::ShippedSurvivorsMeta(), ro::sim::KafraService::kMagnifier);
    REQUIRE(magnifier != nullptr);
    CHECK(effects[1].source == SurvivorsPermanentSource::kKafra);
    CHECK(effects[1].id == magnifier->item_id);
    CHECK(effects[1].figure == SurvivorsEffectFigure::kRerollsLeft);
    CHECK(effects[1].value == 1);

    CHECK(effects[2].source == SurvivorsPermanentSource::kLife);
    CHECK(effects[2].figure == SurvivorsEffectFigure::kLivesLeft);
    CHECK(effects[2].value == 1);
  }
}
