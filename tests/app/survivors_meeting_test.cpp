// LA RENCONTRE : qui attend, où, et ce que le héros qui le rejoint obtient. Rien
// n'y passe par la simulation : la même partie joue pas pour pas avec ou sans elle.

#include "survivors/survivors_meeting.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/sim/survivors_content.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"

#include "survivors/survivors_book.h"
#include "survivors/survivors_save.h"
#include "survivors/survivors_stage.h"

namespace {

using moonclient::SurvivorsBoardKind;
using moonclient::SurvivorsMeeting;
using moonclient::SurvivorsRunBoard;
using moonclient::SurvivorsSave;

constexpr std::string_view kDesertKey = "morroc_desert";
constexpr std::uint64_t kSeed = 11;
constexpr std::uint64_t kMetMs = 90'000;

[[nodiscard]] const ro::sim::SurvivorsRules& Rules() { return ro::sim::ShippedSurvivorsRules(); }

[[nodiscard]] const ro::sim::ArenaRule& ArenaNamed(std::string_view key) {
  const auto found = std::ranges::find_if(
      Rules().arenas, [key](const ro::sim::ArenaRule& arena) { return key == arena.key; });
  REQUIRE(found != Rules().arenas.end());
  return *found;
}

[[nodiscard]] ro::sim::SurvivorsStart PlainsStart() {
  const ro::sim::ClassRule& mage = Rules().classes.front();
  return ro::sim::SurvivorsStart{.seed = kSeed,
                                 .class_id = mage.class_id,
                                 .arena_id = ArenaNamed(moonclient::kMeetingArenaKey).arena_id,
                                 .first_skill_id = mage.starter_skills.front(),
                                 .loadout = {}};
}

}  // namespace

TEST_CASE("Rencontre : le Voleur n'attend qu'en partie libre, seul, sur la prairie, et verrouillé",
          "[app][survivors]") {
  const ro::sim::ArenaRule& plains = ArenaNamed(moonclient::kMeetingArenaKey);
  const SurvivorsSave fresh;
  const ro::sim::ClassRule* awaiting =
      moonclient::ClassAwaitingMeeting(Rules(), plains, SurvivorsRunBoard{}, fresh);
  REQUIRE(awaiting != nullptr);
  CHECK(std::string_view(awaiting->key) == "thief");

  CHECK(moonclient::ClassAwaitingMeeting(Rules(), plains,
                                         SurvivorsRunBoard{.kind = SurvivorsBoardKind::kChallenge},
                                         fresh) == nullptr);
  CHECK(moonclient::ClassAwaitingMeeting(Rules(), plains,
                                         SurvivorsRunBoard{.kind = SurvivorsBoardKind::kRanked},
                                         fresh) == nullptr);
  CHECK(moonclient::ClassAwaitingMeeting(Rules(), plains, SurvivorsRunBoard{.coop = true}, fresh) ==
        nullptr);
  // Dans le désert, c'est l'Archer qui guette.
  const ro::sim::ClassRule* in_desert =
      moonclient::ClassAwaitingMeeting(Rules(), ArenaNamed(kDesertKey), SurvivorsRunBoard{}, fresh);
  REQUIRE(in_desert != nullptr);
  CHECK(std::string_view(in_desert->key) == "archer");

  // L'exploit du Chevalier n'y change rien ; le sien, si.
  SurvivorsSave knight_held = fresh;
  knight_held.album.feats = {"ice_wall"};
  CHECK(moonclient::ClassAwaitingMeeting(Rules(), plains, SurvivorsRunBoard{}, knight_held) ==
        awaiting);
  SurvivorsSave met = fresh;
  moonclient::InscribeFeat(met, moonclient::kMeetingFeat);
  CHECK(moonclient::ClassAwaitingMeeting(Rules(), plains, SurvivorsRunBoard{}, met) == nullptr);
  // Rejoindre le Voleur ne débloque pas l'Archer.
  CHECK(moonclient::ClassAwaitingMeeting(Rules(), ArenaNamed(kDesertKey), SurvivorsRunBoard{},
                                         met) == in_desert);
  moonclient::InscribeFeat(met, moonclient::kArcherMeetingFeat);
  CHECK(moonclient::ClassAwaitingMeeting(Rules(), ArenaNamed(kDesertKey), SurvivorsRunBoard{},
                                         met) == nullptr);
}

