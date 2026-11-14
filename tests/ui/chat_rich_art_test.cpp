// LES IMAGES DU CHAT : l'image d'une animation à un instant donné, et la
// visionneuse qui les montre en grand.

#include "ro/ui/chat_rich_art.h"

#include <array>
#include <cstdint>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "ro/ui/chat_picture_viewer.h"
#include "ro/ui/escape_stack.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/ui_context.h"
#include "ro/ui/window_bounds.h"

using ro::ui::ChatPictureFrame;
using ro::ui::ChatPictureFrameAt;

TEST_CASE("Animation : les images se suivent à leur délai, en boucle", "[ui][chat][image]") {
  constexpr std::array<ChatPictureFrame, 3> kFrames = {
      ChatPictureFrame{.texture = 1, .delay_ms = 100},
      ChatPictureFrame{.texture = 2, .delay_ms = 200},
      ChatPictureFrame{.texture = 3, .delay_ms = 300}};
  CHECK(ChatPictureFrameAt(kFrames, 0) == 0);
  CHECK(ChatPictureFrameAt(kFrames, 99) == 0);
  CHECK(ChatPictureFrameAt(kFrames, 100) == 1);
  CHECK(ChatPictureFrameAt(kFrames, 299) == 1);
  CHECK(ChatPictureFrameAt(kFrames, 300) == 2);
  CHECK(ChatPictureFrameAt(kFrames, 599) == 2);
  // EN BOUCLE : une vignette jouée une fois s'arrêterait avant qu'on la voie.
  CHECK(ChatPictureFrameAt(kFrames, 600) == 0);
  CHECK(ChatPictureFrameAt(kFrames, 6 * 600 + 150) == 1);
}

TEST_CASE("Animation : sans image ou d'une seule, c'est la première", "[ui][chat][image]") {
  CHECK(ChatPictureFrameAt({}, 12345) == 0);
  constexpr std::array<ChatPictureFrame, 1> kStill = {ChatPictureFrame{.texture = 1}};
  CHECK(ChatPictureFrameAt(kStill, 12345) == 0);
}

TEST_CASE("Animation : un délai nul compte pour 100 ms", "[ui][chat][image]") {
  // 🔴 SANS CETTE RÈGLE, UNE ANIMATION DE DURÉE NULLE n'a pas d'image courante —
  // le reste d'une division par zéro.
  constexpr std::array<ChatPictureFrame, 2> kZero = {ChatPictureFrame{.texture = 1},
                                                     ChatPictureFrame{.texture = 2}};
  CHECK(ChatPictureFrameAt(kZero, 50) == 0);
  CHECK(ChatPictureFrameAt(kZero, 150) == 1);
  CHECK(ChatPictureFrameAt(kZero, 200) == 0);
}

TEST_CASE("Animation : 10 ms compte pour 100, 20 ms est tenu", "[ui][chat][image]") {
  constexpr std::array<ChatPictureFrame, 2> kTen = {ChatPictureFrame{.texture = 1, .delay_ms = 10},
                                                    ChatPictureFrame{.texture = 2, .delay_ms = 50}};
  CHECK(ChatPictureFrameAt(kTen, 99) == 0);
  CHECK(ChatPictureFrameAt(kTen, 100) == 1);
  CHECK(ChatPictureFrameAt(kTen, 149) == 1);
  CHECK(ChatPictureFrameAt(kTen, 150) == 0);
  constexpr std::array<ChatPictureFrame, 2> kTwenty = {
      ChatPictureFrame{.texture = 1, .delay_ms = 20},
      ChatPictureFrame{.texture = 2, .delay_ms = 20}};
  CHECK(ChatPictureFrameAt(kTwenty, 19) == 0);
  CHECK(ChatPictureFrameAt(kTwenty, 20) == 1);
}

// ── La visionneuse ───────────────────────────────────────────────────────────

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr int kSettleFrames = 3;
// Une image plus petite que la toile : ajustée, elle est à sa taille réelle.
constexpr float kPictureWidth = 400.0F;
constexpr float kPictureHeight = 300.0F;
// Une texture quelconque : rien n'est rendu, seul l'identifiant voyage.
constexpr ImTextureID kTexture = 7;
constexpr float kZoomStep = 1.25F;

struct ViewerBench {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  ro::ui::ChatPictureViewerState state;
  ro::ui::ChatPictureViewerView view{.state = ro::ui::ChatPictureState::kReady,
                                     .texture = kTexture,
                                     .width_px = kPictureWidth,
                                     .height_px = kPictureHeight};
  ro::ui::EscapeStack escape;

  ViewerBench() {
    state.open = true;
    state.raise = true;
    state.url = "https://moonlight-destiny.fr/images/relay/a.png";
  }

  void Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    ro::ui::DrawChatPictureViewer(state, view, art);
    // ⚠ COMME L'ÉTAGE D'INTERFACE : la visionneuse se déclare à la pile d'Échap
    // par son identité publique.
    escape.Register(ro::ui::kChatPictureViewerId, &state.open);
    escape.Process();
    ui.EndFrame();
  }

  void Settle() {
    for (int frame = 0; frame < kSettleFrames; ++frame) {
      Frame();
    }
  }

  [[nodiscard]] static const ImGuiWindow* Window() {
    return ImGui::FindWindowByName(ro::ui::kChatPictureViewerId);
  }
};

}  // namespace

