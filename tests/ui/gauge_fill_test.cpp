// LE REMPLISSAGE D'UNE JAUGE TIENT DANS SON CADRE, ET EN PREND L'ARRONDI.
//
// 🔴 CE QUE LE JOUEUR A VU : un remplissage sans arrondi, qui sortait de la
// bordure de sa jauge. Une part minuscule redevenait un carré dans les coins
// arrondis, et la part pleine couvrait le trait du cadre. Ce qui se vérifie ici
// est la GÉOMÉTRIE du remplissage — ses sommets —, à 0 %, à 100 %, à une part
// minuscule et au milieu, dans les quatre sens.

#include "ro/ui/gauge_fill.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "support/lone_draw_list.h"

using ro::test::LoneDrawList;
using ro::ui::AxisBox;
using ro::ui::DrawnRounding;
using ro::ui::GaugeFillBox;
using ro::ui::GaugeGrowth;
using ro::ui::GaugeInterior;
using ro::ui::PaintGaugeFill;
using ro::ui::RoundedBox;
using ro::ui::RoundedBoxInsetAt;

namespace {

// Ce qu'un cosinus coûte en flottants simples : un vingtième de pixel.
constexpr float kSlack = 0.05F;

constexpr ImU32 kInk = IM_COL32(10, 230, 10, 255);

// Un cadre de deux cents pixels sur vingt-quatre, arrondi à huit, bordé d'un.
constexpr float kFrameRounding = 8.0F;
constexpr float kBorder = 1.0F;
const ImVec2 kFrameMin(10.0F, 20.0F);
const ImVec2 kFrameMax(210.0F, 44.0F);

[[nodiscard]] RoundedBox Interior() {
  return GaugeInterior(kFrameMin, kFrameMax, kFrameRounding, kBorder);
}

// VRAI quand `point` est dans la forme arrondie, à `kSlack` près : ramené sur
// le rectangle des centres de coins, il en est à au plus un rayon.
[[nodiscard]] bool InsideRounded(const RoundedBox& shape, ImVec2 point) {
  const float radius = shape.rounding;
  const float nearest_x = std::clamp(point.x, shape.min.x + radius, shape.max.x - radius);
  const float nearest_y = std::clamp(point.y, shape.min.y + radius, shape.max.y - radius);
  const float dx = point.x - nearest_x;
  const float dy = point.y - nearest_y;
  return std::sqrt(dx * dx + dy * dy) <= radius + kSlack;
}

// Tous les sommets peints sont dans l'intérieur et dans la boîte remplie.
// Rend leur nombre : un essai qui n'a rien vu ne prouve rien.
[[nodiscard]] int CheckInside(const LoneDrawList& sheet, const RoundedBox& interior,
                              const AxisBox& box) {
  int seen = 0;
  for (const ImDrawVert& vertex : sheet.into.VtxBuffer) {
    ++seen;
    CHECK(InsideRounded(interior, vertex.pos));
    CHECK(vertex.pos.x >= box.min.x - kSlack);
    CHECK(vertex.pos.x <= box.max.x + kSlack);
    CHECK(vertex.pos.y >= box.min.y - kSlack);
    CHECK(vertex.pos.y <= box.max.y + kSlack);
  }
  return seen;
}

}  // namespace

TEST_CASE("Jauge : le rayon du cadre est celui qu'ImGui peint", "[ui][jauge]") {
  // Borné à la moitié du plus petit côté moins un pixel.
  CHECK(DrawnRounding(ImVec2(0.0F, 0.0F), ImVec2(60.0F, 5.0F), 8.0F) == Catch::Approx(1.5F));
  CHECK(DrawnRounding(ImVec2(0.0F, 0.0F), ImVec2(200.0F, 24.0F), 8.0F) == Catch::Approx(8.0F));
  // Sous un demi-pixel, ImGui peint droit.
  CHECK(DrawnRounding(ImVec2(0.0F, 0.0F), ImVec2(200.0F, 24.0F), 0.4F) == 0.0F);
  CHECK(DrawnRounding(ImVec2(0.0F, 0.0F), ImVec2(200.0F, 2.0F), 8.0F) == 0.0F);
}

