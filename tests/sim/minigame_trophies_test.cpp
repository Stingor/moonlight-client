// LES TROPHÉES DES MINI-JEUX : gagnés une fois, meilleur score à la fin, deux
// enregistrements fondus en un.

#include "ro/sim/minigame_trophies.h"

#include <cmath>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/match3_game.h"
#include "ro/sim/peg_game.h"

using ro::sim::MinigameRecord;
using ro::sim::MinigameTrophy;
using ro::sim::Peg;
using ro::sim::PegGame;
using ro::sim::PegPoint;

namespace {

constexpr std::uint32_t kSeed = 11;
// Trois secondes de physique : la bille lâchée en haut est sortie.
constexpr int kStepsToLeave = static_cast<int>(3.0F / ro::sim::kPegStepS);

}  // namespace

TEST_CASE("le catalogue suit l'énumération, et ses clés sont uniques") {
  std::set<std::string_view> keys;
  std::size_t index = 0;
  for (const ro::sim::MinigameTrophyText& text : ro::sim::MinigameTrophies()) {
    CHECK(static_cast<std::size_t>(text.trophy) == index++);
    CHECK(keys.insert(text.key).second);
  }
}

TEST_CASE("une partie de Roggle gagnée donne son trophée une seule fois") {
  PegGame game(kSeed);
  game.SetPegs({Peg{.x = 100.0F, .y = 200.0F, .kind = ro::sim::PegKind::kOrange}});
  // Assez de billes pour en garder cinq en réserve après ce tir.
  game.SetBallsLeft(6);
  MinigameRecord record;
  CHECK(RecordRoggle(record, game).empty());
  game.PlaceBall(PegPoint{.x = 103.0F, .y = ro::sim::kPegCannonY}, PegPoint{});
  for (int step = 0; step < kStepsToLeave; ++step) {
    game.Advance(ro::sim::kPegStepS);
  }
  const std::vector<MinigameTrophy> fresh = RecordRoggle(record, game);
  CHECK(fresh ==
        std::vector<MinigameTrophy>{MinigameTrophy::kRoggleWon, MinigameTrophy::kRoggleFlawless});
  CHECK(record.roggle_best == game.score());
  CHECK(RecordRoggle(record, game).empty());
}

TEST_CASE("le défi du jour gagné garde son meilleur, qui repart à zéro le lendemain") {
  constexpr std::uint32_t kDay = 20396;
  MinigameRecord record;
  record.roggle_daily_day = kDay - 1;
  record.roggle_daily_best = 999999;
  PegGame game(kSeed);
  game.NewDailyGame(kDay);
  REQUIRE(game.daily());
  static_cast<void>(RecordRoggle(record, game));
  CHECK(record.roggle_daily_day == kDay - 1);
  // Des tirs droit vers le bas jusqu'à la fin, gagnée ou perdue.
  const int most_shots = 4 * game.balls_left();
  game.Aim(PegPoint{.x = ro::sim::kPegCannonX, .y = ro::sim::kPegBoardHeight});
  for (int shot = 0; shot < most_shots && game.phase() == ro::sim::PegPhase::kAiming; ++shot) {
    game.Launch();
    // Un tir dure au plus treize secondes de physique, ralenties ou non.
    constexpr int kMostFramesPerShot =
        static_cast<int>(13.0F / ro::sim::kPegStepS / ro::sim::kPegSlowScale);
    for (int frame = 0; frame < kMostFramesPerShot && game.phase() == ro::sim::PegPhase::kFlying;
         ++frame) {
      game.Advance(ro::sim::kPegStepS);
    }
  }
  REQUIRE((game.phase() == ro::sim::PegPhase::kWon || game.phase() == ro::sim::PegPhase::kLost));
  static_cast<void>(RecordRoggle(record, game));
  CHECK(record.roggle_daily_day == kDay);
  CHECK(record.roggle_daily_best == game.score());
}

