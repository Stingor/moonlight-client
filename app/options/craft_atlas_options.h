#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <vector>

#include "ro/ui/craft_atlas_window.h"
#include "ro/ui/options_window.h"

#include "game/settings_file.h"

namespace moonclient {

// ── LA PAGE « RECETTES » ────────────────────────────────────────────────────
//
// Ouvrir l'Atlas, et ses trois cases.
//
// ⚠ L'ATLAS N'AGIT SUR RIEN : il n'envoie aucun paquet et ne remplace aucune
// fenêtre du jeu, il lit un fichier. Rien ici ne se grise donc selon
// l'interface choisie.
inline constexpr int kCraftAtlasRowBase = 5700;

enum class CraftAtlasRow : int {
  kIntro = 0,
  kOpen,
  kPreview,
  kOnlyCraftable,
  kShowUnlearnable,
  kGestures,
  kTally,
  kCount,
};

[[nodiscard]] constexpr int CraftAtlasRowId(CraftAtlasRow row) {
  return kCraftAtlasRowBase + static_cast<int>(row);
}

// Ce que le livre des recettes contient, pour la dernière ligne. Zéro partout :
// il n'a pas été lu.
struct CraftAtlasTally {
  bool available = false;
  std::size_t recipes = 0;
  std::size_t materials = 0;
  std::size_t arrows = 0;
};

class CraftAtlasOptions {
 public:
  // L'aperçu de l'item au survol d'une ligne. ALLUMÉ par défaut.
  [[nodiscard]] bool preview() const noexcept { return preview_; }
  // Ne montrer que les recettes réalisables avec le sac. ÉTEINT par défaut :
  // l'Atlas sert d'abord à préparer ce qu'on n'a PAS encore, et un filtre par
  // défaut cacherait justement ce qu'on venait chercher.
  [[nodiscard]] bool only_craftable() const noexcept { return only_craftable_; }
  // Montrer aussi les recettes dont aucune classe de ce serveur n'apprend le
  // skill. ÉTEINT par défaut : ce sont des lignes que le serveur refusera
  // toujours. ⚠ LE RÉGLAGE EXISTE QUAND MÊME : le fichier sert aussi sur un
  // serveur au contenu différent, et un joueur curieux a le droit de voir la
  // table entière.
  [[nodiscard]] bool show_unlearnable() const noexcept { return show_unlearnable_; }

  // La case de la barre d'outils de l'Atlas, qui est ce même réglage.
  void SetOnlyCraftable(bool on, SettingsFile* file);

  // ⚠ AVANT LA LECTURE DU FICHIER : la valeur présente ici est le défaut.
  void Declare(SettingsFile& into);
  void NoteTally(const CraftAtlasTally& tally) { tally_ = tally; }

  // Refait les lignes, à chaque image où le panneau est ouvert.
  void Compose();
  [[nodiscard]] std::span<const ro::ui::OptionRowView> rows() const noexcept { return rows_; }

  // Rend vrai quand la ligne était l'une des siennes. ⚠ « OUVRIR » AGIT SUR LA
  // FENÊTRE, et non sur un réglage : d'où l'état qu'on lui passe.
  [[nodiscard]] bool Change(const ro::ui::OptionsWindowAction& asked,
                            ro::ui::CraftAtlasWindowState* window);

 private:
  bool preview_ = true;
  bool only_craftable_ = false;
  bool show_unlearnable_ = false;
  CraftAtlasTally tally_;
  std::string tally_text_;
  std::vector<ro::ui::OptionRowView> rows_;
};

}  // namespace moonclient
