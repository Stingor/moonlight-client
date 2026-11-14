#include "game/chat_room_rules.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/actor.h"
#include "ro/sim/chat_room.h"
#include "ro/sim/skill_book.h"
#include "ro/sim/world.h"

#include "game/chat_room_memory.h"

// LES SALONS VUS DE L'ÉCRAN : ce qui empêche d'en ouvrir un, ce que la zone en
// montre, et ce que le formulaire retient.

using moonclient::ChatRoomBlocker;
using moonclient::ChatRoomCreateBlocker;
using moonclient::ChatRoomStoreState;
using ro::sim::Actor;
using ro::sim::ActorKind;
using ro::sim::CellPosition;
using ro::sim::World;

namespace {

constexpr std::uint32_t kPlayerId = 2'000'001;

[[nodiscard]] Actor At(std::uint32_t id, ActorKind kind, CellPosition where) {
  Actor actor;
  actor.id = id;
  actor.kind = kind;
  actor.position = where;
  return actor;
}

// Un joueur posé en (50, 50), Basic Skill appris au rang voulu.
[[nodiscard]] World Ready(std::uint16_t basic_level) {
  World world;
  world.set_player_id(kPlayerId);
  world.Spawn(At(kPlayerId, ActorKind::kPlayer, CellPosition{50, 50, 0}));
  ro::sim::KnownSkill basic;
  basic.id = moonclient::kBasicSkillId;
  basic.level = basic_level;
  world.skill_book().Learn(basic);
  return world;
}

[[nodiscard]] Actor RoomHost(std::uint32_t id, CellPosition where, std::uint32_t room_id,
                             std::string title, std::uint8_t status) {
  Actor host = At(id, ActorKind::kPlayer, where);
  ro::sim::Signboard board;
  board.kind = ro::sim::SignboardKind::kChatRoom;
  board.room_id = room_id;
  board.title = std::move(title);
  board.users = 1;
  board.limit = 20;
  board.status = status;
  host.signboard = board;
  return host;
}

std::filesystem::path FreshRoot(const char* name) {
  const std::filesystem::path root = std::filesystem::temp_directory_path() / name;
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root / moonclient::kSaveFolder);
  return root;
}

}  // namespace

TEST_CASE("Salon : ce qui empêche d'en ouvrir un se dit avant le clic", "[app][salon]") {
  World world = Ready(moonclient::kChatRoomBasicSkillLevel);
  CHECK(ChatRoomCreateBlocker(world, {}) == ChatRoomBlocker::kNone);
  CHECK(ChatRoomCreateBlocker(world, ChatRoomStoreState{.in_room = true}) ==
        ChatRoomBlocker::kInRoom);
  CHECK(ChatRoomCreateBlocker(world, ChatRoomStoreState{.vending_open = true}) ==
        ChatRoomBlocker::kVending);
  CHECK(ChatRoomCreateBlocker(world, ChatRoomStoreState{.buying_store_open = true}) ==
        ChatRoomBlocker::kBuyingStore);

  World novice = Ready(moonclient::kChatRoomBasicSkillLevel - 1);
  CHECK(ChatRoomCreateBlocker(novice, {}) == ChatRoomBlocker::kBasicSkill);

  // UN PNJ à la distance du serveur empêche ; un pas plus loin, non.
  world.Spawn(At(10, ActorKind::kNonPlayer,
                 CellPosition{50 + moonclient::kChatRoomNpcClearanceCells + 1, 50, 0}));
  CHECK(ChatRoomCreateBlocker(world, {}) == ChatRoomBlocker::kNone);
  world.Spawn(At(11, ActorKind::kNonPlayer,
                 CellPosition{50, 50 + moonclient::kChatRoomNpcClearanceCells, 0}));
  CHECK(ChatRoomCreateBlocker(world, {}) == ChatRoomBlocker::kNpcNear);
}

TEST_CASE("Salon : un familier n'est pas un PNJ, une enseigne sur la case empêche",
          "[app][salon]") {
  World world = Ready(moonclient::kChatRoomBasicSkillLevel);
  Actor pet = At(10, ActorKind::kNonPlayer, CellPosition{51, 50, 0});
  pet.familiar = ro::sim::Familiar::kPet;
  world.Spawn(pet);
  CHECK(ChatRoomCreateBlocker(world, {}) == ChatRoomBlocker::kNone);
  world.Spawn(RoomHost(12, CellPosition{50, 50, 0}, 3, "Ici", ro::sim::kRoomPublic));
  CHECK(ChatRoomCreateBlocker(world, {}) == ChatRoomBlocker::kCellTaken);
}

TEST_CASE("Salon : les salons alentour, du plus proche, et le titre déjà pris", "[app][salon]") {
  World world = Ready(moonclient::kChatRoomBasicSkillLevel);
  world.Spawn(RoomHost(20, CellPosition{60, 50, 0}, 7, "Loin", ro::sim::kRoomPublic));
  world.Spawn(RoomHost(21, CellPosition{52, 50, 0}, 8, "Chasse MVP", ro::sim::kRoomPrivate));
  const std::vector<moonclient::NearbyChatRoom> rooms = moonclient::NearbyChatRooms(world);
  REQUIRE(rooms.size() == 2);
  CHECK(rooms[0].room_id == 8);
  CHECK(rooms[0].distance_cells == 2);
  CHECK(rooms[1].room_id == 7);
  CHECK(moonclient::ChatRoomTitleTaken(rooms, "chasse mvp") == 0);
  CHECK(moonclient::ChatRoomTitleTaken(rooms, "Chasse") == -1);
  CHECK(moonclient::ChatRoomTitleTaken(rooms, "") == -1);
}

