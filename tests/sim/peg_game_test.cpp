// ROGGLE, côté règles : chute, rebond, plots, victoire, défaite, visée,
// plateaux, pouvoirs, style et finale.

#include "ro/sim/peg_game.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using ro::sim::Peg;
using ro::sim::PegGame;
using ro::sim::PegKind;
using ro::sim::PegLayout;
using ro::sim::PegPhase;
using ro::sim::PegPoint;
using ro::sim::PegPower;

namespace {

constexpr std::uint32_t kSeed = 3;
// Bien assez pour qu'une bille lâchée en haut sorte par le bas.
constexpr float kLongEnoughS = 3.0F;
// Une colonne où le seau, parti du milieu vers la droite, n'est pas quand la
// bille arrive.
constexpr float kAwayFromBucketX = 100.0F;
// Un lâcher pile au-dessus d'un plot rebondirait à la verticale jusqu'à la
// limite de temps : on le décale d'un rien, pour que la bille glisse.
constexpr float kGlancingX = 3.0F;

// ⚠ PAS FIXE PAR PAS FIXE : un seul `Advance` long ne rattrape qu'un quart de
// seconde, comme après un gel de l'affichage.
void RunFor(PegGame& game, float seconds) {
  const int steps = static_cast<int>(seconds / ro::sim::kPegStepS);
  for (int step = 0; step < steps; ++step) {
    game.Advance(ro::sim::kPegStepS);
  }
}

// Lâche une bille immobile au-dessus de `x`, et la laisse sortir.
void DropAt(PegGame& game, float x) {
  game.PlaceBall(PegPoint{.x = x + kGlancingX, .y = ro::sim::kPegCannonY}, PegPoint{});
  RunFor(game, kLongEnoughS);
}

}  // namespace

TEST_CASE("les billes d'une partie suivent le nombre de plots du plateau") {
  STATIC_CHECK(ro::sim::PegStartBalls(0) == ro::sim::kPegFewestStartBalls);
  STATIC_CHECK(ro::sim::PegStartBalls(30) == ro::sim::kPegFewestStartBalls);
  STATIC_CHECK(ro::sim::PegStartBalls(41) == 5);
  STATIC_CHECK(ro::sim::PegStartBalls(50) == 5);
  STATIC_CHECK(ro::sim::PegStartBalls(81) == 9);
  PegGame game(kSeed);
  for (const PegLayout layout : {PegLayout::kScattered, PegLayout::kPyramid, PegLayout::kRings,
                                 PegLayout::kPoring, PegLayout::kWaves}) {
    game.NewGame(layout);
    CHECK(game.balls_left() == ro::sim::PegStartBalls(static_cast<int>(game.pegs().size())));
  }
  // Le plateau le plus petit donne moins de billes que le plus grand.
  game.NewGame(PegLayout::kRings);
  const int few_balls = game.balls_left();
  game.NewGame(PegLayout::kWaves);
  CHECK(few_balls < game.balls_left());
}

TEST_CASE("une bille qui ne touche rien est perdue") {
  PegGame game(kSeed);
  game.SetPegs({Peg{.x = 500.0F, .y = 150.0F, .kind = ro::sim::PegKind::kOrange}});
  DropAt(game, kAwayFromBucketX);
  CHECK(game.phase() == PegPhase::kAiming);
  CHECK(game.balls_left() == ro::sim::kPegFewestStartBalls - 1);
  CHECK(game.score() == 0);
}

TEST_CASE("un plot touché s'allume, rapporte, puis s'efface à la sortie") {
  PegGame game(kSeed);
  game.SetPegs({Peg{.x = kAwayFromBucketX, .y = 200.0F},
                Peg{.x = 500.0F, .y = 150.0F, .kind = ro::sim::PegKind::kOrange}});
  game.PlaceBall(PegPoint{.x = kAwayFromBucketX + kGlancingX, .y = ro::sim::kPegCannonY},
                 PegPoint{});
  RunFor(game, 1.0F);
  CHECK(game.pegs()[0].lit);
  CHECK(game.score() == ro::sim::kPegBluePoints);
  CHECK(game.TakeNews().blue_hits == 1);
  RunFor(game, kLongEnoughS);
  CHECK(game.pegs()[0].removed);
  CHECK_FALSE(game.pegs()[0].lit);
}

