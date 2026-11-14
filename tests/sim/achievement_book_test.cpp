// LES SUCCÈS DU PERSONNAGE : la liste, la mise à jour qui achève, les gardes de
// la réclamation et le suivi tenu par le client seul.

#include "ro/sim/achievement_book.h"

#include <array>

#include <catch2/catch_test_macros.hpp>

using ro::sim::AchievementBook;
using ro::sim::AchievementProgress;
using ro::sim::AchievementRecord;
using ro::sim::AchievementStanding;
using ro::sim::TrackOutcome;

namespace {

constexpr std::uint32_t kDone = 120001;
constexpr std::uint32_t kRunning = 120002;

[[nodiscard]] AchievementRecord Record(std::uint32_t achievement_id, bool completed,
                                       bool rewarded) {
  AchievementRecord record{.achievement_id = achievement_id};
  record.progress.completed = completed;
  record.progress.rewarded = rewarded;
  record.progress.counts[0] = completed ? 1 : 0;
  return record;
}

[[nodiscard]] AchievementBook BookWithTwo() {
  AchievementBook book;
  const std::array records = {Record(kRunning, false, false), Record(kDone, true, false)};
  book.ApplyList(
      AchievementStanding{.total_points = 565, .rank = 7, .rank_points = 15, .rank_span = 178},
      records);
  return book;
}

}  // namespace

TEST_CASE("Succès : la liste pose le score et range les succès par numéro", "[sim][succes]") {
  const AchievementBook book = BookWithTwo();
  CHECK(book.standing().total_points == 565);
  CHECK(book.standing().rank == 7);
  CHECK(book.standing().rank_points == 15);
  CHECK(book.standing().rank_span == 178);
  REQUIRE(book.records().size() == 2);
  CHECK(book.records()[0].achievement_id == kDone);
  CHECK(book.Find(kDone)->completed);
  CHECK(book.Find(424242) == nullptr);
}

TEST_CASE("Succès : la mise à jour ne dit « achevé » qu'au passage", "[sim][succes]") {
  AchievementBook book = BookWithTwo();
  CHECK(book.ApplyUpdate(AchievementStanding{}, Record(kRunning, true, false)));
  CHECK_FALSE(book.ApplyUpdate(AchievementStanding{}, Record(kRunning, true, false)));
  CHECK(book.ApplyUpdate(AchievementStanding{}, Record(130000, true, false)));
  CHECK_FALSE(book.ApplyUpdate(AchievementStanding{}, Record(130001, false, false)));
}

TEST_CASE("Succès : la mise à jour nulle de la connexion ne pose que le score", "[sim][succes]") {
  AchievementBook book = BookWithTwo();
  CHECK_FALSE(book.ApplyUpdate(AchievementStanding{.total_points = 600},
                               Record(ro::sim::kNoAchievement, true, false)));
  CHECK(book.standing().total_points == 600);
  CHECK(book.records().size() == 2);
}

TEST_CASE("Succès : la réclamation suit les gardes du serveur", "[sim][succes]") {
  AchievementBook book = BookWithTwo();
  CHECK_FALSE(book.CanClaim(kRunning, true));
  CHECK_FALSE(book.CanClaim(kDone, false));
  CHECK(book.CanClaim(kDone, true));
  REQUIRE(book.NoteClaim(kDone, true));
  // ⚠ EN VOL, ELLE NE REPART PAS : un double clic ne vaudrait qu'un refus.
  CHECK_FALSE(book.CanClaim(kDone, true));
  CHECK(book.ClaimPending(kDone));
  book.SettleClaim(kDone);
  CHECK(book.CanClaim(kDone, true));
  CHECK_FALSE(book.ApplyUpdate(AchievementStanding{}, Record(kDone, true, true)));
  CHECK_FALSE(book.CanClaim(kDone, true));
}

TEST_CASE("Succès : une liste qui dit la récompense réclamée clôt la réclamation",
          "[sim][succes]") {
  AchievementBook book = BookWithTwo();
  REQUIRE(book.NoteClaim(kDone, true));
  const std::array records = {Record(kDone, true, true)};
  book.ApplyList(AchievementStanding{}, records);
  CHECK_FALSE(book.ClaimPending(kDone));
  CHECK(book.Find(kRunning) != nullptr);
}

TEST_CASE("Succès : on en suit trois, pas un de plus", "[sim][succes]") {
  AchievementBook book;
  CHECK(book.ToggleTracking(1) == TrackOutcome::kTracked);
  CHECK(book.ToggleTracking(2) == TrackOutcome::kTracked);
  CHECK(book.ToggleTracking(3) == TrackOutcome::kTracked);
  CHECK(book.ToggleTracking(4) == TrackOutcome::kFull);
  CHECK_FALSE(book.IsTracked(4));
  CHECK(book.ToggleTracking(2) == TrackOutcome::kUntracked);
  CHECK(book.ToggleTracking(4) == TrackOutcome::kTracked);
  REQUIRE(book.tracked().size() == 3);
  CHECK(book.tracked()[2] == 4);
}
