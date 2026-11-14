// La liste d'amis tenue hors du monde.

#include "ro/sim/friend_list.h"

#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::sim::Friend;
using ro::sim::FriendList;

TEST_CASE("Amis (sim) : la liste se pose hors ligne, puis se rallume", "[friend]") {
  FriendList friends;
  friends.Replace({Friend{.account_id = 1, .char_id = 10, .name = "Alice", .online = true},
                   Friend{.account_id = 2, .char_id = 20, .name = "Bob"}});
  REQUIRE(friends.friends().size() == 2);
  CHECK_FALSE(friends.FindByName("Alice")->online);
  CHECK(friends.TellOnline(1, 10, true, "Alice"));
  CHECK(friends.Find(1, 10)->online);
  // Un autre personnage du même compte n'est pas notre ami.
  CHECK_FALSE(friends.TellOnline(1, 11, true, "Alicia"));
  CHECK(friends.HasName("Bob"));
  CHECK_FALSE(friends.HasName("bob"));
  CHECK(friends.FindByAccount(2)->name == "Bob");
}

TEST_CASE("Amis (sim) : ajout, retrait, plafond", "[friend]") {
  FriendList friends;
  friends.Add(Friend{.account_id = 3, .char_id = 30, .name = "Carole", .online = true});
  friends.Add(Friend{.account_id = 3, .char_id = 30, .name = "Carole"});
  CHECK(friends.friends().size() == 1);
  CHECK(friends.Remove(3, 30));
  CHECK_FALSE(friends.Remove(3, 30));

  std::vector<Friend> many(ro::sim::kMaxFriends + 5);
  friends.Replace(std::move(many));
  CHECK(friends.friends().size() == ro::sim::kMaxFriends);
  CHECK(friends.full());
}

TEST_CASE("Amis (sim) : une demande attend, un avis se prend une fois", "[friend]") {
  FriendList friends;
  friends.TellInvitation(ro::sim::FriendInvitation{.account_id = 4, .char_id = 40, .name = "Dan"});
  REQUIRE(friends.invitation().has_value());
  CHECK(friends.invitation()->name == "Dan");
  friends.ForgetInvitation();
  CHECK_FALSE(friends.invitation().has_value());

  friends.Notify(
      ro::sim::FriendNotice{.kind = ro::sim::FriendNoticeKind::kDeclined, .name = "Eve"});
  CHECK(friends.TakeNotices().size() == 1);
  CHECK(friends.TakeNotices().empty());
}
