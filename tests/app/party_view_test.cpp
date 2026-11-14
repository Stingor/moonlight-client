// La fenêtre Groupe / Amis et la grille, composées : ma tuile hors groupe, les
// PV qu'on ne connaît pas, un PS périmé qui se tait, les droits du chef, et la
// demande reçue qu'on présente.

#include <array>
#include <cstddef>
#include <cstdint>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/friend_list.h"
#include "ro/sim/party_roster.h"

#include "party/party_compose.h"

using moonclient::PartyComposeInput;
using moonclient::PartyLens;
using moonclient::PartyMinimap;
using moonclient::PartyStatuses;
using moonclient::PartyViews;
using ro::sim::FriendList;
using ro::sim::PartyMember;
using ro::sim::PartyRoster;

namespace {

constexpr std::uint32_t kMe = 2000001;
constexpr std::uint32_t kAlice = 2000002;
constexpr std::uint32_t kBob = 2000003;
constexpr std::uint32_t kMyChar = 150001;
constexpr std::uint32_t kAliceChar = 150002;
constexpr std::uint32_t kStart = 10000;
constexpr std::uint32_t kPartyId = 77;

// Le jeu réduit à ce que la composition lui demande.
class FakeLens final : public PartyLens {
 public:
  [[nodiscard]] bool ActorPresent(std::uint32_t account_id) const override {
    return present.contains(account_id);
  }
  [[nodiscard]] ro::ui::SkinImage JobIcon(std::uint16_t /*job*/) override { return {}; }
  [[nodiscard]] std::string_view JobName(std::uint16_t /*job*/) const override { return "Acolyte"; }
  [[nodiscard]] std::string_view MapName(std::string_view map_raw) const override {
    return map_raw == "prontera.gat" ? "Prontera, capitale" : "";
  }
  [[nodiscard]] PartyMinimap Minimap(std::string_view /*map_raw*/) override { return {}; }
  [[nodiscard]] ro::ui::SkinImage Head(std::uint32_t /*account_id*/,
                                       std::uint32_t /*char_id*/) override {
    return {};
  }
  [[nodiscard]] ro::ui::SkinImage Crown() override { return {}; }
  [[nodiscard]] PartyStatuses Statuses(std::uint32_t /*account_id*/, bool is_me) override {
    return PartyStatuses{
        .cells = cells, .known = is_me, .genuine = genuine, .genuinely_known = is_me};
  }
  [[nodiscard]] const char* MvpInviteRefusal(std::string_view name,
                                             bool same_account) const override {
    mvp_asked.emplace_back(name, same_account);
    return mvp_refusal;
  }

  std::set<std::uint32_t> present;
  // La raison que le carnet donnerait, la même pour tous, et à qui on l'a
  // demandée.
  const char* mvp_refusal = nullptr;
  mutable std::vector<std::pair<std::string, bool>> mvp_asked;
  // Les mêmes cases pour tous : les `genuine` premières sont vraies.
  std::span<const ro::ui::StatusCellView> cells;
  std::size_t genuine = 0;
};

[[nodiscard]] PartyMember Member(std::uint32_t account_id, std::string name, bool leader) {
  PartyMember member;
  member.account_id = account_id;
  member.char_id = account_id == kAlice ? kAliceChar : kMyChar;
  member.name = std::move(name);
  member.map = "prontera.gat";
  member.leader = leader;
  return member;
}

[[nodiscard]] PartyComposeInput Input(const PartyRoster& party, const FriendList& friends,
                                      std::uint32_t now_ms) {
  PartyComposeInput input;
  input.party = &party;
  input.friends = &friends;
  input.self.account_id = kMe;
  input.self.char_id = kMyChar;
  input.self.name = "Moi";
  input.self.map_raw = "prontera.gat";
  input.self.hp_known = true;
  input.self.hp = 900;
  input.self.max_hp = 1000;
  input.self.sp_known = true;
  input.self.sp = 40;
  input.self.max_sp = 200;
  input.now_ms = now_ms;
  return input;
}

}  // namespace

TEST_CASE("Groupe, vues : hors groupe, la grille porte ma seule tuile", "[app][party]") {
  const PartyRoster party;
  const FriendList friends;
  FakeLens lens;
  PartyViews views;
  views.Compose(Input(party, friends, kStart), lens);

  CHECK_FALSE(views.window().in_party);
  CHECK(views.window().members.empty());
  CHECK_FALSE(views.window().options.known);
  REQUIRE(views.grid().tiles.size() == 1);
  const ro::ui::PartyGridTileView& me = views.grid().tiles[0];
  CHECK(me.is_me);
  CHECK(me.name == "Moi");
  CHECK(me.in_sight);
  CHECK(me.hp_known);
  CHECK(me.hp == 900);
  CHECK(me.sp_known);
  CHECK(me.max_sp == 200);
  CHECK(me.statuses_known);
}

