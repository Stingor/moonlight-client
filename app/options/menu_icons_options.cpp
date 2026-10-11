#include "options/menu_icons_options.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <string_view>

#include "ro/core/i18n.h"
#include "ro/ui/window_places.h"

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

// Une place d'usine, par nom de bitmap : voir `FactoryMenuIcons`.
struct FactoryIconPlace {
  std::string_view stem;
  float x = 0.0F;
  float y = 0.0F;
  ro::ui::AnchorSide across = ro::ui::AnchorSide::kStart;
};

// ⚠ PAR NOM DE BITMAP ET NON PAR RANG : le nom est celui du fichier du jeu, et
// ne bouge pas quand le catalogue s'ordonne autrement.
constexpr std::array kFactoryIconPlaces = {
    FactoryIconPlace{.stem = "status", .x = 3.0F, .y = 55.0F},
    FactoryIconPlace{.stem = "party", .x = 35.0F, .y = 56.0F},
    FactoryIconPlace{.stem = "item", .x = 67.0F, .y = 55.0F},
    FactoryIconPlace{.stem = "skill", .x = 99.0F, .y = 55.0F},
    FactoryIconPlace{.stem = "booking", .x = 131.0F, .y = 55.0F},
    FactoryIconPlace{.stem = "equip", .x = 3.0F, .y = 90.0F},
    FactoryIconPlace{.stem = "guild", .x = 35.0F, .y = 89.0F},
    FactoryIconPlace{.stem = "battle", .x = 67.0F, .y = 89.0F},
    FactoryIconPlace{.stem = "quest", .x = 99.0F, .y = 89.0F},
    FactoryIconPlace{.stem = "map", .x = 131.0F, .y = 89.0F},
    FactoryIconPlace{.stem = "navigation", .x = 3.0F, .y = 123.0F},
    FactoryIconPlace{.stem = "option", .x = 35.0F, .y = 123.0F},
    FactoryIconPlace{.stem = "bank", .x = 67.0F, .y = 123.0F},
    FactoryIconPlace{.stem = "mail", .x = 99.0F, .y = 123.0F},
    FactoryIconPlace{.stem = "achievement", .x = 131.0F, .y = 123.0F},
    FactoryIconPlace{.stem = "shop", .x = 3.0F, .y = 155.0F},
    FactoryIconPlace{.stem = "keyboard", .x = 35.0F, .y = 157.0F},
    FactoryIconPlace{.stem = "album", .x = 67.0F, .y = 157.0F},
    FactoryIconPlace{.stem = "atlas", .x = 99.0F, .y = 157.0F},
    FactoryIconPlace{.stem = "tracker", .x = 131.0F, .y = 157.0F},
    FactoryIconPlace{.stem = "ranking", .x = 3.0F, .y = 191.0F},
    FactoryIconPlace{.stem = "pet", .x = 35.0F, .y = 191.0F},
    FactoryIconPlace{
        .stem = "NC_CashShop", .x = 254.0F, .y = 14.0F, .across = ro::ui::AnchorSide::kEnd},
};

[[nodiscard]] OptionRowView Toggle(int id, const char* label, const char* help, bool on) {
  return OptionRowView{
      .id = id, .label = label, .help = help, .kind = OptionKind::kToggle, .on = on};
}

[[nodiscard]] int IconRowId(MenuCommand command) {
  return MenuIconsRowId(MenuIconsRow::kFirstIcon) + static_cast<int>(command);
}

}  // namespace

ro::ui::MenuIconsLook FactoryMenuIcons() {
  ro::ui::MenuIconsLook look;
  for (const ro::ui::MenuIconEntry& entry : ro::ui::MenuIconCatalogue()) {
    const auto found = std::ranges::find(kFactoryIconPlaces, entry.stem, &FactoryIconPlace::stem);
    if (found == kFactoryIconPlaces.end()) {
      continue;
    }
    ro::ui::MenuIconPlace& place = look.places[static_cast<std::size_t>(entry.command)];
    place.x = found->x;
    place.y = found->y;
    place.across = static_cast<int>(found->across);
  }
  return look;
}

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
