#include "quest/quest_memory.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <string_view>
#include <system_error>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "ro/core/error.h"
#include "ro/core/file_write.h"
#include "ro/core/log.h"

#include "game/save_folder.h"

namespace moonclient {
namespace {

// Les champs du fichier. ⚠ GRAVÉS : les changer fait oublier à chacun ses
// quêtes suivies et son historique, sans que rien ne le dise.
constexpr const char* kCharactersField = "characters";
constexpr const char* kCharacterField = "character";
constexpr const char* kTrackedField = "tracked";
constexpr const char* kUntrackedField = "untracked";
constexpr const char* kSeenField = "seen";
constexpr const char* kSeenRecommendedField = "recommended_read";
constexpr const char* kReceivedField = "received";
constexpr const char* kHistoryField = "history";
constexpr const char* kQuestField = "quest";
constexpr const char* kTitleField = "title";
constexpr const char* kLeftField = "left";
constexpr const char* kEndingField = "ending";
constexpr const char* kNextQuestField = "next";
constexpr const char* kTabField = "tab";
constexpr const char* kSortField = "sort";
constexpr const char* kCategoryField = "category";
constexpr const char* kGroupField = "group";

constexpr const char* kFileHeader =
    "# Les quêtes du joueur, telles que le client les retient.\n"
    "#\n"
    "# Sous « characters », pour chacun : les quêtes suivies à l'écran, celles\n"
    "# qu'il a cessé de suivre, celles qu'il a regardées, les recommandations qu'il\n"
    "# a lues, l'instant où chacune est arrivée (« received » : quête → secondes\n"
    "# Unix), l'historique des quêtes sorties du journal, et la fenêtre telle\n"
    "# qu'il l'a laissée — onglet (0 en cours, 1 suspendues, 2 historique,\n"
    "# 3 recommandées, 4 en attente), ordre (0 récentes, 1 temps restant,\n"
    "# 2 nom, 3 progression), catégorie, regroupement.\n"
    "#\n"
    "# Une ligne d'historique dit pourquoi la quête est sortie quand le serveur\n"
    "# l'a dit (« ending » : completed, abandoned, replaced — « next » nomme\n"
    "# alors l'étape d'après —, retired) ; sans « ending », on ne le sait pas.\n"
    "#\n"
    "# ⚠ TOUT CECI EST AU CLIENT : le serveur n'en sait rien.\n";

// LES MOTS DE « ending », dans l'ordre de `QuestEndReason`. ⚠ GRAVÉS, comme
// les champs.
struct EndingWord {
  ro::sim::QuestEndReason reason;
  std::string_view word;
};
constexpr std::array<EndingWord, 4> kEndingWords = {{
    {ro::sim::QuestEndReason::kCompleted, "completed"},
    {ro::sim::QuestEndReason::kErased, "abandoned"},
    {ro::sim::QuestEndReason::kReplaced, "replaced"},
    {ro::sim::QuestEndReason::kRetired, "retired"},
}};

[[nodiscard]] std::string_view WordOf(ro::sim::QuestEndReason reason) {
  const auto found = std::ranges::find(kEndingWords, reason, &EndingWord::reason);
  return found == kEndingWords.end() ? std::string_view() : found->word;
}

// La fin d'une ligne lue ; rien pour un mot absent ou inconnu.
[[nodiscard]] std::optional<ro::sim::QuestEnding> ReadEnding(const YAML::Node& left) {
  const YAML::Node word_node = left[kEndingField];
  if (!word_node || !word_node.IsScalar()) {
    return std::nullopt;
  }
  const std::string word = word_node.as<std::string>(std::string());
  const auto found = std::ranges::find(kEndingWords, word, &EndingWord::word);
  if (found == kEndingWords.end()) {
    return std::nullopt;
  }
  ro::sim::QuestEnding ending{.reason = found->reason};
  if (ending.reason == ro::sim::QuestEndReason::kReplaced) {
    ending.next_quest_id = left[kNextQuestField].as<std::uint32_t>(0);
  }
  return ending;
}

template <typename Enum>
[[nodiscard]] Enum Bounded(int value, std::size_t count, Enum fallback) {
  return value >= 0 && static_cast<std::size_t>(value) < count ? static_cast<Enum>(value)
                                                               : fallback;
}

void ReadIds(const YAML::Node& listed, std::vector<std::uint32_t>& into) {
  if (!listed || !listed.IsSequence()) {
    return;
  }
  for (const YAML::Node& id : listed) {
    const auto quest_id = id.as<std::uint32_t>(0);
    if (quest_id != 0 && std::ranges::find(into, quest_id) == into.end()) {
      into.push_back(quest_id);
    }
  }
}

void WriteIds(YAML::Emitter& out, const char* field, const std::vector<std::uint32_t>& ids) {
  out << YAML::Key << field << YAML::Value << YAML::Flow << YAML::BeginSeq;
  for (const std::uint32_t quest_id : ids) {
    out << quest_id;
  }
  out << YAML::EndSeq;
}

[[nodiscard]] RememberedQuests ReadOne(const YAML::Node& entry) {
  RememberedQuests kept;
  kept.character = entry[kCharacterField].as<std::string>(std::string());
  ReadIds(entry[kTrackedField], kept.tracked);
  ReadIds(entry[kUntrackedField], kept.untracked);
  ReadIds(entry[kSeenField], kept.seen);
  ReadIds(entry[kSeenRecommendedField], kept.seen_recommended);
  if (const YAML::Node received = entry[kReceivedField]; received && received.IsMap()) {
    for (const auto& pair : received) {
      const auto quest_id = pair.first.as<std::uint32_t>(0);
      if (quest_id != 0) {
        kept.received_unix_s[quest_id] = pair.second.as<std::int64_t>(0);
      }
    }
  }
  if (const YAML::Node history = entry[kHistoryField]; history && history.IsSequence()) {
    for (const YAML::Node& left : history) {
      if (!left.IsMap()) {
        continue;
      }
      QuestHistoryEntry one{.quest_id = left[kQuestField].as<std::uint32_t>(0),
                            .title = left[kTitleField].as<std::string>(std::string()),
                            .left_unix_s = left[kLeftField].as<std::int64_t>(0),
                            .ending = ReadEnding(left)};
      if (one.quest_id != 0) {
        // UNE LIGNE PAR QUÊTE : la dernière sortie l'emporte.
        std::erase_if(kept.history, [&one](const QuestHistoryEntry& earlier) {
          return earlier.quest_id == one.quest_id;
        });
        kept.history.push_back(std::move(one));
      }
    }
    if (kept.history.size() > kMostQuestHistory) {
      kept.history.erase(kept.history.begin(),
                         kept.history.end() - static_cast<std::ptrdiff_t>(kMostQuestHistory));
    }
  }
  kept.tab =
      Bounded(entry[kTabField].as<int>(0), ro::ui::kQuestTabCount, ro::ui::QuestTab::kActive);
  kept.sort =
      Bounded(entry[kSortField].as<int>(0), ro::ui::kQuestSortCount, ro::ui::QuestSort::kRecent);
  kept.category = entry[kCategoryField].as<std::string>(std::string("all"));
  kept.group_by_category = entry[kGroupField].as<bool>(true);
  return kept;
}

}  // namespace

RememberedQuests* QuestMemory::Of(std::string_view character) {
  const auto found = std::ranges::find(characters, character, &RememberedQuests::character);
  return found == characters.end() ? nullptr : &*found;
}

const RememberedQuests* QuestMemory::Of(std::string_view character) const {
  const auto found = std::ranges::find(characters, character, &RememberedQuests::character);
  return found == characters.end() ? nullptr : &*found;
}

void QuestMemory::Remember(RememberedQuests kept) {
  std::erase_if(characters, [&kept](const RememberedQuests& other) {
    return other.character == kept.character;
  });
  characters.push_back(std::move(kept));
}

std::filesystem::path QuestMemoryPath(const std::filesystem::path& client_root) {
  return client_root / kSaveFolder / kQuestMemoryFileName;
}

void ParseQuestMemory(std::string_view text, QuestMemory& into) {
  YAML::Node whole;
  try {
    whole = YAML::Load(std::string(text));
  } catch (const YAML::Exception& broken) {
    ro::core::LogWarning("quêtes : fichier illisible ({}) — les défauts s'appliquent",
                         broken.what());
    return;
  }
  // ⚠ UNE RACINE QUI N'EST PAS UNE TABLE N'A PAS DE CHAMPS : l'indexer lèverait
  // hors du `try`.
  if (!whole.IsMap()) {
    return;
  }
  try {
    const YAML::Node listed = whole[kCharactersField];
    if (!listed || !listed.IsSequence()) {
      return;
    }
    for (const YAML::Node& entry : listed) {
      if (!entry.IsMap()) {
        continue;
      }
      RememberedQuests kept = ReadOne(entry);
      if (!kept.character.empty()) {
        into.Remember(std::move(kept));
      }
    }
  } catch (const YAML::Exception& broken) {
    // Un champ d'un autre type : ce qui a été lu reste, le reste aux défauts.
    ro::core::LogWarning("quêtes : champ illisible ({})", broken.what());
  }
}

std::string QuestMemoryText(const QuestMemory& from) {
  YAML::Emitter out;
  out << YAML::BeginMap;
  out << YAML::Key << kCharactersField << YAML::Value << YAML::BeginSeq;
  for (const RememberedQuests& kept : from.characters) {
    out << YAML::BeginMap;
    out << YAML::Key << kCharacterField << YAML::Value << kept.character;
    WriteIds(out, kTrackedField, kept.tracked);
    WriteIds(out, kUntrackedField, kept.untracked);
    WriteIds(out, kSeenField, kept.seen);
    WriteIds(out, kSeenRecommendedField, kept.seen_recommended);
    // Rangées par identifiant : un fichier qui se réécrit dans le même ordre se
    // compare d'un coup d'œil.
    const std::map<std::uint32_t, std::int64_t> received(kept.received_unix_s.begin(),
                                                         kept.received_unix_s.end());
    out << YAML::Key << kReceivedField << YAML::Value << YAML::BeginMap;
    for (const auto& [quest_id, unix_s] : received) {
      out << YAML::Key << quest_id << YAML::Value << unix_s;
    }
    out << YAML::EndMap;
    out << YAML::Key << kHistoryField << YAML::Value << YAML::BeginSeq;
    for (const QuestHistoryEntry& left : kept.history) {
      out << YAML::Flow << YAML::BeginMap;
      out << YAML::Key << kQuestField << YAML::Value << left.quest_id;
      out << YAML::Key << kTitleField << YAML::Value << left.title;
      out << YAML::Key << kLeftField << YAML::Value << left.left_unix_s;
      if (const std::string_view word =
              left.ending.has_value() ? WordOf(left.ending->reason) : std::string_view();
          !word.empty()) {
        out << YAML::Key << kEndingField << YAML::Value << std::string(word);
        if (left.ending->next_quest_id != 0) {
          out << YAML::Key << kNextQuestField << YAML::Value << left.ending->next_quest_id;
        }
      }
      out << YAML::EndMap;
    }
    out << YAML::EndSeq;
    out << YAML::Key << kTabField << YAML::Value << static_cast<int>(kept.tab);
    out << YAML::Key << kSortField << YAML::Value << static_cast<int>(kept.sort);
    out << YAML::Key << kCategoryField << YAML::Value << kept.category;
    out << YAML::Key << kGroupField << YAML::Value << kept.group_by_category;
    out << YAML::EndMap;
  }
  out << YAML::EndSeq << YAML::EndMap;
  return std::string(kFileHeader) + out.c_str() + "\n";
}

void LoadQuestMemory(const std::filesystem::path& client_root, QuestMemory& into) {
  const std::filesystem::path path = QuestMemoryPath(client_root);
  std::error_code missing;
  if (!std::filesystem::exists(path, missing)) {
    return;
  }
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    ro::core::LogWarning("quêtes : {} ne s'ouvre pas — les défauts s'appliquent", path.string());
    return;
  }
  const std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  ParseQuestMemory(text, into);
}

bool SaveQuestMemory(const std::filesystem::path& client_root, const QuestMemory& from) {
  if (const ro::core::Result<void> written =
          ro::core::WriteFileAtomically(QuestMemoryPath(client_root), QuestMemoryText(from));
      !written) {
    ro::core::LogWarning("quêtes : écriture impossible ({})", ro::core::Describe(written.error()));
    return false;
  }
  return true;
}

}  // namespace moonclient