TEST_CASE("Groupe, vues : le mode réglage ajoute des faux membres, un par état", "[app][party]") {
  const PartyRoster party;
  const FriendList friends;
  FakeLens lens;
  PartyViews views;
  PartyComposeInput input = Input(party, friends, kStart);
  input.fake_members = true;
  views.Compose(input, lens);

  const std::span<const ro::ui::PartyGridTileView> tiles = views.grid().tiles;
  REQUIRE(tiles.size() > 1);
  CHECK(tiles[0].is_me);
  bool leader = false;
  bool far = false;
  bool offline = false;
  bool down = false;
  for (const ro::ui::PartyGridTileView& tile : tiles.subspan(1)) {
    // 🔴 AUCUN NE PORTE UN VRAI COMPTE : leurs gestes sont écartés à ce signe.
    CHECK(moonclient::IsFakeMember(tile.account_id));
    CHECK_FALSE(tile.is_me);
    leader = leader || tile.is_leader;
    far = far || (!tile.offline && !tile.hp_known && !tile.in_sight);
    offline = offline || tile.offline;
    down = down || (tile.hp_known && tile.hp == 0);
  }
  CHECK(leader);
  CHECK(far);
  CHECK(offline);
  CHECK(down);
  // Le mode éteint, ils disparaissent.
  views.Compose(Input(party, friends, kStart), lens);
  CHECK(views.grid().tiles.size() == 1);
}

TEST_CASE("Groupe, vues : ma tuile en groupe prend mes valeurs, pas celles de la liste",
          "[app][party]") {
  PartyRoster party;
  PartyMember me = Member(kMe, "Moi", true);
  me.health = ro::sim::PartyGauge{.current = 1, .max = 5};
  party.Replace("Les Lunes", {me, Member(kAlice, "Alice", false)});
  const FriendList friends;
  FakeLens lens;
  PartyViews views;
  views.Compose(Input(party, friends, kStart), lens);

  REQUIRE(views.grid().tiles.size() == 2);
  CHECK(views.grid().tiles[0].is_me);
  CHECK(views.grid().tiles[0].hp == 900);
  CHECK(views.grid().tiles[0].max_hp == 1000);
  CHECK(views.window().members[0].map_name == "Prontera, capitale");
  CHECK(views.grid().tiles[1].same_map);
}

TEST_CASE("Groupe, vues : chaque surface a son aperçu des faux états", "[app][party]") {
  // 🔴 LES CASES SONT PARTAGÉES, PAS LES APERÇUS. Allumer celui de la grille
  // remplissait aussi la liste, et l'inverse.
  PartyRoster party;
  party.Replace("Les Lunes", {Member(kMe, "Moi", true), Member(kAlice, "Alice", false)});
  const FriendList friends;
  const std::array<ro::ui::StatusCellView, 3> cells{};
  FakeLens lens;
  lens.cells = cells;
  lens.genuine = 1;
  PartyViews views;
  PartyComposeInput input = Input(party, friends, kStart);
  input.grid_preview = true;
  views.Compose(input, lens);

  REQUIRE(views.grid().tiles.size() == 2);
  CHECK(views.grid().tiles[0].statuses.size() == 3);
  CHECK(views.window().members[0].statuses.size() == 1);

  input.grid_preview = false;
  input.window_preview = true;
  views.Compose(input, lens);
  CHECK(views.grid().tiles[0].statuses.size() == 1);
  CHECK(views.window().members[0].statuses.size() == 3);
}

TEST_CASE("Groupe, vues : des PV jamais reçus restent inconnus, pas nuls", "[app][party]") {
  PartyRoster party;
  PartyMember far = Member(kAlice, "Alice", false);
  PartyMember zero = Member(kBob, "Bob", false);
  zero.health = ro::sim::PartyGauge{.current = 0, .max = 0};
  party.Replace("Les Lunes", {Member(kMe, "Moi", true), far, zero});
  const FriendList friends;
  FakeLens lens;
  PartyViews views;
  views.Compose(Input(party, friends, kStart), lens);

  REQUIRE(views.window().members.size() == 3);
  CHECK_FALSE(views.window().members[1].hp_known);
  CHECK_FALSE(views.window().members[2].hp_known);
  CHECK_FALSE(views.grid().tiles[1].hp_known);
  CHECK_FALSE(views.grid().tiles[1].in_sight);
  CHECK_FALSE(views.grid().tiles[1].statuses_known);
}

TEST_CASE("Groupe, vues : un PS périmé se tait", "[app][party]") {
  PartyRoster party;
  party.Replace("Les Lunes", {Member(kMe, "Moi", true), Member(kAlice, "Alice", false)});
  REQUIRE(party.TellMagic(kAlice, ro::sim::PartyGauge{.current = 30, .max = 120}, kStart));
  const FriendList friends;
  FakeLens lens;
  PartyViews views;

  views.Compose(Input(party, friends, kStart + 1000), lens);
  CHECK(views.grid().tiles[1].sp_known);
  CHECK(views.grid().tiles[1].sp == 30);

  views.Compose(Input(party, friends, kStart + ro::sim::kPartyMagicStaleMs + 1), lens);
  CHECK_FALSE(views.grid().tiles[1].sp_known);
  CHECK_FALSE(views.window().members[1].sp_known);
}

