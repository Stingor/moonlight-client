// LA MINIMAP CONTRE LES BORDS DE L'ÉCRAN : posée contre un bord, elle y reste
// quand le nom de la carte change, quand l'écran change, et quand le joueur en
// tire la poignée.
//
// 🔴 TOUT PASSE PAR LE GESTE DU JOUEUR, jamais par les réglages écrits à la
// main : c'est le glisser qui doit retenir le bon bord.

#include <cstddef>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/skin_pieces.h"
#include "ro/ui/minimap_window.h"
#include "ro/ui/ui_context.h"

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr ro::ui::Viewport kWideScreen{.width_px = 1920.0F, .height_px = 1080.0F};
// Assez bas pour que la part d'écran rabote le côté par la HAUTEUR.
constexpr ro::ui::Viewport kSmallScreen{.width_px = 640.0F, .height_px = 480.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr float kPlainPiecePixels = 8.0F;
constexpr int kSettleFrames = 4;
constexpr int kGestureSteps = 4;
constexpr int kMapCells = 100;
constexpr float kPlayerCell = 50.0F;
constexpr float kSidePixels = 220.0F;
constexpr int kHugeSide = 5000;
// Le point saisi pour glisser la carte, en part du carré : loin du bouton de
// réglages, en haut à gauche, et des poignées, en bas.
constexpr float kGrabShare = 0.5F;
// Vers le coin bas-droit, bien au-delà de l'écran.
constexpr ImVec2 kTowardBottomRight{3000.0F, 3000.0F};
// Le point saisi dans la poignée bas-droite, en retrait du coin, et le chemin
// que le joueur lui fait faire, au-delà du bord droit.
constexpr float kGripInsetPixels = 4.0F;
constexpr ImVec2 kGripPull{120.0F, 60.0F};

// Un nom qui se replie sur plusieurs lignes au côté de l'essai, et un autre
// qui tient sur une seule.
constexpr std::string_view kLongName =
    "Le très long nom d'une carte qui ne tient jamais sur une seule ligne de la minimap";
constexpr std::string_view kShortName = "Prontera";

[[nodiscard]] ro::ui::SkinArt PlainArt() {
  ro::ui::SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<ro::render::SkinPiece>(rank),
            ro::ui::SkinImage{.id = 1, .width = kPlainPiecePixels, .height = kPlainPiecePixels});
  }
  return art;
}

struct EdgeProbe {
  ro::ui::UiContext ui;
  ro::ui::SkinArt art = PlainArt();
  ro::ui::MinimapSettings settings;
  ro::ui::MinimapWindowState state;
  ro::ui::MinimapView view{.map_name = "prontera",
                           .display_name = kLongName,
                           .cells_width = kMapCells,
                           .cells_height = kMapCells,
                           .player_cell_x = kPlayerCell,
                           .player_cell_y = kPlayerCell};
  ro::ui::Viewport viewport = kViewport;

  EdgeProbe() { settings.side_px = static_cast<int>(kSidePixels); }

  void Frame() {
    ui.BeginFrame(viewport, kFrameSeconds);
    static_cast<void>(ro::ui::DrawMinimapWindow(settings, state, view, art));
    ui.EndFrame();
  }

  void Settle() {
    for (int frame = 0; frame < kSettleFrames; ++frame) {
      Frame();
    }
  }

  [[nodiscard]] const ImGuiWindow& Window() const {
    const ImGuiWindow* window = ImGui::FindWindowByName("###minimap");
    REQUIRE(window != nullptr);
    return *window;
  }

  // Le centre du carré de carte : on y saisit la fenêtre pour la glisser.
  [[nodiscard]] ImVec2 MapPoint() const {
    const ImVec2 origin = Window().DC.CursorStartPos;
    return {origin.x + kSidePixels * kGrabShare, origin.y + kSidePixels * kGrabShare};
  }

  [[nodiscard]] float RightGap() const {
    return viewport.width_px - (Window().Pos.x + Window().Size.x);
  }
  [[nodiscard]] float BottomGap() const {
    return viewport.height_px - (Window().Pos.y + Window().Size.y);
  }
};

