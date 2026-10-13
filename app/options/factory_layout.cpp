#include "options/factory_layout.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "ro/ui/hud_frame.h"
#include "ro/ui/menu_icons.h"
#include "ro/ui/status_portrait.h"
#include "ro/ui/window_places.h"

namespace moonclient {
namespace {

using ro::ui::AnchorSide;

[[nodiscard]] constexpr int Side(AnchorSide side) { return static_cast<int>(side); }

// LES JAUGES : une colonne à droite du portrait de statut — vie et magie, les
// deux expériences en barres minces, le poids et la bourse —, et l'incantation
// centrée au-dessus du bas de l'écran.
constexpr float kGaugeColumnLeft = 108.0F;
constexpr float kGaugeColumnWidth = 224.0F;
constexpr float kExpGaugeHeight = 8.0F;
constexpr float kLifeTop = 27.0F;
constexpr float kMagicTop = 47.0F;
constexpr float kBaseExpTop = 67.0F;
constexpr float kJobExpTop = 79.0F;
constexpr float kWeightTop = 91.0F;
constexpr float kZenyTop = 111.0F;
constexpr float kCastAboveBottom = 210.0F;
constexpr float kCastWidth = 192.0F;
constexpr float kCastHeight = 24.0F;

// LE PORTRAIT DE STATUT, au coin haut-gauche : le portrait et le niveau sous
// lui, le pseudo et la classe côte à côte à sa droite, au-dessus des jauges.
constexpr ro::ui::HudRect kPortraitHeadRect{
    .x = 4.0F, .y = 3.0F, .width = 100.0F, .height = 100.0F};
constexpr ro::ui::HudRect kPortraitNameRect{
    .x = 108.0F, .y = 3.0F, .width = 112.0F, .height = 20.0F};
constexpr ro::ui::HudRect kPortraitClassRect{
    .x = 224.0F, .y = 3.0F, .width = 108.0F, .height = 20.0F};
constexpr ro::ui::HudRect kPortraitLevelRect{
    .x = 4.0F, .y = 103.0F, .width = 100.0F, .height = 24.0F};
// Assis, un peu dézoomé, le personnage entier descendu vers le bas du cadre.
constexpr int kPortraitSittingAnimation = 2;
constexpr float kPortraitZoom = 0.8F;
constexpr float kPortraitOffsetY = 0.21F;

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
    FactoryIconPlace{.stem = "status", .x = 4.0F, .y = 135.0F},
    FactoryIconPlace{.stem = "party", .x = 36.0F, .y = 136.0F},
    FactoryIconPlace{.stem = "item", .x = 68.0F, .y = 135.0F},
    FactoryIconPlace{.stem = "skill", .x = 100.0F, .y = 135.0F},
    FactoryIconPlace{.stem = "booking", .x = 132.0F, .y = 135.0F},
    FactoryIconPlace{.stem = "equip", .x = 4.0F, .y = 170.0F},
    FactoryIconPlace{.stem = "guild", .x = 36.0F, .y = 169.0F},
    FactoryIconPlace{.stem = "battle", .x = 68.0F, .y = 169.0F},
    FactoryIconPlace{.stem = "quest", .x = 100.0F, .y = 169.0F},
    FactoryIconPlace{.stem = "map", .x = 132.0F, .y = 169.0F},
    FactoryIconPlace{.stem = "navigation", .x = 4.0F, .y = 203.0F},
    FactoryIconPlace{.stem = "option", .x = 36.0F, .y = 203.0F},
    FactoryIconPlace{.stem = "bank", .x = 68.0F, .y = 203.0F},
    FactoryIconPlace{.stem = "mail", .x = 100.0F, .y = 203.0F},
    FactoryIconPlace{.stem = "achievement", .x = 132.0F, .y = 203.0F},
    FactoryIconPlace{.stem = "shop", .x = 4.0F, .y = 235.0F},
    FactoryIconPlace{.stem = "keyboard", .x = 36.0F, .y = 237.0F},
    FactoryIconPlace{.stem = "album", .x = 68.0F, .y = 237.0F},
    FactoryIconPlace{.stem = "atlas", .x = 100.0F, .y = 237.0F},
    FactoryIconPlace{.stem = "tracker", .x = 132.0F, .y = 237.0F},
    FactoryIconPlace{.stem = "ranking", .x = 4.0F, .y = 271.0F},
    FactoryIconPlace{.stem = "pet", .x = 36.0F, .y = 271.0F},
    FactoryIconPlace{
        .stem = "NC_CashShop", .x = 254.0F, .y = 14.0F, .across = ro::ui::AnchorSide::kEnd},
};

// Les altérations d'état : icônes de trente pixels, sans filet, sur un fond
// entièrement transparent — la teinte d'origine, l'opacité retirée.
constexpr float kStatusSidePixels = 30.0F;
constexpr ImU32 kStatusAlphaMask = 0x00FFFFFFU;

// Les barres et les noms au-dessus des entités, serrés contre le sprite.
constexpr float kEntityBarGapPixels = 3.0F;
constexpr float kOwnBarHeightPixels = 3.0F;

// La grille d'alignement au pixel près, aimantée.
constexpr int kAlignGridStepPixels = 1;

// Le bandeau d'objet obtenu, centré : voir `ItemObtainToastLook::pos_x`.
constexpr int kToastCentered = -1;

[[nodiscard]] ro::ui::StatBarLook& Bar(ro::ui::StatBarsLook& look, ro::ui::StatBar which) {
  return look.bars[static_cast<std::size_t>(which)];
}

void Place(ro::ui::StatBarLook& bar, float x, float y, float width) {
  bar.rect.x = x;
  bar.rect.y = y;
  bar.rect.width = width;
}

}  // namespace