TEST_CASE("Groupe, vues : les droits du chef suivent la liste", "[app][party]") {
  PartyRoster party;
  party.Replace("Les Lunes", {Member(kMe, "Moi", false), Member(kAlice, "Alice", true)});
  const FriendList friends;
  FakeLens lens;
  lens.present.insert(kAlice);
  PartyViews views;
  views.Compose(Input(party, friends, kStart), lens);
  CHECK_FALSE(views.window().i_am_leader);
  CHECK_FALSE(views.grid().i_am_leader);
  CHECK(views.window().members[1].actor_present);
  CHECK(views.grid().tiles[1].in_sight);
  CHECK(views.grid().tiles[1].is_leader);

  REQUIRE(party.TellLeader(kAlice, kMe));
  views.Compose(Input(party, friends, kStart), lens);
  CHECK(views.window().i_am_leader);
  CHECK(views.grid().i_am_leader);
}

TEST_CASE("Groupe, vues : la demande la plus récente passe devant", "[app][party]") {
  PartyRoster party;
  FriendList friends;
  FakeLens lens;
  PartyViews views;

  party.TellInvitation(ro::sim::PartyInvitation{.party_id = kPartyId, .party_name = "Les Lunes"});
  views.Compose(Input(party, friends, kStart), lens);
  REQUIRE(views.window().invite.active);
  CHECK_FALSE(views.window().invite.is_friend);
  CHECK(views.window().invite.party_id == kPartyId);
  CHECK(views.window().invite.name == "Les Lunes");

  friends.TellInvitation(
      ro::sim::FriendInvitation{.account_id = kAlice, .char_id = kAliceChar, .name = "Alice"});
  views.Compose(Input(party, friends, kStart), lens);
  REQUIRE(views.window().invite.active);
  CHECK(views.window().invite.is_friend);
  CHECK(views.window().invite.name == "Alice");

  // Répondue, elle laisse la place à celle qui attendait.
  friends.ForgetInvitation();
  views.Compose(Input(party, friends, kStart), lens);
  REQUIRE(views.window().invite.active);
  CHECK_FALSE(views.window().invite.is_friend);

  party.ForgetInvitation();
  views.Compose(Input(party, friends, kStart), lens);
  CHECK_FALSE(views.window().invite.active);
}

TEST_CASE("Groupe, vues : un ami membre du groupe, et le partage sans le bit de carte",
          "[app][party]") {
  PartyRoster party;
  PartyMember alice = Member(kAlice, "Alice", false);
  alice.map = "geffen.gat";
  party.Replace("Les Lunes", {Member(kMe, "Moi", true), alice});
  party.TellShare(0, {ro::sim::PartyShareRow{.account_id = kAlice, .flags = ro::sim::kShareIdle}});
  FriendList friends;
  friends.Replace({ro::sim::Friend{.account_id = kAlice, .char_id = kAliceChar, .name = "Alice"}});
  FakeLens lens;
  PartyViews views;
  views.Compose(Input(party, friends, kStart), lens);

  REQUIRE(views.window().members.size() == 2);
  CHECK(views.window().members[1].is_friend);
  CHECK(views.window().members[1].share_flags == ro::sim::kShareIdle);
  CHECK_FALSE(views.grid().tiles[1].same_map);
  REQUIRE(views.window().friends.size() == 1);
  CHECK(views.window().friends[0].in_my_party);
}

TEST_CASE("Groupe, vues : le refus du carnet MVP suit chaque rangée, sauf la mienne",
          "[app][party]") {
  PartyRoster party;
  party.Replace("Les Lunes", {Member(kMe, "Moi", true), Member(kAlice, "Alice", false)});
  FriendList friends;
  friends.Replace({ro::sim::Friend{.account_id = kBob, .char_id = kAliceChar, .name = "Bob"}});
  FakeLens lens;
  constexpr const char* kRefusal = "Pas dans ta guilde ni tes amis.";
  lens.mvp_refusal = kRefusal;
  PartyViews views;
  views.Compose(Input(party, friends, kStart), lens);

  REQUIRE(views.window().members.size() == 2);
  CHECK(views.window().members[0].is_me);
  CHECK(views.window().members[0].mvp_refusal == nullptr);
  CHECK(views.window().members[1].mvp_refusal == kRefusal);
  REQUIRE(views.window().friends.size() == 1);
  CHECK(views.window().friends[0].mvp_refusal == kRefusal);
  // Le juge est interrogé par nom, ni sur moi ni sur mon compte.
  const std::vector<std::pair<std::string, bool>> expected = {{"Alice", false}, {"Bob", false}};
  CHECK(lens.mvp_asked == expected);

  // Offerte, la rangée ne porte aucune raison ; et sans juge, rien ne s'y oppose.
  lens.mvp_refusal = nullptr;
  views.Compose(Input(party, friends, kStart), lens);
  CHECK(views.window().members[1].mvp_refusal == nullptr);
  CHECK(views.window().friends[0].mvp_refusal == nullptr);
  CHECK(lens.PartyLens::MvpInviteRefusal("Alice", false) == nullptr);
}
