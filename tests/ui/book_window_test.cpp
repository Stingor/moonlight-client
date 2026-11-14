// LA FENÊTRE D'UN LIVRE : le repli garde la couleur d'une ligne à l'autre, et la
// page se borne.

#include "ro/ui/book_window.h"

#include <array>
#include <cstddef>
#include <string>
#include <vector>

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

TEST_CASE("Livre : une couleur ouverte suit sur la ligne repliée", "[ui][livres]") {
  ro::ui::UiContext ui;
  ui.BeginFrame(kViewport, kFrameSeconds);
  const std::array<std::string, 1> lines = {"^880000rouge rouge rouge rouge rouge rouge"};
  // Assez étroit pour couper la ligne.
  constexpr float kNarrowPixels = 80.0F;
  const std::vector<std::string> rows = ro::ui::WrapBookLines(lines, kNarrowPixels);
  ui.EndFrame();
  REQUIRE(rows.size() > 1);
  CHECK(rows[1].starts_with("^880000"));
  CHECK(ro::ui::BookLinePlain(rows[0]).find('^') == std::string::npos);
}

TEST_CASE("Livre : la page se borne au nombre de pages", "[ui][livres]") {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art = PlainArt();
  std::vector<std::string> lines(25, "ligne");
  ro::ui::BookWindowState state;
  state.open = true;
  state.item_id = 11000;
  state.page = 9;
  const ro::ui::BookView view{.item_id = 11000, .title = "Livre", .lines = lines};
  for (int rank = 0; rank < 2; ++rank) {
    ui.BeginFrame(kViewport, kFrameSeconds);
    static_cast<void>(ro::ui::DrawBookWindow(state, view, art));
    ui.EndFrame();
  }
  // Vingt-cinq lignes font deux pages.
  CHECK(state.page == 2);
}
