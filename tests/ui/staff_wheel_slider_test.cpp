// LE CURSEUR DU STAFF QUI PREND LA MOLETTE : une salve de crans ou de clics sur
// une flèche part en un seul envoi, une fois le geste fini.

#include <optional>

#include <imgui.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/skin_art.h"
#include "ro/ui/staff_slider.h"
#include "ro/ui/ui_context.h"

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 800.0F, .height_px = 600.0F};
constexpr float kFrameSeconds = 1.0F / 30.0F;
// Plus long que le délai de pose du portail de la molette, et que le repos du geste.
constexpr int kSettleFrames = 20;
constexpr int kLowest = 20;
constexpr int kHighest = 1000;
constexpr int kShown = 150;
constexpr float kBarWidthPixels = 300.0F;
constexpr float kBarInsetPixels = 40.0F;
constexpr float kBarInsetYPixels = 6.0F;
constexpr float kNotchUp = 1.0F;
constexpr float kNotchDown = -1.0F;
constexpr int kShiftedStep = 10;
constexpr int kBurstNotches = 3;
constexpr int kFramesBetweenNotches = 2;

struct Bench {
  ro::ui::UiContext ui;
  ro::ui::StaffWheelDraft draft;
  ImVec2 bar_at{};
  int shown = kShown;
  int sent_count = 0;
  std::optional<int> last_sent;

  void Frame() {
    const ro::ui::SkinArt art;
    ui.BeginFrame(kViewport, kFrameSeconds);
    ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
    ImGui::Begin("banc", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::SetNextItemWidth(kBarWidthPixels);
    const std::optional<int> sent =
        ro::ui::StaffWheelSlider("##vitesse", shown, kLowest, kHighest, draft, art);
    bar_at = ImGui::GetItemRectMin();
    ImGui::End();
    ui.EndFrame();
    if (sent.has_value()) {
      ++sent_count;
      last_sent = sent;
      shown = *sent;
    }
  }

  void Frames(int count) {
    for (int frame = 0; frame < count; ++frame) {
      Frame();
    }
  }

  // Pose le pointeur sur la barre et attend que le portail de la molette s'ouvre.
  void HoverBar() {
    Frames(kFramesBetweenNotches);
    ImGui::GetIO().AddMousePosEvent(bar_at.x + kBarInsetPixels, bar_at.y + kBarInsetYPixels);
    Frames(kSettleFrames);
  }
};

}  // namespace

TEST_CASE("Curseur à molette : un cran part une fois, et pas avant le repos", "[ui][staff_wheel]") {
  Bench bench;
  bench.HoverBar();
  ImGui::GetIO().AddMouseWheelEvent(0.0F, kNotchDown);
  bench.Frame();
  CHECK(bench.sent_count == 0);
  CHECK(bench.draft.value == kShown - 1);
  bench.Frames(kSettleFrames);
  CHECK(bench.sent_count == 1);
  CHECK(bench.last_sent == kShown - 1);
  CHECK_FALSE(bench.draft.value.has_value());
}

TEST_CASE("Curseur à molette : une salve se résume en un seul envoi", "[ui][staff_wheel]") {
  Bench bench;
  bench.HoverBar();
  for (int notch = 0; notch < kBurstNotches; ++notch) {
    ImGui::GetIO().AddMouseWheelEvent(0.0F, kNotchUp);
    bench.Frames(kFramesBetweenNotches);
  }
  CHECK(bench.sent_count == 0);
  bench.Frames(kSettleFrames);
  CHECK(bench.sent_count == 1);
  CHECK(bench.last_sent == kShown + kBurstNotches);
}

TEST_CASE("Curseur à molette : Majuscule avance d'un grand pas, et part aussi",
          "[ui][staff_wheel]") {
  Bench bench;
  bench.HoverBar();
  ImGuiIO& io = ImGui::GetIO();
  io.AddKeyEvent(ImGuiMod_Shift, true);
  bench.Frame();
  io.AddMouseWheelEvent(0.0F, kNotchDown);
  bench.Frame();
  io.AddKeyEvent(ImGuiMod_Shift, false);
  bench.Frames(kSettleFrames);
  CHECK(bench.sent_count == 1);
  CHECK(bench.last_sent == kShown - kShiftedStep);
}

TEST_CASE("Curseur à molette : revenir à la valeur montrée n'envoie rien", "[ui][staff_wheel]") {
  Bench bench;
  bench.HoverBar();
  ImGuiIO& io = ImGui::GetIO();
  io.AddMouseWheelEvent(0.0F, kNotchUp);
  bench.Frames(kFramesBetweenNotches);
  io.AddMouseWheelEvent(0.0F, kNotchDown);
  bench.Frames(kFramesBetweenNotches);
  bench.Frames(kSettleFrames);
  CHECK(bench.sent_count == 0);
  CHECK_FALSE(bench.draft.value.has_value());
}

TEST_CASE("Curseur à molette : un geste à la souris part au relâchement", "[ui][staff_wheel]") {
  Bench bench;
  bench.HoverBar();
  ImGuiIO& io = ImGui::GetIO();
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  bench.Frames(kFramesBetweenNotches);
  CHECK(bench.sent_count == 0);
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  bench.Frames(kFramesBetweenNotches);
  CHECK(bench.sent_count == 1);
  REQUIRE(bench.last_sent.has_value());
  CHECK(*bench.last_sent != kShown);
}