TEST_CASE("le dernier orange effacé gagne, et chaque bille restante rapporte") {
  PegGame game(kSeed);
  game.SetPegs({Peg{.x = kAwayFromBucketX, .y = 200.0F, .kind = ro::sim::PegKind::kOrange}});
  DropAt(game, kAwayFromBucketX);
  CHECK(game.phase() == PegPhase::kWon);
  CHECK(game.orange_left() == 0);
  // LE DERNIER ORANGE OUVRE LA FINALE : la bille tombe dans une case.
  CHECK(game.finale());
  const int spare = ro::sim::kPegFewestStartBalls - 1;
  const int bin = game.score() - ro::sim::kPegOrangePoints - spare * ro::sim::kPegBallLeftPoints;
  CHECK(std::ranges::find(ro::sim::kPegFinaleBins, bin) != ro::sim::kPegFinaleBins.end());
  const ro::sim::PegNews news = game.TakeNews();
  CHECK(news.finished);
  CHECK(std::ranges::count_if(news.callouts, [](const ro::sim::PegCallout& callout) {
          return callout.kind == ro::sim::PegCalloutKind::kBin;
        }) == 1);
}

TEST_CASE("sans bille, la partie est perdue") {
  PegGame game(kSeed);
  game.SetPegs({Peg{.x = 500.0F, .y = 150.0F, .kind = ro::sim::PegKind::kOrange}});
  // ⚠ LE SEAU PEUT RENDRE UNE BILLE en passant : on lâche jusqu'à la fin.
  constexpr int kMostDrops = 4 * ro::sim::kPegFewestStartBalls;
  for (int drop = 0; drop < kMostDrops && game.phase() == PegPhase::kAiming; ++drop) {
    DropAt(game, kAwayFromBucketX);
  }
  CHECK(game.phase() == PegPhase::kLost);
  CHECK(game.balls_left() == 0);
}

TEST_CASE("le multiplicateur monte avec la part d'orange effacée") {
  // Deux plots côte à côte, que la bille lâchée entre eux touche ensemble ;
  // elle y reste coincée, jusqu'à ce que la garde les efface, bien avant la
  // limite de temps d'un tir.
  constexpr float kLeftX = 100.0F;
  constexpr float kRightX = 121.0F;
  constexpr float kPegsY = 200.0F;
  constexpr float kUnstuckAndOutS = 4.0F;
  PegGame game(kSeed);
  std::vector<Peg> pegs;
  pegs.push_back(Peg{.x = kLeftX, .y = kPegsY, .kind = ro::sim::PegKind::kOrange});
  pegs.push_back(Peg{.x = kRightX, .y = kPegsY, .kind = ro::sim::PegKind::kOrange});
  pegs.push_back(Peg{.x = 450.0F, .y = 150.0F, .kind = ro::sim::PegKind::kOrange});
  pegs.push_back(Peg{.x = 500.0F, .y = 150.0F, .kind = ro::sim::PegKind::kOrange});
  game.SetPegs(pegs);
  CHECK(game.multiplier() == 1);
  game.PlaceBall(PegPoint{.x = (kLeftX + kRightX) * 0.5F, .y = ro::sim::kPegCannonY}, PegPoint{});
  RunFor(game, kUnstuckAndOutS);
  CHECK(game.phase() == PegPhase::kAiming);
  CHECK(game.orange_left() == 2);
  CHECK(game.multiplier() == 2);
}

TEST_CASE("un tir visé vers le haut part quand même vers le bas") {
  PegGame game(kSeed);
  game.SetPegs({});
  game.Aim(PegPoint{.x = ro::sim::kPegCannonX + 100.0F, .y = 0.0F});
  game.Launch();
  game.Advance(ro::sim::kPegStepS);
  CHECK(game.ball().y > ro::sim::kPegCannonY);
  CHECK(game.ball().x > ro::sim::kPegCannonX);
}

