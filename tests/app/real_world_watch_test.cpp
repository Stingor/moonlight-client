// LE MONDE RÉEL PENDANT UNE PARTIE DE SURVIVORS : la garde d'entrée (chaque
// raison seule, puis leur ordre), le seuil des monstres à la borne, chaque
// alerte par sa seule différence, et la posture rendue à la sortie.

#include "survivors/real_world_watch.h"

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "survivors/posture_plan.h"

using moonclient::BannerText;
using moonclient::EntryGuardFacts;
using moonclient::EntryRefusal;
using moonclient::GroundCell;
using moonclient::Judge;
using moonclient::kEntryRefusalCount;
using moonclient::kHostileGuardCells;
using moonclient::kRealWorldAlertCount;
using moonclient::NearestCells;
using moonclient::Pauses;
using moonclient::PosturePlan;
using moonclient::RealWorldAlert;
using moonclient::RealWorldFacts;
using moonclient::RealWorldReport;
using moonclient::RealWorldWatch;
using moonclient::RefusalText;

namespace {

// Un personnage en ville, seul et libre : la partie peut commencer.
EntryGuardFacts ShelteredFacts() {
  EntryGuardFacts facts;
  facts.on_town_map = true;
  return facts;
}

// Chaque raison, et le fait qui la provoque seul, dans l'ordre de Judge.
struct RefusalCase {
  EntryRefusal refusal;
  std::function<void(EntryGuardFacts&)> provoke;
};
const std::array<RefusalCase, kEntryRefusalCount>& RefusalCases() {
  static const std::array<RefusalCase, kEntryRefusalCount> cases{{
      {EntryRefusal::kLinkClosed, [](EntryGuardFacts& facts) { facts.link_closed = true; }},
      {EntryRefusal::kLoading, [](EntryGuardFacts& facts) { facts.loading = true; }},
      {EntryRefusal::kDead, [](EntryGuardFacts& facts) { facts.dead = true; }},
      {EntryRefusal::kNotSheltered, [](EntryGuardFacts& facts) { facts.on_town_map = false; }},
      {EntryRefusal::kHostileNearby,
       [](EntryGuardFacts& facts) { facts.nearest_hostile_cells = kHostileGuardCells - 1; }},
      {EntryRefusal::kTradeOpen, [](EntryGuardFacts& facts) { facts.trade_open = true; }},
      {EntryRefusal::kNpcDialogueOpen,
       [](EntryGuardFacts& facts) { facts.npc_dialogue_open = true; }},
      {EntryRefusal::kShopOpen, [](EntryGuardFacts& facts) { facts.shop_open = true; }},
      {EntryRefusal::kStorageOpen, [](EntryGuardFacts& facts) { facts.storage_open = true; }},
  }};
  return cases;
}

// Un personnage en ligne, debout, en bonne santé.
RealWorldFacts CalmWorld() {
  RealWorldFacts facts;
  constexpr std::uint32_t kCalmStrikes = 7;
  constexpr std::uint32_t kCalmMapEntries = 3;
  constexpr std::uint32_t kCalmWhispers = 5;
  facts.strikes_taken = kCalmStrikes;
  facts.map_entries = kCalmMapEntries;
  facts.whispers = kCalmWhispers;
  return facts;
}

// Chaque alerte et la seule différence qui la déclenche.
struct AlertCase {
  RealWorldAlert alert;
  std::function<void(RealWorldFacts&)> provoke;
};
std::vector<AlertCase> AlertCases() {
  return {
      {RealWorldAlert::kLinkLost, [](RealWorldFacts& facts) { facts.link_open = false; }},
      {RealWorldAlert::kDied, [](RealWorldFacts& facts) { facts.dead = true; }},
      {RealWorldAlert::kHit, [](RealWorldFacts& facts) { facts.strikes_taken += 1; }},
      {RealWorldAlert::kMoved, [](RealWorldFacts& facts) { facts.map_entries += 1; }},
      {RealWorldAlert::kTrade, [](RealWorldFacts& facts) { facts.trade_offered = true; }},
      {RealWorldAlert::kInvite, [](RealWorldFacts& facts) { facts.party_invited = true; }},
      {RealWorldAlert::kInvite, [](RealWorldFacts& facts) { facts.guild_invited = true; }},
      {RealWorldAlert::kInvite, [](RealWorldFacts& facts) { facts.friend_invited = true; }},
      {RealWorldAlert::kDialogue, [](RealWorldFacts& facts) { facts.server_dialogue_open = true; }},
      {RealWorldAlert::kWhisper, [](RealWorldFacts& facts) { facts.whispers += 1; }},
  };
}

}  // namespace

