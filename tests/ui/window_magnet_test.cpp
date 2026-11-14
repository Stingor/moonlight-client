// L'aimant entre fenêtres : la géométrie pure d'abord, puis le geste réel d'une
// fenêtre qu'ImGui traîne, image par image.

#include "ro/ui/window_magnet.h"

#include <array>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/align_grid.h"
#include "ro/ui/ui_context.h"

using ro::ui::MagnetBox;
using ro::ui::MagnetizedWindow;
using ro::ui::WindowMagnetAsk;
using ro::ui::WindowMagnetCatch;
using ro::ui::WindowMagnetReachPixels;

namespace {

constexpr ImVec2 kScreen(1280.0F, 720.0F);
constexpr ImVec2 kSize(100.0F, 80.0F);
// La voisine, posée loin des bords de l'écran : 400..550 en largeur, 300..400
// en hauteur.
constexpr MagnetBox kNeighbour{.left = 400.0F, .top = 300.0F, .right = 550.0F, .bottom = 400.0F};
constexpr float kNear = 7.0F;
constexpr float kFar = 9.0F;
constexpr float kDoubleScale = 2.0F;

[[nodiscard]] WindowMagnetCatch Posed(ImVec2 at, float scale = 1.0F) {
  static const std::array<MagnetBox, 1> neighbours{kNeighbour};
  return MagnetizedWindow(WindowMagnetAsk{.at = at,
                                          .size = kSize,
                                          .neighbours = neighbours,
                                          .screen = kScreen,
                                          .reach_px = WindowMagnetReachPixels(scale)});
}

}  // namespace

TEST_CASE("Aimant : la portée suit l'échelle de l'interface", "[ui][aimant]") {
  CHECK(WindowMagnetReachPixels(1.0F) == ro::ui::kWindowMagnetReachPixels);
  CHECK(WindowMagnetReachPixels(kDoubleScale) == ro::ui::kWindowMagnetReachPixels * kDoubleScale);
}

TEST_CASE("Aimant : bord à bord dans les quatre sens", "[ui][aimant]") {
  SECTION("à droite de la voisine") {
    const WindowMagnetCatch caught = Posed(ImVec2(kNeighbour.right + kNear, 310.0F));
    CHECK(caught.at.x == kNeighbour.right);
    REQUIRE(caught.across.has_value());
    // ⚠ LE TRAIT COURT LE LONG DU CONTACT, pas sur toute la fenêtre.
    CHECK(caught.across->from.x == kNeighbour.right);
    CHECK(caught.across->from.y == 310.0F);
    CHECK(caught.across->to.y == 310.0F + kSize.y);
  }
  SECTION("à gauche de la voisine") {
    const WindowMagnetCatch caught = Posed(ImVec2(kNeighbour.left - kSize.x - kNear, 310.0F));
    CHECK(caught.at.x == kNeighbour.left - kSize.x);
    CHECK(caught.across.has_value());
  }
  SECTION("sous la voisine") {
    const WindowMagnetCatch caught = Posed(ImVec2(420.0F, kNeighbour.bottom + kNear));
    CHECK(caught.at.y == kNeighbour.bottom);
    REQUIRE(caught.down.has_value());
    CHECK(caught.down->from.y == kNeighbour.bottom);
  }
  SECTION("au-dessus de la voisine") {
    const WindowMagnetCatch caught = Posed(ImVec2(420.0F, kNeighbour.top - kSize.y - kNear));
    CHECK(caught.at.y == kNeighbour.top - kSize.y);
    CHECK(caught.down.has_value());
  }
}

TEST_CASE("Aimant : un bord ne colle qu'à une fenêtre qu'il longe", "[ui][aimant]") {
  // Le bord gauche juste au droit du bord droit de la voisine, mais la fenêtre
  // est bien plus bas : rien ne les rapproche.
  const WindowMagnetCatch caught = Posed(ImVec2(kNeighbour.right + kNear, 600.0F));
  CHECK(caught.at.x == kNeighbour.right + kNear);
  CHECK_FALSE(caught.across.has_value());

  SECTION("presque en face, à la portée près, il colle") {
    const WindowMagnetCatch close =
        Posed(ImVec2(kNeighbour.right + kNear, kNeighbour.bottom + kNear));
    CHECK(close.at.x == kNeighbour.right);
  }
}