TEST_CASE("le guide d'un tir vertical descend droit sous le canon") {
  PegGame game(kSeed);
  game.SetPegs({});
  game.Aim(PegPoint{.x = ro::sim::kPegCannonX, .y = ro::sim::kPegBoardHeight});
  const std::vector<PegPoint> guide = game.AimGuide();
  REQUIRE(guide.size() >= 2);
  CHECK(guide.front().x == Catch::Approx(ro::sim::kPegCannonX));
  CHECK(guide.back().y > guide.front().y);
}

TEST_CASE("la physique ne dépend pas de la cadence d'affichage") {
  PegGame slow(kSeed);
  PegGame fast(kSeed);
  for (PegGame* game : {&slow, &fast}) {
    game->SetPegs({Peg{.x = 250.0F, .y = 300.0F}, Peg{.x = 290.0F, .y = 380.0F}});
    game->Aim(PegPoint{.x = 250.0F, .y = 300.0F});
    game->Launch();
  }
  constexpr int kSlowFrames = 30;
  constexpr int kFastPerSlow = 8;
  for (int frame = 0; frame < kSlowFrames; ++frame) {
    slow.Advance(kFastPerSlow * ro::sim::kPegStepS);
    for (int step = 0; step < kFastPerSlow; ++step) {
      fast.Advance(ro::sim::kPegStepS);
    }
  }
  CHECK(slow.ball().x == Catch::Approx(fast.ball().x));
  CHECK(slow.ball().y == Catch::Approx(fast.ball().y));
}

TEST_CASE("une partie neuve a des plots, dont un quart d'orange") {
  PegGame game(kSeed);
  game.NewGame();
  REQUIRE_FALSE(game.pegs().empty());
  CHECK(game.orange_left() == static_cast<int>(game.pegs().size() / 4));
  CHECK(game.phase() == PegPhase::kAiming);
}

TEST_CASE("chaque plateau tient dans le cadre, sans plots qui se chevauchent") {
  for (int layout = 0; layout < ro::sim::kPegLayoutCount; ++layout) {
    CAPTURE(layout);
    std::mt19937 random(kSeed);
    const std::vector<Peg> pegs = BuildPegLayout(static_cast<PegLayout>(layout), random);
    REQUIRE(pegs.size() >= 8);
    for (std::size_t index = 0; index < pegs.size(); ++index) {
      const Peg& peg = pegs[index];
      CHECK(peg.x >= ro::sim::kPegRadius);
      CHECK(peg.x <= ro::sim::kPegBoardWidth - ro::sim::kPegRadius);
      CHECK(peg.y > ro::sim::kPegCannonY + ro::sim::kPegRadius);
      CHECK(peg.y < ro::sim::kPegBucketY - ro::sim::kPegBucketHeight);
      for (std::size_t other = index + 1; other < pegs.size(); ++other) {
        const float dx = pegs[other].x - peg.x;
        const float dy = pegs[other].y - peg.y;
        CHECK(dx * dx + dy * dy >= 4.0F * ro::sim::kPegRadius * ro::sim::kPegRadius);
      }
    }
    const auto count = [&pegs](PegKind kind) {
      return std::ranges::count_if(pegs, [kind](const Peg& peg) { return peg.kind == kind; });
    };
    CHECK(count(PegKind::kOrange) == static_cast<std::ptrdiff_t>(pegs.size() / 4));
    CHECK(count(PegKind::kGreen) == 2);
    CHECK(std::ranges::all_of(pegs, [](const Peg& peg) {
      return (peg.kind == PegKind::kGreen) == (peg.power != PegPower::kNone);
    }));
  }
}

