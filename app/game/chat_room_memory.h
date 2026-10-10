#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "ro/sim/chat_room.h"

#include "game/save_folder.h"

namespace moonclient {

// CE QUE LE CLIENT RETIENT DU FORMULAIRE DE SALON, SUR LE DISQUE.
//
// Le dernier titre, la dernière limite et le dernier accès, pour qu'un joueur
// qui rouvre chaque soir le même salon n'ait rien à retaper ; et les derniers
// titres envoyés, du plus récent au plus ancien.
//
// ⚠ LE MOT DE PASSE NE S'ÉCRIT PAS : ce fichier se lit en clair, et un mot de
// passe de salon se tape en quatre touches.
//
// ⚠ C'EST ENTIÈREMENT CLIENT : le serveur n'en sait rien.

inline constexpr std::string_view kChatRoomMemoryFileName = "salons.yaml";

// Combien de titres le menu des derniers garde.
inline constexpr std::size_t kChatRoomRecentTitles = 10;

struct ChatRoomMemory {
  std::string title;
  int limit = ro::sim::kChatRoomMostSeats;
  bool is_public = true;
  std::vector<std::string> recent_titles;

  // Retient le titre qui vient de partir, en tête et sans doublon.
  void RememberTitle(std::string_view title);
};

[[nodiscard]] std::filesystem::path ChatRoomMemoryPath(const std::filesystem::path& client_root);

// Un fichier absent ou illisible laisse les défauts : on joue quand même.
void LoadChatRoomMemory(const std::filesystem::path& client_root, ChatRoomMemory& into);

[[nodiscard]] bool SaveChatRoomMemory(const std::filesystem::path& client_root,
                                      const ChatRoomMemory& from);

}  // namespace moonclient