TEST_CASE("Aimant : accolées, les bords de même côté s'alignent", "[ui][aimant]") {
  SECTION("côte à côte, les hauts ensemble") {
    const WindowMagnetCatch caught = Posed(ImVec2(kNeighbour.right + 2.0F, kNeighbour.top + 5.0F));
    CHECK(caught.at.x == kNeighbour.right);
    CHECK(caught.at.y == kNeighbour.top);
    REQUIRE(caught.down.has_value());
    // Le trait d'alignement couvre les deux fenêtres.
    CHECK(caught.down->from.x == kNeighbour.left);
    CHECK(caught.down->to.x == kNeighbour.right + kSize.x);
  }
  SECTION("l'une sur l'autre, les droites ensemble") {
    const WindowMagnetCatch caught =
        Posed(ImVec2(kNeighbour.right - kSize.x - 6.0F, kNeighbour.bottom + 1.0F));
    CHECK(caught.at.y == kNeighbour.bottom);
    CHECK(caught.at.x == kNeighbour.right - kSize.x);
  }
  SECTION("sans contact, pas d'alignement") {
    // Les hauts sont à cinq pixels, mais les fenêtres ne se touchent pas.
    const WindowMagnetCatch caught = Posed(ImVec2(kNeighbour.right + 40.0F, kNeighbour.top + 5.0F));
    CHECK(caught.at.y == kNeighbour.top + 5.0F);
    CHECK_FALSE(caught.down.has_value());
  }
}

TEST_CASE("Aimant : les bords de l'écran aimantent", "[ui][aimant]") {
  const WindowMagnetCatch left = Posed(ImVec2(kNear, 100.0F));
  CHECK(left.at.x == 0.0F);
  const WindowMagnetCatch bottom = Posed(ImVec2(800.0F, kScreen.y - kSize.y - kNear));
  CHECK(bottom.at.y == kScreen.y - kSize.y);
  REQUIRE(bottom.down.has_value());
  CHECK(bottom.down->from.y == kScreen.y);
}

TEST_CASE("Aimant : sept pixels collent, neuf non, à l'échelle", "[ui][aimant]") {
  CHECK(Posed(ImVec2(kNeighbour.right + kNear, 310.0F)).at.x == kNeighbour.right);
  CHECK(Posed(ImVec2(kNeighbour.right + kFar, 310.0F)).at.x == kNeighbour.right + kFar);
  // ⚠ À DEUX FOIS LA TAILLE, LA PORTÉE DOUBLE : quinze collent, dix-sept non.
  const float near_twice = kNear * kDoubleScale + 1.0F;
  const float far_twice = kFar * kDoubleScale - 1.0F;
  CHECK(Posed(ImVec2(kNeighbour.right + near_twice, 310.0F), kDoubleScale).at.x ==
        kNeighbour.right);
  CHECK(Posed(ImVec2(kNeighbour.right + far_twice, 310.0F), kDoubleScale).at.x ==
        kNeighbour.right + far_twice);
}

TEST_CASE("Aimant : un axe déjà tenu n'est pas touché", "[ui][aimant]") {
  const std::array<MagnetBox, 1> neighbours{kNeighbour};
  const WindowMagnetCatch caught =
      MagnetizedWindow(WindowMagnetAsk{.at = ImVec2(kNeighbour.right + kNear, 310.0F),
                                       .size = kSize,
                                       .neighbours = neighbours,
                                       .screen = kScreen,
                                       .held_across = true});
  CHECK(caught.at.x == kNeighbour.right + kNear);
  CHECK_FALSE(caught.across.has_value());
}

// ── LE GESTE ─────────────────────────────────────────────────────────────────
//
// 🔴 LA FENÊTRE EST TRAÎNÉE PAR IMGUI, image par image, et l'aimant passe en
// tête de chaque image comme dans le jeu : c'est ce qui prouve qu'il n'y a pas
// de saut au lâcher.

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = kScreen.x, .height_px = kScreen.y};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr int kSettleFrames = 4;
constexpr int kDragSteps = 4;
constexpr ImVec2 kStillAt(400.0F, 300.0F);
constexpr ImVec2 kStillSize(150.0F, 100.0F);
constexpr ImVec2 kMovedFrom(700.0F, 300.0F);
constexpr int kGridCell = 32;

struct DragProbe {
  ro::ui::UiContext ui;
  ro::ui::AlignGrid grid;
  ImGuiWindowFlags still_flags = ImGuiWindowFlags_NoSavedSettings;

  explicit DragProbe(float scale = 1.0F) {
    ui.SetInterfaceScale(scale);
    for (int frame = 0; frame < kSettleFrames; ++frame) {
      Frame();
    }
  }

  void Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    ro::ui::SnapMovingWindow(grid);
    ImGui::SetNextWindowPos(kStillAt, ImGuiCond_Always);
    ImGui::SetNextWindowSize(kStillSize, ImGuiCond_Always);
    ImGui::Begin("still", nullptr, still_flags);
    ImGui::End();
    ImGui::SetNextWindowPos(kMovedFrom, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(kSize, ImGuiCond_Always);
    ImGui::Begin("moved", nullptr, ImGuiWindowFlags_NoSavedSettings);
    ImGui::End();
    ui.EndFrame();
  }

  [[nodiscard]] static ImGuiWindow& Moved() {
    ImGuiWindow* window = ImGui::FindWindowByName("moved");
    REQUIRE(window != nullptr);
    return *window;
  }