TEST_CASE("Rencontre : sa place est fixe depuis le départ, et praticable", "[app][survivors]") {
  auto run = ro::sim::SurvivorsRun::Start(Rules(), PlainsStart());
  REQUIRE(run.has_value());
  const ro::sim::SurvivorsField& field = run->field();
  const ro::sim::FieldPoint start = field.hero.position;

  const moonclient::SurvivorsMeetingRule& thief = *moonclient::MeetingOfClass("thief");
  const std::optional<ro::sim::FieldPoint> place =
      moonclient::MeetingPlaceOf(field.ground, start, thief);
  REQUIRE(place.has_value());
  CHECK(place == moonclient::MeetingPlaceOf(field.ground, start, thief));
  CHECK(field.ground.Passable(place->x / ro::sim::kSubunitsPerCell,
                              place->y / ro::sim::kSubunitsPerCell));
  // Près de son décalage, à 150 cases du départ.
  const ro::sim::FieldPoint wanted = start + ro::sim::FieldPoint{ro::sim::Cells(thief.east_cells),
                                                                 ro::sim::Cells(thief.north_cells)};
  const auto margin_units = ro::sim::Cells(moonclient::kMeetingSearchCells);
  CHECK(ro::sim::DistanceSquared(*place, wanted) <= 2 * std::int64_t{margin_units} * margin_units);

  // La chercher ne touche pas au sol de la partie.
  const std::uint64_t generated = field.ground.chunks_generated();
  static_cast<void>(moonclient::MeetingPlaceOf(field.ground, start, thief));
  CHECK(field.ground.chunks_generated() == generated);

  // Un sol borné n'a pas de prairie où attendre.
  CHECK_FALSE(moonclient::MeetingPlaceOf(ro::sim::SurvivorsGround{}, start, thief).has_value());
}

TEST_CASE("Rencontre : à deux cases le héros le rejoint, une fois, puis il s'efface",
          "[app][survivors]") {
  constexpr ro::sim::FieldPoint kPlace = ro::sim::CellCenter(100, 100);
  SurvivorsMeeting meeting;
  CHECK_FALSE(meeting.BodyAt(0).has_value());
  CHECK_FALSE(meeting.Follow(kPlace, 0, 0));

  meeting.Place(kPlace, 0);
  CHECK(meeting.waiting_at() == kPlace);
  const ro::sim::FieldPoint far =
      kPlace + ro::sim::FieldPoint{ro::sim::Cells(moonclient::kMeetingReachCells) + 1, 0};
  CHECK_FALSE(meeting.Follow(far, kMetMs, kMetMs));
  CHECK(meeting.waiting_at().has_value());
  CHECK(meeting.BodyAt(kMetMs)->opacity == Catch::Approx(1.0F));
  CHECK_FALSE(meeting.TellingAt(kMetMs));

  const ro::sim::FieldPoint near =
      kPlace + ro::sim::FieldPoint{ro::sim::Cells(moonclient::kMeetingReachCells), 0};
  CHECK(meeting.Follow(near, kMetMs, kMetMs));
  CHECK_FALSE(meeting.Follow(near, kMetMs, kMetMs + 1));
  CHECK_FALSE(meeting.waiting_at().has_value());
  CHECK(meeting.TellingAt(kMetMs));
  CHECK(meeting.TellingAt(kMetMs + moonclient::kMeetingNoticeMs - 1));
  CHECK_FALSE(meeting.TellingAt(kMetMs + moonclient::kMeetingNoticeMs));
  // Le corps s'efface, puis il n'est plus là.
  REQUIRE(meeting.BodyAt(kMetMs + moonclient::kMeetingFarewellMs / 2).has_value());
  CHECK(meeting.BodyAt(kMetMs + moonclient::kMeetingFarewellMs / 2)->opacity ==
        Catch::Approx(0.5F));
  CHECK_FALSE(meeting.BodyAt(kMetMs + moonclient::kMeetingFarewellMs).has_value());

  // Une partie neuve l'oublie.
  meeting.Clear();
  CHECK_FALSE(meeting.TellingAt(kMetMs));
  CHECK_FALSE(meeting.BodyAt(kMetMs).has_value());
}

TEST_CASE("Rencontre : l'Archer repart à 5:00 de partie, rejoint ou non", "[app][survivors]") {
  constexpr ro::sim::FieldPoint kPlace = ro::sim::CellCenter(100, 100);
  const moonclient::SurvivorsMeetingRule& archer = *moonclient::MeetingOfClass("archer");
  CHECK(archer.arena_key == moonclient::kArcherMeetingArenaKey);
  CHECK(archer.leaves_at_run_ms == moonclient::kArcherLeavesAtRunMs);
  SurvivorsMeeting meeting;
  meeting.Place(kPlace, archer.leaves_at_run_ms);
  // Une seconde avant, il attend ; à l'heure dite, il part, même à portée.
  constexpr std::uint64_t kSecondMs = 1000;
  const ro::sim::FieldPoint far =
      kPlace + ro::sim::FieldPoint{ro::sim::Cells(moonclient::kMeetingReachCells) + 1, 0};
  CHECK_FALSE(meeting.Follow(far, archer.leaves_at_run_ms - kSecondMs, kMetMs));
  CHECK(meeting.waiting_at().has_value());
  CHECK_FALSE(meeting.Follow(kPlace, archer.leaves_at_run_ms, kMetMs));
  CHECK_FALSE(meeting.waiting_at().has_value());
  CHECK_FALSE(meeting.TellingAt(kMetMs));
  CHECK_FALSE(meeting.Follow(kPlace, archer.leaves_at_run_ms, kMetMs + 1));
  // Il s'efface comme un rejoint, sans ligne.
  CHECK(meeting.BodyAt(kMetMs + moonclient::kMeetingFarewellMs / 2).has_value());
  CHECK_FALSE(meeting.BodyAt(kMetMs + moonclient::kMeetingFarewellMs).has_value());
}
