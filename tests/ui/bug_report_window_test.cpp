// LA BOÎTE DU RAPPORT DE BUG : elle s'ouvre sur demande, ne part pas vide, et
// l'accusé se montre puis s'éteint.

#include "ro/ui/bug_report_window.h"

#include <cstddef>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/skin_pieces.h"
#include "ro/ui/ui_context.h"

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr float kPlainPiecePixels = 8.0F;

[[nodiscard]] ro::ui::SkinArt PlainArt() {
  ro::ui::SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<ro::render::SkinPiece>(rank),
            ro::ui::SkinImage{.id = 1, .width = kPlainPiecePixels, .height = kPlainPiecePixels});
  }
  return art;
}

}  // namespace

TEST_CASE("Rapport de bug : la boîte s'ouvre sur demande et ne part pas vide", "[ui][bugreport]") {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art = PlainArt();
  ro::ui::BugReportWindowState state;
  state.context_label = "Item : Knife (#1201)";
  state.asked = true;
  for (int rank = 0; rank < 3; ++rank) {
    ui.BeginFrame(kViewport, kFrameSeconds);
    const ro::ui::BugReportAction action = ro::ui::DrawBugReportWindow(state, art);
    ui.EndFrame();
    CHECK_FALSE(action.send);
  }
  CHECK(state.open);
  CHECK_FALSE(state.asked);
}

TEST_CASE("Rapport de bug : l'accusé se montre, puis s'éteint", "[ui][bugreport]") {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art = PlainArt();
  ro::ui::BugReportWindowState state;
  state.ack_status = ro::ui::kBugReportSaved;
  state.ack_fresh = true;
  ui.BeginFrame(kViewport, kFrameSeconds);
  static_cast<void>(ro::ui::DrawBugReportWindow(state, art));
  ui.EndFrame();
  CHECK_FALSE(state.ack_fresh);
  CHECK(state.ack_status == ro::ui::kBugReportSaved);
  // Six secondes plus tard, il est parti.
  constexpr float kLater = 6.0F;
  ui.BeginFrame(kViewport, kLater);
  static_cast<void>(ro::ui::DrawBugReportWindow(state, art));
  ui.EndFrame();
  CHECK(state.ack_status < 0);
}
