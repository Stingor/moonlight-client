// LES CLÉS DES BARRES DE RACCOURCIS, ET CE QUI LES DÉCLARE.
//
// ⚠ À PART DES AUTRES CLÉS parce que huit barres portent chacune une vingtaine
// de réglages : c'est une boucle, et elle se lit pour elle-même.

#include <cstddef>
#include <string>

#include "ro/core/printed.h"
#include "ro/ui/shortcut_bar.h"

#include "options/game_options.h"

namespace moonclient {
namespace {

// LES CLÉS DES BARRES DE RACCOURCIS, composées : « bar3_columns ».
//
// ⚠ LE NUMÉRO EST CELUI QUE LE JOUEUR VOIT, à partir de UN, comme dans le
// fichier des barres et dans celui des raccourcis. Trois fichiers qui
// compteraient différemment obligeraient à traduire de tête.
//
// ⚠ GRAVÉES comme toutes les autres : les changer rend à tous les joueurs la
// disposition d'origine, sans que rien ne le dise.
constexpr const char* kShortcutBarKeys = "bar%zu_%s";
constexpr const char* kShortcutShowKeysKey = "bars_keys";
constexpr const char* kShortcutStickyKey = "bars_magnetized";
constexpr const char* kShortcutWheelBarKey = "bars_wheel";

[[nodiscard]] std::string BarKey(std::size_t bar, const char* what) {
  return ro::core::Printed(kShortcutBarKeys, bar + 1, what);
}

}  // namespace

void GameOptions::DeclareShortcutBars(SettingsFile& into) {
  // ⚠ UNE BOUCLE ET NON QUARANTE LIGNES. Les huit barres portent les mêmes cinq
  // réglages ; les écrire à la main serait quarante occasions de se tromper de
  // numéro, et la quarante-et-unième le jour où une sixième s'ajoute.
  //
  // ⚠ ET C'EST L'ADRESSE DU CHAMP QU'ON DÉCLARE, comme partout ailleurs : la
  // lecture et l'écriture parcourent la même table, donc elles ne peuvent pas
  // diverger. Voir `settings_file.h`.
  into.Declare(kShortcutShowKeysKey, &shortcut_bars_.show_keys);
  into.Declare(kShortcutStickyKey, &shortcut_bars_.sticky);
  into.Declare(kShortcutWheelBarKey, &shortcut_bars_.wheel_bar);
  for (std::size_t bar = 0; bar < ro::ui::kShortcutBarCount; ++bar) {
    ro::ui::ShortcutBarLayout& look = shortcut_bars_.bars[bar];
    into.Declare(BarKey(bar, "shown"), &look.shown);
    into.Declare(BarKey(bar, "passthrough"), &look.click_through);
    into.Declare(BarKey(bar, "columns"), &look.columns);
    into.Declare(BarKey(bar, "slots"), &look.shown_slots);
    into.Declare(BarKey(bar, "size"), &look.slot_side_px);
    into.Declare(BarKey(bar, "spacing"), &look.gap_px);
    // L'APPARENCE, PAR BARRE. ⚠ MÊME BOUCLE, et c'est ce qui la rend tenable :
    // treize réglages de plus écrits à la main pour huit barres, ce serait cent
    // quatre lignes et autant d'occasions de se tromper de numéro.
    into.Declare(BarKey(bar, "background_empty"), &look.empty_color);
    into.Declare(BarKey(bar, "background_skill"), &look.skill_color);
    into.Declare(BarKey(bar, "background_item"), &look.item_color);
    into.Declare(BarKey(bar, "border"), &look.border_color);
    into.Declare(BarKey(bar, "border_thickness"), &look.border_thickness);
    into.Declare(BarKey(bar, "key_color"), &look.key_color);
    into.Declare(BarKey(bar, "key_size"), &look.key_scale);
    into.Declare(BarKey(bar, "number_color"), &look.count_color);
    into.Declare(BarKey(bar, "number_size"), &look.count_scale);
    into.Declare(BarKey(bar, "countdown_color"), &look.countdown_color);
    into.Declare(BarKey(bar, "shadow_color"), &look.shadow_color);
    into.Declare(BarKey(bar, "shadow_kind"), &look.shadow_kind);
    into.Declare(BarKey(bar, "shadow_offset"), &look.shadow_offset);
    into.Declare(BarKey(bar, "worn_color"), &look.worn_color);
    into.Declare(BarKey(bar, "worn_thickness"), &look.worn_thickness);
    into.Declare(BarKey(bar, "worn_softness"), &look.worn_softness);
  }
}

}  // namespace moonclient
