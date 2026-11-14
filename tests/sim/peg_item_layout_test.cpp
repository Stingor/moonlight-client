// ROGGLE, LE PLATEAU D'OBJET : une image trace les plots, l'orange tombe sur
// ses couleurs vives, et vider l'orange dévoile l'objet et le range au carnet.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/sim/minigame_trophies.h"
#include "ro/sim/peg_game.h"

using ro::sim::MinigameRecord;
using ro::sim::MinigameTrophy;
using ro::sim::Peg;
using ro::sim::PegFrame;
using ro::sim::PegGame;
using ro::sim::PegKind;
using ro::sim::PegLayout;
using ro::sim::PegPhase;
using ro::sim::PegPicture;
using ro::sim::PegPoint;

namespace {

constexpr std::uint32_t kSeed = 5;
constexpr std::uint32_t kItemId = 501;
constexpr std::uint8_t kOpaque = 255;
// Le côté des images d'essai, en pixels de l'image.
constexpr int kSide = 40;

// Une image carrée, opaque là où `covered` le dit, vive là où `vivid` le dit.
template <typename Covered, typename Vivid>
[[nodiscard]] PegPicture PictureWhere(Covered covered, Vivid vivid) {
  PegPicture picture{.item_id = kItemId, .width = kSide, .height = kSide};
  for (int y = 0; y < kSide; ++y) {
    for (int x = 0; x < kSide; ++x) {
      const bool inside = covered(x, y);
      picture.opacity.push_back(inside ? kOpaque : 0);
      picture.vividness.push_back(inside && vivid(x, y) ? kOpaque : 0);
    }
  }
  return picture;
}

[[nodiscard]] PegPicture FullSquare() {
  return PictureWhere([](int, int) { return true; }, [](int, int) { return false; });
}

// UN ANNEAU : rien au centre, rien dans les coins.
constexpr float kRingOuter = 20.0F;
constexpr float kRingInner = 11.0F;
[[nodiscard]] bool InRing(int x, int y) {
  const float dx = static_cast<float>(x) + 0.5F - static_cast<float>(kSide) * 0.5F;
  const float dy = static_cast<float>(y) + 0.5F - static_cast<float>(kSide) * 0.5F;
  const float distance_sq = dx * dx + dy * dy;
  return distance_sq <= kRingOuter * kRingOuter && distance_sq >= kRingInner * kRingInner;
}

// La plus petite distance entre deux centres de plots, pour que la bille passe.
constexpr float kNarrowestGap = 2.0F * (ro::sim::kPegRadius + ro::sim::kPegBallRadius);
constexpr float kRounding = 0.01F;

// Joue jusqu'à la fin du tir en cours, ralenti compris.
void FinishShot(PegGame& game) {
  constexpr int kMostSteps = static_cast<int>(13.0F / ro::sim::kPegStepS / ro::sim::kPegSlowScale);
  for (int step = 0; step < kMostSteps && game.phase() == PegPhase::kFlying; ++step) {
    game.Advance(ro::sim::kPegStepS);
  }
}

// GAGNE LA PARTIE en posant la bille juste au-dessus de chaque orange encore
// éteint ; chaque tir en efface au moins un.
void WinByDropping(PegGame& game) {
  constexpr float kAbovePeg = ro::sim::kPegRadius + ro::sim::kPegBallRadius + 2.0F;
  constexpr float kGlancingX = 3.0F;
  while (game.phase() == PegPhase::kAiming) {
    const auto target = std::ranges::find_if(game.pegs(), [](const Peg& peg) {
      return peg.kind == PegKind::kOrange && !peg.removed && !peg.lit;
    });
    REQUIRE(target != game.pegs().end());
    game.PlaceBall(PegPoint{.x = target->x + kGlancingX, .y = target->y - kAbovePeg}, PegPoint{});
    FinishShot(game);
  }
}

}  // namespace