TEST_CASE("un violet par plateau, qui change de place après chaque tir") {
  PegGame game(kSeed);
  game.NewGame(PegLayout::kPyramid);
  const auto purples = [&game] {
    return std::ranges::count_if(game.pegs(),
                                 [](const Peg& peg) { return peg.kind == PegKind::kPurple; });
  };
  CHECK(purples() == 1);
  game.Aim(PegPoint{.x = ro::sim::kPegCannonX, .y = ro::sim::kPegBoardHeight});
  game.Launch();
  // La limite de temps d'un tir garantit sa fin.
  RunFor(game, 13.0F);
  REQUIRE(game.phase() == PegPhase::kAiming);
  CHECK(purples() == 1);
}

TEST_CASE("le défi du jour donne la même partie à tous") {
  constexpr std::uint32_t kDay = 20396;
  PegGame one(1);
  PegGame other(2);
  one.NewDailyGame(kDay);
  other.NewDailyGame(kDay);
  REQUIRE(one.pegs().size() == other.pegs().size());
  for (std::size_t index = 0; index < one.pegs().size(); ++index) {
    CHECK(one.pegs()[index].x == other.pegs()[index].x);
    CHECK(one.pegs()[index].kind == other.pegs()[index].kind);
  }
  CHECK(one.layout() == static_cast<PegLayout>(kDay % ro::sim::kPegLayoutCount));
  CHECK(one.daily());
  one.NewGame();
  CHECK_FALSE(one.daily());
}

TEST_CASE("le guide suit la bille jusqu'au mur et après") {
  PegGame game(kSeed);
  game.SetPegs({});
  game.Aim(PegPoint{.x = ro::sim::kPegBoardWidth, .y = ro::sim::kPegCannonY});
  const std::vector<PegPoint> guide = game.AimGuide();
  REQUIRE(guide.size() >= 2);
  // Le guide montre le point d'après le pas 1, puis un tous les trois pas.
  constexpr int kGuideEvery = 3;
  const int steps = 1 + kGuideEvery * static_cast<int>(guide.size() - 1);
  game.Launch();
  bool reached_wall = false;
  for (int step = 0; step < steps; ++step) {
    game.Advance(ro::sim::kPegStepS);
    reached_wall = reached_wall || game.TakeNews().wall_hits > 0;
  }
  REQUIRE(reached_wall);
  CHECK(game.ball().x == Catch::Approx(guide.back().x).margin(0.01));
  CHECK(game.ball().y == Catch::Approx(guide.back().y).margin(0.01));
}

TEST_CASE("Magnum Break allume les plots autour du vert") {
  PegGame game(kSeed);
  game.SetPegs(
      {Peg{.x = 200.0F, .y = 300.0F, .kind = PegKind::kGreen, .power = PegPower::kMagnumBreak},
       Peg{.x = 250.0F, .y = 330.0F}, Peg{.x = 400.0F, .y = 330.0F},
       Peg{.x = 500.0F, .y = 150.0F, .kind = PegKind::kOrange}});
  game.PlaceBall(PegPoint{.x = 203.0F, .y = 284.0F}, PegPoint{});
  game.Advance(ro::sim::kPegStepS);
  CHECK(game.pegs()[1].lit);
  CHECK_FALSE(game.pegs()[2].lit);
  CHECK(game.powers_used() == 1);
  CHECK(game.TakeNews().power);
}

TEST_CASE("Double Strafe dédouble la bille, et le tir finit quand les deux sont sorties") {
  PegGame game(kSeed);
  game.SetPegs(
      {Peg{.x = 200.0F, .y = 300.0F, .kind = PegKind::kGreen, .power = PegPower::kDoubleStrafe},
       Peg{.x = 500.0F, .y = 150.0F, .kind = PegKind::kOrange}});
  game.PlaceBall(PegPoint{.x = 203.0F, .y = 284.0F}, PegPoint{});
  game.Advance(ro::sim::kPegStepS);
  CHECK(game.balls().size() == 2);
  RunFor(game, kLongEnoughS);
  CHECK(game.phase() == PegPhase::kAiming);
  CHECK(game.balls().empty());
}