TEST_CASE("la garde d'entrée laisse passer en ville comme en intérieur", "[app][survivors]") {
  CHECK_FALSE(Judge(ShelteredFacts()).has_value());
  EntryGuardFacts indoor;
  indoor.on_indoor_map = true;
  CHECK_FALSE(Judge(indoor).has_value());
}

TEST_CASE("chaque raison de refus se déclenche seule", "[app][survivors]") {
  for (const RefusalCase& refusal_case : RefusalCases()) {
    EntryGuardFacts facts = ShelteredFacts();
    refusal_case.provoke(facts);
    INFO("raison " << static_cast<int>(refusal_case.refusal));
    CHECK(Judge(facts) == refusal_case.refusal);
  }
}

TEST_CASE("les raisons combinées rendent la première dans l'ordre fixe", "[app][survivors]") {
  const auto& cases = RefusalCases();
  // Toutes les raisons à partir de la k-ième : c'est la k-ième qui est dite.
  for (std::size_t first = 0; first < cases.size(); ++first) {
    EntryGuardFacts facts = ShelteredFacts();
    for (std::size_t later = first; later < cases.size(); ++later) cases[later].provoke(facts);
    INFO("première raison " << first);
    CHECK(Judge(facts) == cases[first].refusal);
  }
}

TEST_CASE("le seuil des monstres hostiles à la borne", "[app][survivors]") {
  EntryGuardFacts facts = ShelteredFacts();
  facts.nearest_hostile_cells = kHostileGuardCells - 1;
  CHECK(Judge(facts) == EntryRefusal::kHostileNearby);
  facts.nearest_hostile_cells = kHostileGuardCells;
  CHECK_FALSE(Judge(facts).has_value());
  facts.nearest_hostile_cells = 0;
  CHECK(Judge(facts) == EntryRefusal::kHostileNearby);
}

TEST_CASE("la distance aux monstres est le plus grand des deux écarts", "[app][survivors]") {
  constexpr GroundCell kHero{.x = 100, .y = 100};
  CHECK_FALSE(NearestCells(kHero, {}).has_value());
  const std::array<GroundCell, 3> hostiles{{
      {.x = 120, .y = 100},  // 20 cases à l'est
      {.x = 87, .y = 110},   // 13 à l'ouest, 10 au nord : 13
      {.x = 90, .y = 86},    // 10 à l'ouest, 14 au sud : 14
  }};
  constexpr std::uint32_t kNearestCells = 13;
  CHECK(NearestCells(kHero, hostiles) == kNearestCells);
}

TEST_CASE("chaque raison de refus a son texte", "[app][survivors]") {
  for (std::uint8_t index = 0; index < kEntryRefusalCount; ++index) {
    const char* text = RefusalText(static_cast<EntryRefusal>(index));
    INFO("raison " << static_cast<int>(index));
    REQUIRE(text != nullptr);
    CHECK_FALSE(std::string_view(text).empty());
  }
}

TEST_CASE("chaque alerte qui met en pause a son bandeau", "[app][survivors]") {
  for (std::uint8_t index = 0; index < kRealWorldAlertCount; ++index) {
    const auto alert = static_cast<RealWorldAlert>(index);
    INFO("alerte " << static_cast<int>(index));
    CHECK((BannerText(alert) != nullptr) == Pauses(alert));
  }
  CHECK_FALSE(Pauses(RealWorldAlert::kWhisper));
  CHECK_FALSE(Pauses(RealWorldAlert::kLinkLost));
}

TEST_CASE("chaque alerte se déclenche par sa seule différence", "[app][survivors]") {
  const RealWorldFacts previous = CalmWorld();
  for (const AlertCase& alert_case : AlertCases()) {
    RealWorldFacts now = previous;
    alert_case.provoke(now);
    const RealWorldReport report = RealWorldWatch::Compare(previous, now);
    INFO("alerte " << static_cast<int>(alert_case.alert));
    CHECK(report.alerts == std::vector<RealWorldAlert>{alert_case.alert});
    CHECK(report.pauses() == Pauses(alert_case.alert));
    CHECK(report.link_lost() == (alert_case.alert == RealWorldAlert::kLinkLost));
  }
}

