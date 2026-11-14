// LE LABORATOIRE DE LIGHTHALZEN : l'arène 5 joue son contenu, tire son MVP
// par la graine, et ses cuves cèdent à l'heure de leur événement.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string_view>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/survivors_content.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"

#include "support/survivors_fixture.h"

namespace {

using ro::sim::SurvivorsRun;

constexpr std::uint16_t kLabArenaId = 5;
constexpr std::uint16_t kMageClassId = 1;
constexpr std::uint16_t kFireBoltId = 19;
constexpr std::uint16_t kLordKnightSeyrenId = 1646;
constexpr std::uint16_t kHighWizardKathryneId = 1651;
constexpr std::uint16_t kEgnigemCeniaId = 1652;
constexpr std::uint64_t kSeedsTried = 6;
constexpr std::uint32_t kTankSeed = 2026;

[[nodiscard]] const ro::sim::SurvivorsRules& Rules() { return ro::sim::ShippedSurvivorsRules(); }

[[nodiscard]] SurvivorsRun LabRun(std::uint64_t seed) {
  auto run = SurvivorsRun::Start(Rules(), ro::sim::SurvivorsStart{.seed = seed,
                                                                  .class_id = kMageClassId,
                                                                  .arena_id = kLabArenaId,
                                                                  .first_skill_id = kFireBoltId});
  REQUIRE(run.has_value());
  return std::move(*run);
}

// Le rang du premier événement des cuves dans le contenu de la partie.
[[nodiscard]] std::size_t FirstTanksEvent(const ro::sim::SurvivorsField& field) {
  const auto events = field.content.events;
  const auto found = std::ranges::find_if(events, [](const ro::sim::EventRule& event) {
    return event.kind == ro::sim::EventKind::kTanks;
  });
  REQUIRE(found != events.end());
  return static_cast<std::size_t>(found - events.begin());
}

// Fait jouer, au prochain pas, l'événement de rang `index` : la partie au pas
// de son heure, sans monstre ni skill.
void PlayEventNext(ro::sim::SurvivorsField& field, std::size_t index) {
  field.foes.clear();
  field.hazards.clear();
  field.hero.skills.clear();
  field.next_event = index;
  const std::uint32_t at_ms = field.content.events[index].at_ms;
  field.step = at_ms / ro::sim::kSurvivorsStepMs;
}

[[nodiscard]] std::size_t CountOf(const ro::sim::SurvivorsField& field, std::uint16_t foe_id) {
  return static_cast<std::size_t>(std::ranges::count_if(
      field.foes, [&](const ro::sim::SurvivorsFoe& foe) { return foe.foe_id == foe_id; }));
}

}  // namespace

TEST_CASE("Labo survivors : la graine tire le MVP final parmi deux", "[sim][survivors]") {
  std::set<std::uint16_t> drawn;
  for (std::uint64_t seed = 0; seed < kSeedsTried; ++seed) {
    SurvivorsRun run = LabRun(seed);
    const auto bosses = run.field().content.bosses;
    REQUIRE_FALSE(bosses.empty());
    CHECK(bosses.back().final);
    drawn.insert(bosses.back().foe_id);
    // Le même tirage que ContentOf pour cette graine : le rejeu le retrouve.
    const ro::sim::ArenaRule* arena = ro::sim::FindArena(Rules(), kLabArenaId);
    REQUIRE(arena != nullptr);
    CHECK(ro::sim::ContentOf(Rules(), *arena, seed).bosses.data() == bosses.data());
  }
  CHECK(drawn == std::set<std::uint16_t>{kLordKnightSeyrenId, kHighWizardKathryneId});
}

TEST_CASE("Labo survivors : les cuves cèdent une fois, dans l'acide", "[sim][survivors]") {
  SurvivorsRun run = LabRun(kTankSeed);
  ro::sim::SurvivorsField& field = run.field_for_tests();
  const std::size_t tanks = FirstTanksEvent(field);
  const ro::sim::EventRule event = field.content.events[tanks];
  REQUIRE(event.foe_id == kEgnigemCeniaId);

  PlayEventNext(field, tanks);
  run.Step(ro::sim::SurvivorsInput{});
  const std::size_t first_burst = field.burst_tanks.size();
  // Le départ est entouré de salles : des cuves y cèdent, au plus `count`.
  CHECK(first_burst > 0);
  CHECK(first_burst <= event.count);
  // La vague en cours fait aussi naître des chercheuses : au moins une par cuve.
  CHECK(CountOf(field, event.foe_id) >= first_burst);
  const auto puddles = std::ranges::count_if(field.hazards, [](const ro::sim::SurvivorsHazard& h) {
    return h.kind == ro::sim::HazardKind::kPuddle;
  });
  CHECK(static_cast<std::size_t>(puddles) == first_burst);

  // L'événement rejoué : les cuves déjà rompues ne cèdent plus.
  PlayEventNext(field, tanks);
  run.Step(ro::sim::SurvivorsInput{});
  REQUIRE(field.burst_tanks.size() >= first_burst);
  std::set<std::pair<std::uint16_t, std::uint16_t>> corners;
  for (const ro::sim::CellPosition& corner : field.burst_tanks) {
    CHECK(corners.emplace(corner.x, corner.y).second);
  }
}
