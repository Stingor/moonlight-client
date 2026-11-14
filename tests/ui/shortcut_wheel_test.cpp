// La géométrie de la roue d'utilisation : quelle case la souris vise, où chaque
// case se pose, et quelle barre la roue montre.
//
// ⚠ C'EST CE CALCUL QUI DÉCIDE DE LA CASE QUI PART AU RELÂCHER. Faux d'un
// demi-secteur, il lance la case voisine de celle qui s'éclaire — et cela ne se
// voit qu'en jeu, au mauvais moment.

#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <optional>

#include <imgui.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "ro/ui/shortcut_bar.h"
#include "ro/ui/ui_context.h"

using Catch::Matchers::WithinAbs;
using ro::ui::kNoWheelBar;
using ro::ui::kShortcutBarCount;
using ro::ui::kShortcutSlotsPerBar;
using ro::ui::kShortcutSlotTotal;
using ro::ui::kWheelDeadZoneRadiusPx;
using ro::ui::ShortcutBarsLayout;
using ro::ui::ShortcutWheelView;
using ro::ui::WheelBarOf;
using ro::ui::WheelRingRadius;
using ro::ui::WheelSectorAt;
using ro::ui::WheelSlotCenter;

namespace {

// Assez loin du centre pour sortir de la zone morte.
constexpr float kFar = 100.0F;
constexpr float kTolerancePx = 0.001F;
constexpr float kSectorTurn = 2.0F * std::numbers::pi_v<float> / kShortcutSlotsPerBar;

// Le décalage qui vise l'angle `turn` depuis le haut, dans le sens des aiguilles.
[[nodiscard]] ImVec2 Toward(float turn) {
  return ImVec2(kFar * std::sin(turn), -kFar * std::cos(turn));
}

}  // namespace

TEST_CASE("Roue : la case 1 est en haut, la 4 à droite, la 7 en bas, la 10 à gauche",
          "[ui][roue]") {
  CHECK(WheelSectorAt(0.0F, -kFar, kWheelDeadZoneRadiusPx) == 0U);
  CHECK(WheelSectorAt(kFar, 0.0F, kWheelDeadZoneRadiusPx) == 3U);
  CHECK(WheelSectorAt(0.0F, kFar, kWheelDeadZoneRadiusPx) == 6U);
  CHECK(WheelSectorAt(-kFar, 0.0F, kWheelDeadZoneRadiusPx) == 9U);
}

TEST_CASE("Roue : chaque secteur est centré sur sa case", "[ui][roue]") {
  for (std::size_t rank = 0; rank < kShortcutSlotsPerBar; ++rank) {
    const float middle = static_cast<float>(rank) * kSectorTurn;
    // ⚠ UN PEU AVANT ET UN PEU APRÈS LE MILIEU : la case déborde des deux côtés.
    const ImVec2 before = Toward(middle - kSectorTurn * 0.45F);
    const ImVec2 after = Toward(middle + kSectorTurn * 0.45F);
    CHECK(WheelSectorAt(before.x, before.y, kWheelDeadZoneRadiusPx) == rank);
    CHECK(WheelSectorAt(after.x, after.y, kWheelDeadZoneRadiusPx) == rank);
  }
}

TEST_CASE("Roue : juste à gauche du haut, c'est encore la case 1", "[ui][roue]") {
  // Le secteur de la case 1 enjambe le haut : sans le demi-secteur de décalage,
  // ce point tomberait sur la case 12.
  const ImVec2 left_of_top = Toward(-kSectorTurn * 0.25F);
  CHECK(WheelSectorAt(left_of_top.x, left_of_top.y, kWheelDeadZoneRadiusPx) == 0U);
  const ImVec2 into_twelve = Toward(-kSectorTurn * 0.75F);
  CHECK(WheelSectorAt(into_twelve.x, into_twelve.y, kWheelDeadZoneRadiusPx) == 11U);
}

TEST_CASE("Roue : la zone morte ne vise rien", "[ui][roue]") {
  CHECK_FALSE(WheelSectorAt(0.0F, 0.0F, kWheelDeadZoneRadiusPx).has_value());
  CHECK_FALSE(WheelSectorAt(kWheelDeadZoneRadiusPx * 0.5F, 0.0F, kWheelDeadZoneRadiusPx));
  // Le bord lui-même vise déjà.
  CHECK(WheelSectorAt(kWheelDeadZoneRadiusPx, 0.0F, kWheelDeadZoneRadiusPx) == 3U);
}

