// L'ÉTAGE DU CARNET DE CHASSE MVP contre un serveur qui écrit les QUEUES de
// ses trames (`kUiCapMvpTrackerExt`) : le résultat dit sa commande,
// l'invitation son origine, le groupe notre compte, et la présence est
// poussée. Chaque règle a son repli, que les essais voisins tiennent ; ceux-ci
// tiennent ce que la queue change.

#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/mvp_tracker.h"
#include "ro/ui/mvp_tracker_window.h"

#include "mvp/mvp_tracker_stage.h"
#include "mvp_tracker_stage_bench.h"

using namespace mvp_tracker_stage_bench;
using moonclient::kMvpPresenceRefreshMs;
using moonclient::kMvpPushedPresenceRefreshMs;

namespace {

constexpr std::uint8_t kOk = 0;
constexpr auto kFull = static_cast<std::uint8_t>(ro::sim::MvpGroupResult::kFull);
constexpr auto kKept = static_cast<std::uint8_t>(ro::sim::MvpGroupResult::kObservationKept);
constexpr auto kFromSnapshot = static_cast<std::uint8_t>(ro::sim::MvpInviteOrigin::kSnapshot);
constexpr auto kFromPush = static_cast<std::uint8_t>(ro::sim::MvpInviteOrigin::kPushed);
constexpr std::uint32_t kInviteGroup = 12;

[[nodiscard]] std::vector<std::string> Lines(std::vector<std::string> lines) { return lines; }

// L'invitation `kInviteGroup` reçue, puis congédiée : seule une invitation
// nouvelle la remontre.
void DismissInvitation(Bench& bench) {
  bench.Apply(Group(1, kInviteGroup, "Les Chasseurs"));
  bench.windows.answer.dismiss_invite = true;
  bench.Advance(true);
  REQUIRE(bench.windows.view->pending_invite == 0);
}

}  // namespace

TEST_CASE("Carnet MVP, étage étendu : toute commande porte un identifiant qui grandit",
          "[app][mvp]") {
  Bench bench;
  bench.SwitchOn();
  bench.Advance(true);
  const Command snapshot = bench.Next();
  CHECK(snapshot.cmd == Raw(MvpCommand::kSnapshot));
  CHECK(snapshot.request_id != 0);
  bench.stage.Invite("Bob", bench.link);
  const Command invite = bench.Next();
  CHECK(invite.text == "Bob");
  CHECK(invite.request_id == snapshot.request_id + 1);
}

TEST_CASE("Carnet MVP, étage étendu : un résultat étiqueté va à sa commande", "[app][mvp]") {
  Bench bench;
  bench.SwitchOnPastSnapshots();
  bench.Advance(true);
  bench.stage.Invite("Ann", bench.link);
  bench.stage.Invite("Bob", bench.link);
  bench.stage.Invite("Cid", bench.link);
  const std::uint32_t ann = bench.Next().request_id;
  const std::uint32_t bob = bench.Next().request_id;
  const std::uint32_t cid = bench.Next().request_id;
  const std::uint8_t invite = Raw(MvpCommand::kInvite);

  SECTION("la troisième répond : les deux d'avant n'attendent plus rien") {
    bench.Apply(TaggedResult(kOk, cid, invite));
    CHECK(bench.stage.TakeNotices() == Lines({"Carnet MVP : invitation envoyée à Cid."}));
    bench.Apply(TaggedResult(kFull, ann, invite));
    bench.Apply(Result(kFull));
    CHECK(bench.stage.TakeNotices().empty());
  }
  SECTION("une étiquette qui ne désigne rien d'attendu ne prend la place de personne") {
    bench.Apply(TaggedResult(kFull, cid + 1, invite));
    CHECK(bench.stage.TakeNotices().empty());
    bench.Apply(TaggedResult(kOk, ann, invite));
    CHECK(bench.stage.TakeNotices() == Lines({"Carnet MVP : invitation envoyée à Ann."}));
  }
  SECTION("un écho de commande qui diffère retombe sur l'ordre d'envoi") {
    bench.Apply(TaggedResult(kFull, bob, Raw(MvpCommand::kKick)));
    CHECK(bench.stage.TakeNotices() ==
          Lines({std::string("Carnet MVP : invitation à Ann refusée — ") +
                 ro::ui::MvpResultText(kFull)}));
  }
}

