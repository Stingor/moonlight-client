#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"
#include "ro/formats/lua_table.h"
#include "ro/vfs/achievement_database.h"
#include "ro/vfs/vfs.h"

namespace ro::vfs {

// LE CATALOGUE DES SUCCÈS : ce que chacun s'appelle, ce qu'il demande, ce qu'il
// rapporte et sous quel onglet il se range. Il joint deux sources :
//
//   * la BASE DU SERVEUR (`AchievementDatabase`) : quels succès existent, leur
//     nom, leurs points, le compte qui achève chaque objectif, l'item, le titre
//     et le script qu'ils donnent, et les succès qu'ils requièrent ;
//
//   * la TABLE DU CLIENT, `achievement_tbl` du fichier traduit, pour ce que le
//     serveur ne tient pas : le résumé et le détail, le texte de chaque
//     objectif et s'il désigne un autre succès, la jauge, le groupe qui nomme
//     l'icône, l'onglet et le sous-onglet, et l'état que montre l'aide donnée.
//
// 🔴 UN SUCCÈS N'EST AU CATALOGUE QUE S'IL EST DANS LES DEUX. Absent de la base,
// il ne peut pas s'obtenir ; absent de la table, il n'a ni texte ni onglet.
//
// La lecture de la table refait ce que fait sa fonction `main`, qui la parcourt
// et déclare chaque entrée au client. ⚠ UNE DIFFÉRENCE, VOULUE : `main`
// abandonne le fichier ENTIER au premier succès mal formé ; ici, seule l'entrée
// fautive est écartée.
//
// ⚠ LE FICHIER TRADUIT, PAS CELUI DU JEU D'ORIGINE : c'est lui que ce serveur
// complète de ses succès maison.
inline constexpr std::string_view kAchievementTablePath = "SystemEN\\achievements.lub";

// La globale qui range les succès, indexés par leur numéro.
inline constexpr std::string_view kAchievementTableGlobal = "achievement_tbl";

// COMMENT LE DÉTAIL MONTRE LES OBJECTIFS, tel que l'entrée du catalogue le dit.
enum class AchievementGauge : std::uint8_t {
  // Une puce par objectif, sans chiffre : l'objectif est fait ou non.
  kBullets = 0,
  // « texte ( fait / but ) » et une jauge : l'objectif se compte.
  kCounts = 1,
};

struct AchievementObjective {
  std::string text;
  // Le but à atteindre, celui du serveur pour l'objectif de même rang. ⚠ ZÉRO
  // EN MODE PUCES, et zéro quand le serveur ne suit pas cet objectif.
  std::uint32_t target = 0;
  // Le succès que l'objectif DÉSIGNE — un succès fait de succès —, ou rien.
  // Cliquer la ligne le sélectionne. La table dit si la ligne désigne un
  // succès ; le serveur dit lequel : le requis de même rang.
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
  // L'état que cette aide montre en infobulle, selon la table du client. Il ne
  // fait pas la récompense : sans script, il n'y a pas d'aide.
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
  // La table du client, jointe à la base du serveur déjà lue.
  [[nodiscard]] static core::Result<AchievementCatalog> Load(Vfs& vfs,
                                                             const AchievementDatabase& database);

  // La MÊME jointure, à partir de globales déjà lues : la seule moitié qui se
  // mette sous test, puisqu'aucun asset du jeu n'est versé au dépôt.
  [[nodiscard]] static core::Result<AchievementCatalog> Parse(const AchievementDatabase& database,
                                                              const formats::LuaTable& globals);

  // Le succès de ce numéro, ou nullptr.
  [[nodiscard]] const AchievementInfo* Find(std::uint32_t achievement_id) const;

  // Tous les succès, par numéro croissant.
  [[nodiscard]] std::span<const AchievementInfo> entries() const noexcept { return entries_; }

  // Combien d'entrées de la table la lecture a écartées faute d'être bien
  // formées.
  [[nodiscard]] std::size_t skipped() const noexcept { return skipped_; }

  // Combien d'entrées de la table le serveur ne connaît pas : elles ne peuvent
  // pas s'obtenir, et ne se montrent pas.
  [[nodiscard]] std::size_t unknown_to_server() const noexcept { return unknown_to_server_; }

  // Combien de succès du serveur la table ne décrit pas : sans texte ni
  // onglet, ils ne se montrent pas.
  [[nodiscard]] std::size_t without_texts() const noexcept { return without_texts_; }

 private:
  std::vector<AchievementInfo> entries_;
  std::size_t skipped_ = 0;
  std::size_t unknown_to_server_ = 0;
  std::size_t without_texts_ = 0;
};

}  // namespace ro::vfs