TEST_CASE("Jauge : l'intérieur déduit la bordure, et son rayon aussi", "[ui][jauge]") {
  const RoundedBox interior = Interior();
  CHECK(interior.min.x == Catch::Approx(kFrameMin.x + kBorder));
  CHECK(interior.min.y == Catch::Approx(kFrameMin.y + kBorder));
  CHECK(interior.max.x == Catch::Approx(kFrameMax.x - kBorder));
  CHECK(interior.max.y == Catch::Approx(kFrameMax.y - kBorder));
  CHECK(interior.rounding == Catch::Approx(kFrameRounding - kBorder));

  // ⚠ LE RAYON PART DE CELUI QU'IMGUI DONNE AU CADRE, pas de celui demandé : la
  // barre de cinq pixels du client n'a qu'un pixel et demi d'arrondi.
  const RoundedBox thin = GaugeInterior(ImVec2(0.0F, 0.0F), ImVec2(60.0F, 5.0F), 8.0F, kBorder);
  CHECK(thin.rounding == Catch::Approx(0.5F));
  // Jamais négatif, et sans bordure, l'intérieur est le cadre.
  CHECK(GaugeInterior(kFrameMin, kFrameMax, 0.5F, 2.0F).rounding == 0.0F);
  const RoundedBox bare = GaugeInterior(kFrameMin, kFrameMax, kFrameRounding, 0.0F);
  CHECK(bare.min.x == kFrameMin.x);
  CHECK(bare.rounding == Catch::Approx(kFrameRounding));
  // Un cadre plus étroit que ses deux bordures n'a pas d'intérieur.
  const RoundedBox crushed = GaugeInterior(ImVec2(0.0F, 0.0F), ImVec2(1.0F, 1.0F), 0.0F, kBorder);
  CHECK(crushed.max.x == crushed.min.x);
  CHECK(crushed.max.y == crushed.min.y);
}

TEST_CASE("Jauge : la boîte remplie part du bon bord", "[ui][jauge]") {
  const RoundedBox interior = Interior();
  const float width = interior.max.x - interior.min.x;
  const float height = interior.max.y - interior.min.y;

  const AxisBox right = GaugeFillBox(interior, 0.25F, GaugeGrowth::kRightward);
  CHECK(right.min.x == interior.min.x);
  CHECK(right.max.x == Catch::Approx(interior.min.x + width * 0.25F));
  const AxisBox left = GaugeFillBox(interior, 0.25F, GaugeGrowth::kLeftward);
  CHECK(left.max.x == interior.max.x);
  CHECK(left.min.x == Catch::Approx(interior.max.x - width * 0.25F));
  const AxisBox up = GaugeFillBox(interior, 0.25F, GaugeGrowth::kUpward);
  CHECK(up.max.y == interior.max.y);
  CHECK(up.min.y == Catch::Approx(interior.max.y - height * 0.25F));
  const AxisBox down = GaugeFillBox(interior, 0.25F, GaugeGrowth::kDownward);
  CHECK(down.min.y == interior.min.y);
  CHECK(down.max.y == Catch::Approx(interior.min.y + height * 0.25F));

  // Bornée, et le NaN ne peint rien.
  CHECK(GaugeFillBox(interior, 3.0F, GaugeGrowth::kRightward).max.x == interior.max.x);
  CHECK(GaugeFillBox(interior, -1.0F, GaugeGrowth::kRightward).max.x == interior.min.x);
  CHECK(GaugeFillBox(interior, std::numeric_limits<float>::quiet_NaN(), GaugeGrowth::kRightward)
            .max.x == interior.min.x);
}

TEST_CASE("Jauge : à zéro, rien n'est peint", "[ui][jauge]") {
  LoneDrawList sheet;
  PaintGaugeFill(sheet.into, Interior(), 0.0F, GaugeGrowth::kRightward, kInk);
  CHECK(sheet.into.VtxBuffer.Size == 0);
  CHECK(sheet.into._Path.Size == 0);
}

TEST_CASE("Jauge : pleine, elle est l'intérieur arrondi", "[ui][jauge]") {
  const RoundedBox interior = Interior();
  LoneDrawList sheet;
  PaintGaugeFill(sheet.into, interior, 1.0F, GaugeGrowth::kRightward, kInk);
  CHECK(CheckInside(sheet, interior, AxisBox{interior.min, interior.max}) > 0);

  // 🔴 L'ARRONDI EST APPLIQUÉ : aucun sommet n'atteint le coin de l'intérieur.
  // Tout point de l'arc en est au moins à `r·(√2 − 1)` — un carré y aurait zéro.
  float nearest = std::numeric_limits<float>::max();
  for (const ImDrawVert& vertex : sheet.into.VtxBuffer) {
    const float dx = vertex.pos.x - interior.min.x;
    const float dy = vertex.pos.y - interior.min.y;
    nearest = std::min(nearest, std::sqrt(dx * dx + dy * dy));
  }
  CHECK(nearest >= interior.rounding * (std::sqrt(2.0F) - 1.0F) - kSlack);
}