TEST_CASE("Carnet MVP, étage étendu : une saisie se dit ajoutée, ou sans effet", "[app][mvp]") {
  Bench bench;
  bench.SwitchOnPastSnapshots();
  bench.Apply(Catalogue(kSlots));
  bench.windows.answer.manual_slot = 1;
  bench.windows.answer.manual_kill_time_s = kLocalS - 60;
  bench.Advance(true);
  const Command sent = bench.Next();
  REQUIRE(sent.cmd == Raw(MvpCommand::kReportManual));
  SECTION("acceptée par un serveur qui dit la saisie sans effet : elle est au carnet") {
    // La queue du groupe prouve la forme étendue.
    bench.Apply(GroupWith(0, 0, {}, 0));
    bench.Apply(TaggedResult(kOk, sent.request_id, sent.cmd));
    CHECK(bench.stage.TakeNotices() ==
          Lines({"Carnet MVP : heure de mort de Baphomet ajoutée au carnet du groupe."}));
  }
  SECTION("acceptée et étiquetée, sans queue qui prouve la forme étendue : transmise") {
    // Le serveur étiquette toute commande identifiée, même à une session à
    // qui il ramène la saisie sans effet à OK.
    bench.Apply(TaggedResult(kOk, sent.request_id, sent.cmd));
    CHECK(bench.stage.TakeNotices() ==
          Lines({"Carnet MVP : heure de mort de Baphomet transmise au groupe."}));
  }
  SECTION("sans effet, carnet fermé : le chat le dit, sans parler de refus") {
    bench.Apply(TaggedResult(kKept, sent.request_id, sent.cmd));
    CHECK(bench.stage.TakeNotices() ==
          Lines({"Carnet MVP : saisie de Baphomet sans effet — le groupe tient déjà une heure "
                 "au moins aussi précise."}));
  }
  SECTION("sans effet, carnet ouvert : le bandeau le dit, le chat se tait") {
    // Une seconde saisie, carnet ouvert au geste comme à la réponse.
    bench.windows.window.open = true;
    bench.Advance(true);
    bench.windows.answer.manual_slot = 1;
    bench.windows.answer.manual_kill_time_s = kLocalS - 30;
    bench.Advance(true);
    const Command again = bench.Next();
    REQUIRE(again.cmd == Raw(MvpCommand::kReportManual));
    bench.Apply(TaggedResult(kKept, again.request_id, again.cmd));
    CHECK(bench.stage.TakeNotices().empty());
    bench.Advance(true);
    CHECK(bench.windows.view->result_shown);
    CHECK(bench.windows.view->result_code == kKept);
    CHECK_FALSE(ro::ui::MvpResultRefuses(bench.windows.view->result_code));
  }
}

TEST_CASE("Carnet MVP, étage étendu : l'origine dite de l'invitation fait foi", "[app][mvp]") {
  Bench bench;
  bench.SwitchOnPastSnapshots();
  DismissInvitation(bench);
  const std::uint32_t later_ms = kNowMs + 1;
  SECTION("poussée dans l'image d'une réponse qui finit sur le groupe : elle se montre") {
    ApplySnapshotReply(bench, later_ms, 0);
    bench.Apply(InvitationFrom(kInviteGroup, kFromPush), later_ms);
    bench.Advance(true, later_ms);
    CHECK(bench.windows.view->pending_invite == kInviteGroup);
  }
  SECTION("renvoyée par l'instantané après un groupe d'une autre image : congédiée") {
    bench.Apply(Group(0, 0, ""), later_ms);
    bench.Advance(true, later_ms);
    bench.Apply(InvitationFrom(kInviteGroup, kFromSnapshot), later_ms);
    bench.Advance(true, later_ms);
    CHECK(bench.windows.view->pending_invite == 0);
  }
  SECTION("poussée en premier après une poignée de main, sans réponse : elle se montre") {
    bench.Advance(false, later_ms);
    bench.Advance(true, later_ms);
    bench.Apply(InvitationFrom(kInviteGroup, kFromPush), later_ms);
    bench.Advance(true, later_ms);
    CHECK(bench.windows.view->pending_invite == kInviteGroup);
  }
  SECTION("une origine que cette version ne connaît pas laisse l'ordre des trames décider") {
    constexpr std::uint8_t kFutureOrigin = 9;
    ApplySnapshotReply(bench, later_ms, 0);
    bench.Apply(InvitationFrom(kInviteGroup, kFutureOrigin), later_ms);
    bench.Advance(true, later_ms);
    CHECK(bench.windows.view->pending_invite == 0);
  }
}