TEST_CASE("les trophées de style de Roggle se gagnent pendant la partie") {
  // Un plot vert Magnum Break cerné de dix violets : un tir les allume tous.
  constexpr ro::sim::PegPoint kMiddle{.x = 260.0F, .y = 300.0F};
  constexpr int kRing = 10;
  constexpr float kRingRadius = 50.0F;
  std::vector<Peg> pegs;
  pegs.push_back(Peg{.x = kMiddle.x,
                     .y = kMiddle.y,
                     .kind = ro::sim::PegKind::kGreen,
                     .power = ro::sim::PegPower::kMagnumBreak});
  for (int index = 0; index < kRing; ++index) {
    const float angle = 6.2831853F * static_cast<float>(index) / static_cast<float>(kRing);
    pegs.push_back(Peg{.x = kMiddle.x + std::cos(angle) * kRingRadius,
                       .y = kMiddle.y + std::sin(angle) * kRingRadius,
                       .kind = ro::sim::PegKind::kPurple});
  }
  pegs.push_back(Peg{.x = 500.0F, .y = 150.0F, .kind = ro::sim::PegKind::kOrange});
  PegGame game(kSeed);
  game.SetPegs(pegs);
  game.PlaceBall(PegPoint{.x = kMiddle.x + 3.0F, .y = kMiddle.y - 16.0F}, PegPoint{});
  game.Advance(ro::sim::kPegStepS);
  MinigameRecord record;
  const std::vector<MinigameTrophy> fresh = RecordRoggle(record, game);
  CHECK(fresh == std::vector<MinigameTrophy>{MinigameTrophy::kRoggleFreeBall});
  CHECK(game.free_balls() == 1);
}

TEST_CASE("les deux pouvoirs d'un plateau suffisent à Arsenal") {
  // Le Magnum Break du premier vert allume le second, qui déclenche le sien.
  constexpr ro::sim::PegPoint kMiddle{.x = 260.0F, .y = 300.0F};
  std::vector<Peg> pegs;
  pegs.push_back(Peg{.x = kMiddle.x,
                     .y = kMiddle.y,
                     .kind = ro::sim::PegKind::kGreen,
                     .power = ro::sim::PegPower::kMagnumBreak});
  pegs.push_back(Peg{.x = kMiddle.x + ro::sim::kPegMagnumRadius * 0.5F,
                     .y = kMiddle.y,
                     .kind = ro::sim::PegKind::kGreen,
                     .power = ro::sim::PegPower::kSafetyWall});
  pegs.push_back(Peg{.x = 500.0F, .y = 150.0F, .kind = ro::sim::PegKind::kOrange});
  PegGame game(kSeed);
  game.SetPegs(pegs);
  game.PlaceBall(PegPoint{.x = kMiddle.x - 3.0F, .y = kMiddle.y - 16.0F}, PegPoint{});
  game.Advance(ro::sim::kPegStepS);
  REQUIRE(game.powers_used() == 2);
  MinigameRecord record;
  static_cast<void>(RecordRoggle(record, game));
  CHECK(record.Has(MinigameTrophy::kRoggleBothPowers));
}

TEST_CASE("le meilleur score n'est inscrit qu'à la fin de la partie") {
  ro::sim::Match3Game game(kSeed);
  game.NewGame(0);
  MinigameRecord record;
  static_cast<void>(RecordRojeweled(record, game));
  CHECK(record.rojeweled_best == 0);
  CHECK_FALSE(record.Has(MinigameTrophy::kRojeweledFinished));
}

TEST_CASE("deux enregistrements se fondent sans rien perdre ni doubler") {
  MinigameRecord into;
  into.rojeweled_best = 500;
  into.roggle_best = 10;
  into.unlocked[static_cast<std::size_t>(MinigameTrophy::kRojeweledFinished)] = true;
  into.foreign_trophy_keys = {"demain"};
  MinigameRecord from;
  from.rojeweled_best = 100;
  from.roggle_best = 300;
  from.unlocked[static_cast<std::size_t>(MinigameTrophy::kRoggleWon)] = true;
  from.foreign_trophy_keys = {"demain", "apres_demain"};

  ro::sim::MergeMinigameRecord(into, from);
  CHECK(into.rojeweled_best == 500);
  CHECK(into.roggle_best == 300);
  CHECK(into.Has(MinigameTrophy::kRojeweledFinished));
  CHECK(into.Has(MinigameTrophy::kRoggleWon));
  CHECK_FALSE(into.Has(MinigameTrophy::kRoggleFlawless));
  CHECK(into.foreign_trophy_keys == std::vector<std::string>{"demain", "apres_demain"});
}
