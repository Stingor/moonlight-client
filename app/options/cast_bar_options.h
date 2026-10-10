#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include "ro/ui/cast_bars.h"
#include "ro/ui/options_window.h"

#include "game/settings_file.h"
#include "options/weapon_refine_options.h"

namespace moonclient {

// ── LES BARRES DE CAST, DANS LA PAGE « ENTITÉS » ────────────────────────────
//
// Une barre PAR ESPÈCE, rangée dans l'onglet de l'espèce avec ses barres de vie
// et son nom : on la règle en la regardant au-dessus de la même entité.
//
// 🔴 SEUL LE REMPLACEMENT RESTE COMMUN : c'est la barre du client OU la nôtre,
// pas un goût par espèce.
inline constexpr int kCastBarRowBase = 6900;

// LES RÉGLAGES D'UNE ESPÈCE. ⚠ L'ORDRE EST CELUI DES RANGÉES À L'ÉCRAN, et il
// entre dans les identifiants : ne pas intercaler sans relire la garde.
enum class CastBarField : int {
  kShown = 0,
  kName,
  kTimeLeft,
  kBorder,
  kWidth,
  kHeight,
  kOffset,
  kRounding,
  kFontScale,
  kOpacity,
  kBackground,
  kFill,
  kCount,
};

// La place d'une espèce dans les identifiants : de quoi ajouter des champs
// sans décaler les espèces suivantes.
inline constexpr int kCastBarFieldsPerKind = 16;
static_assert(static_cast<int>(CastBarField::kCount) <= kCastBarFieldsPerKind,
              "les réglages d'une espèce débordent sur la suivante");

[[nodiscard]] constexpr int CastBarRowId(std::size_t kind, CastBarField field) {
  return kCastBarRowBase + static_cast<int>(kind) * kCastBarFieldsPerKind + static_cast<int>(field);
}

// Le remplacement, commun, puis la fin de la plage.
inline constexpr int kCastBarReplacedRow =
    kCastBarRowBase + static_cast<int>(ro::ui::kEntityBarKindCount) * kCastBarFieldsPerKind;
inline constexpr int kCastBarRowEnd = kCastBarReplacedRow + 1;

static_assert(kWeaponRefineRowBase + static_cast<int>(WeaponRefineRow::kCount) <= kCastBarRowBase,
              "la plage de la barre de cast chevauche celle du refine");

inline constexpr std::size_t kCastBarNameCount =
    static_cast<std::size_t>(ro::ui::CastBarName::kCount);

class CastBarOptions {
 public:
  // Le réglage à peindre, rangs de nom bornés.
  [[nodiscard]] ro::ui::CastBarsLook look() const noexcept;

  // ⚠ AVANT LA LECTURE DU FICHIER : la valeur présente ici est le défaut.
  void Declare(SettingsFile& into);

  // Les rangées de l'espèce ouverte, ou aucune — un item au sol n'incante pas.
  void Compose(std::optional<ro::ui::EntityBarKind> kind);
  [[nodiscard]] std::span<const ro::ui::OptionRowView> rows() const noexcept { return rows_; }
  // Ce qui vaut pour toutes les espèces : le remplacement.
  [[nodiscard]] std::span<const ro::ui::OptionRowView> shared_rows() const noexcept {
    return shared_rows_;
  }

  // Rend vrai quand la ligne était l'une des siennes.
  [[nodiscard]] bool Change(const ro::ui::OptionsWindowAction& asked);

 private:
  // ⚠ LE MODE DE NOM SE LIT DU FICHIER EN RANG BRUT : `look()` le borne.
  ro::ui::CastBarsLook look_;
  // Les libellés de la liste, traduits à chaque composition. ⚠ ICI ET NON SUR
  // LA PILE : la rangée n'en garde qu'une vue, lue après la composition.
  std::array<const char*, kCastBarNameCount> name_choices_{};
  std::vector<ro::ui::OptionRowView> rows_;
  std::vector<ro::ui::OptionRowView> shared_rows_;
};

}  // namespace moonclient
