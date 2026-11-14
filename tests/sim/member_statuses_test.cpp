// Les états des membres du groupe : la liste complète qui remplace, la durée de
// référence qui ne recule pas, le refus qui fait taire, et la purge.

#include "ro/sim/member_statuses.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::sim::ActiveStatus;
using ro::sim::KeepLongestDuration;
using ro::sim::kMemberStatusRefreshMarginMs;
using ro::sim::kMemberStatusRefusedQuietMs;
using ro::sim::MemberStatuses;
using ro::sim::StatusReport;

namespace {

constexpr std::uint32_t kAlice = 2'000'002;
constexpr std::uint32_t kBob = 2'000'003;
constexpr ro::sim::StatusEffectId kBlessing = 10;
constexpr ro::sim::StatusEffectId kAgiUp = 12;
constexpr std::uint32_t kNow = 500'000;

[[nodiscard]] const ActiveStatus* EffectOf(const MemberStatuses& statuses, std::uint32_t account,
                                           ro::sim::StatusEffectId effect) {
  const std::vector<ActiveStatus>* list = statuses.Find(account);
  if (list == nullptr) {
    return nullptr;
  }
  for (const ActiveStatus& status : *list) {
    if (status.effect == effect) {
      return &status;
    }
  }
  return nullptr;
}

[[nodiscard]] bool KeepAll(std::uint32_t /*account_id*/) { return true; }

}  // namespace

TEST_CASE("États des membres : rien de connu n'est pas « aucun état »", "[sim][party]") {
  MemberStatuses statuses;
  CHECK(statuses.Find(kAlice) == nullptr);
  statuses.Tell(kAlice, {}, kNow);
  const std::vector<ActiveStatus>* list = statuses.Find(kAlice);
  REQUIRE(list != nullptr);
  CHECK(list->empty());
}

TEST_CASE("États des membres : la liste complète remplace la précédente", "[sim][party]") {
  MemberStatuses statuses;
  const std::vector<StatusReport> first = {{.effect = kBlessing, .remaining_ms = 60000},
                                           {.effect = kAgiUp, .remaining_ms = 0}};
  statuses.Tell(kAlice, first, kNow);
  const ActiveStatus* blessing = EffectOf(statuses, kAlice, kBlessing);
  REQUIRE(blessing != nullptr);
  CHECK(blessing->expires_ms == kNow + 60000);
  CHECK(blessing->total_ms == 60000);
  const ActiveStatus* agi = EffectOf(statuses, kAlice, kAgiUp);
  REQUIRE(agi != nullptr);
  CHECK(agi->expires_ms == 0);
  CHECK(agi->total_ms == 0);

  const std::vector<StatusReport> second = {{.effect = kAgiUp, .remaining_ms = 0}};
  statuses.Tell(kAlice, second, kNow + 1000);
  CHECK(EffectOf(statuses, kAlice, kBlessing) == nullptr);
  CHECK(EffectOf(statuses, kAlice, kAgiUp) != nullptr);
}

TEST_CASE("États des membres : la durée de référence garde le plus grand restant vu",
          "[sim][party]") {
  MemberStatuses statuses;
  // Vu commencer : la durée pleine.
  statuses.Apply(kAlice, kBlessing, true, 60000, 60000, kNow);
  // Vingt secondes plus tard, la réponse annonce ce qu'il reste : la référence
  // ne suit pas le restant, sinon le voile ne bougerait jamais.
  const std::vector<StatusReport> later = {
      {.effect = kBlessing, .remaining_ms = 40000, .total_ms = 40000}};
  statuses.Tell(kAlice, later, kNow + 20000);
  const ActiveStatus* blessing = EffectOf(statuses, kAlice, kBlessing);
  REQUIRE(blessing != nullptr);
  CHECK(blessing->total_ms == 60000);
  CHECK(blessing->expires_ms == kNow + 60000);

  // Relancé : le restant dépasse l'extrapolé de plus que la marge, la
  // référence repart de lui.
  const std::vector<StatusReport> recast = {
      {.effect = kBlessing, .remaining_ms = 45000, .total_ms = 45000}};
  statuses.Tell(kAlice, recast, kNow + 30000);
  blessing = EffectOf(statuses, kAlice, kBlessing);
  REQUIRE(blessing != nullptr);
  CHECK(blessing->total_ms == 45000);
}