TEST_CASE("Jauge : une part minuscule suit la courbe du coin", "[ui][jauge]") {
  // 🔴 LE DÉFAUT VU : le rayon d'un rectangle se borne à sa largeur, et un
  // remplissage d'un demi-pixel redevenait un carré de toute la hauteur, posé
  // dans les coins arrondis du cadre.
  constexpr float kTiny = 0.002F;
  const RoundedBox interior = Interior();
  const AxisBox box = GaugeFillBox(interior, kTiny, GaugeGrowth::kRightward);
  REQUIRE(box.max.x - box.min.x < 1.0F);

  LoneDrawList sheet;
  PaintGaugeFill(sheet.into, interior, kTiny, GaugeGrowth::kRightward, kInk);
  CHECK(CheckInside(sheet, interior, box) > 0);

  // Il ne monte pas jusqu'aux bords haut et bas : c'est la courbe qui le borne.
  float highest = std::numeric_limits<float>::max();
  float lowest = std::numeric_limits<float>::lowest();
  for (const ImDrawVert& vertex : sheet.into.VtxBuffer) {
    highest = std::min(highest, vertex.pos.y);
    lowest = std::max(lowest, vertex.pos.y);
  }
  CHECK(highest > interior.min.y + 1.0F);
  CHECK(lowest < interior.max.y - 1.0F);
}

TEST_CASE("Jauge : au milieu, le front reste franc", "[ui][jauge]") {
  const RoundedBox interior = Interior();
  const AxisBox box = GaugeFillBox(interior, 0.5F, GaugeGrowth::kRightward);
  LoneDrawList sheet;
  PaintGaugeFill(sheet.into, interior, 0.5F, GaugeGrowth::kRightward, kInk);
  CHECK(CheckInside(sheet, interior, box) > 0);

  bool front_top = false;
  bool front_bottom = false;
  for (const ImDrawVert& vertex : sheet.into.VtxBuffer) {
    if (std::abs(vertex.pos.x - box.max.x) > kSlack) {
      continue;
    }
    front_top = front_top || std::abs(vertex.pos.y - interior.min.y) <= kSlack;
    front_bottom = front_bottom || std::abs(vertex.pos.y - interior.max.y) <= kSlack;
  }
  CHECK(front_top);
  CHECK(front_bottom);
}

TEST_CASE("Jauge : presque pleine, le front suit la courbe opposée", "[ui][jauge]") {
  constexpr float kNearlyFull = 0.995F;
  const RoundedBox interior = Interior();
  const AxisBox box = GaugeFillBox(interior, kNearlyFull, GaugeGrowth::kRightward);
  REQUIRE(interior.max.x - box.max.x < interior.rounding);
  LoneDrawList sheet;
  PaintGaugeFill(sheet.into, interior, kNearlyFull, GaugeGrowth::kRightward, kInk);
  CHECK(CheckInside(sheet, interior, box) > 0);
}

TEST_CASE("Jauge : les quatre sens restent dans l'intérieur", "[ui][jauge]") {
  // ⚠ UNE JAUGE VERTICALE, dont la courbe des coins se lit en hauteur.
  const RoundedBox interior = GaugeInterior(ImVec2(0.0F, 0.0F), ImVec2(24.0F, 120.0F), 10.0F, 2.0F);
  for (const GaugeGrowth growth : {GaugeGrowth::kRightward, GaugeGrowth::kLeftward,
                                   GaugeGrowth::kUpward, GaugeGrowth::kDownward}) {
    for (const float fill : {0.001F, 0.03F, 0.5F, 0.98F, 1.0F}) {
      LoneDrawList sheet;
      PaintGaugeFill(sheet.into, interior, fill, growth, kInk);
      CHECK(CheckInside(sheet, interior, GaugeFillBox(interior, fill, growth)) > 0);
    }
  }
}

TEST_CASE("Jauge : le retrait d'un bord suit l'arc", "[ui][jauge]") {
  const RoundedBox interior = Interior();
  // Au milieu de la hauteur, le bord est droit.
  CHECK(RoundedBoxInsetAt(interior, (interior.min.y + interior.max.y) * 0.5F) == 0.0F);
  // Tout en haut, la courbe mange tout le rayon.
  CHECK(RoundedBoxInsetAt(interior, interior.min.y) == Catch::Approx(interior.rounding));
  // À mi-rayon : r − √(r² − (r/2)²).
  const float radius = interior.rounding;
  const float expected = radius - std::sqrt(radius * radius - radius * radius * 0.25F);
  CHECK(RoundedBoxInsetAt(interior, interior.min.y + radius * 0.5F) ==
        Catch::Approx(expected).margin(kSlack));
  // Sans arrondi, jamais de retrait.
  CHECK(RoundedBoxInsetAt(GaugeInterior(kFrameMin, kFrameMax, 0.0F, 0.0F), kFrameMin.y) == 0.0F);
}
