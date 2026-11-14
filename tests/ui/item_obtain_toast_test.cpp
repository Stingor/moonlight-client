#include "ro/ui/item_obtain_toast.h"

#include <catch2/catch_test_macros.hpp>

namespace {

using ro::ui::ColorAlphaPercent;
using ro::ui::ItemObtainFontPixels;
using ro::ui::ItemObtainMetrics;
using ro::ui::ItemObtainOriginX;
using ro::ui::ItemObtainRowHeight;
using ro::ui::ItemObtainRowWidth;
using ro::ui::ItemObtainTextLeft;
using ro::ui::ItemObtainToastLook;
using ro::ui::SnapToPixel;
using ro::ui::WithAlphaPercent;

// ⚠ LES MESURES SE RÉÉCRIVENT EN TOUTES LETTRES ICI — 14, 28, 42, 34, 41, 13.
// Un essai qui relirait la constante qu'il vérifie ne vérifierait rien : il
// suivrait la faute.
constexpr float kTile = 14.0F;

[[nodiscard]] ItemObtainMetrics Metrics(float font_px, float tile_px, float scale = 1.0F) {
  ItemObtainMetrics metrics;
  metrics.font_px = font_px;
  metrics.tile_px = tile_px;
  metrics.scale = scale;
  metrics.screen_width_px = 1920.0F;
  return metrics;
}

}  // namespace

TEST_CASE("Bandeau : avec le cadre, la hauteur tombe sur la grille de tuiles") {
  ItemObtainToastLook look;
  look.show_frame = true;
  look.show_icon = false;
  look.pad_vertical_px = 0;

  // Deux coins posés un pour un, et rien entre eux : c'est le seul palier où le
  // neuf-morceaux n'écrase aucune tuile.
  CHECK(ItemObtainRowHeight(look, Metrics(16.0F, kTile)) == 28.0F);
  // Un contenu qui dépasse monte d'une tuile entière, jamais d'un demi-pas.
  CHECK(ItemObtainRowHeight(look, Metrics(30.0F, kTile)) == 42.0F);
  CHECK(ItemObtainRowHeight(look, Metrics(43.0F, kTile)) == 56.0F);
}

TEST_CASE("Bandeau : sans le cadre, la hauteur suit le contenu et la marge") {
  ItemObtainToastLook look;
  look.show_frame = false;
  look.show_icon = false;
  look.pad_vertical_px = 0;
  CHECK(ItemObtainRowHeight(look, Metrics(16.0F, kTile)) == 16.0F);

  look.pad_vertical_px = 3;
  CHECK(ItemObtainRowHeight(look, Metrics(16.0F, kTile)) == 22.0F);

  // L'icône fait 24 : elle impose la hauteur tant que la police est plus petite.
  look.pad_vertical_px = 0;
  look.show_icon = true;
  CHECK(ItemObtainRowHeight(look, Metrics(16.0F, kTile)) == 24.0F);
  CHECK(ItemObtainRowHeight(look, Metrics(30.0F, kTile)) == 30.0F);
}

TEST_CASE("Bandeau : le texte recule quand il n'y a pas d'icône devant") {
  const ItemObtainMetrics metrics = Metrics(16.0F, kTile);
  CHECK(ItemObtainTextLeft(metrics, /*has_icon=*/true) == 41.0F);
  CHECK(ItemObtainTextLeft(metrics, /*has_icon=*/false) == 13.0F);
}

TEST_CASE("Bandeau : la largeur arrondit VERS LE HAUT et respecte son plancher") {
  ItemObtainToastLook look;
  look.show_frame = false;

  // 13 + 40,2 + 30 + 13 = 96,2 → 97 : arrondir vers le bas rognerait le dernier
  // glyphe d'un demi-pixel.
  CHECK(ItemObtainRowWidth(look, Metrics(16.0F, kTile), /*has_icon=*/false, 40.2F, 30.0F) == 97.0F);
  // Le plancher du client, quand il n'y a presque rien à écrire.
  CHECK(ItemObtainRowWidth(look, Metrics(16.0F, kTile), /*has_icon=*/false, 0.0F, 0.0F) == 34.0F);
}

TEST_CASE("Bandeau : avec le cadre, l'intérieur se cale sur la grille de tuiles") {
  ItemObtainToastLook look;
  look.show_frame = true;

  // 🔴 LA BANDE CENTRALE EST ÉTIRÉE : un intérieur qui n'est pas un multiple de
  // la tuile fait échantillonner la texture entre ses texels sur toute la
  // longueur du bandeau.
  const float width =
      ItemObtainRowWidth(look, Metrics(16.0F, kTile), /*has_icon=*/true, 100.0F, 60.0F);
  const float inner = width - 2.0F * kTile;
  CHECK(inner > 0.0F);
  CHECK(inner == std::floor(inner / kTile) * kTile);
  // Et jamais en dessous de ce qu'il faut pour écrire : 41 + 100 + 60 + 13 = 214.
  CHECK(width >= 214.0F);
}

TEST_CASE("Bandeau : une abscisse négative veut dire centré") {
  ItemObtainToastLook look;
  const ItemObtainMetrics metrics = Metrics(16.0F, kTile);

  look.pos_x = 220;
  CHECK(ItemObtainOriginX(look, metrics, 300.0F) == 220.0F);

  look.pos_x = -1;
  // (1920 - 300) / 2 : le centrage divise par deux, d'où l'arrondi au pixel.
  CHECK(ItemObtainOriginX(look, metrics, 300.0F) == 810.0F);
  CHECK(ItemObtainOriginX(look, metrics, 301.0F) == 810.0F);
}

TEST_CASE("Bandeau : le corps de police reste entier") {
  ItemObtainToastLook look;
  look.font_scale_percent = 130;
  // 16 × 1,30 = 20,8 : un corps fractionnaire ferait rééchantillonner l'atlas.
  CHECK(ItemObtainFontPixels(look, 16.0F) == 21.0F);

  // Sous le plancher, la police ne se distingue plus : le réglage s'y arrête.
  look.font_scale_percent = 10;
  CHECK(ItemObtainFontPixels(look, 16.0F) == 8.0F);
}

TEST_CASE("Bandeau : l'opacité fait l'aller-retour sans dériver") {
  // 🔴 L'ALPHA ET LE CURSEUR SONT LE MÊME OCTET, vu de deux façons : sans
  // arrondi au plus proche, chaque aller-retour par le curseur ferait perdre un
  // point d'opacité, et un fond opaque finirait translucide.
  for (int percent = 0; percent <= 100; ++percent) {
    const ImU32 color = WithAlphaPercent(IM_COL32(0x10, 0x20, 0x30, 0), percent);
    CHECK(ColorAlphaPercent(color) == percent);
    // La teinte ne bouge pas : seul l'alpha est touché.
    CHECK((color & ~IM_COL32_A_MASK) == (IM_COL32(0x10, 0x20, 0x30, 0) & ~IM_COL32_A_MASK));
  }
  CHECK(ColorAlphaPercent(IM_COL32(0, 0, 0, 255)) == 100);
  CHECK(WithAlphaPercent(IM_COL32(0, 0, 0, 0), 300) == IM_COL32(0, 0, 0, 255));
  CHECK(WithAlphaPercent(IM_COL32(0, 0, 0, 255), -5) == IM_COL32(0, 0, 0, 0));
}

TEST_CASE("Bandeau : tout tombe sur un pixel entier") {
  CHECK(SnapToPixel(10.4F) == 10.0F);
  CHECK(SnapToPixel(10.5F) == 11.0F);
  CHECK(SnapToPixel(10.6F) == 11.0F);
}
