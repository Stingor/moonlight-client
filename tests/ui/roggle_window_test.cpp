// La fenêtre de Roggle : elle épouse son plateau sans qu'un bouton dépasse, et
// ses boutons rendent leur intention.

#include "ro/ui/roggle_window.h"

#include <functional>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/ui/ui_context.h"

namespace {

using Catch::Approx;
using ro::ui::RoggleAsk;
using ro::ui::RoggleNotebookEntry;
using ro::ui::RoggleState;
using ro::ui::RoggleView;
using ro::ui::SkinArt;

constexpr ro::ui::Viewport kViewport{.width_px = 1920.0F, .height_px = 1080.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
// Une fenêtre auto-ajustée prend sa taille à la deuxième image.
constexpr int kSettleFrames = 3;
constexpr float kSweepStep = 6.0F;
// Les rangées du haut de la fenêtre, là où sont ses boutons.
constexpr float kHeaderSweepPx = 72.0F;
constexpr float kBoardWidth = 520.0F;
constexpr float kBoardHeight = 640.0F;
// Le pire en-tête : un grand score, un multiplicateur, un carnet à trois chiffres.
constexpr int kLargestScore = 9999999;
constexpr int kLargestMultiplier = 10;
constexpr std::size_t kFullNotebook = 123;
const SkinArt kNoArt;

struct Bench {
  ro::ui::UiContext ui;
  RoggleState state{.open = true};
  std::vector<RoggleNotebookEntry> notebook = std::vector<RoggleNotebookEntry>(kFullNotebook);
  RoggleView view;

  Bench() {
    view.board.width = kBoardWidth;
    view.board.height = kBoardHeight;
    view.score = kLargestScore;
    view.multiplier = kLargestMultiplier;
    view.layout_name = "Football Cake";
    view.notebook = notebook;
    view.paused = false;
  }

  RoggleAsk Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    const RoggleAsk asked = ro::ui::DrawRoggleWindow(state, view, kNoArt);
    ui.EndFrame();
    return asked;
  }

  void Settle() {
    for (int frame = 0; frame < kSettleFrames; ++frame) {
      static_cast<void>(Frame());
    }
  }
};

// Balaie la fenêtre de clics, jusqu'à ce que l'intention guettée soit rendue.
[[nodiscard]] bool SweepFor(Bench& bench, const char* window_key,
                            const std::function<bool(const RoggleAsk&)>& wanted) {
  const ImGuiWindow* window = ImGui::FindWindowByName(window_key);
  REQUIRE(window != nullptr);
  // ⚠ SOUS LA BARRE DE TITRE : sa croix fermerait la fenêtre en plein balayage.
  const ImVec2 low(window->Pos.x, window->ContentRegionRect.Min.y);
  const ImVec2 high(low.x + window->Size.x, low.y + kHeaderSweepPx);
  ImGuiIO& io = ImGui::GetIO();
  for (float y = low.y; y < high.y; y += kSweepStep) {
    for (float x = low.x; x < high.x; x += kSweepStep) {
      io.AddMousePosEvent(x, y);
      bool found = wanted(bench.Frame());
      io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
      found = wanted(bench.Frame()) || found;
      io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
      found = wanted(bench.Frame()) || found;
      if (found) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace

TEST_CASE("la fenêtre de Roggle épouse son plateau, sans qu'un bouton dépasse") {
  Bench bench;
  bench.Settle();
  const ImGuiWindow* window = ImGui::FindWindowByName("###roggle");
  REQUIRE(window != nullptr);
  CHECK(window->ContentSize.x == Approx(kBoardWidth));
  const float padding_x = ImGui::GetStyle().WindowPadding.x;
  CHECK(window->SizeFull.x == Approx(kBoardWidth + 2.0F * padding_x));
  // Le plateau finit dans la fenêtre : rien ne se coupe en bas non plus.
  CHECK(window->ContentSize.y >= kBoardHeight);
  CHECK(window->SizeFull.y >= window->ContentSize.y);
  CHECK_FALSE(window->ScrollbarY);
}

TEST_CASE("le bouton de la fenêtre de Roggle demande de rejouer le même dessin") {
  Bench bench;
  bench.Settle();
  CHECK(SweepFor(bench, "###roggle", [](const RoggleAsk& asked) { return asked.replay_game; }));
}

TEST_CASE("la case du carnet de Roggle change l'option") {
  Bench bench;
  bench.state.notebook_open = true;
  bench.Settle();
  CHECK(SweepFor(bench, "###roggle_notebook",
                 [](const RoggleAsk& asked) { return asked.skip_revealed.value_or(false); }));
}