TEST_CASE("Roue : un décalage qui n'est pas un nombre ne vise rien", "[ui][roue]") {
  CHECK_FALSE(WheelSectorAt(std::nanf(""), 0.0F, kWheelDeadZoneRadiusPx).has_value());
}

TEST_CASE("Roue : les cases se posent sur le cercle, en horloge", "[ui][roue]") {
  const ImVec2 center(400.0F, 300.0F);
  constexpr float kRing = 80.0F;
  const ImVec2 top = WheelSlotCenter(center, 0, kRing);
  CHECK_THAT(top.x, WithinAbs(400.0F, kTolerancePx));
  CHECK_THAT(top.y, WithinAbs(220.0F, kTolerancePx));
  const ImVec2 right = WheelSlotCenter(center, 3, kRing);
  CHECK_THAT(right.x, WithinAbs(480.0F, kTolerancePx));
  CHECK_THAT(right.y, WithinAbs(300.0F, kTolerancePx));
  // 🔴 LA CASE POSÉE EST CELLE QUE LA SOURIS VISE : le dessin et la décision
  // ne peuvent pas diverger.
  for (std::size_t rank = 0; rank < kShortcutSlotsPerBar; ++rank) {
    const ImVec2 at = WheelSlotCenter(center, rank, kRing);
    CHECK(WheelSectorAt(at.x - center.x, at.y - center.y, kWheelDeadZoneRadiusPx) == rank);
  }
}

TEST_CASE("Roue : le cercle grandit avec les cases, sans descendre sous son minimum",
          "[ui][roue]") {
  CHECK(WheelRingRadius(1.0F) == ro::ui::kWheelSmallestRingRadiusPx);
  constexpr float kBigSide = 64.0F;
  CHECK(WheelRingRadius(kBigSide) == kBigSide * ro::ui::kWheelRingRadiusPerSlotSide);
  // Douze grandes cases ne se chevauchent pas : la corde entre deux centres
  // voisins dépasse le côté.
  const float ring = WheelRingRadius(kBigSide);
  const float chord = 2.0F * ring * std::sin(kSectorTurn * 0.5F);
  CHECK(chord > kBigSide);
}

TEST_CASE("Roue : aucune barre par défaut, et un hors-bornes vaut aucune", "[ui][roue]") {
  ShortcutBarsLayout layout;
  CHECK(layout.wheel_bar == kNoWheelBar);
  CHECK_FALSE(WheelBarOf(layout).has_value());
  layout.wheel_bar = 1;
  CHECK(WheelBarOf(layout) == 0U);
  layout.wheel_bar = static_cast<int>(kShortcutBarCount);
  CHECK(WheelBarOf(layout) == kShortcutBarCount - 1);
  layout.wheel_bar = static_cast<int>(kShortcutBarCount) + 1;
  CHECK_FALSE(WheelBarOf(layout).has_value());
  layout.wheel_bar = -3;
  CHECK_FALSE(WheelBarOf(layout).has_value());
}

TEST_CASE("Roue : le dessin rend la case visée, et rien fermée", "[ui][roue]") {
  ro::ui::UiContext ui;
  const ShortcutBarsLayout layout;
  std::array<ro::ui::ShortcutSlotView, kShortcutSlotTotal> slots{};
  // Une case pleine et des cases vides : les deux dessins passent.
  slots[ro::ui::ShortcutSlotIndex(2, 3)].skill_id = 28;
  slots[ro::ui::ShortcutSlotIndex(2, 3)].known = true;
  constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
  constexpr float kFrameSeconds = 1.0F / 60.0F;

  ui.BeginFrame(kViewport, kFrameSeconds);
  const ShortcutWheelView closed;
  CHECK_FALSE(ro::ui::DrawShortcutWheel(closed, layout, slots).has_value());
  const ShortcutWheelView aiming_right{.open = true,
                                       .bar = 2,
                                       .center = ImVec2(640.0F, 360.0F),
                                       .pointer = ImVec2(640.0F + kFar, 360.0F)};
  CHECK(ro::ui::DrawShortcutWheel(aiming_right, layout, slots) == 3U);
  const ShortcutWheelView centered{
      .open = true, .bar = 2, .center = ImVec2(640.0F, 360.0F), .pointer = ImVec2(640.0F, 360.0F)};
  CHECK_FALSE(ro::ui::DrawShortcutWheel(centered, layout, slots).has_value());
  ui.EndFrame();
}
