#pragma once

#include <vector>

#include "ro/ui/options_window.h"
#include "ro/ui/quick_cast_look.h"

#include "game/settings_file.h"
#include "options/craft_atlas_options.h"

namespace moonclient {

// ── QUICK CAST, DANS « GAMEPLAY » ───────────────────────────────────────────
//
// Les cinq réglages du quick cast. L'établi du staff les montre aussi : c'est
// une copie qui descend avant son dessin et remonte après, voir `Note`.
//
// 🔴 LE DROIT CHANGE LA PAGE, PAS LA FENÊTRE : la borne basse de la cadence
// d'item et son aide suivent le droit du compte. Voir `NoteStaff`.
//
// ⚠ LES LIGNES PORTENT UN IDENTIFIANT COMPOSÉ, hors de `GameOption`, comme
// celles de l'atlas des recettes, dont la plage précède la sienne.
inline constexpr int kQuickCastRowBase = 5800;
static_assert(kCraftAtlasRowBase + static_cast<int>(CraftAtlasRow::kCount) <= kQuickCastRowBase,
              "les lignes de l'atlas des recettes déborderaient sur celles de Quick cast");

enum class QuickCastRow : int {
  kGround = 0,
  kTarget,
  kRepeat,
  kItem,
  kItemRepeat,
  kCount,
};

[[nodiscard]] constexpr int QuickCastRowId(QuickCastRow row) {
  return kQuickCastRowBase + static_cast<int>(row);
}

class QuickCastOptions {
 public:
  [[nodiscard]] const ro::ui::QuickCastLook& look() const noexcept { return look_; }

  // ⚠ AVANT LA LECTURE DU FICHIER : la valeur présente ici est le défaut.
  void Declare(SettingsFile& into);

  // Le droit du compte, relu à chaque image : il arrive au login et peut
  // changer en cours de session.
  void NoteStaff(bool staff) noexcept { staff_ = staff; }

  // Ajoute les lignes à la section « Gameplay ». ⚠ AVANT QUE LA VUE DE LA
  // SECTION NE SE PRENNE : elle n'en garde qu'un span.
  void ComposeInto(std::vector<ro::ui::OptionRowView>& rows);

  // Rend vrai quand la ligne était l'une des siennes.
  [[nodiscard]] bool Change(const ro::ui::OptionsWindowAction& asked);

  // Ce que l'établi du staff a montré. Rend vrai quand quelque chose a changé
  // — il faut alors toucher le fichier.
  [[nodiscard]] bool Note(const ro::ui::QuickCastLook& seen) noexcept;

 private:
  ro::ui::QuickCastLook look_;
  bool staff_ = false;
};

}  // namespace moonclient
