#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "ro/sim/world.h"

namespace moonclient {

// CE QUI DÉCIDE, AVANT LE CLIC, SI UN SALON PEUT S'OUVRIR — et ce que la zone
// en montre déjà.
//
// 🔴 LE SERVEUR REFUSE SOUVENT SANS UN MOT : déjà dans un salon, échoppe
// ouverte. D'autres refus ne disent qu'un « échec de compétence » qu'on ne
// relie à rien. Tout ce qu'il vérifie et que le client sait voir se dit donc
// ici, avant l'envoi, avec sa raison.

// Le rang de Basic Skill qu'un salon exige, et la compétence du Summoner qui en
// tient lieu.
inline constexpr std::uint16_t kBasicSkillId = 1;
inline constexpr std::uint16_t kChatRoomBasicSkillLevel = 4;
inline constexpr std::uint16_t kSummonerBasicSkillId = 5018;
// La distance, en cases sur chaque axe, sous laquelle un PNJ empêche d'ouvrir
// un salon ou une échoppe. ⚠ UN RÉGLAGE DU SERVEUR (`min_npc_vendchat_distance`),
// que le fil ne dit pas : c'est la valeur de Moonlight.
inline constexpr int kChatRoomNpcClearanceCells = 2;

enum class ChatRoomBlocker : std::uint8_t {
  kNone,
  kInRoom,
  kVending,
  kBuyingStore,
  kBasicSkill,
  kNpcNear,
  // Une enseigne — salon ou échoppe — occupe déjà la case du joueur. C'est le
  // client d'origine qui le refuse, pas le serveur.
  kCellTaken,
};

struct ChatRoomStoreState {
  bool in_room = false;
  bool vending_open = false;
  bool buying_store_open = false;
};

[[nodiscard]] ChatRoomBlocker ChatRoomCreateBlocker(const ro::sim::World& world,
                                                    const ChatRoomStoreState& stores);

// UN SALON VISIBLE ALENTOUR, lu sur son enseigne.
struct NearbyChatRoom {
  std::uint32_t room_id = 0;
  std::uint32_t owner_id = 0;
  std::string title;
  std::uint16_t users = 0;
  std::uint16_t limit = 0;
  std::uint8_t status = 0;
  // À combien de cases du joueur, sur l'axe le plus long.
  int distance_cells = 0;
};

// Les salons que la zone montre, du plus proche au plus lointain.
[[nodiscard]] std::vector<NearbyChatRoom> NearbyChatRooms(const ro::sim::World& world);

// Un salon visible porte-t-il déjà ce titre ? Rend son rang dans `rooms`, ou
// moins un. La comparaison ignore la casse : deux titres qui ne diffèrent que
// par elle se confondent à l'œil.
[[nodiscard]] int ChatRoomTitleTaken(const std::vector<NearbyChatRoom>& rooms,
                                     std::string_view title);

// L'ENSEIGNE D'UN SALON SE REJOINT-ELLE AU DOUBLE-CLIC ? Les salles de PNJ
// fermées (zone PK) non ; un salon privé demande son mot de passe d'abord.
enum class ChatRoomJoinStep : std::uint8_t { kNone, kAskPassword, kEnter };
[[nodiscard]] ChatRoomJoinStep ChatRoomJoinStepOf(std::uint8_t status) noexcept;

}  // namespace moonclient
