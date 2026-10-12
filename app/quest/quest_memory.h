#pragma once

// CE QUE LE CLIENT RETIENT DES QUÊTES, et que le serveur ne sait pas : celles
// qu'on suit à l'écran, celles qu'on a déjà regardées, quand chacune est
// arrivée, celles qui sont sorties du journal, et la fenêtre telle qu'on l'a
// laissée. Par personnage, dans `savedata\quetes.yml`.

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "ro/sim/quest_log.h"
#include "ro/ui/quest_window.h"

namespace moonclient {

inline constexpr std::string_view kQuestMemoryFileName = "quetes.yml";

// L'historique ne garde que les plus récentes : au-delà, la plus ancienne part.
inline constexpr std::size_t kMostQuestHistory = 200;

// UNE QUÊTE SORTIE DU JOURNAL, à sa dernière sortie : une seule ligne par
// quête.
struct QuestHistoryEntry {
  std::uint32_t quest_id = 0;
  // Le titre qu'elle avait, pour qu'une quête sortie du catalogue se nomme
  // encore.
  std::string title;
  std::int64_t left_unix_s = 0;
  // Pourquoi elle est sortie. ⚠ RIEN QUAND ON NE LE SAIT PAS : un serveur qui
  // ne le dit pas, ou une ligne écrite avant qu'il le dise.
  std::optional<ro::sim::QuestEnding> ending;
};

struct RememberedQuests {
  std::string character;
  std::vector<std::uint32_t> tracked;
  // Celles que le joueur a cessé de suivre : le suivi automatique des
  // nouvelles ne les reprend pas à la reconnexion.
  std::vector<std::uint32_t> untracked;
  // Celles qu'il a regardées : elles ne sont plus « nouvelles ».
  std::vector<std::uint32_t> seen;
  // Les affiches recommandées qu'il a ouvertes : elles ne sont plus « nouvelles ».
  std::vector<std::uint32_t> seen_recommended;
  // Quand le client a vu chacune arriver, en secondes Unix.
  std::unordered_map<std::uint32_t, std::int64_t> received_unix_s;
  // Les plus récentes en dernier.
  std::vector<QuestHistoryEntry> history;
  ro::ui::QuestTab tab = ro::ui::QuestTab::kActive;
  ro::ui::QuestSort sort = ro::ui::QuestSort::kRecent;
  std::string category = "all";
  bool group_by_category = true;
};

struct QuestMemory {
  std::vector<RememberedQuests> characters;

  [[nodiscard]] RememberedQuests* Of(std::string_view character);
  [[nodiscard]] const RememberedQuests* Of(std::string_view character) const;
  // Remplace la part de ce personnage.
  void Remember(RememberedQuests kept);
};

[[nodiscard]] std::filesystem::path QuestMemoryPath(const std::filesystem::path& client_root);

// Un fichier absent ou illisible laisse `into` tel quel : les défauts valent.
//
// 🔴 FAUX QUAND LE FICHIER EXISTE ET NE SE LIT PAS, et l'appelant doit alors
// renoncer à l'écrire : le fichier porte les quêtes de TOUS les personnages,
// et le réécrire depuis une lecture ratée n'y laisserait que celui du moment.
// Absent, il est lisible : il n'y a rien à perdre.
bool LoadQuestMemory(const std::filesystem::path& client_root, QuestMemory& into);
bool SaveQuestMemory(const std::filesystem::path& client_root, const QuestMemory& from);

// Lecture et écriture du TEXTE, pour les essais. Même verdict que
// `LoadQuestMemory`.
bool ParseQuestMemory(std::string_view text, QuestMemory& into);
[[nodiscard]] std::string QuestMemoryText(const QuestMemory& from);

}  // namespace moonclient