TEST_CASE("deux relevés égaux ne disent rien", "[app][survivors]") {
  const RealWorldFacts calm = CalmWorld();
  const RealWorldReport report = RealWorldWatch::Compare(calm, calm);
  CHECK(report.alerts.empty());
  CHECK(report.new_whispers == 0);
  CHECK_FALSE(report.banner().has_value());

  // Un état qui dure n'alerte qu'à son passage à vrai.
  RealWorldFacts lasting = calm;
  lasting.trade_offered = true;
  lasting.server_dialogue_open = true;
  CHECK(RealWorldWatch::Compare(lasting, lasting).alerts.empty());
}

TEST_CASE("les messages privés n'incrémentent que le compteur", "[app][survivors]") {
  const RealWorldFacts previous = CalmWorld();
  RealWorldFacts now = previous;
  constexpr std::uint32_t kArrived = 3;
  now.whispers += kArrived;
  const RealWorldReport report = RealWorldWatch::Compare(previous, now);
  CHECK(report.new_whispers == kArrived);
  CHECK_FALSE(report.pauses());
  CHECK_FALSE(report.banner().has_value());
}

TEST_CASE("la mort ne compte pas aussi comme un coup", "[app][survivors]") {
  const RealWorldFacts previous = CalmWorld();
  RealWorldFacts now = previous;
  now.strikes_taken += 1;
  now.dead = true;
  const RealWorldReport report = RealWorldWatch::Compare(previous, now);
  CHECK(report.alerts == std::vector<RealWorldAlert>{RealWorldAlert::kDied});
  CHECK(report.banner() == RealWorldAlert::kDied);
}

TEST_CASE("le bandeau dit l'alerte la plus grave de l'image", "[app][survivors]") {
  const RealWorldFacts previous = CalmWorld();
  RealWorldFacts now = previous;
  now.whispers += 1;
  now.trade_offered = true;
  now.strikes_taken += 1;
  const RealWorldReport report = RealWorldWatch::Compare(previous, now);
  CHECK(report.alerts == std::vector<RealWorldAlert>{RealWorldAlert::kHit, RealWorldAlert::kTrade,
                                                     RealWorldAlert::kWhisper});
  CHECK(report.banner() == RealWorldAlert::kHit);
}

TEST_CASE("la veille compare chaque relevé au précédent", "[app][survivors]") {
  RealWorldWatch watch;
  RealWorldFacts facts = CalmWorld();
  watch.Start(facts);
  CHECK(watch.Observe(facts).alerts.empty());
  facts.strikes_taken += 1;
  CHECK(watch.Observe(facts).alerts == std::vector<RealWorldAlert>{RealWorldAlert::kHit});
  // Le coup est déjà dit : un compteur qui ne bouge plus ne l'alerte plus.
  CHECK(watch.Observe(facts).alerts.empty());
}

TEST_CASE("assis à l'entrée seulement si le personnage était debout", "[app][survivors]") {
  PosturePlan standing;
  CHECK(standing.SitOnEntry(false));
  CHECK(standing.StandOnExit(true));

  PosturePlan already_seated;
  CHECK_FALSE(already_seated.SitOnEntry(true));
  CHECK_FALSE(already_seated.StandOnExit(true));
}

TEST_CASE("relevé à la sortie seulement si c'est nous qui l'avons assis", "[app][survivors]") {
  // Le jeu l'a relevé pendant la partie : rien à refaire.
  PosturePlan stood_by_game;
  CHECK(stood_by_game.SitOnEntry(false));
  CHECK_FALSE(stood_by_game.StandOnExit(false));

  // Le plan s'oublie à la sortie : une seconde sortie ne relève rien.
  PosturePlan twice;
  CHECK(twice.SitOnEntry(false));
  CHECK(twice.StandOnExit(true));
  CHECK_FALSE(twice.StandOnExit(true));
  CHECK_FALSE(twice.seated_by_us());
}
