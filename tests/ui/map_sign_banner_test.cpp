// LE BANDEAU DU NOM DE LA CARTE : sa place à l'écran, la taille de ses textes
// et son effacement.

#include "ro/ui/map_sign_banner.h"

#include <cmath>

#include <catch2/catch_test_macros.hpp>

namespace {

// L'image livrée, et un écran de 720 lignes.
constexpr ImVec2 kImage(500.0F, 190.0F);
constexpr ImVec2 kScreen(1280.0F, 720.0F);

[[nodiscard]] bool Whole(float value) { return std::trunc(value) == value; }

}  // namespace

TEST_CASE("Bandeau de carte : un cinquième de l'écran, centré, au tiers haut", "[ui][map_sign]") {
  const ro::ui::MapSignLayout layout = ro::ui::MapSignLayoutOf(kScreen, kImage);
  // La bande : 144 pixels, dont le haut — 86,4 — tombe sur le pixel 86.
  CHECK(layout.image_size.y == 144.0F);
  CHECK(layout.image_top_left.y == 86.0F);
  // ⚠ L'IMAGE GARDE SES PROPORTIONS : la largeur suit la hauteur, pas l'écran.
  // 500 × 144/190 = 378,9 → 378, posée à (1280 − 378)/2.
  CHECK(layout.image_size.x == 378.0F);
  CHECK(layout.image_top_left.x == 451.0F);
  // Les textes suivent la même échelle : un pixel de l'image en vaut 144/190.
  CHECK(layout.scale == layout.image_size.y / kImage.y);
  CHECK(layout.subtitle_top == layout.image_top_left.y + std::trunc(72.0F * layout.scale));
  CHECK(layout.title_top == layout.image_top_left.y + std::trunc(95.0F * layout.scale));
}

TEST_CASE("Bandeau de carte : la cellule d'un texte est tronquée, puis grandie de trois pixels",
          "[ui][map_sign]") {
  // 16 × 144/190 = 12,1 → 12 + 3 ; 44 × 144/190 = 33,3 → 33 + 3.
  const ro::ui::MapSignLayout hd = ro::ui::MapSignLayoutOf(kScreen, kImage);
  CHECK(hd.subtitle_size == 15.0F);
  CHECK(hd.title_size == 36.0F);
  // Sur 1080 lignes : bande de 216, 16 × 216/190 = 18,2 → 21 ; 44 × … = 50,0 → 53.
  const ro::ui::MapSignLayout full = ro::ui::MapSignLayoutOf(ImVec2(1920.0F, 1080.0F), kImage);
  CHECK(full.image_top_left.y == 129.0F);
  CHECK(full.subtitle_size == 21.0F);
  CHECK(full.title_size == 53.0F);
}

TEST_CASE("Bandeau de carte : la même allure à toutes les résolutions, au pixel près",
          "[ui][map_sign]") {
  const ro::ui::MapSignLayout small = ro::ui::MapSignLayoutOf(kScreen, kImage);
  const ro::ui::MapSignLayout large =
      ro::ui::MapSignLayoutOf(ImVec2(kScreen.x * 2.0F, kScreen.y * 2.0F), kImage);
  CHECK(large.scale == small.scale * 2.0F);
  // La troncature coûte au plus un pixel de chaque côté ; les trois pixels
  // ajoutés, eux, ne doublent pas.
  const float doubled_cell = (small.title_size - ro::ui::kMapSignCellGrowthPx) * 2.0F;
  CHECK(large.title_size - ro::ui::kMapSignCellGrowthPx >= doubled_cell);
  CHECK(large.title_size - ro::ui::kMapSignCellGrowthPx <= doubled_cell + 1.0F);
}

TEST_CASE("Bandeau de carte : tout tombe sur le pixel", "[ui][map_sign]") {
  // ⚠ UNE LETTRE SANS LISSAGE posée entre deux pixels serait floutée : aucune
  // résolution ne doit donner de place ni de taille fractionnaire.
  for (const ImVec2 screen : {ImVec2(800.0F, 600.0F), ImVec2(1366.0F, 768.0F),
                              ImVec2(1600.0F, 900.0F), ImVec2(2560.0F, 1440.0F)}) {
    const ro::ui::MapSignLayout layout = ro::ui::MapSignLayoutOf(screen, kImage);
    INFO(screen.x << " × " << screen.y);
    CHECK(Whole(layout.image_top_left.x));
    CHECK(Whole(layout.image_top_left.y));
    CHECK(Whole(layout.image_size.x));
    CHECK(Whole(layout.image_size.y));
    CHECK(Whole(layout.subtitle_size));
    CHECK(Whole(layout.subtitle_top));
    CHECK(Whole(layout.title_size));
    CHECK(Whole(layout.title_top));
  }
}

TEST_CASE("Bandeau de carte : cinq secondes plein, puis un quart de seconde d'effacement",
          "[ui][map_sign]") {
  CHECK(ro::ui::MapSignAlpha(0) == 255);
  CHECK(ro::ui::MapSignAlpha(ro::ui::kMapSignShownMs) == 255);
  // L'effacement est linéaire : une milliseconde, un cran.
  CHECK(ro::ui::MapSignAlpha(ro::ui::kMapSignShownMs + 1) == 254);
  CHECK(ro::ui::MapSignAlpha(ro::ui::kMapSignShownMs + 200) == 55);
  CHECK(ro::ui::MapSignAlpha(ro::ui::kMapSignShownMs + ro::ui::kMapSignFadeMs - 1) == 1);
  // Fini : plus rien à dessiner.
  CHECK_FALSE(ro::ui::MapSignAlpha(ro::ui::kMapSignShownMs + ro::ui::kMapSignFadeMs).has_value());
}