ro::ui::StatBarsLook FactoryStatBars() {
  using ro::ui::StatBar;
  ro::ui::StatBarsLook look;
  look.shown = true;
  look.locked = true;
  look.keep_maxed_exp = true;
  Place(Bar(look, StatBar::kHealth), kGaugeColumnLeft, kLifeTop, kGaugeColumnWidth);
  Place(Bar(look, StatBar::kMagic), kGaugeColumnLeft, kMagicTop, kGaugeColumnWidth);
  Place(Bar(look, StatBar::kBaseExp), kGaugeColumnLeft, kBaseExpTop, kGaugeColumnWidth);
  Bar(look, StatBar::kBaseExp).rect.height = kExpGaugeHeight;
  Place(Bar(look, StatBar::kJobExp), kGaugeColumnLeft, kJobExpTop, kGaugeColumnWidth);
  Bar(look, StatBar::kJobExp).rect.height = kExpGaugeHeight;
  Place(Bar(look, StatBar::kWeight), kGaugeColumnLeft, kWeightTop, kGaugeColumnWidth);
  // Le poids dit sa part ET ses valeurs : on sait ce qui rentre encore.
  Bar(look, StatBar::kWeight).text = ro::ui::StatBarText::kBoth;
  Place(Bar(look, StatBar::kZeny), kGaugeColumnLeft, kZenyTop, kGaugeColumnWidth);
  ro::ui::StatBarLook& cast = Bar(look, StatBar::kCast);
  Place(cast, 0.0F, kCastAboveBottom, kCastWidth);
  cast.rect.height = kCastHeight;
  cast.rect.across = Side(AnchorSide::kCenter);
  cast.rect.down = Side(AnchorSide::kEnd);
  return look;
}

StatusPortraitSetup FactoryStatusPortrait() {
  using ro::ui::PortraitPart;
  StatusPortraitSetup setup;
  setup.look.shown = true;
  setup.look.locked = true;
  const auto rect = [&setup](PortraitPart part) -> ro::ui::HudRect& {
    return setup.look.parts[static_cast<std::size_t>(part)].rect;
  };
  rect(PortraitPart::kHead) = kPortraitHeadRect;
  rect(PortraitPart::kName) = kPortraitNameRect;
  rect(PortraitPart::kClass) = kPortraitClassRect;
  rect(PortraitPart::kLevel) = kPortraitLevelRect;
  setup.framing.head_only = false;
  setup.framing.zoom = kPortraitZoom;
  setup.framing.offset_y = kPortraitOffsetY;
  setup.framing.animation = kPortraitSittingAnimation;
  return setup;
}

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

ro::ui::StatusBarLayout FactoryStatusBar() {
  ro::ui::StatusBarLayout look;
  look.border = false;
  look.background_color = ro::ui::kDefaultStatusBackgroundColor & kStatusAlphaMask;
  look.side_px = kStatusSidePixels;
  look.anchor = static_cast<int>(ro::ui::StatusAnchor::kTopRight);
  return look;
}

ro::ui::ShortcutBarsLayout FactoryShortcutBars() {
  ro::ui::ShortcutBarsLayout look;
  // La seconde barre, montrée : deux rangées de douze sous la main.
  look.bars[1].shown = true;
  return look;
}

ro::ui::EntityBarsLook FactoryEntityBars() {
  using ro::ui::EntityBarKind;
  ro::ui::EntityBarsLook look;
  for (ro::ui::EntityBarLook& kind : look.kinds) {
    kind.offset_pixels = kEntityBarGapPixels;
  }
  ro::ui::EntityBarLook& own = look.kinds[static_cast<std::size_t>(EntityBarKind::kOwn)];
  own.shown = true;
  own.height_pixels = kOwnBarHeightPixels;
  look.kinds[static_cast<std::size_t>(EntityBarKind::kParty)].magic = true;
  return look;
}

ro::render::EntityNamesLook FactoryEntityNames() {
  using ro::render::NamePlateKind;
  ro::render::EntityNamesLook look;
  const auto gap = [&look](NamePlateKind kind, float pixels) {
    look.kinds[static_cast<std::size_t>(kind)].offset_pixels = pixels;
  };
  look.kinds[static_cast<std::size_t>(NamePlateKind::kOwn)].shown = true;
  // Collés au sprite pour les êtres, à deux pixels pour un PNJ et un butin, à
  // trois pour soi : son nom passe sous sa propre barre.
  gap(NamePlateKind::kOwn, 3.0F);
  gap(NamePlateKind::kPlayer, 0.0F);
  gap(NamePlateKind::kNonPlayer, 2.0F);
  gap(NamePlateKind::kMonster, 0.0F);
  gap(NamePlateKind::kBoss, 0.0F);
  gap(NamePlateKind::kGroundItem, 2.0F);
  return look;
}

ro::ui::AlignGrid FactoryAlignGrid() {
  ro::ui::AlignGrid grid;
  grid.magnetic = true;
  grid.cell_pixels = kAlignGridStepPixels;
  return grid;
}

ro::ui::ItemObtainToastLook FactoryItemObtain() {
  ro::ui::ItemObtainToastLook look;
  look.pos_x = kToastCentered;
  look.show_icon = true;
  return look;
}

ro::ui::ChatAppearance FactoryChatAppearance() {
  ro::ui::ChatAppearance look;
  look.battle_mode = true;
  return look;
}

}  // namespace moonclient