TEST_CASE("Visionneuse : elle s'ouvre ajustée à 90 % de l'écran", "[ui][chat][image]") {
  ViewerBench bench;
  bench.Settle();
  const ImGuiWindow* window = ViewerBench::Window();
  REQUIRE(window != nullptr);
  CHECK_THAT(window->Size.x,
             Catch::Matchers::WithinAbs(kViewport.width_px * ro::ui::kMaxScreenFraction, 1.0));
  CHECK_THAT(window->Size.y,
             Catch::Matchers::WithinAbs(kViewport.height_px * ro::ui::kMaxScreenFraction, 1.0));
  // AJUSTÉE : aucun zoom figé tant qu'aucun geste n'a été fait.
  CHECK(bench.state.zoom == 0.0F);
}

TEST_CASE("Visionneuse : Échap la ferme, par la pile d'Échap", "[ui][chat][image]") {
  ViewerBench bench;
  bench.Settle();
  REQUIRE(bench.state.open);
  ImGuiIO& io = ImGui::GetIO();
  io.AddKeyEvent(ImGuiKey_Escape, true);
  bench.Frame();
  io.AddKeyEvent(ImGuiKey_Escape, false);
  bench.Frame();
  CHECK_FALSE(bench.state.open);
}

TEST_CASE("Visionneuse : la molette zoome d'un quart par cran, le double-clic bascule",
          "[ui][chat][image]") {
  ViewerBench bench;
  bench.Settle();
  const ImGuiWindow* window = ViewerBench::Window();
  REQUIRE(window != nullptr);
  ImGuiIO& io = ImGui::GetIO();
  // Au centre de la fenêtre : sur la toile, loin du pied.
  io.AddMousePosEvent(window->Pos.x + window->Size.x * 0.5F, window->Pos.y + window->Size.y * 0.5F);
  bench.Frame();

  io.AddMouseWheelEvent(0.0F, 1.0F);
  bench.Frame();
  // L'image tient en entier à sa taille réelle : ajustée, elle est à 1.
  CHECK_THAT(bench.state.zoom, Catch::Matchers::WithinAbs(kZoomStep, 1e-4));
  io.AddMouseWheelEvent(0.0F, -2.0F);
  bench.Frame();
  CHECK_THAT(bench.state.zoom, Catch::Matchers::WithinAbs(1.0F / kZoomStep, 1e-4));

  // Double-clic : la taille réelle, puis de nouveau ajustée.
  const auto click = [&bench, &io]() {
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    bench.Frame();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    bench.Frame();
  };
  click();
  click();
  CHECK(bench.state.zoom == 1.0F);
  // 🔴 UN SIMPLE CLIC NE FIGE RIEN : sans mouvement, ce n'est pas un glisser.
  // ⚠ ET LA PAIRE SUIVANTE ATTEND LA FIN DU DÉLAI DE DOUBLE-CLIC : sans quoi
  // ImGui la compterait en troisième et quatrième clics.
  const int beyond_double_click =
      static_cast<int>(io.MouseDoubleClickTime / kFrameSeconds) + kSettleFrames;
  for (int frame = 0; frame < beyond_double_click; ++frame) {
    bench.Frame();
  }
  click();
  click();
  CHECK(bench.state.zoom == 0.0F);
}

TEST_CASE("Visionneuse : le glisser déplace l'image", "[ui][chat][image]") {
  ViewerBench bench;
  bench.Settle();
  const ImGuiWindow* window = ViewerBench::Window();
  REQUIRE(window != nullptr);
  ImGuiIO& io = ImGui::GetIO();
  const ImVec2 middle(window->Pos.x + window->Size.x * 0.5F, window->Pos.y + window->Size.y * 0.5F);
  constexpr float kDragPixels = 40.0F;
  io.AddMousePosEvent(middle.x, middle.y);
  bench.Frame();
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  bench.Frame();
  io.AddMousePosEvent(middle.x + kDragPixels, middle.y);
  bench.Frame();
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  bench.Frame();
  // LE PREMIER GESTE FIGE LE ZOOM AJUSTÉ, et c'est de là qu'il part.
  CHECK(bench.state.zoom == 1.0F);
  CHECK_THAT(bench.state.pan.x, Catch::Matchers::WithinAbs(kDragPixels, 1e-3));
  CHECK(bench.state.pan.y == 0.0F);
}

TEST_CASE("Visionneuse : la toile survolée se réserve la molette, la caméra n'en reçoit rien",
          "[ui][chat][image]") {
  ViewerBench bench;
  bench.Settle();
  const ImGuiWindow* window = ViewerBench::Window();
  REQUIRE(window != nullptr);
  ImGui::GetIO().AddMousePosEvent(window->Pos.x + window->Size.x * 0.5F,
                                  window->Pos.y + window->Size.y * 0.5F);
  bench.Frame();
  bench.Frame();
  // C'est ce que lit la caméra avant d'accepter Ctrl+molette au-dessus d'une
  // fenêtre.
  CHECK(bench.ui.gate().wheel_to_ui);
}
