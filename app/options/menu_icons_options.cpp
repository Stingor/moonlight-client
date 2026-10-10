#include "options/menu_icons_options.h"

#include <cstddef>
#include <string>

#include "ro/core/i18n.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::ui::MenuCommand;
using ro::ui::OptionKind;
using ro::ui::OptionRowView;

// Les clés du fichier. GRAVÉES : les changer rend à tous les joueurs les
// réglages d'origine, sans que rien ne le dise.
constexpr const char* kLockedKey = "menu_icons_locked";
constexpr const char* kStickyKey = "menu_icons_snapped";

// La clé d'une propriété d'une icône : `menu_icon_<nom du bitmap>_<propriété>`.
// ⚠ LE NOM DU CATALOGUE, qui est celui du fichier du jeu : il ne bouge pas quand
// un libellé se traduit autrement.
[[nodiscard]] std::string IconKey(const ro::ui::MenuIconEntry& entry, const char* property) {
  std::string key = "menu_icon_";
  key += entry.stem;
  key += '_';
  key += property;
  return key;
}

[[nodiscard]] OptionRowView Toggle(int id, const char* label, const char* help, bool on) {
  return OptionRowView{
      .id = id, .label = label, .help = help, .kind = OptionKind::kToggle, .on = on};
}

[[nodiscard]] int IconRowId(MenuCommand command) {
  return MenuIconsRowId(MenuIconsRow::kFirstIcon) + static_cast<int>(command);
}

}  // namespace

void MenuIconsOptions::Declare(SettingsFile& into) {
  into.Declare(kLockedKey, &look_.locked);
  into.Declare(kStickyKey, &look_.sticky);
  // ⚠ SEULES LES ICÔNES QUE LE CATALOGUE MONTRE : les autres n'ont ni place ni
  // case, et leurs clés ne seraient que du bruit dans un fichier qu'on relit.
  for (const ro::ui::MenuIconEntry& entry : ro::ui::MenuIconCatalogue()) {
    if (!entry.shown) {
      continue;
    }
    ro::ui::MenuIconPlace& place = look_.places[static_cast<std::size_t>(entry.command)];
    into.Declare(IconKey(entry, "x"), &place.x);
    into.Declare(IconKey(entry, "y"), &place.y);
    into.Declare(IconKey(entry, "anchor_x"), &place.across);
    into.Declare(IconKey(entry, "anchor_y"), &place.down);
    into.Declare(IconKey(entry, "hidden"), &place.hidden);
  }
}

void MenuIconsOptions::Compose() {
  rows_.clear();
  rows_.push_back(Toggle(MenuIconsRowId(MenuIconsRow::kLocked), Tr("Verrouiller"),
                         Tr("Verrouillées, les icônes se cliquent. Déverrouillées, on les "
                            "déplace une à une au glisser — elles n'ouvrent alors plus rien. "
                            "CTRL emporte celles qui se touchent, d'un seul bloc."),
                         look_.locked));
  rows_.back().heading = Tr("Placement");
  rows_.push_back(Toggle(MenuIconsRowId(MenuIconsRow::kSticky), Tr("Aimanter entre elles"),
                         Tr("Les bords de l'icône qu'on déplace s'alignent sur ceux des autres. "
                            "La grille d'alignement de l'interface aimante aussi, si elle est "
                            "allumée."),
                         look_.sticky));
  rows_.back().disabled = look_.locked;
  rows_.push_back(OptionRowView{
      .id = MenuIconsRowId(MenuIconsRow::kForgetPlaces),
      .label = Tr("Remettre en grille"),
      .help = Tr("Chaque icône revient à sa case d'origine, au coin haut-droit de l'écran. "
                 "Les icônes masquées le restent."),
      .kind = OptionKind::kAction,
      .confirm = Tr("Remettre toutes les icônes à leur place d'origine ?")});

  bool first = true;
  for (const ro::ui::MenuIconEntry& entry : ro::ui::MenuIconCatalogue()) {
    if (!entry.shown) {
      continue;
    }
    const bool hidden = look_.places[static_cast<std::size_t>(entry.command)].hidden;
    const char* help =
        entry.command == MenuCommand::kCashShopButton
            ? Tr("Le bouton à part, à gauche des icônes, qui ouvre le Vote Shop. C'est le "
                 "réglage de la commande /cashshop.")
            : nullptr;
    rows_.push_back(
        Toggle(IconRowId(entry.command), ro::ui::MenuIconLabel(entry.command), help, !hidden));
    if (first) {
      rows_.back().heading = Tr("Icônes montrées");
      first = false;
    }
  }
}

bool MenuIconsOptions::Change(const ro::ui::OptionsWindowAction& asked) {
  if (asked.changed < kMenuIconsRowBase ||
      asked.changed >= kMenuIconsRowBase + static_cast<int>(MenuIconsRow::kCount)) {
    return false;
  }
  const int row = asked.changed - kMenuIconsRowBase;
  if (row >= static_cast<int>(MenuIconsRow::kFirstIcon)) {
    const auto index = static_cast<std::size_t>(row - static_cast<int>(MenuIconsRow::kFirstIcon));
    look_.places[index].hidden = !asked.on;
    return true;
  }
  switch (static_cast<MenuIconsRow>(row)) {
    case MenuIconsRow::kLocked:
      look_.locked = asked.on;
      break;
    case MenuIconsRow::kSticky:
      look_.sticky = asked.on;
      break;
    case MenuIconsRow::kForgetPlaces:
      ro::ui::ForgetMenuIconPlaces(look_);
      break;
    case MenuIconsRow::kFirstIcon:
    case MenuIconsRow::kCount:
      break;
  }
  return true;
}

bool MenuIconsOptions::ToggleCashShopButton() noexcept {
  ro::ui::MenuIconPlace& place =
      look_.places[static_cast<std::size_t>(MenuCommand::kCashShopButton)];
  place.hidden = !place.hidden;
  return !place.hidden;
}

}  // namespace moonclient
