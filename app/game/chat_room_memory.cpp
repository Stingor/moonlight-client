#include "game/chat_room_memory.h"

#include <algorithm>
#include <system_error>

#include <yaml-cpp/yaml.h>

#include "ro/core/error.h"
#include "ro/core/file_write.h"
#include "ro/core/log.h"

namespace moonclient {
namespace {

// Les champs du fichier. ⚠ GRAVÉS : les changer fait oublier à tous les
// joueurs leur dernier salon, sans que rien ne le dise.
constexpr const char* kTitleField = "title";
constexpr const char* kLimitField = "limit";
constexpr const char* kPublicField = "public";
constexpr const char* kRecentField = "recent_titles";

constexpr const char* kFileHeader =
    "# Le formulaire de salon, tel que le client le retient.\n"
    "#\n"
    "# Le dernier titre, la dernière limite et le dernier accès, puis les\n"
    "# derniers titres envoyés, du plus récent au plus ancien. Le mot de passe\n"
    "# ne s'y écrit jamais.\n"
    "#\n"
    "# ⚠ TOUT CECI EST AU CLIENT : le serveur n'en sait rien.\n";

}  // namespace

void ChatRoomMemory::RememberTitle(std::string_view sent) {
  if (sent.empty()) {
    return;
  }
  std::erase(recent_titles, sent);
  recent_titles.insert(recent_titles.begin(), std::string(sent));
  if (recent_titles.size() > kChatRoomRecentTitles) {
    recent_titles.resize(kChatRoomRecentTitles);
  }
}

std::filesystem::path ChatRoomMemoryPath(const std::filesystem::path& client_root) {
  return client_root / kSaveFolder / kChatRoomMemoryFileName;
}

void LoadChatRoomMemory(const std::filesystem::path& client_root, ChatRoomMemory& into) {
  const std::filesystem::path path = ChatRoomMemoryPath(client_root);
  std::error_code missing;
  if (!std::filesystem::exists(path, missing)) {
    return;
  }
  YAML::Node whole;
  try {
    whole = YAML::LoadFile(path.string());
  } catch (const YAML::Exception& broken) {
    ro::core::LogWarning("salons : fichier illisible ({}) — les défauts s'appliquent",
                         broken.what());
    return;
  }
  // ⚠ UNE RACINE QUI N'EST PAS UNE TABLE N'A PAS DE CHAMPS : l'indexer lèverait
  // hors du `try`.
  if (!whole.IsMap()) {
    return;
  }
  try {
    into.title = whole[kTitleField].as<std::string>(into.title);
    into.limit = whole[kLimitField].as<int>(into.limit);
    into.is_public = whole[kPublicField].as<bool>(into.is_public);
    const YAML::Node listed = whole[kRecentField];
    if (listed && listed.IsSequence()) {
      for (const YAML::Node& entry : listed) {
        if (into.recent_titles.size() >= kChatRoomRecentTitles) {
          break;
        }
        const std::string title = entry.as<std::string>(std::string());
        if (!title.empty() &&
            std::ranges::find(into.recent_titles, title) == into.recent_titles.end()) {
          into.recent_titles.push_back(title);
        }
      }
    }
  } catch (const YAML::Exception& broken) {
    ro::core::LogWarning("salons : champ illisible ({}) — les défauts s'appliquent", broken.what());
  }
}

bool SaveChatRoomMemory(const std::filesystem::path& client_root, const ChatRoomMemory& from) {
  YAML::Emitter out;
  out << YAML::BeginMap;
  out << YAML::Key << kTitleField << YAML::Value << from.title;
  out << YAML::Key << kLimitField << YAML::Value << from.limit;
  out << YAML::Key << kPublicField << YAML::Value << from.is_public;
  out << YAML::Key << kRecentField << YAML::Value << YAML::BeginSeq;
  for (const std::string& title : from.recent_titles) {
    out << title;
  }
  out << YAML::EndSeq << YAML::EndMap;

  const std::string text = std::string(kFileHeader) + out.c_str() + "\n";
  if (const ro::core::Result<void> written =
          ro::core::WriteFileAtomically(ChatRoomMemoryPath(client_root), text);
      !written) {
    ro::core::LogWarning("salons : écriture impossible ({})", ro::core::Describe(written.error()));
    return false;
  }
  return true;
}

}  // namespace moonclient