TEST_CASE("Safety Wall élargit le seau pour ce tir et le suivant") {
  PegGame game(kSeed);
  game.SetPegs(
      {Peg{.x = 200.0F, .y = 300.0F, .kind = PegKind::kGreen, .power = PegPower::kSafetyWall},
       Peg{.x = 500.0F, .y = 150.0F, .kind = PegKind::kOrange}});
  CHECK(game.bucket_width() == ro::sim::kPegBucketWidth);
  game.PlaceBall(PegPoint{.x = 203.0F, .y = 284.0F}, PegPoint{});
  RunFor(game, kLongEnoughS);
  CHECK(game.bucket_width() == ro::sim::kPegSafetyWallWidth);
  DropAt(game, kAwayFromBucketX);
  CHECK(game.bucket_width() == ro::sim::kPegBucketWidth);
}

TEST_CASE("un orange touché juste après un mur est une bande") {
  PegGame game(kSeed);
  game.SetPegs({Peg{.x = 30.0F, .y = 282.0F, .kind = PegKind::kOrange},
                Peg{.x = 500.0F, .y = 150.0F, .kind = PegKind::kOrange}});
  game.PlaceBall(PegPoint{.x = 60.0F, .y = 250.0F}, PegPoint{.x = -900.0F, .y = 300.0F});
  RunFor(game, 0.2F);
  const ro::sim::PegNews news = game.TakeNews();
  CHECK(std::ranges::any_of(news.callouts, [](const ro::sim::PegCallout& callout) {
    return callout.kind == ro::sim::PegCalloutKind::kBankShot;
  }));
}

TEST_CASE("une bille près du dernier orange ralentit le temps, pas la physique") {
  PegGame game(kSeed);
  game.SetPegs({Peg{.x = 300.0F, .y = 400.0F, .kind = PegKind::kOrange}});
  game.PlaceBall(PegPoint{.x = 300.0F, .y = 350.0F}, PegPoint{});
  CHECK(game.slowed());
  const float before_y = game.ball().y;
  game.Advance(ro::sim::kPegStepS);
  // Un pas de temps réel ne suffit plus à un pas de physique.
  CHECK(game.ball().y == before_y);
  game.Advance(ro::sim::kPegStepS / ro::sim::kPegSlowScale);
  CHECK(game.ball().y > before_y);
}

TEST_CASE("une bille qui rebondit sur place efface son plot, et le tir finit") {
  // UNE COUPE : la bille lâchée pile au-dessus du plot du fond y rebondit à la
  // verticale sans jamais descendre plus bas ; les deux bords la retiennent.
  constexpr float kCupX = 120.0F;
  constexpr float kCupY = 320.0F;
  constexpr float kRimOffsetX = 20.0F;
  constexpr float kRimOffsetY = 20.0F;
  constexpr float kDropAboveY = 30.0F;
  // Bien moins que la limite de temps d'un tir, bien plus que la garde.
  constexpr float kWellBeforeTimeoutS = 4.0F;
  PegGame game(kSeed);
  game.SetPegs({Peg{.x = kCupX, .y = kCupY, .kind = PegKind::kOrange},
                Peg{.x = kCupX - kRimOffsetX, .y = kCupY - kRimOffsetY},
                Peg{.x = kCupX + kRimOffsetX, .y = kCupY - kRimOffsetY},
                Peg{.x = 500.0F, .y = 150.0F, .kind = PegKind::kOrange}});
  game.PlaceBall(PegPoint{.x = kCupX, .y = kCupY - kDropAboveY}, PegPoint{});
  RunFor(game, kWellBeforeTimeoutS);
  CHECK(game.phase() == PegPhase::kAiming);
  CHECK(game.pegs()[0].removed);
  CHECK(game.orange_left() == 1);
}

TEST_CASE("la visée fine tourne le canon d'un quart de degré par cran") {
  PegGame game(kSeed);
  game.Aim(PegPoint{.x = ro::sim::kPegCannonX, .y = ro::sim::kPegBoardHeight});
  game.NudgeAim(4);
  CHECK(game.aim().x < ro::sim::kPegCannonX);
  game.NudgeAim(-8);
  CHECK(game.aim().x > ro::sim::kPegCannonX);
}
