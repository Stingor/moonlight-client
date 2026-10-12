#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ro/vfs/achievement_database.h"

namespace ro::vfs {

// LE CATALOGUE DES SUCCÈS : ce que chacun s'appelle, ce qu'il demande, ce qu'il
// rapporte et sous quel onglet il se range, tel que la fenêtre le montre. Il se
// tire de la base (`AchievementDatabase`), qui tient à la fois ce que le
// serveur sait d'un succès et, sous `window`, ses textes, sa jauge, son icône et
// son onglet.
//
// 🔴 UN SUCCÈS N'EST AU CATALOGUE QUE S'IL A SA FENÊTRE : sans elle, il n'a ni
// texte ni onglet.

struct AchievementObjective {
  std::string text;
  // Le but à atteindre, celui du serveur pour l'objectif de même rang. ⚠ ZÉRO
  // EN MODE PUCES, et zéro quand le serveur ne suit pas cet objectif.
  std::uint32_t target = 0;
  // Le succès que l'objectif DÉSIGNE — un succès fait de succès —, ou rien.
  // Cliquer la ligne le sélectionne.
  std::optional<std::uint32_t> shortcut;
};

struct AchievementInfo {
  std::uint32_t achievement_id = 0;
  std::string name;
  std::string summary;
  std::string details;
  std::int32_t score = 0;
  AchievementGauge gauge = AchievementGauge::kBullets;
  // Le groupe, tel qu'écrit : `BATTLE`, `ADVENTURE`… Il nomme l'icône de la ligne.
  std::string group;
  // L'onglet et le sous-onglet : `major` de 1 à 6, `minor` à partir de zéro.
  std::uint8_t major = 0;
  std::uint8_t minor = 0;
  std::vector<AchievementObjective> objectives;
  // L'item et le titre donnés, selon le serveur.
  std::optional<std::int32_t> reward_item;
  std::optional<std::int32_t> reward_title;
  // Le serveur exécute un script : la fenêtre le montre comme une aide.
  bool reward_script = false;
  // L'état que cette aide montre en infobulle. Il ne fait pas la récompense :
  // sans script, il n'y a pas d'aide.
  std::optional<std::int32_t> reward_buff;

  // Une récompense n'existe que si son numéro est POSITIF : le client ne la
  // compte qu'à cette condition, jamais à la seule présence du champ.
  [[nodiscard]] static bool Grants(const std::optional<std::int32_t>& reward) noexcept {
    return reward.has_value() && *reward > 0;
  }

  // Le succès rapporte-t-il quelque chose ? Sans récompense, il n'y a rien à
  // réclamer, et le bouton reste caché.
  [[nodiscard]] bool rewards() const noexcept {
    return Grants(reward_item) || Grants(reward_title) || reward_script;
  }
};

class AchievementCatalog {
 public:
  // Les succès de la base qui ont leur fenêtre.
  [[nodiscard]] static AchievementCatalog Of(const AchievementDatabase& database);

  // Le succès de ce numéro, ou nullptr.
  [[nodiscard]] const AchievementInfo* Find(std::uint32_t achievement_id) const;

  // Tous les succès, par numéro croissant.
  [[nodiscard]] std::span<const AchievementInfo> entries() const noexcept { return entries_; }

  // Combien de succès de la base n'ont pas de fenêtre : sans texte ni onglet,
  // ils ne se montrent pas.
  [[nodiscard]] std::size_t without_texts() const noexcept { return without_texts_; }

 private:
  std::vector<AchievementInfo> entries_;
  std::size_t without_texts_ = 0;
};

}  // namespace ro::vfs