TEST_CASE("Carnet MVP, étage étendu : la présence poussée ralentit l'instantané à intervalle",
          "[app][mvp]") {
  STATIC_CHECK(kMvpPushedPresenceRefreshMs > 2 * kMvpPresenceRefreshMs);
  Bench bench;
  bench.SwitchOn();
  bench.windows.window.open = true;
  bench.Advance(true);
  CHECK(bench.Next().cmd == Raw(MvpCommand::kSnapshot));
  bench.Apply(GroupWith(9, 5, {{.user_id = 5, .name = "Sting"}}, 5));
  bench.Advance(true, kNowMs + 2 * kMvpPresenceRefreshMs);
  CHECK(bench.Silent());
  // Le niveau d'un membre ne se pousse pas : il se redemande, lentement.
  bench.Advance(true, kNowMs + kMvpPushedPresenceRefreshMs - 1);
  CHECK(bench.Silent());
  bench.Advance(true, kNowMs + kMvpPushedPresenceRefreshMs);
  CHECK(bench.Next().cmd == Raw(MvpCommand::kSnapshot));
  bench.Apply(GroupWith(9, 5, {{.user_id = 5, .name = "Sting"}}, 5));

  // UNE NOUVELLE PARTIE se prouve à nouveau : sans queue, l'intervalle rapide
  // revient.
  const std::uint32_t replay_ms = kNowMs + kMvpPushedPresenceRefreshMs;
  bench.stage.StartSession(bench.windows);
  bench.windows.window.open = true;
  bench.Advance(true, replay_ms + kMvpPresenceRefreshMs);
  CHECK(bench.Next().cmd == Raw(MvpCommand::kSnapshot));
  bench.Advance(true, replay_ms + 2 * kMvpPresenceRefreshMs);
  CHECK(bench.Next().cmd == Raw(MvpCommand::kSnapshot));
}

TEST_CASE("Carnet MVP, étage étendu : le serveur dit notre compte, et nos lignes", "[app][mvp]") {
  Bench bench;
  bench.SwitchOnPastSnapshots();
  // Notre personnage n'est pas parmi les membres : le nom ne dirait rien.
  bench.own_name = "Absent";
  const std::vector<Member> members = {{.user_id = 5, .name = "Chef"},
                                       {.user_id = 6, .name = "Moi"},
                                       {.user_id = 6, .name = "Moi2"}};
  SECTION("notre compte possède") {
    bench.Apply(GroupWith(9, 5, members, 5));
    bench.Advance(false);
    CHECK(bench.windows.view->owner_known);
    CHECK(bench.windows.view->i_own);
    REQUIRE(bench.windows.view->members.size() == 3);
    CHECK(bench.windows.view->members[0].our_account);
    CHECK_FALSE(bench.windows.view->members[1].our_account);
  }
  SECTION("un autre compte possède ; toutes nos sessions sont « nous »") {
    bench.Apply(GroupWith(9, 5, members, 6));
    bench.Advance(false);
    CHECK(bench.windows.view->owner_known);
    CHECK_FALSE(bench.windows.view->i_own);
    REQUIRE(bench.windows.view->members.size() == 3);
    CHECK_FALSE(bench.windows.view->members[0].our_account);
    CHECK(bench.windows.view->members[1].our_account);
    CHECK(bench.windows.view->members[2].our_account);
  }
  SECTION("un compte non rattaché ne possède rien, et n'a pas de ligne") {
    bench.Apply(GroupWith(9, 0, members, 0));
    bench.Advance(false);
    CHECK(bench.windows.view->owner_known);
    CHECK_FALSE(bench.windows.view->i_own);
    CHECK_FALSE(bench.windows.view->members[0].our_account);
  }
  SECTION("sans queue, le nom décide, et aucune ligne n'est « nous »") {
    bench.Apply(GroupWith(9, 5, members));
    bench.own_name = "Moi";
    bench.Advance(false);
    CHECK(bench.windows.view->owner_known);
    CHECK_FALSE(bench.windows.view->i_own);
    CHECK_FALSE(bench.windows.view->members[1].our_account);
  }
}