// Bouton gauche pressé en `from`, mené jusqu'à `from + by`, relâché. `during`
// est appelé après chaque image du geste.
template <typename During>
void Drag(EdgeProbe& probe, ImVec2 from, ImVec2 by, During during) {
  ImGuiIO& io = ImGui::GetIO();
  io.AddMousePosEvent(from.x, from.y);
  probe.Frame();
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  probe.Frame();
  for (int step = 1; step <= kGestureSteps; ++step) {
    const float done = static_cast<float>(step) / static_cast<float>(kGestureSteps);
    io.AddMousePosEvent(from.x + by.x * done, from.y + by.y * done);
    probe.Frame();
    during();
  }
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  probe.Settle();
}

void Drag(EdgeProbe& probe, ImVec2 from, ImVec2 by) {
  Drag(probe, from, by, [] {});
}

// La minimap glissée par le joueur jusque dans le coin bas-droit.
void PushToBottomRight(EdgeProbe& probe) {
  probe.Settle();
  Drag(probe, probe.MapPoint(), kTowardBottomRight);
  REQUIRE(probe.RightGap() == Catch::Approx(0.0F));
  REQUIRE(probe.BottomGap() == Catch::Approx(0.0F));
}

}  // namespace

TEST_CASE("Minimap : le nom de la carte ne fait pas varier sa largeur", "[ui][minimap][bords]") {
  EdgeProbe probe;
  probe.viewport = kSmallScreen;
  probe.settings.side_px = kHugeSide;
  probe.view.display_name = kShortName;
  probe.Settle();
  const float width_short = probe.Window().Size.x;
  probe.view.display_name = kLongName;
  probe.Settle();
  CHECK(probe.Window().Size.x == width_short);
}

TEST_CASE("Minimap : posée dans le coin bas-droit, elle y reste quand le nom change",
          "[ui][minimap][bords]") {
  EdgeProbe probe;
  PushToBottomRight(probe);
  const float tall = probe.Window().Size.y;

  probe.view.display_name = kShortName;
  probe.Settle();
  // Le nom court la fait rapetisser : c'est le cas qui la décollait du bas.
  REQUIRE(probe.Window().Size.y < tall);
  CHECK(probe.RightGap() == Catch::Approx(0.0F));
  CHECK(probe.BottomGap() == Catch::Approx(0.0F));

  probe.view.display_name = kLongName;
  probe.Settle();
  CHECK(probe.RightGap() == Catch::Approx(0.0F));
  CHECK(probe.BottomGap() == Catch::Approx(0.0F));
}

TEST_CASE("Minimap : posée dans le coin bas-droit, elle le suit quand l'écran change",
          "[ui][minimap][bords]") {
  EdgeProbe probe;
  PushToBottomRight(probe);

  probe.viewport = kWideScreen;
  probe.Settle();
  CHECK(probe.RightGap() == Catch::Approx(0.0F));
  CHECK(probe.BottomGap() == Catch::Approx(0.0F));

  probe.viewport = kViewport;
  probe.Settle();
  CHECK(probe.RightGap() == Catch::Approx(0.0F));
  CHECK(probe.BottomGap() == Catch::Approx(0.0F));
}

TEST_CASE("Minimap : la poignée tirée au-delà du bord ne la fait pas sauter",
          "[ui][minimap][bords]") {
  EdgeProbe probe;
  PushToBottomRight(probe);
  // Remontée d'un cran pour que la poignée ait de quoi grandir vers le bas.
  Drag(probe, probe.MapPoint(), ImVec2(0.0F, -kGripPull.y * 2.0F));
  const ImVec2 left_top = probe.Window().Pos;
  const ImGuiWindow& window = probe.Window();
  const ImVec2 grip(window.Pos.x + window.Size.x - kGripInsetPixels,
                    window.Pos.y + window.Size.y - kGripInsetPixels);

  // À CHAQUE IMAGE DU GESTE : le coin haut-gauche ne bouge pas, et le bord
  // droit ne passe pas l'écran.
  Drag(probe, grip, kGripPull, [&] {
    CHECK(probe.Window().Pos.x == Catch::Approx(left_top.x));
    CHECK(probe.Window().Pos.y == Catch::Approx(left_top.y));
    CHECK(probe.RightGap() >= 0.0F);
  });
  CHECK(probe.Window().Pos.x == Catch::Approx(left_top.x));
  CHECK(probe.Window().Pos.y == Catch::Approx(left_top.y));
  CHECK(probe.RightGap() == Catch::Approx(0.0F));
}
