#include "chat/player_link_judge.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/friend_list.h"
#include "ro/sim/guild.h"
#include "ro/sim/mvp_tracker.h"
#include "ro/sim/party_roster.h"
#include "ro/ui/mvp_tracker_window.h"

// LE MENU D'UN PSEUDO GRISE CE QUE LE SERVEUR REFUSERAIT, et pour la raison
// qu'il aurait.

using moonclient::JudgeMvpInvite;
using moonclient::JudgePlayerLink;
using moonclient::MvpInviteCircle;
using moonclient::PlayerLinkCircle;

namespace {

constexpr std::uint32_t kMyAccount = 100;
constexpr std::uint32_t kMyChar = 200;

}  // namespace

TEST_CASE("Pseudo : sans groupe, sans guilde, tout s'explique", "[app][whisper]") {
  const ro::sim::PartyRoster party;
  const ro::sim::GuildBook guild;
  const ro::sim::FriendList friends;
  const ro::ui::PlayerLinkRefusals refused =
      JudgePlayerLink("Bob", PlayerLinkCircle{.party = party,
                                              .guild = guild,
                                              .friends = friends,
                                              .own_account_id = kMyAccount,
                                              .own_char_id = kMyChar});
  // ⚠ LE SERVEUR REFUSERAIT LES DEUX INVITATIONS SANS UN MOT.
  CHECK(refused.party != nullptr);
  CHECK(refused.guild != nullptr);
  // L'amitié, elle, reste offerte.
  CHECK(refused.befriend == nullptr);
}

TEST_CASE("Pseudo : un ami ne se redemande pas, une liste pleine non plus", "[app][whisper]") {
  const ro::sim::PartyRoster party;
  const ro::sim::GuildBook guild;
  ro::sim::FriendList friends;
  friends.Add(ro::sim::Friend{.account_id = 1, .char_id = 2, .name = "Bob"});
  const PlayerLinkCircle circle{.party = party,
                                .guild = guild,
                                .friends = friends,
                                .own_account_id = kMyAccount,
                                .own_char_id = kMyChar};
  CHECK(JudgePlayerLink("Bob", circle).befriend != nullptr);
  CHECK(JudgePlayerLink("Carol", circle).befriend == nullptr);

  ro::sim::FriendList full;
  for (std::uint32_t rank = 0; rank < ro::sim::kMaxFriends; ++rank) {
    full.Add(ro::sim::Friend{
        .account_id = rank + 1, .char_id = rank + 1, .name = "Ami" + std::to_string(rank)});
  }
  CHECK(JudgePlayerLink("Carol", PlayerLinkCircle{.party = party,
                                                  .guild = guild,
                                                  .friends = full,
                                                  .own_account_id = kMyAccount,
                                                  .own_char_id = kMyChar})
            .befriend != nullptr);
}

TEST_CASE("Pseudo : un membre de ma guilde ne s'y invite pas", "[app][whisper]") {
  const ro::sim::PartyRoster party;
  ro::sim::GuildBook guild;
  guild.TellBelonging(ro::sim::GuildBelonging{.guild_id = 7, .name = "Lune"});
  guild.ReplaceMembers({ro::sim::GuildMember{.account_id = 3, .char_id = 4, .name = "Bob"}});
  const ro::sim::FriendList friends;
  const PlayerLinkCircle circle{.party = party,
                                .guild = guild,
                                .friends = friends,
                                .own_account_id = kMyAccount,
                                .own_char_id = kMyChar};
  CHECK(JudgePlayerLink("Bob", circle).guild != nullptr);
  // Les droits de notre position ne sont pas connus : l'invitation reste offerte.
  CHECK(JudgePlayerLink("Carol", circle).guild == nullptr);
}

// ── « Inviter dans mon carnet MVP » ─────────────────────────────────────────

namespace {

// Un carnet dont le groupe de chasse compte `members` parmi ses personnages.
[[nodiscard]] ro::sim::MvpTrackerBook HuntWith(std::uint32_t group_id,
                                               std::vector<std::string> members) {
  ro::sim::MvpTrackerBook book;
  ro::sim::MvpGroup group{.group_id = group_id, .owner_user_id = 1, .name = "Chasse"};
  for (std::string& name : members) {
    group.members.push_back(ro::sim::MvpMember{.user_id = 2, .online = true, .name = name});
  }
  book.ReceiveGroup(std::move(group), /*members_complete=*/true);
  return book;
}

}  // namespace

