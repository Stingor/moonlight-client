#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ro/core/error.h"
#include "ro/formats/lua_table.h"
#include "ro/vfs/vfs.h"

namespace ro::vfs {

// LE CATALOGUE DES SUCCÈS : ce que chacun s'appelle, ce qu'il demande, ce qu'il
// rapporte et sous quel onglet il se range.
//
// 🔴 LE SERVEUR N'EN DIT RIEN. Il pousse des numéros, des compteurs et des
// drapeaux ; le nom, les objectifs, les récompenses et l'onglet ne vivent que
// dans `achievement_tbl`, la table que lit aussi `AchievementTitles`. Un succès
// que le serveur annonce sans que la table le connaisse n'a ni nom ni onglet, et
// ne paraît nulle part — comme chez le client d'origine.
//
// La lecture refait ce que fait la fonction `main` du fichier, qui parcourt la
// table et déclare chaque entrée au client. ⚠ UNE DIFFÉRENCE, VOULUE : `main`
// abandonne le fichier ENTIER au premier succès mal formé ; ici, seule l'entrée
// fautive est écartée.

// Le plus d'objectifs qu'un succès porte : le serveur tient dix compteurs, et la
// fenêtre dix lignes.
inline constexpr std::size_t kAchievementObjectiveCount = 10;

// COMMENT LE DÉTAIL MONTRE LES OBJECTIFS, tel que l'entrée du catalogue le dit.
enum class AchievementGauge : std::uint8_t {
  // Une puce par objectif, sans chiffre : l'objectif est fait ou non.
  kBullets = 0,
  // « texte ( fait / but ) » et une jauge : l'objectif se compte.
  kCounts = 1,
};

struct AchievementObjective {
  std::string text;
  // Le but à atteindre. ⚠ ZÉRO EN MODE PUCES, quoi que dise le fichier : `main`
  // ne passe le compte qu'aux succès qui se comptent.
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
  std::optional<std::int32_t> reward_item;
  std::optional<std::int32_t> reward_title;
  std::optional<std::int32_t> reward_buff;

  // Une récompense n'existe que si son numéro est POSITIF : le client ne la
  // compte qu'à cette condition, jamais à la seule présence du champ.
  [[nodiscard]] static bool Grants(const std::optional<std::int32_t>& reward) noexcept {
    return reward.has_value() && *reward > 0;
  }

  // Le succès rapporte-t-il quelque chose ? Sans récompense, il n'y a rien à
  // réclamer, et le bouton reste caché.
  [[nodiscard]] bool rewards() const noexcept {
    return Grants(reward_item) || Grants(reward_title) || Grants(reward_buff);
  }
};

class AchievementCatalog {
 public:
  [[nodiscard]] static core::Result<AchievementCatalog> Load(Vfs& vfs);

  // La MÊME lecture, à partir de globales déjà lues : la seule moitié qui se
  // mette sous test, puisqu'aucun asset du jeu n'est versé au dépôt.
  [[nodiscard]] static core::Result<AchievementCatalog> Parse(const formats::LuaTable& globals);

  // Le succès de ce numéro, ou nullptr.
  [[nodiscard]] const AchievementInfo* Find(std::uint32_t achievement_id) const;

  // Tous les succès, par numéro croissant.
  [[nodiscard]] std::span<const AchievementInfo> entries() const noexcept { return entries_; }

  // Combien d'entrées la lecture a écartées faute d'être bien formées.
  [[nodiscard]] std::size_t skipped() const noexcept { return skipped_; }

 private:
  std::vector<AchievementInfo> entries_;
  std::size_t skipped_ = 0;
};

}  // namespace ro::vfs