TEST_CASE("États des membres : la marge de relance absorbe l'écart d'horloge", "[sim][party]") {
  // Restant extrapolé 30 s, annoncé un peu plus : ce n'est pas une relance.
  CHECK(KeepLongestDuration(30000 + kMemberStatusRefreshMarginMs, 60000, 30000) == 60000);
  CHECK(KeepLongestDuration(30000 + kMemberStatusRefreshMarginMs + 1, 60000, 30000) ==
        30000 + kMemberStatusRefreshMarginMs + 1);
  // Découvert en route, puis vu plus grand : la référence grandit.
  CHECK(KeepLongestDuration(50000, 40000, 49000) == 50000);
  // Un état permanent garde la référence d'avant.
  CHECK(KeepLongestDuration(0, 60000, 0) == 60000);
}

TEST_CASE("États des membres : une transition ne touche que l'état qu'elle nomme", "[sim][party]") {
  MemberStatuses statuses;
  const std::vector<StatusReport> told = {{.effect = kAgiUp, .remaining_ms = 30000}};
  statuses.Tell(kAlice, told, kNow);
  // « 9999 » n'est pas une durée : sans échéance.
  statuses.Apply(kAlice, kBlessing, true, ro::sim::kUnknownDurationMs, ro::sim::kUnknownDurationMs,
                 kNow);
  const ActiveStatus* blessing = EffectOf(statuses, kAlice, kBlessing);
  REQUIRE(blessing != nullptr);
  CHECK(blessing->expires_ms == 0);
  CHECK(blessing->total_ms == 0);
  CHECK(EffectOf(statuses, kAlice, kAgiUp) != nullptr);

  statuses.Apply(kAlice, kAgiUp, false, 0, 0, kNow + 1);
  CHECK(EffectOf(statuses, kAlice, kAgiUp) == nullptr);
  CHECK(EffectOf(statuses, kAlice, kBlessing) != nullptr);
  // Un retrait sur un inconnu ne le rend pas connu.
  statuses.Apply(kBob, kAgiUp, false, 0, 0, kNow);
  CHECK(statuses.Find(kBob) == nullptr);
}

TEST_CASE("États des membres : un refus oublie et fait taire un moment", "[sim][party]") {
  MemberStatuses statuses;
  const std::vector<StatusReport> told = {{.effect = kAgiUp, .remaining_ms = 30000}};
  statuses.Tell(kAlice, told, kNow);
  statuses.Refuse(kAlice, kNow);
  CHECK(statuses.Find(kAlice) == nullptr);
  CHECK(statuses.Refused(kAlice, kNow + kMemberStatusRefusedQuietMs - 1));
  CHECK_FALSE(statuses.Refused(kAlice, kNow + kMemberStatusRefusedQuietMs));
  CHECK_FALSE(statuses.Refused(kBob, kNow));

  statuses.Tell(kBob, told, kNow);
  statuses.Forget(kBob);
  CHECK(statuses.Find(kBob) == nullptr);
  CHECK_FALSE(statuses.Refused(kBob, kNow));
}

TEST_CASE("États des membres : la purge retire l'échu et ceux qu'on ne garde pas", "[sim][party]") {
  MemberStatuses statuses;
  const std::vector<StatusReport> told = {{.effect = kBlessing, .remaining_ms = 1000},
                                          {.effect = kAgiUp, .remaining_ms = 0}};
  statuses.Tell(kAlice, told, kNow);
  statuses.Tell(kBob, told, kNow);

  statuses.Purge(kNow + 999, KeepAll);
  CHECK(EffectOf(statuses, kAlice, kBlessing) != nullptr);
  statuses.Purge(kNow + 1000, [](std::uint32_t account_id) { return account_id != kBob; });
  CHECK(EffectOf(statuses, kAlice, kBlessing) == nullptr);
  CHECK(EffectOf(statuses, kAlice, kAgiUp) != nullptr);
  CHECK(statuses.Find(kBob) == nullptr);

  statuses.Refuse(kBob, kNow);
  statuses.Clear();
  CHECK(statuses.Find(kAlice) == nullptr);
  CHECK_FALSE(statuses.Refused(kBob, kNow));
}
