// LE MENU D'UN MEMBRE DU SALON : ses lignes, celles qui se grisent, et
// l'intention que chacune émet.

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/chat_room_window.h"

using ro::ui::ChatRoomMemberEntry;
using ro::ui::ChatRoomMemberMenu;
using ro::ui::ChatRoomMemberVerb;
using ro::ui::ChatRoomMemberView;
using ro::ui::ChatRoomView;
using ro::ui::ChatRoomWindowAction;

namespace {

[[nodiscard]] std::vector<ChatRoomMemberVerb> VerbsOf(
    const std::vector<ChatRoomMemberEntry>& menu) {
  std::vector<ChatRoomMemberVerb> verbs;
  verbs.reserve(menu.size());
  for (const ChatRoomMemberEntry& entry : menu) {
    verbs.push_back(entry.verb);
  }
  return verbs;
}

[[nodiscard]] const ChatRoomMemberEntry* Find(const std::vector<ChatRoomMemberEntry>& menu,
                                              ChatRoomMemberVerb verb) {
  const auto found = std::ranges::find(menu, verb, &ChatRoomMemberEntry::verb);
  return found == menu.end() ? nullptr : &*found;
}

}  // namespace

TEST_CASE("Salon : le menu d'un autre membre offre l'amitié et l'enregistrement",
          "[ui][chat_room]") {
  const ChatRoomView view;
  const ChatRoomMemberView member{.name = "Alice"};
  CHECK(VerbsOf(ChatRoomMemberMenu(member, view)) ==
        std::vector{ChatRoomMemberVerb::kBefriend, ChatRoomMemberVerb::kSaveChat});
  // Permises, sans raison à dire.
  for (const ChatRoomMemberEntry& entry : ChatRoomMemberMenu(member, view)) {
    CHECK(entry.refusal == nullptr);
  }
}

TEST_CASE("Salon : le chef y trouve en plus l'expulsion et ses droits", "[ui][chat_room]") {
  ChatRoomView view;
  view.owned = true;
  const ChatRoomMemberView member{.name = "Alice"};
  CHECK(VerbsOf(ChatRoomMemberMenu(member, view)) ==
        std::vector{ChatRoomMemberVerb::kExpel, ChatRoomMemberVerb::kGiveOwner,
                    ChatRoomMemberVerb::kBefriend, ChatRoomMemberVerb::kSaveChat});
}

TEST_CASE("Salon : on ne s'ajoute jamais soi-même en ami", "[ui][chat_room]") {
  ChatRoomView view;
  view.owned = true;
  const ChatRoomMemberView me{.name = "Moi", .owner = true, .me = true};
  // 🔴 NI AMITIÉ NI GESTE DE CHEF sur soi : il reste l'enregistrement seul.
  CHECK(VerbsOf(ChatRoomMemberMenu(me, view)) == std::vector{ChatRoomMemberVerb::kSaveChat});
}

TEST_CASE("Salon : l'amitié se grise pour un ami, ou une liste pleine", "[ui][chat_room]") {
  ChatRoomView view;
  const ChatRoomMemberView chum{.name = "Alice", .befriended = true};
  const std::vector<ChatRoomMemberEntry> chum_menu = ChatRoomMemberMenu(chum, view);
  const ChatRoomMemberEntry* already = Find(chum_menu, ChatRoomMemberVerb::kBefriend);
  REQUIRE(already != nullptr);
  CHECK(already->refusal != nullptr);

  view.friends_full = true;
  const ChatRoomMemberView stranger{.name = "Bob"};
  const std::vector<ChatRoomMemberEntry> menu = ChatRoomMemberMenu(stranger, view);
  const ChatRoomMemberEntry* full = Find(menu, ChatRoomMemberVerb::kBefriend);
  REQUIRE(full != nullptr);
  CHECK(full->refusal != nullptr);
  // L'enregistrement, lui, ne dépend de personne.
  const ChatRoomMemberEntry* save = Find(menu, ChatRoomMemberVerb::kSaveChat);
  REQUIRE(save != nullptr);
  CHECK(save->refusal == nullptr);
}

TEST_CASE("Salon : chaque ligne du menu émet son intention", "[ui][chat_room]") {
  ChatRoomWindowAction befriend;
  ro::ui::ChooseChatRoomMemberVerb(ChatRoomMemberVerb::kBefriend, "Alice", befriend);
  CHECK(befriend.befriend == std::optional<std::string>("Alice"));
  CHECK_FALSE(befriend.save_chat);
  CHECK_FALSE(befriend.expel.has_value());

  ChatRoomWindowAction save;
  ro::ui::ChooseChatRoomMemberVerb(ChatRoomMemberVerb::kSaveChat, "Alice", save);
  CHECK(save.save_chat);
  CHECK_FALSE(save.befriend.has_value());

  ChatRoomWindowAction expel;
  ro::ui::ChooseChatRoomMemberVerb(ChatRoomMemberVerb::kExpel, "Bob", expel);
  CHECK(expel.expel == std::optional<std::string>("Bob"));

  ChatRoomWindowAction owner;
  ro::ui::ChooseChatRoomMemberVerb(ChatRoomMemberVerb::kGiveOwner, "Bob", owner);
  CHECK(owner.give_owner == std::optional<std::string>("Bob"));
}
