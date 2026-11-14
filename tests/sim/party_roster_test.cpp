// Le groupe tenu hors du monde : ce qui survit à une redescription, ce qui se
// périme, ce qui s'annonce, et qui reprend la direction quand le chef part.

#include "ro/sim/party_roster.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::sim::LeaveStep;
using ro::sim::PartyGauge;
using ro::sim::PartyMember;
using ro::sim::PartyNoticeKind;
using ro::sim::PartyOptions;
using ro::sim::PartyRoster;

namespace {

constexpr std::uint32_t kSelf = 2'000'001;
constexpr std::uint32_t kMate = 2'000'002;
constexpr std::uint32_t kFar = 2'000'003;

PartyMember Member(std::uint32_t account_id, const char* map, bool leader = false,
                   bool offline = false) {
  return PartyMember{.account_id = account_id, .map = map, .leader = leader, .offline = offline};
}

PartyRoster Three(bool mate_offline = false, const char* mate_map = "prontera.gat") {
  PartyRoster party;
  party.Replace("Groupe",
                {Member(kSelf, "prontera.gat", true), Member(kMate, mate_map, false, mate_offline),
                 Member(kFar, "geffen.gat")});
  return party;
}

}  // namespace

TEST_CASE("Groupe (sim) : la vie survit à la liste sur la même carte, pas ailleurs", "[party]") {
  PartyRoster party = Three();
  REQUIRE(party.TellHealth(kMate, PartyGauge{.current = 50, .max = 100}));
  REQUIRE(party.TellPosition(kMate, ro::sim::PartyPosition{.x = 10, .y = 20}));
  CHECK_FALSE(party.Find(kFar)->health.has_value());

  party.Replace("Groupe", {Member(kSelf, "prontera.gat", true), Member(kMate, "prontera.gat"),
                           Member(kFar, "geffen.gat")});
  REQUIRE(party.Find(kMate)->health.has_value());
  CHECK(party.Find(kMate)->health->current == 50);
  CHECK(party.Find(kMate)->position.has_value());

  party.Upsert(Member(kMate, "payon.gat"));
  CHECK_FALSE(party.Find(kMate)->health.has_value());
  CHECK_FALSE(party.Find(kMate)->position.has_value());
  CHECK_FALSE(party.TellHealth(999, PartyGauge{}));
}

TEST_CASE("Groupe (sim) : un PS trop vieux n'est plus cru", "[party]") {
  PartyRoster party = Three();
  REQUIRE(party.TellMagic(kMate, PartyGauge{.current = 30, .max = 90}, 1000));
  REQUIRE(party.FreshMagic(kMate, 1000 + ro::sim::kPartyMagicStaleMs).has_value());
  CHECK(party.FreshMagic(kMate, 1000 + ro::sim::kPartyMagicStaleMs)->current == 30);
  CHECK_FALSE(party.FreshMagic(kMate, 1001 + ro::sim::kPartyMagicStaleMs).has_value());
  CHECK_FALSE(party.FreshMagic(kFar, 1000).has_value());
}

TEST_CASE("Groupe (sim) : le chef se lit, se transmet, se cherche par nom", "[party]") {
  PartyRoster party = Three();
  CHECK(party.AmILeader(kSelf));
  CHECK_FALSE(party.IsLeader(kMate));
  REQUIRE(party.TellLeader(kSelf, kMate));
  CHECK(party.IsLeader(kMate));
  CHECK_FALSE(party.AmILeader(kSelf));
  CHECK_FALSE(party.TellLeader(kMate, 42));
  CHECK(party.IsLeader(kMate));
  CHECK(party.FindByName("") != nullptr);
  CHECK(party.FindByName("personne") == nullptr);
}

TEST_CASE("Groupe (sim) : un changement de règles s'annonce, la première lecture non", "[party]") {
  PartyRoster party = Three();
  party.TellOptions(PartyOptions{});
  CHECK(party.TakeNotices().empty());
  party.TellOptions(PartyOptions{});
  CHECK(party.TakeNotices().empty());
  party.TellOptions(PartyOptions{.exp_shared = true});
  const std::vector<ro::sim::PartyNotice> notices = party.TakeNotices();
  REQUIRE(notices.size() == 1);
  CHECK(notices[0].kind == PartyNoticeKind::kOptionsChanged);
  CHECK(notices[0].options.exp_shared);
  CHECK(party.TakeNotices().empty());

  // Les règles d'objets seules ne s'annoncent pas et laissent l'EXP.
  party.TellItemOptions(true, true);
  CHECK(party.TakeNotices().empty());
  CHECK(party.options()->exp_shared);
  CHECK(party.options()->loot_shared);
}

TEST_CASE("Groupe (sim) : le partage ajoute la carte, se tait hors ligne", "[party]") {
  PartyRoster party = Three(false, "payon.gat");
  CHECK(party.ShareFlagsFor(kMate, kSelf) == 0);
  party.TellShare(15, {ro::sim::PartyShareRow{.account_id = kFar, .flags = ro::sim::kShareIdle}});
  CHECK(party.share_known());
  CHECK(party.share_idle_s() == 15);
  CHECK(party.ShareFlagsFor(kMate, kSelf) == ro::sim::kShareOtherMap);
  CHECK(party.ShareFlagsFor(kFar, kSelf) == (ro::sim::kShareIdle | ro::sim::kShareOtherMap));
  CHECK(party.ShareFlagsFor(kSelf, kSelf) == 0);
  party.Clear();
  CHECK_FALSE(party.share_known());
  CHECK_FALSE(party.in_party());
}

TEST_CASE("Groupe (sim) : le chef qui part passe la main sur sa carte", "[party]") {
  SECTION("un membre en ligne sur ma carte reprend la direction") {
    const ro::sim::LeavePlan plan = ro::sim::PlanLeave(Three(), kSelf);
    CHECK(plan.step == LeaveStep::kHandOverThenLeave);
    CHECK(plan.successor_account_id == kMate);
  }
  SECTION("en ligne ailleurs seulement : on ne part pas") {
    CHECK(ro::sim::PlanLeave(Three(true), kSelf).step == LeaveStep::kNeedsHandOver);
  }
  SECTION("personne en ligne : on confirme") {
    PartyRoster party;
    party.Replace("Groupe", {Member(kSelf, "prontera.gat", true),
                             Member(kMate, "prontera.gat", false, true)});
    CHECK(ro::sim::PlanLeave(party, kSelf).step == LeaveStep::kConfirmDisband);
  }
  SECTION("un simple membre part tout de suite") {
    CHECK(ro::sim::PlanLeave(Three(), kMate).step == LeaveStep::kLeave);
  }
}
