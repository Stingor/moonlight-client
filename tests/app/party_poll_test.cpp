// L'interrogation du PS et des états des membres : une demande à la fois, en
// rotation — sans moi, sans les membres hors ligne.

#include "party/party_poll.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/party_roster.h"

using moonclient::kMemberStatusPollIntervalMs;
using moonclient::kPartyPollIntervalMs;
using moonclient::MemberStatusPoll;
using moonclient::PartyVitalsPoll;
using ro::sim::MemberStatuses;
using ro::sim::PartyMember;

namespace {

constexpr std::uint32_t kMe = 2000001;
constexpr std::uint32_t kAlice = 2000002;
constexpr std::uint32_t kBob = 2000003;
constexpr std::uint32_t kCarol = 2000004;
constexpr std::uint32_t kStart = 10000;

[[nodiscard]] PartyMember Member(std::uint32_t account_id, bool offline = false) {
  PartyMember member;
  member.account_id = account_id;
  member.offline = offline;
  return member;
}

[[nodiscard]] bool Everyone(std::uint32_t /*account_id*/) { return true; }

}  // namespace

TEST_CASE("Groupe, PS : une demande par tour, en rotation, sans moi ni les absents",
          "[app][party]") {
  const std::vector<PartyMember> members = {Member(kMe), Member(kAlice), Member(kBob, true),
                                            Member(kCarol)};
  PartyVitalsPoll poll;
  CHECK(poll.Next(members, kMe, true, kStart) == kAlice);
  // Moins de 250 ms plus tard : rien ne part.
  CHECK(poll.Next(members, kMe, true, kStart + kPartyPollIntervalMs - 1) == 0);
  // Bob est hors ligne : on passe à Carol.
  CHECK(poll.Next(members, kMe, true, kStart + kPartyPollIntervalMs) == kCarol);
  // La liste repart du début, et me saute.
  CHECK(poll.Next(members, kMe, true, kStart + 2 * kPartyPollIntervalMs) == kAlice);
}

TEST_CASE("Groupe, PS : rien sans surface qui le montre, ni seul dans la liste", "[app][party]") {
  PartyVitalsPoll poll;
  const std::vector<PartyMember> pair = {Member(kMe), Member(kAlice)};
  CHECK(poll.Next(pair, kMe, false, kStart) == 0);
  // Une demande refusée n'a pas compté : la suivante part aussitôt.
  CHECK(poll.Next(pair, kMe, true, kStart + 1) == kAlice);

  PartyVitalsPoll alone;
  const std::vector<PartyMember> solo = {Member(kMe)};
  CHECK(alone.Next(solo, kMe, true, kStart) == 0);
}

TEST_CASE("Groupe, PS : personne d'interrogeable ne consomme pas le tour", "[app][party]") {
  PartyVitalsPoll poll;
  std::vector<PartyMember> members = {Member(kMe), Member(kAlice, true)};
  CHECK(poll.Next(members, kMe, true, kStart) == 0);
  members[1].offline = false;
  CHECK(poll.Next(members, kMe, true, kStart + 1) == kAlice);
}

TEST_CASE("Groupe, états : une demande par tour de 300 ms, en rotation", "[app][party]") {
  const std::vector<PartyMember> members = {Member(kMe), Member(kAlice), Member(kBob, true),
                                            Member(kCarol)};
  const MemberStatuses statuses;
  MemberStatusPoll poll;
  CHECK(poll.Next(members, kMe, true, statuses, Everyone, kStart) == kAlice);
  CHECK(poll.Next(members, kMe, true, statuses, Everyone,
                  kStart + kMemberStatusPollIntervalMs - 1) == 0);
  // Bob est hors ligne : on passe à Carol, puis la liste repart et me saute.
  CHECK(poll.Next(members, kMe, true, statuses, Everyone, kStart + kMemberStatusPollIntervalMs) ==
        kCarol);
  CHECK(poll.Next(members, kMe, true, statuses, Everyone,
                  kStart + 2 * kMemberStatusPollIntervalMs) == kAlice);
}

TEST_CASE("Groupe, états : ni refusé, ni hors de vue, ni sans surface", "[app][party]") {
  const std::vector<PartyMember> members = {Member(kMe), Member(kAlice), Member(kCarol)};
  MemberStatuses statuses;
  statuses.Refuse(kAlice, kStart);
  MemberStatusPoll poll;
  CHECK(poll.Next(members, kMe, false, statuses, Everyone, kStart) == 0);
  // Alice vient d'être refusée : Carol passe.
  CHECK(poll.Next(members, kMe, true, statuses, Everyone, kStart + 1) == kCarol);

  MemberStatusPoll unseen;
  const auto only_alice = [](std::uint32_t account_id) { return account_id == kAlice; };
  const MemberStatuses fresh;
  CHECK(unseen.Next(members, kMe, true, fresh, only_alice, kStart) == kAlice);
  CHECK(unseen.Next(members, kMe, true, fresh, only_alice, kStart + kMemberStatusPollIntervalMs) ==
        kAlice);

  // Le refus se tait passé son délai.
  MemberStatusPoll later;
  CHECK(later.Next(members, kMe, true, statuses, only_alice,
                   kStart + ro::sim::kMemberStatusRefusedQuietMs) == kAlice);
}

TEST_CASE("Groupe, états : le monde qui repart remet la rotation au début", "[app][party]") {
  const std::vector<PartyMember> members = {Member(kMe), Member(kAlice), Member(kCarol)};
  const MemberStatuses statuses;
  MemberStatusPoll poll;
  CHECK(poll.Next(members, kMe, true, statuses, Everyone, kStart) == kAlice);
  poll.Reset();
  CHECK(poll.Next(members, kMe, true, statuses, Everyone, kStart + 1) == kAlice);
}