TEST_CASE("Carnet MVP : les cinq raisons, dans leur ordre", "[app][whisper][mvp]") {
  ro::sim::GuildBook guild;
  guild.TellBelonging(ro::sim::GuildBelonging{.guild_id = 7, .name = "Lune"});
  guild.ReplaceMembers({ro::sim::GuildMember{.account_id = 3, .char_id = 4, .name = "Gilda"}});
  ro::sim::FriendList friends;
  friends.Add(ro::sim::Friend{.account_id = 1, .char_id = 2, .name = "Bob"});
  // « Zed » est du groupe de chasse sans être ni ami ni de la guilde.
  const ro::sim::MvpTrackerBook hunting = HuntWith(9, {"Bob", "Zed"});
  const ro::sim::MvpTrackerBook alone = HuntWith(0, {});

  // 1. LA LIAISON NON VÉRIFIÉE l'emporte sur tout, carnet éteint compris : le
  // serveur jetterait l'invitation sans répondre.
  const char* pending =
      JudgeMvpInvite("Gilda", MvpInviteCircle{.book = hunting, .guild = guild, .friends = friends});
  REQUIRE(pending != nullptr);
  CHECK(std::string_view(pending) == ro::ui::MvpLinkPendingText());
  CHECK(JudgeMvpInvite("Inconnu",
                       MvpInviteCircle{.book = alone, .guild = guild, .friends = friends}) ==
        pending);

  // 2. LE CARNET ÉTEINT, même sur un étranger sans groupe.
  const char* off = JudgeMvpInvite(
      "Inconnu",
      MvpInviteCircle{.book = alone, .guild = guild, .friends = friends, .link_ready = true});
  REQUIRE(off != nullptr);
  CHECK(std::string(off).find("éteint") != std::string::npos);
  // Et sur un membre du groupe.
  CHECK(JudgeMvpInvite(
            "Bob",
            MvpInviteCircle{
                .book = hunting, .guild = guild, .friends = friends, .link_ready = true}) == off);

  // 3. AUCUN GROUPE DE CHASSE, même pour un ami.
  const MvpInviteCircle groupless{
      .book = alone, .guild = guild, .friends = friends, .enabled = true, .link_ready = true};
  const char* no_group = JudgeMvpInvite("Bob", groupless);
  REQUIRE(no_group != nullptr);
  CHECK(std::string(no_group).find("groupe de chasse") != std::string::npos);
  // 🔴 ET POUR UN ÉTRANGER : l'absence de groupe se dit avant le cercle.
  CHECK(JudgeMvpInvite("Inconnu", groupless) == no_group);

  const MvpInviteCircle circle{
      .book = hunting, .guild = guild, .friends = friends, .enabled = true, .link_ready = true};
  // 4. NI AMI NI MEMBRE DE LA GUILDE.
  const char* stranger = JudgeMvpInvite("Inconnu", circle);
  REQUIRE(stranger != nullptr);
  CHECK(std::string(stranger).find("amis") != std::string::npos);
  // ⚠ LA CASSE COMPTE : les noms se comparent exactement.
  CHECK(JudgeMvpInvite("bob", circle) == stranger);
  // 🔴 UN MEMBRE DU GROUPE HORS DU CERCLE : le cercle se dit avant l'appartenance.
  CHECK(JudgeMvpInvite("Zed", circle) == stranger);

  // 5. DÉJÀ MEMBRE DU GROUPE.
  const char* member = JudgeMvpInvite("Bob", circle);
  REQUIRE(member != nullptr);
  CHECK(std::string(member).find("Déjà dans votre carnet") != std::string::npos);

  // OFFERTE : un compagnon de guilde qui n'est pas encore du groupe.
  CHECK(JudgeMvpInvite("Gilda", circle) == nullptr);
  // Et un ami qui ne l'est pas non plus.
  friends.Add(ro::sim::Friend{.account_id = 5, .char_id = 6, .name = "Carol"});
  CHECK(JudgeMvpInvite("Carol", circle) == nullptr);

  // 🔴 UN AUTRE PERSONNAGE DE NOTRE COMPTE, de la guilde : déjà du groupe, puisque
  // nous y sommes — le serveur refuserait de nous inviter nous-mêmes.
  CHECK(JudgeMvpInvite("Gilda", circle, /*same_account=*/true) == member);
  // Les quatre premières raisons passent toujours avant.
  CHECK(JudgeMvpInvite("Gilda", groupless, /*same_account=*/true) == no_group);
  CHECK(JudgeMvpInvite("Inconnu", circle, /*same_account=*/true) == stranger);
}

TEST_CASE("Carnet MVP : le menu d'un pseudo en porte la raison", "[app][whisper][mvp]") {
  const ro::sim::PartyRoster party;
  const ro::sim::GuildBook guild;
  const ro::sim::FriendList friends;
  const PlayerLinkCircle bare{.party = party,
                              .guild = guild,
                              .friends = friends,
                              .own_account_id = kMyAccount,
                              .own_char_id = kMyChar};
  // SANS CERCLE DU CARNET, rien n'est refusé d'avance.
  CHECK(JudgePlayerLink("Bob", bare).mvp == nullptr);
  CHECK_FALSE(JudgePlayerLink("Bob", bare).mvp_withdrawn);

  const ro::sim::MvpTrackerBook book;
  const MvpInviteCircle hunt{.book = book, .guild = guild, .friends = friends};
  PlayerLinkCircle with_hunt = bare;
  with_hunt.mvp = &hunt;
  const char* refused = JudgePlayerLink("Bob", with_hunt).mvp;
  REQUIRE(refused != nullptr);
  CHECK(refused == JudgeMvpInvite("Bob", hunt));
}

TEST_CASE("Carnet MVP : éteint, le menu d'un pseudo n'offre pas l'invitation",
          "[app][whisper][mvp]") {
  const ro::sim::PartyRoster party;
  const ro::sim::GuildBook guild;
  const ro::sim::FriendList friends;
  const ro::sim::MvpTrackerBook book;
  for (const bool enabled : {false, true}) {
    INFO(enabled);
    const MvpInviteCircle hunt{
        .book = book, .guild = guild, .friends = friends, .enabled = enabled, .link_ready = true};
    const PlayerLinkCircle circle{.party = party,
                                  .guild = guild,
                                  .friends = friends,
                                  .own_account_id = kMyAccount,
                                  .own_char_id = kMyChar,
                                  .mvp = &hunt};
    CHECK(JudgePlayerLink("Bob", circle).mvp_withdrawn == !enabled);
  }
  // LA RAISON, dernier rempart si un menu l'offrait quand même.
  const MvpInviteCircle off{.book = book, .guild = guild, .friends = friends, .link_ready = true};
  CHECK(std::string_view(JudgeMvpInvite("Bob", off)) == ro::ui::MvpTrackerOffText());
}