TEST_CASE("le cadre de l'image tient dans le plateau, sans la déformer") {
  PegPicture tall = FullSquare();
  tall.width = kSide / 2;
  tall.opacity.resize(static_cast<std::size_t>(tall.width * tall.height));
  tall.vividness.resize(tall.opacity.size());
  for (const PegPicture& picture : {FullSquare(), tall}) {
    const PegFrame frame = ro::sim::PegPictureFrame(picture);
    CHECK(frame.origin.x >= ro::sim::kPegRadius);
    CHECK(frame.origin.x + frame.width <= ro::sim::kPegBoardWidth - ro::sim::kPegRadius);
    CHECK(frame.origin.y > ro::sim::kPegCannonY + ro::sim::kPegRadius);
    CHECK(frame.origin.y + frame.height < ro::sim::kPegBucketY - ro::sim::kPegBucketHeight);
    CHECK(frame.width * static_cast<float>(picture.height) ==
          Catch::Approx(frame.height * static_cast<float>(picture.width)));
  }
}

TEST_CASE("une image trace ses plots là où elle couvre, assez écartés pour la bille") {
  std::mt19937 random(kSeed);
  const PegPicture ring = PictureWhere(InRing, [](int, int) { return false; });
  const std::vector<Peg> pegs = ro::sim::BuildItemPegLayout(ring, random);
  REQUIRE(pegs.size() >= static_cast<std::size_t>(ro::sim::kPegItemMinPegs));
  const PegFrame frame = ro::sim::PegPictureFrame(ring);
  const float scale = frame.width / static_cast<float>(kSide);
  const PegPoint middle{.x = frame.origin.x + frame.width * 0.5F,
                        .y = frame.origin.y + frame.height * 0.5F};
  for (std::size_t index = 0; index < pegs.size(); ++index) {
    const Peg& peg = pegs[index];
    // Aucun plot dans le trou de l'anneau, ni hors de son bord.
    const float dx = (peg.x - middle.x) / scale;
    const float dy = (peg.y - middle.y) / scale;
    const float from_middle_sq = dx * dx + dy * dy;
    CHECK(from_middle_sq > (kRingInner * 0.5F) * (kRingInner * 0.5F));
    CHECK(from_middle_sq < (kRingOuter + 4.0F) * (kRingOuter + 4.0F));
    for (std::size_t other = index + 1; other < pegs.size(); ++other) {
      const float gap_x = pegs[other].x - peg.x;
      const float gap_y = pegs[other].y - peg.y;
      CHECK(gap_x * gap_x + gap_y * gap_y >=
            (kNarrowestGap - kRounding) * (kNarrowestGap - kRounding));
    }
  }
}

TEST_CASE("l'orange tombe sur les couleurs vives de l'objet") {
  std::mt19937 random(kSeed);
  // Une moitié gauche vive, une droite terne.
  const PegPicture picture =
      PictureWhere([](int, int) { return true; }, [](int x, int) { return x < kSide / 2; });
  const std::vector<Peg> pegs = ro::sim::BuildItemPegLayout(picture, random);
  REQUIRE_FALSE(pegs.empty());
  const PegFrame frame = ro::sim::PegPictureFrame(picture);
  const float middle_x = frame.origin.x + frame.width * 0.5F;
  const auto oranges =
      std::ranges::count_if(pegs, [](const Peg& peg) { return peg.kind == PegKind::kOrange; });
  CHECK(oranges == static_cast<std::ptrdiff_t>(pegs.size() / 4));
  for (const Peg& peg : pegs) {
    if (peg.kind == PegKind::kOrange) {
      CHECK(peg.x < middle_x);
    }
  }
  CHECK(std::ranges::count_if(pegs, [](const Peg& peg) { return peg.kind == PegKind::kGreen; }) ==
        2);
}

