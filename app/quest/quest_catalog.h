#pragma once

// LE CATALOGUE DES QUÊTES : ce que le journal affiche d'une quête (titre,
// résumé, description, PNJ, récompenses) et la façon dont les quêtes se
// rangent en catégories. Il se lit dans `data\yaml_files\quests.yml`.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"

namespace moonclient {

inline constexpr std::string_view kQuestCatalogPath = "data\\yaml_files\\quests.yml";

struct QuestNpc {
  std::string sprite;
  std::string map_name;
  int cell_x = 0;
  int cell_y = 0;
};

struct QuestRewardItem {
  std::uint32_t item_id = 0;
  std::uint32_t count = 0;
};

struct QuestInfo {
  std::uint32_t quest_id = 0;
  // UTF-8 ; le balisage du texte (couleurs, liens) est conservé tel quel.
  std::string title;
  std::string summary;
  std::string description;
  // Clé d'icône, vide si la quête n'en a pas.
  std::string icon;
  std::optional<QuestNpc> npc;
  std::uint64_t reward_base_exp = 0;
  std::uint64_t reward_job_exp = 0;
  std::vector<QuestRewardItem> reward_items;
  // UNE QUÊTE D'ATTENTE : elle ne fait que courir jusqu'à son échéance (« revenez
  // demain »). Le journal la range dans « En attente », hors du suivi.
  bool cooldown = false;
};

struct QuestIcon {
  std::string key;
  std::string image;
  std::string label;
};

struct QuestCategory {
  std::string key;
  std::string label;
  // Les icônes que cette catégorie réunit ; vide pour « tout » et « le reste ».
  std::vector<std::string> icons;
};

class QuestCatalog {
 public:
  // `language` : « fr », « en » ou « es ». Un texte écrit { fr, en, es } se
  // résout : langue demandée, puis langue du fichier (`language:`), puis fr,
  // puis la première entrée. Un texte nu est pris tel quel.
  // Refuse un texte qui n'est pas du YAML ou dont `version` n'est pas 1.
  [[nodiscard]] static ro::core::Result<QuestCatalog> Parse(std::string_view yaml_text,
                                                            std::string_view language);

  // nullptr si la quête est inconnue.
  [[nodiscard]] const QuestInfo* Find(std::uint32_t quest_id) const;
  [[nodiscard]] const std::vector<QuestIcon>& icons() const noexcept { return icons_; }
  [[nodiscard]] const QuestIcon* FindIcon(std::string_view key) const;
  [[nodiscard]] const std::vector<QuestCategory>& categories() const noexcept {
    return categories_;
  }
  // La première catégorie qui liste l'icône de la quête ; sinon « etc ».
  // Jamais « all ».
  [[nodiscard]] std::string_view CategoryOf(const QuestInfo& quest) const;

  [[nodiscard]] std::size_t size() const noexcept { return quests_.size(); }
  // Entrées mal formées, écartées une à une.
  [[nodiscard]] std::size_t skipped() const noexcept { return skipped_; }

 private:
  std::vector<QuestInfo> quests_;  // triées par quest_id
  std::vector<QuestIcon> icons_;
  std::vector<QuestCategory> categories_;
  std::size_t skipped_ = 0;
};

}  // namespace moonclient
