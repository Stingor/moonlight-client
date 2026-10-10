#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ro/ui/achievement_window.h"

#include "game/save_folder.h"

namespace moonclient {

// CE QUE LE CLIENT RETIENT DES SUCCÈS DU JOUEUR, SUR LE DISQUE.
//
// Des réglages communs — la bannière d'un succès achevé, le panneau de suivi et
// sa place — et, PAR PERSONNAGE, les succès qu'il suit et la fenêtre telle qu'il
// l'a laissée : onglet, filtre, ordre, succès choisi. La recherche ne se retient
// pas.
//
// ⚠ C'EST ENTIÈREMENT CLIENT : le serveur ne sait rien du suivi, et le client
// d'origine l'oubliait à chaque session.

inline constexpr std::string_view kAchievementMemoryFileName = "succes.yml";

struct RememberedAchievements {
  // Le nom du personnage : un compte en porte plusieurs, un nom n'en désigne
  // qu'un sur le serveur.
  std::string character;
  std::vector<std::uint32_t> tracked;
  int tab = ro::ui::kAchievementSummaryTab;
  int subtab = ro::ui::kAchievementWholeTab;
  ro::ui::AchievementFilter filter = ro::ui::AchievementFilter::kAll;
  ro::ui::AchievementSort sort = ro::ui::AchievementSort::kGame;
  std::optional<std::uint32_t> selected;
};

struct AchievementMemory {
  ro::ui::AchievementBannerStyle banner = ro::ui::AchievementBannerStyle::kNative;
  std::vector<RememberedAchievements> characters;

  // Ce que ce personnage a laissé, ou nul.
  [[nodiscard]] const RememberedAchievements* Of(std::string_view character) const;
  // Retient ce personnage, à la place de ce qu'il avait laissé.
  void Remember(RememberedAchievements kept);
};

[[nodiscard]] std::filesystem::path AchievementMemoryPath(const std::filesystem::path& client_root);

// Un fichier absent ou illisible laisse les défauts : on joue quand même. Une
// valeur hors de ses bornes — onglet, filtre, ordre, style — retombe aussi à
// son défaut, et l'on ne garde pas plus de suivis que le client n'en permet.
void LoadAchievementMemory(const std::filesystem::path& client_root, AchievementMemory& into);

// Vrai si le fichier est écrit.
bool SaveAchievementMemory(const std::filesystem::path& client_root, const AchievementMemory& from);

}  // namespace moonclient