TEST_CASE("une image trop petite ne fait pas de plateau, et la partie reste") {
  constexpr int kDot = 3;
  const PegPicture dot = PictureWhere([](int x, int y) { return x < kDot && y < kDot; },
                                      [](int, int) { return false; });
  std::mt19937 random(kSeed);
  CHECK(ro::sim::BuildItemPegLayout(dot, random).empty());
  PegGame game(kSeed);
  game.NewGame(PegLayout::kPyramid);
  const std::size_t before = game.pegs().size();
  CHECK_FALSE(game.NewItemGame(dot));
  CHECK(game.layout() == PegLayout::kPyramid);
  CHECK(game.pegs().size() == before);
  // Une image illisible non plus.
  PegPicture broken = FullSquare();
  broken.opacity.pop_back();
  CHECK_FALSE(game.NewItemGame(broken));
}

TEST_CASE("le plateau d'objet du jour est le même pour tous") {
  constexpr std::uint32_t kDay = 20400;
  const PegPicture picture = PictureWhere(InRing, [](int x, int) { return x % 3 == 0; });
  PegGame one(1);
  PegGame other(2);
  REQUIRE(one.NewDailyItemGame(kDay, picture));
  REQUIRE(other.NewDailyItemGame(kDay, picture));
  CHECK(one.layout() == PegLayout::kItem);
  CHECK(one.daily());
  CHECK(one.item_id() == kItemId);
  CHECK(one.picture_frame().width > 0.0F);
  REQUIRE(one.pegs().size() == other.pegs().size());
  for (std::size_t index = 0; index < one.pegs().size(); ++index) {
    CHECK(one.pegs()[index].x == other.pegs()[index].x);
    CHECK(one.pegs()[index].kind == other.pegs()[index].kind);
  }
  // Un plateau dessiné oublie l'objet.
  one.NewGame(PegLayout::kWaves);
  CHECK(one.item_id() == 0);
  CHECK(one.picture_frame().width == 0.0F);
}

TEST_CASE("vider l'orange d'un plateau d'objet le range au carnet, une seule fois") {
  // Un quart de l'image couvert : une trentaine de plots. Chaque bille lâchée
  // efface au moins un orange : en donner autant suffit à gagner.
  const PegPicture corner = PictureWhere(
      [](int x, int y) { return x < kSide / 2 && y < kSide / 2; }, [](int, int) { return false; });
  PegGame game(kSeed);
  REQUIRE(game.NewItemGame(corner));
  game.SetBallsLeft(game.orange_left() + 1);
  MinigameRecord record;
  static_cast<void>(RecordRoggle(record, game));
  CHECK(record.roggle_revealed.empty());
  WinByDropping(game);
  REQUIRE(game.phase() == PegPhase::kWon);
  const std::vector<MinigameTrophy> fresh = RecordRoggle(record, game);
  CHECK(record.roggle_revealed == std::vector<std::uint32_t>{kItemId});
  CHECK(std::ranges::find(fresh, MinigameTrophy::kRoggleFirstItem) != fresh.end());
  CHECK_FALSE(record.Has(MinigameTrophy::kRoggleTenItems));
  static_cast<void>(RecordRoggle(record, game));
  CHECK(record.roggle_revealed.size() == 1);
}

TEST_CASE("le carnet gagne ses trophées au compte, et se fond sans doublon") {
  MinigameRecord record;
  for (std::uint32_t item_id = 1; item_id < 25; ++item_id) {
    record.roggle_revealed.push_back(item_id);
  }
  MinigameRecord other;
  other.roggle_revealed = {24, 25, 30};
  ro::sim::MergeMinigameRecord(record, other);
  CHECK(record.roggle_revealed.size() == 26);
  CHECK(std::ranges::is_sorted(record.roggle_revealed));
  PegGame game(kSeed);
  game.NewGame(PegLayout::kPyramid);
  static_cast<void>(RecordRoggle(record, game));
  CHECK(record.Has(MinigameTrophy::kRoggleFirstItem));
  CHECK(record.Has(MinigameTrophy::kRoggleTenItems));
  CHECK(record.Has(MinigameTrophy::kRoggleTwentyFiveItems));
}

