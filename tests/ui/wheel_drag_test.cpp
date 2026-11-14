// LA MOLETTE SUR UN CHAMP À GLISSER : un pas par cran, dix avec Majuscule,
// sur la seule composante survolée, et rien au passage.

#include "ro/ui/wheel_drag.h"

#include <array>

#include <imgui.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/ui_context.h"

using ro::ui::WheelDrag;
using ro::ui::WheelDragResult;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
// Plus que le délai de pose : trente images.
constexpr int kSettleFrames = 30;
// Un point sur la première composante, sous la barre de titre de la fenêtre.
constexpr float kOnFirstX = 40.0F;
constexpr float kOnFirstY = 36.0F;
constexpr WheelDrag kDrag{
    .drag_step = 0.1F, .wheel_step = 1.0F, .lowest = -50.0F, .highest = 50.0F, .format = "%.1f"};

template <typename Draw>
void Frame(ro::ui::UiContext& ui, Draw&& draw) {
  ui.BeginFrame(kViewport, kFrameSeconds);
  draw();
  ui.EndFrame();
}

template <typename Draw>
void Frames(ro::ui::UiContext& ui, int count, Draw&& draw) {
  for (int frame = 0; frame < count; ++frame) {
    Frame(ui, draw);
  }
}

}  // namespace

TEST_CASE("Champ à glisser : la molette avance d'un pas, de dix avec Majuscule",
          "[ui][widgets][wheel]") {
  ro::ui::UiContext ui;
  std::array<float, 3> values{0.0F, 5.0F, 7.0F};
  WheelDragResult last;
  ImGuiIO& io = ImGui::GetIO();
  const auto draw = [&values, &last]() {
    ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
    ImGui::SetNextWindowSize(ImVec2(kViewport.width_px, kViewport.height_px));
    ImGui::Begin("essai");
    last = ro::ui::WheelDragFloat3("Position", values, kDrag);
    ImGui::End();
  };

  Frame(ui, draw);
  io.AddMousePosEvent(kOnFirstX, kOnFirstY);
  Frames(ui, kSettleFrames, draw);

  // Un cran vers le haut : un pas, et un geste entier.
  io.AddMouseWheelEvent(0.0F, 1.0F);
  Frame(ui, draw);
  CHECK(values[0] == 1.0F);
  CHECK(last.changed);
  CHECK(last.released);
  // Les autres composantes ne bougent pas.
  CHECK(values[1] == 5.0F);
  CHECK(values[2] == 7.0F);

  // Majuscule : dix pas.
  io.AddKeyEvent(ImGuiMod_Shift, true);
  io.AddMouseWheelEvent(0.0F, -1.0F);
  Frame(ui, draw);
  CHECK(values[0] == -9.0F);
  io.AddKeyEvent(ImGuiMod_Shift, false);
  Frame(ui, draw);

  // Bornée : la molette s'arrête à la borne.
  for (int notch = 0; notch < 10; ++notch) {
    io.AddKeyEvent(ImGuiMod_Shift, true);
    io.AddMouseWheelEvent(0.0F, -1.0F);
    Frame(ui, draw);
  }
  io.AddKeyEvent(ImGuiMod_Shift, false);
  Frame(ui, draw);
  CHECK(values[0] == kDrag.lowest);
}

TEST_CASE("Champ à glisser : un cran à peine arrivé dessus ne règle rien", "[ui][widgets][wheel]") {
  ro::ui::UiContext ui;
  float value = 3.0F;
  ImGuiIO& io = ImGui::GetIO();
  const auto draw = [&value]() {
    ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
    ImGui::SetNextWindowSize(ImVec2(kViewport.width_px, kViewport.height_px));
    ImGui::Begin("essai");
    static_cast<void>(ro::ui::WheelDragFloat("Portée", value, kDrag));
    ImGui::End();
  };
  Frame(ui, draw);
  io.AddMousePosEvent(kOnFirstX, kOnFirstY);
  io.AddMouseWheelEvent(0.0F, 1.0F);
  Frame(ui, draw);
  CHECK(value == 3.0F);
}
