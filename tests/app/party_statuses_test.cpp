// Les cases d'état d'un membre du groupe, composées depuis le registre : le
// restant et la durée de référence arrivent jusqu'au voile.

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/member_statuses.h"
#include "ro/vfs/status_table.h"

#include "character/status_view.h"

using moonclient::StatusView;
using ro::sim::MemberStatuses;
using ro::sim::StatusReport;

namespace {

constexpr std::uint32_t kAlice = 2'000'002;
constexpr ro::sim::StatusEffectId kBlessing = 37;
constexpr std::uint32_t kNow = 100'000;

}  // namespace

TEST_CASE("Groupe, états : les cases d'un membre portent restant et durée", "[app][party]") {
  MemberStatuses statuses;
  // Vu commencer à la zone, puis décrit vingt secondes plus tard.
  statuses.Apply(kAlice, kBlessing, true, 60'000, 60'000, kNow);
  const std::vector<StatusReport> told = {
      {.effect = kBlessing, .remaining_ms = 40'000, .total_ms = 40'000}};
  statuses.Tell(kAlice, told, kNow + 20'000);

  ro::vfs::StatusTable table;
  table.by_id[kBlessing] = ro::vfs::StatusInfo{.name = "Blessing"};
  StatusView view(&table, nullptr);
  const std::vector<ro::sim::ActiveStatus>* known = statuses.Find(kAlice);
  REQUIRE(known != nullptr);
  view.RefreshList(*known, kNow + 30'000, /*preview=*/0);
  REQUIRE(view.cells().size() == 1);
  CHECK(view.cells()[0].name == "Blessing");
  CHECK(view.cells()[0].remaining_ms == 30'000);
  CHECK(view.cells()[0].total_ms == 60'000);
}
