#pragma once

// LES QUÊTES RECOMMANDÉES : des affiches que le journal montre dans son onglet
// « Recommandées » — titre, résumé, illustration, PNJ de départ et quelques
// blocs de texte. Ce ne sont pas des quêtes : le serveur n'en sait rien, et les
// lire n'engage à rien. Elles se lisent dans
// `data\yaml_files\recommended_quests.yml`, dans l'ordre du fichier.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"

#include "quest/quest_catalog.h"

namespace moonclient {

inline constexpr std::string_view kRecommendedCatalogPath =
    "data\\yaml_files\\recommended_quests.yml";

struct RecommendedQuest {
  // La clé de l'affiche dans le fichier ; elle ne désigne aucune quête.
  std::uint32_t entry_id = 0;
  std::string title;
  std::string summary;
  // Clé d'icône, comme dans `quests.yml` ; vide : aucune.
  std::string icon;
  // L'illustration, sous `renew_questui\` ; vide : aucune.
  std::string image;
  std::optional<QuestNpc> npc;
  // Les blocs de texte, balisage du jeu compris ; chacun peut tenir plusieurs
  // lignes.
  std::vector<std::string> blocks;
};

class RecommendedCatalog {
 public:
  // `language` : comme pour le catalogue des quêtes. Refuse un texte qui n'est
  // pas du YAML ou dont `version` n'est pas 1 ; une affiche mal formée est
  // écartée seule.
  [[nodiscard]] static ro::core::Result<RecommendedCatalog> Parse(std::string_view yaml_text,
                                                                  std::string_view language);

  // Dans l'ordre du fichier.
  [[nodiscard]] const std::vector<RecommendedQuest>& entries() const noexcept { return entries_; }
  // nullptr si l'affiche est inconnue.
  [[nodiscard]] const RecommendedQuest* Find(std::uint32_t entry_id) const;
  [[nodiscard]] std::size_t skipped() const noexcept { return skipped_; }

 private:
  std::vector<RecommendedQuest> entries_;
  std::size_t skipped_ = 0;
};

}  // namespace moonclient
