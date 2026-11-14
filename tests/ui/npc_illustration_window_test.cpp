// OÙ SE POSE L'ILLUSTRATION D'UN PNJ : le placement, sans fenêtre.

#include "ro/ui/npc_illustration_window.h"

#include <catch2/catch_test_macros.hpp>

using ro::ui::IllustrationAnchor;
using ro::ui::IllustrationTopLeft;
using ro::ui::kIllustrationSideCenterPixels;

namespace {

constexpr ImVec2 kScreenOrigin(0.0F, 0.0F);
constexpr ImVec2 kScreen(1760.0F, 990.0F);
constexpr ImVec2 kImage(300.0F, 400.0F);

}  // namespace

TEST_CASE("Illustration : les trois places du bas, le bord bas sur celui de l'écran",
          "[ui][illustration]") {
  const ImVec2 left =
      IllustrationTopLeft(IllustrationAnchor::kBottomLeft, kImage, kScreenOrigin, kScreen, 1.0F);
  CHECK(left.x == kIllustrationSideCenterPixels - kImage.x / 2.0F);
  CHECK(left.y == kScreen.y - kImage.y);

  const ImVec2 middle =
      IllustrationTopLeft(IllustrationAnchor::kBottomMiddle, kImage, kScreenOrigin, kScreen, 1.0F);
  CHECK(middle.x == (kScreen.x - kImage.x) / 2.0F);
  CHECK(middle.y == kScreen.y - kImage.y);

  const ImVec2 right =
      IllustrationTopLeft(IllustrationAnchor::kBottomRight, kImage, kScreenOrigin, kScreen, 1.0F);
  CHECK(right.x == kScreen.x - kIllustrationSideCenterPixels - kImage.x / 2.0F);
  CHECK(right.y == kScreen.y - kImage.y);
}

TEST_CASE("Illustration : les deux places du milieu, centrées", "[ui][illustration]") {
  for (const IllustrationAnchor anchor :
       {IllustrationAnchor::kCenteredWindow, IllustrationAnchor::kCenteredFloating}) {
    const ImVec2 corner = IllustrationTopLeft(anchor, kImage, kScreenOrigin, kScreen, 1.0F);
    CHECK(corner.x == (kScreen.x - kImage.x) / 2.0F);
    CHECK(corner.y == (kScreen.y - kImage.y) / 2.0F);
  }
}

TEST_CASE("Illustration : l'écart du bord suit l'échelle de l'interface, et l'origine de l'écran",
          "[ui][illustration]") {
  constexpr float kScale = 1.5F;
  constexpr ImVec2 kOrigin(10.0F, 20.0F);
  const ImVec2 left =
      IllustrationTopLeft(IllustrationAnchor::kBottomLeft, kImage, kOrigin, kScreen, kScale);
  CHECK(left.x == kOrigin.x + kIllustrationSideCenterPixels * kScale - kImage.x / 2.0F);
  CHECK(left.y == kOrigin.y + kScreen.y - kImage.y);
}