TEST_CASE("Salon : une étiquette de PNJ, salon sans place, n'est pas dans l'annuaire",
          "[app][salon]") {
  World world = Ready(moonclient::kChatRoomBasicSkillLevel);
  Actor label = RoomHost(22, CellPosition{51, 50, 0}, 9, "Personne en PVP", ro::sim::kRoomPublic);
  label.kind = ActorKind::kNonPlayer;
  label.signboard->limit = 0;
  world.Spawn(label);
  world.Spawn(RoomHost(23, CellPosition{53, 50, 0}, 10, "Vente", ro::sim::kRoomPublic));
  const std::vector<moonclient::NearbyChatRoom> rooms = moonclient::NearbyChatRooms(world);
  REQUIRE(rooms.size() == 1);
  CHECK(rooms[0].room_id == 10);
}

TEST_CASE("Salon : l'enseigne dit comment on y entre", "[app][salon]") {
  using moonclient::ChatRoomJoinStep;
  using moonclient::ChatRoomJoinStepOf;
  CHECK(ChatRoomJoinStepOf(ro::sim::kRoomPrivate) == ChatRoomJoinStep::kAskPassword);
  CHECK(ChatRoomJoinStepOf(ro::sim::kRoomPublic) == ChatRoomJoinStep::kEnter);
  CHECK(ChatRoomJoinStepOf(ro::sim::kRoomArena) == ChatRoomJoinStep::kEnter);
  CHECK(ChatRoomJoinStepOf(ro::sim::kRoomPkZone) == ChatRoomJoinStep::kNone);
}

TEST_CASE("Salon : l'arrivée d'un autre membre sonne, ni son départ ni le joueur", "[app][salon]") {
  using moonclient::ChatRoomNewsRings;
  using ro::sim::ChatRoomNews;
  using ro::sim::ChatRoomNewsKind;
  constexpr std::string_view kSelf = "Moi";
  constexpr std::uint16_t kTwoUsers = 2;
  constexpr std::uint16_t kThreeUsers = 3;
  constexpr std::uint32_t kRoomId = 7;

  // Les nouvelles telles que le salon les forme, dans l'ordre du fil.
  ro::sim::ChatRoom room;
  room.TellEntered(ro::sim::ChatRoomInside{.room_id = kRoomId,
                                           .members = {{.name = "Hote", .owner = true},
                                                       {.name = std::string(kSelf)}}},
                   kSelf);
  static_cast<void>(room.TakeNews());
  room.TellMemberJoined(kThreeUsers, "Tiers");
  room.TellMemberJoined(kThreeUsers, std::string(kSelf));
  room.TellMemberLeft(kTwoUsers, "Tiers", false, kSelf);
  room.TellMemberJoined(kThreeUsers, "Tiers");
  room.TellMemberLeft(kTwoUsers, "Tiers", true, kSelf);
  const std::vector<ChatRoomNews> news = room.TakeNews();
  REQUIRE(news.size() == 5);
  CHECK(ChatRoomNewsRings(news[0], kSelf));
  CHECK_FALSE(ChatRoomNewsRings(news[1], kSelf));
  CHECK_FALSE(ChatRoomNewsRings(news[2], kSelf));
  CHECK(ChatRoomNewsRings(news[3], kSelf));
  CHECK_FALSE(ChatRoomNewsRings(news[4], kSelf));

  // Aucune autre nouvelle ne sonne, même portant un nom.
  for (const ChatRoomNewsKind kind :
       {ChatRoomNewsKind::kCreated, ChatRoomNewsKind::kEnterRefused, ChatRoomNewsKind::kMemberLeft,
        ChatRoomNewsKind::kMemberExpelled, ChatRoomNewsKind::kExpelRefused}) {
    CHECK_FALSE(ChatRoomNewsRings(ChatRoomNews{.kind = kind, .name = "Tiers"}, kSelf));
  }
}

TEST_CASE("Salon : le formulaire se retient, titres récents en tête et sans doublon",
          "[app][salon]") {
  moonclient::ChatRoomMemory memory;
  for (std::size_t rank = 0; rank <= moonclient::kChatRoomRecentTitles; ++rank) {
    memory.RememberTitle("Salon " + std::to_string(rank));
  }
  memory.RememberTitle("Salon 3");
  REQUIRE(memory.recent_titles.size() == moonclient::kChatRoomRecentTitles);
  CHECK(memory.recent_titles.front() == "Salon 3");
  CHECK(memory.recent_titles[1] == "Salon 10");

  memory.title = "Vente";
  memory.limit = 7;
  memory.is_public = false;
  const std::filesystem::path root =
      FreshRoot(("moonclient_salons_essai_" +
                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))
                    .c_str());
  REQUIRE(moonclient::SaveChatRoomMemory(root, memory));
  moonclient::ChatRoomMemory read;
  moonclient::LoadChatRoomMemory(root, read);
  CHECK(read.title == "Vente");
  CHECK(read.limit == 7);
  CHECK_FALSE(read.is_public);
  CHECK(read.recent_titles == memory.recent_titles);
  std::filesystem::remove_all(root);
}
