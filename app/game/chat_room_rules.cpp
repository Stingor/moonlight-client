#include "game/chat_room_rules.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

#include "ro/sim/actor.h"
#include "ro/sim/skill_book.h"

namespace moonclient {
namespace {

[[nodiscard]] int CellsBetween(const ro::sim::CellPosition& from, const ro::sim::CellPosition& to) {
  return std::max(std::abs(static_cast<int>(from.x) - static_cast<int>(to.x)),
                  std::abs(static_cast<int>(from.y) - static_cast<int>(to.y)));
}

[[nodiscard]] std::uint16_t LevelOf(const ro::sim::World& world, std::uint16_t skill_id) {
  const ro::sim::KnownSkill* known = world.skill_book().Find(skill_id);
  return known == nullptr ? 0 : known->level;
}

[[nodiscard]] bool SameLetters(std::string_view left, std::string_view right) {
  return std::ranges::equal(left, right, [](char a, char b) {
    return std::tolower(static_cast<unsigned char>(a)) ==
           std::tolower(static_cast<unsigned char>(b));
  });
}

}  // namespace

ChatRoomBlocker ChatRoomCreateBlocker(const ro::sim::World& world,
                                      const ChatRoomStoreState& stores) {
  if (stores.in_room) {
    return ChatRoomBlocker::kInRoom;
  }
  if (stores.vending_open) {
    return ChatRoomBlocker::kVending;
  }
  if (stores.buying_store_open) {
    return ChatRoomBlocker::kBuyingStore;
  }
  if (LevelOf(world, kBasicSkillId) < kChatRoomBasicSkillLevel &&
      LevelOf(world, kSummonerBasicSkillId) == 0) {
    return ChatRoomBlocker::kBasicSkill;
  }
  const ro::sim::Actor* self = world.player();
  if (self == nullptr) {
    return ChatRoomBlocker::kNone;
  }
  for (const auto& [id, actor] : world.actors()) {
    if (id == self->id) {
      continue;
    }
    // UN PNJ, et non un familier : le serveur ne compte que les siens.
    if (actor.kind == ro::sim::ActorKind::kNonPlayer &&
        actor.familiar == ro::sim::Familiar::kNone &&
        CellsBetween(actor.position, self->position) <= kChatRoomNpcClearanceCells) {
      return ChatRoomBlocker::kNpcNear;
    }
  }
  for (const auto& [id, actor] : world.actors()) {
    if (id != self->id && actor.signboard.has_value() && actor.position.x == self->position.x &&
        actor.position.y == self->position.y) {
      return ChatRoomBlocker::kCellTaken;
    }
  }
  return ChatRoomBlocker::kNone;
}

std::vector<NearbyChatRoom> NearbyChatRooms(const ro::sim::World& world) {
  std::vector<NearbyChatRoom> rooms;
  const ro::sim::Actor* self = world.player();
  for (const auto& [id, actor] : world.actors()) {
    if (!actor.signboard.has_value() ||
        actor.signboard->kind != ro::sim::SignboardKind::kChatRoom) {
      continue;
    }
    const ro::sim::Signboard& board = *actor.signboard;
    // UN SALON SANS PLACE N'EST PAS UN SALON DE JOUEUR : le formulaire n'en
    // ouvre pas sous deux places, et un script de PNJ en pose à zéro pour
    // écrire une étiquette au-dessus d'un PNJ. Personne n'y entre.
    if (board.limit == 0) {
      continue;
    }
    rooms.push_back(NearbyChatRoom{
        .room_id = board.room_id,
        .owner_id = id,
        .title = board.title,
        .users = board.users,
        .limit = board.limit,
        .status = board.status,
        .distance_cells = self == nullptr ? 0 : CellsBetween(actor.position, self->position)});
  }
  // Du plus proche au plus lointain ; à distance égale, par numéro, pour qu'une
  // liste qui ne change pas ne saute pas d'une image à l'autre.
  std::ranges::sort(rooms, [](const NearbyChatRoom& left, const NearbyChatRoom& right) {
    return left.distance_cells != right.distance_cells ? left.distance_cells < right.distance_cells
                                                       : left.room_id < right.room_id;
  });
  return rooms;
}

int ChatRoomTitleTaken(const std::vector<NearbyChatRoom>& rooms, std::string_view title) {
  if (title.empty()) {
    return -1;
  }
  for (std::size_t rank = 0; rank < rooms.size(); ++rank) {
    if (SameLetters(rooms[rank].title, title)) {
      return static_cast<int>(rank);
    }
  }
  return -1;
}

ChatRoomJoinStep ChatRoomJoinStepOf(std::uint8_t status) noexcept {
  switch (status) {
    case ro::sim::kRoomPrivate:
      return ChatRoomJoinStep::kAskPassword;
    case ro::sim::kRoomPkZone:
      return ChatRoomJoinStep::kNone;
    default:
      return ChatRoomJoinStep::kEnter;
  }
}

bool ChatRoomNewsRings(const ro::sim::ChatRoomNews& news, std::string_view self_name) noexcept {
  return news.kind == ro::sim::ChatRoomNewsKind::kMemberJoined && news.name != self_name;
}

}  // namespace moonclient