namespace {

// Les mêmes plots, aux mêmes places et de mêmes sortes, tous intacts.
void CheckSameOpening(const std::vector<Peg>& opening, const std::vector<Peg>& pegs) {
  REQUIRE(opening.size() == pegs.size());
  for (std::size_t index = 0; index < opening.size(); ++index) {
    CHECK(opening[index].x == pegs[index].x);
    CHECK(opening[index].y == pegs[index].y);
    CHECK(opening[index].kind == pegs[index].kind);
    CHECK(opening[index].power == pegs[index].power);
    CHECK_FALSE(pegs[index].lit);
    CHECK_FALSE(pegs[index].removed);
  }
}

}  // namespace

TEST_CASE("rejouer le même dessin rétablit le plateau d'objet et remet la partie à neuf") {
  PegGame game(kSeed);
  REQUIRE(game.NewItemGame(FullSquare()));
  const std::vector<Peg> opening = game.pegs();
  const int opening_balls = game.balls_left();
  const PegFrame opening_frame = game.picture_frame();

  game.SetBallsLeft(game.orange_left() + 1);
  WinByDropping(game);
  REQUIRE(game.phase() == PegPhase::kWon);

  game.Replay();
  CHECK(game.phase() == PegPhase::kAiming);
  CHECK(game.score() == 0);
  CHECK(game.balls_left() == opening_balls);
  CHECK(game.orange_left() > 0);
  CHECK_FALSE(game.finale());
  CHECK(game.layout() == PegLayout::kItem);
  CHECK(game.item_id() == kItemId);
  CHECK(game.picture_frame().origin.x == opening_frame.origin.x);
  CHECK(game.picture_frame().width == opening_frame.width);
  CheckSameOpening(opening, game.pegs());
}

TEST_CASE("rejouer le même dessin refait le plateau dessiné, avec le même hasard") {
  PegGame game(kSeed);
  game.NewGame(PegLayout::kPyramid);
  const std::vector<Peg> opening = game.pegs();

  game.Launch();
  FinishShot(game);
  const std::vector<Peg> after_shot = game.pegs();

  game.Replay();
  CHECK(game.layout() == PegLayout::kPyramid);
  CheckSameOpening(opening, game.pegs());

  // Le même tir rejoué : les mêmes plots effacés, et le violet au même endroit.
  game.Launch();
  FinishShot(game);
  REQUIRE(game.pegs().size() == after_shot.size());
  for (std::size_t index = 0; index < after_shot.size(); ++index) {
    CHECK(game.pegs()[index].kind == after_shot[index].kind);
    CHECK(game.pegs()[index].removed == after_shot[index].removed);
  }
}

TEST_CASE("rejouer ne fait rien sur des plots posés à la main") {
  PegGame game(kSeed);
  game.NewGame(PegLayout::kPyramid);
  game.SetPegs({Peg{.x = 100.0F, .y = 100.0F, .kind = PegKind::kOrange}});
  game.SetBallsLeft(1);
  game.Replay();
  CHECK(game.pegs().size() == 1);
  CHECK(game.balls_left() == 1);
}

TEST_CASE("l'option du carnet écarte les objets dévoilés, sauf quand il n'en reste aucun") {
  const std::vector<std::uint32_t> items = {501, 502, 503};
  MinigameRecord record;
  record.roggle_revealed = {502};
  CHECK(ro::sim::RoggleItemChoices(items, record) == items);

  record.roggle_skip_revealed = true;
  CHECK(ro::sim::RoggleItemChoices(items, record) == std::vector<std::uint32_t>{501, 503});

  record.roggle_revealed = {501, 502, 503};
  CHECK(ro::sim::RoggleItemChoices(items, record) == items);
}