  // Traîne la fenêtre pour que son coin haut-gauche arrive en `to`, Alt tenu ou
  // non, et rend sa place pendant qu'on la tient encore.
  ImVec2 DragTo(ImVec2 to, bool alt = false) {
    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 from = Moved().Pos;
    const ImVec2 grip(Moved().Size.x * 0.5F, Moved().Size.y * 0.5F);
    io.AddMousePosEvent(from.x + grip.x, from.y + grip.y);
    Frame();
    io.AddKeyEvent(ImGuiMod_Alt, alt);
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    Frame();
    for (int step = 1; step <= kDragSteps; ++step) {
      const float part = static_cast<float>(step) / static_cast<float>(kDragSteps);
      io.AddMousePosEvent(from.x + (to.x - from.x) * part + grip.x,
                          from.y + (to.y - from.y) * part + grip.y);
      Frame();
    }
    return Moved().Pos;
  }

  void Release() {
    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    io.AddKeyEvent(ImGuiMod_Alt, false);
    Frame();
    Frame();
  }
};

constexpr float kStillRight = kStillAt.x + kStillSize.x;

}  // namespace

TEST_CASE("Aimant : la fenêtre traînée colle, et y reste au lâcher", "[ui][aimant]") {
  DragProbe probe;
  const ImVec2 held = probe.DragTo(ImVec2(kStillRight + kNear, kStillAt.y + 3.0F));
  CHECK(held.x == kStillRight);
  CHECK(held.y == kStillAt.y);
  probe.Release();
  CHECK(DragProbe::Moved().Pos.x == kStillRight);
  CHECK(DragProbe::Moved().Pos.y == kStillAt.y);
}

TEST_CASE("Aimant : neuf pixels ne collent pas, à l'échelle deux, quinze oui", "[ui][aimant]") {
  SECTION("échelle 1") {
    DragProbe probe;
    CHECK(probe.DragTo(ImVec2(kStillRight + kFar, kStillAt.y + 30.0F)).x == kStillRight + kFar);
  }
  SECTION("échelle 2, quinze pixels") {
    DragProbe probe(kDoubleScale);
    const float near_twice = kNear * kDoubleScale + 1.0F;
    CHECK(probe.DragTo(ImVec2(kStillRight + near_twice, kStillAt.y + 30.0F)).x == kStillRight);
  }
  SECTION("échelle 2, dix-sept pixels") {
    DragProbe probe(kDoubleScale);
    const float far_twice = kFar * kDoubleScale - 1.0F;
    CHECK(probe.DragTo(ImVec2(kStillRight + far_twice, kStillAt.y + 30.0F)).x ==
          kStillRight + far_twice);
  }
}

TEST_CASE("Aimant : Alt tenu laisse la fenêtre libre", "[ui][aimant]") {
  DragProbe probe;
  const ImVec2 to(kStillRight + kNear, kStillAt.y + 3.0F);
  const ImVec2 held = probe.DragTo(to, true);
  CHECK(held.x == to.x);
  CHECK(held.y == to.y);
}

TEST_CASE("Aimant : une fenêtre verrouillée n'aimante rien", "[ui][aimant]") {
  DragProbe probe;
  probe.still_flags |= ImGuiWindowFlags_NoMove;
  const ImVec2 to(kStillRight + kNear, kStillAt.y + 3.0F);
  CHECK(probe.DragTo(to).x == to.x);
}

TEST_CASE("Aimant : décoché, le geste redevient celui d'avant", "[ui][aimant]") {
  DragProbe probe;
  probe.grid.windows_magnetic = false;
  const ImVec2 to(kStillRight + kNear, kStillAt.y + 3.0F);
  const ImVec2 held = probe.DragTo(to);
  CHECK(held.x == to.x);
  CHECK(held.y == to.y);

  SECTION("la grille seule aimante, comme avant") {
    probe.Release();
    probe.grid.magnetic = true;
    probe.grid.cell_pixels = kGridCell;
    const ImVec2 snapped = probe.DragTo(to);
    CHECK(snapped.x == probe.grid.Snap(snapped.x, kScreen.x));
  }
}

TEST_CASE("Aimant : il l'emporte sur la grille", "[ui][aimant]") {
  DragProbe probe;
  probe.grid.magnetic = true;
  probe.grid.cell_pixels = kGridCell;
  // Le bord droit de la voisine ne tombe sur aucune ligne de la grille.
  REQUIRE(probe.grid.Snap(kStillRight, kScreen.x) != kStillRight);
  const ImVec2 held = probe.DragTo(ImVec2(kStillRight + kNear, kStillAt.y + 30.0F));
  CHECK(held.x == kStillRight);
  // L'axe que l'aimant entre fenêtres laisse libre reste à la grille.
  CHECK(held.y == probe.grid.Snap(held.y, kScreen.y));
}
