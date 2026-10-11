#include "options/factory_layout.h"

#include <cstddef>
#include <cstdint>

#include "ro/ui/hud_frame.h"
#include "ro/ui/window_places.h"

namespace moonclient {
namespace {

using ro::ui::AnchorSide;

[[nodiscard]] constexpr int Side(AnchorSide side) { return static_cast<int>(side); }

// LES JAUGES : un bloc collé au coin haut-gauche — vie et magie sur deux
// lignes, les deux expériences côte à côte dessous —, et l'incantation centrée
// au-dessus du bas de l'écran.
constexpr float kGaugeBlockWidth = 224.0F;
constexpr float kLifeHeight = 18.0F;
constexpr float kExpRowTop = 34.0F;
constexpr float kBaseExpWidth = 104.0F;
constexpr float kJobExpWidth = kGaugeBlockWidth - kBaseExpWidth;
constexpr float kCastAboveBottom = 210.0F;
constexpr float kCastWidth = 192.0F;
constexpr float kCastHeight = 24.0F;

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
  Place(Bar(look, StatBar::kHealth), 0.0F, 0.0F, kGaugeBlockWidth);
  Bar(look, StatBar::kHealth).rect.height = kLifeHeight;
  Place(Bar(look, StatBar::kMagic), 0.0F, kLifeHeight, kGaugeBlockWidth);
  Place(Bar(look, StatBar::kBaseExp), 0.0F, kExpRowTop, kBaseExpWidth);
  Place(Bar(look, StatBar::kJobExp), kBaseExpWidth, kExpRowTop, kJobExpWidth);
  Bar(look, StatBar::kWeight).shown = false;
  Bar(look, StatBar::kZeny).shown = false;
  ro::ui::StatBarLook& cast = Bar(look, StatBar::kCast);
  Place(cast, 0.0F, kCastAboveBottom, kCastWidth);
  cast.rect.height = kCastHeight;
  cast.rect.across = Side(AnchorSide::kCenter);
  cast.rect.down = Side(AnchorSide::kEnd);
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
