#pragma once

#include <vector>

#include "ro/render/grey_world.h"
#include "ro/ui/options_window.h"

#include "game/settings_file.h"
#include "options/whisper_options.h"

namespace moonclient {

// ── GREYWORLD, DANS « GAMEPLAY » ────────────────────────────────────────────
//
// Un bloc repliable qui range quatre leviers, chacun sous son intertitre.
// Ce n'est pas de l'habillage — c'est ce que le joueur VOIT du terrain sur
// lequel il se bat.
//
// ⚠ LES LIGNES PORTENT UN IDENTIFIANT COMPOSÉ, hors de `GameOption`, comme
// celles des conversations 1:1, dont la plage précède la sienne.
inline constexpr int kGreyWorldRowBase = 7400;

enum class GreyWorldRow : int {
  kIntro = 0,
  kEnabled,
  kHideModels,
  kHideLightmap,
  kHideMapEffects,
  kGrid,
  kPattern,
  kGap,
  kFill,
  kRadius,
  kWalkColor,
  kWallColor,
  kSnipeColor,
  kFlatGround,
  kGroundColor,
  kStaffGround,
  kFlatten,
  kCount,
};

static_assert(kWhisperRowBase + static_cast<int>(WhisperRow::kCount) <= kGreyWorldRowBase,
              "la plage de GreyWorld chevauche celle des conversations 1:1");

class GreyWorldOptions {
 public:
  [[nodiscard]] const ro::render::GreyWorldLook& look() const noexcept { return look_; }

  // ⚠ AVANT LA LECTURE DU FICHIER : la valeur présente ici est le défaut.
  void Declare(SettingsFile& into);

  // L'établi du staff impose-t-il sa couleur au sol ? Relu à chaque image :
  // la page le dit alors sous la couleur de GreyWorld, qui ne s'affiche pas.
  void NoteStaffGround(bool staff_ground_wins) noexcept { staff_ground_wins_ = staff_ground_wins; }

  // Ajoute les lignes à la section « Gameplay ». ⚠ AVANT QUE LA VUE DE LA
  // SECTION NE SE PRENNE : elle n'en garde qu'un span.
  void ComposeInto(std::vector<ro::ui::OptionRowView>& rows);

  // Rend vrai quand la ligne était l'une des siennes.
  [[nodiscard]] bool Change(const ro::ui::OptionsWindowAction& asked);

 private:
  ro::render::GreyWorldLook look_;
  bool staff_ground_wins_ = false;
  // Les libellés des listes. ⚠ DES MEMBRES ET NON DES LOCAUX : la rangée n'en
  // garde qu'une vue, et un local serait mort avant le dessin.
  std::vector<const char*> pattern_names_;
  std::vector<const char*> fill_names_;
};

}  // namespace moonclient
