// La minimap des Survivants : l'échelle et le bord du carré, et un dessin qui
// ne paraît que demandé.

#include "ro/ui/survivors_minimap.h"

#include <array>

#include <imgui.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/ui/survivors_hud.h"
#include "ro/ui/ui_context.h"

namespace {

using Catch::Approx;
using ro::ui::kSurvivorsMinimapRadiusCells;
using ro::ui::PlaceOnSurvivorsMinimap;
using ro::ui::SurvivorsMinimapDot;
using ro::ui::SurvivorsMinimapMark;

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
const ImVec2 kCenter(100.0F, 100.0F);
constexpr float kHalf = 80.0F;

[[nodiscard]] int VerticesOf(const ro::ui::SurvivorsHudView& view) {
  ro::ui::UiContext ui;
  ui.BeginFrame(kViewport, kFrameSeconds);
  const ImDrawList* list = ImGui::GetBackgroundDrawList();
  const int before = list->VtxBuffer.Size;
  ro::ui::DrawSurvivorsHud(view);
  return list->VtxBuffer.Size - before;
}

}  // namespace

TEST_CASE("Minimap : l'est à droite, le nord en haut, à l'échelle du rayon",
          "[ui][survivors][minimap]") {
  const float half_radius = kSurvivorsMinimapRadiusCells * 0.5F;
  const auto east = PlaceOnSurvivorsMinimap(kCenter, kHalf, ImVec2(half_radius, 0.0F));
  CHECK(east.at.x == Approx(kCenter.x + kHalf * 0.5F));
  CHECK(east.at.y == Approx(kCenter.y));
  CHECK_FALSE(east.on_edge);
  const auto north = PlaceOnSurvivorsMinimap(kCenter, kHalf, ImVec2(0.0F, half_radius));
  CHECK(north.at.x == Approx(kCenter.x));
  CHECK(north.at.y == Approx(kCenter.y - kHalf * 0.5F));
}

TEST_CASE("Minimap : au-delà du rayon, la cible se colle au bord sur sa droite",
          "[ui][survivors][minimap]") {
  // Deux fois le rayon à l'est, un rayon au sud : le bord est, à mi-chemin du
  // centre vers le bas.
  const auto far = PlaceOnSurvivorsMinimap(
      kCenter, kHalf, ImVec2(kSurvivorsMinimapRadiusCells * 2.0F, -kSurvivorsMinimapRadiusCells));
  CHECK(far.on_edge);
  CHECK(far.at.x == Approx(kCenter.x + kHalf));
  CHECK(far.at.y == Approx(kCenter.y + kHalf * 0.5F));
}

TEST_CASE("Minimap : elle ne se peint que montrée", "[ui][survivors][minimap]") {
  const std::array<SurvivorsMinimapDot, 3> dots{{
      {.offset_cells = ImVec2(4.0F, 2.0F), .mark = SurvivorsMinimapMark::kPlant},
      {.offset_cells = ImVec2(-90.0F, 9.0F), .mark = SurvivorsMinimapMark::kChest},
      {.offset_cells = ImVec2(12.0F, -30.0F), .mark = SurvivorsMinimapMark::kBoss},
  }};
  ro::ui::SurvivorsHudView view;
  view.minimap.dots = dots;
  const int hidden = VerticesOf(view);
  view.minimap.shown = true;
  view.minimap.screen_known = true;
  view.minimap.screen_corners_cells = {ImVec2(-15.0F, 10.0F), ImVec2(15.0F, 10.0F),
                                       ImVec2(12.0F, -8.0F), ImVec2(-12.0F, -8.0F)};
  CHECK(VerticesOf(view) > hidden);
}
