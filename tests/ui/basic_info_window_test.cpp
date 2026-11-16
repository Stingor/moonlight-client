// LA FENÊTRE « BASIC INFO » : ses deux tailles, ce qui la replie, le coin où la
// grille s'accroche, et les règles de ses jauges et de ses textes.
//
// 🔴 CE SONT LES RÈGLES DU JEU, chiffre pour chiffre : un pour cent arrondi au
// lieu de tronqué, une jauge d'expérience restée au niveau maximum ou un poids
// rouge avant que le serveur ait dit son seuil se verraient à peine à l'œil, et
// un nostalgique les verrait tout de suite.

#include "ro/ui/basic_info_window.h"

#include <cmath>
#include <cstddef>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/skin_pieces.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/stat_bars.h"
#include "ro/ui/ui_context.h"

using ro::render::SkinPiece;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr int kSettleFrames = 4;
constexpr const char* kWindowName = "###basic_info";
constexpr float kLargeScale = 1.5F;
constexpr float kPlainPiecePixels = 8.0F;
// Un point de la barre de titre loin des boutons, en pixels d'art.
constexpr float kTitleBarX = 110.0F;

[[nodiscard]] ro::ui::SkinArt BasicInfoArt() {
  ro::ui::SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<SkinPiece>(rank),
            ro::ui::SkinImage{.id = 1, .width = kPlainPiecePixels, .height = kPlainPiecePixels});
  }
  return art;
}

struct BasicInfoProbe {
  ro::ui::UiContext ui;
  ro::ui::SkinArt art = BasicInfoArt();
  ro::ui::BasicInfoWindowState state;
  ro::ui::BasicInfoView view;

  BasicInfoProbe() {
    state.shown = true;
    view.name = "Sting";
    view.job = "High Wizard";
    view.base_level = 99;
    view.job_level = 50;
    view.hp = 900;
    view.max_hp = 1000;
    view.sp = 100;
    view.max_sp = 1000;
  }

  ro::ui::BasicInfoAnswer Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    const ro::ui::BasicInfoAnswer answer = ro::ui::DrawBasicInfoWindow(state, view, art);
    ui.EndFrame();
    return answer;
  }

  void Settle() {
    for (int frame = 0; frame < kSettleFrames; ++frame) {
      static_cast<void>(Frame());
    }
  }

  [[nodiscard]] static const ImGuiWindow& Window() {
    const ImGuiWindow* window = ImGui::FindWindowByName(kWindowName);
    REQUIRE(window != nullptr);
    return *window;
  }

  // Un point de la barre de titre réelle, à `x` pixels d'art du bord gauche.
  [[nodiscard]] static ImVec2 OnBar(float x) {
    const ImGuiWindow& window = Window();
    return {window.Pos.x + x * ImGui::GetStyle().FontScaleMain,
            window.Pos.y + window.TitleBarHeight / 2.0F};
  }

  // LE BOUTON DE RÉDUCTION DE LA BARRE COMMUNE : sans croix ni épingle, c'est
  // le premier posé depuis le bord droit.
  [[nodiscard]] static ImVec2 CollapseButton() {
    const ImGuiWindow& window = Window();
    const float scale = ImGui::GetStyle().FontScaleMain;
    return {window.Pos.x + window.Size.x -
                (ro::ui::kSystemEdgePixels + kPlainPiecePixels / 2.0F) * scale,
            window.Pos.y + window.TitleBarHeight / 2.0F};
  }

  // La hauteur attendue : la barre réelle, puis le corps du jeu sous elle.
  [[nodiscard]] static float HeightFor(float game_pixels) {
    return Window().TitleBarHeight + std::ceil((game_pixels - ro::ui::kBasicInfoTitleBarPixels) *
                                               ImGui::GetStyle().FontScaleMain);
  }

  void Click(ImVec2 at) {
    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent(at.x, at.y);
    static_cast<void>(Frame());
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    static_cast<void>(Frame());
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    static_cast<void>(Frame());
  }
};

}  // namespace

TEST_CASE("Basic Info : dépliée, la fenêtre fait 220 sur 146, une ligne de plus que le jeu",
          "[ui][basic_info]") {
  for (const float scale : {1.0F, kLargeScale}) {
    BasicInfoProbe probe;
    probe.ui.SetInterfaceScale(scale);
    probe.Settle();
    const ImGuiWindow& window = BasicInfoProbe::Window();
    CHECK(window.Size.x == ro::ui::kBasicInfoWidthPixels * scale);
    CHECK(window.Size.y == BasicInfoProbe::HeightFor(ro::ui::kBasicInfoExpandedPixels));
  }
}

TEST_CASE("Basic Info : repliée, elle fait 220 sur 55", "[ui][basic_info]") {
  BasicInfoProbe probe;
  probe.state.expanded = false;
  probe.Settle();
  CHECK(BasicInfoProbe::Window().Size.y ==
        BasicInfoProbe::HeightFor(ro::ui::kBasicInfoCollapsedPixels));
}

TEST_CASE("Basic Info : la grille s'accroche sous son bord bas-gauche", "[ui][basic_info]") {
  BasicInfoProbe probe;
  probe.Settle();
  const ro::ui::BasicInfoAnswer answer = probe.Frame();
  REQUIRE(answer.dock.has_value());
  const ImGuiWindow& window = BasicInfoProbe::Window();
  CHECK(answer.dock->x == window.Pos.x);
  CHECK(answer.dock->y == window.Pos.y + window.Size.y);
}

TEST_CASE("Basic Info : elle se traîne par ImGui, donc par l'aimant", "[ui][basic_info]") {
  BasicInfoProbe probe;
  probe.Settle();
  CHECK((BasicInfoProbe::Window().Flags & ImGuiWindowFlags_NoMove) == 0);
}

TEST_CASE("Basic Info : éteinte, elle ne dessine rien et n'accroche rien", "[ui][basic_info]") {
  BasicInfoProbe probe;
  probe.state.shown = false;
  CHECK_FALSE(probe.Frame().dock.has_value());
  CHECK(ImGui::FindWindowByName(kWindowName) == nullptr);
}

TEST_CASE("Basic Info : son bouton la replie, puis la déplie", "[ui][basic_info]") {
  BasicInfoProbe probe;
  probe.Settle();
  probe.Click(BasicInfoProbe::CollapseButton());
  CHECK_FALSE(probe.state.expanded);
  // ⚠ JAMAIS RÉDUITE PAR IMGUI : repliée, elle garde ses deux lignes.
  probe.Settle();
  CHECK_FALSE(BasicInfoProbe::Window().Collapsed);
  CHECK_FALSE(probe.state.expanded);
  probe.Click(BasicInfoProbe::CollapseButton());
  CHECK(probe.state.expanded);
}

TEST_CASE("Basic Info : un double-clic sur la barre de titre la replie", "[ui][basic_info]") {
  BasicInfoProbe probe;
  probe.Settle();
  probe.Click(BasicInfoProbe::OnBar(kTitleBarX));
  REQUIRE(probe.state.expanded);
  probe.Click(BasicInfoProbe::OnBar(kTitleBarX));
  CHECK_FALSE(probe.state.expanded);
  CHECK_FALSE(BasicInfoProbe::Window().Collapsed);
}

TEST_CASE("Basic Info : le pour cent d'EXP se tronque au dixième", "[ui][basic_info]") {
  // 99,99 % : arrondi, il afficherait 100,0.
  CHECK(ro::ui::BasicInfoExpPercent(9'999, 10'000) == 99.9);
  CHECK(ro::ui::BasicInfoExpPercent(0, 10'000) == 0.0);
  // Un acquis négatif compte pour zéro ; un palier nul pour l'acquis plus un.
  CHECK(ro::ui::BasicInfoExpPercent(-5, 10'000) == 0.0);
  CHECK(ro::ui::BasicInfoExpPercent(0, 0) == 0.0);
}

TEST_CASE("Basic Info : au niveau maximum, l'EXP vaut 100 et sa jauge s'efface",
          "[ui][basic_info]") {
  CHECK(ro::ui::BasicInfoExpPercent(12, ro::ui::kBaseExpCeiling) == 100.0);
  CHECK(ro::ui::BasicInfoExpPercent(12, ro::ui::kJobExpCeiling) == 100.0);
  CHECK_FALSE(ro::ui::BasicInfoExpBarShown(12, ro::ui::kBaseExpCeiling));
  CHECK_FALSE(ro::ui::BasicInfoExpBarShown(12, ro::ui::kJobExpCeiling));
  // Un palier sous l'acquis ne se dessine pas non plus.
  CHECK_FALSE(ro::ui::BasicInfoExpBarShown(500, 400));
  CHECK(ro::ui::BasicInfoExpBarShown(400, 500));
}

TEST_CASE("Basic Info : la vie passe au rouge sous le quart, pas au quart", "[ui][basic_info]") {
  CHECK(ro::ui::BasicInfoLow(249, 1000));
  CHECK_FALSE(ro::ui::BasicInfoLow(250, 1000));
  // Un maximum inconnu ne colore rien.
  CHECK_FALSE(ro::ui::BasicInfoLow(0, 0));
}

TEST_CASE("Basic Info : le remplissage se tronque au pixel", "[ui][basic_info]") {
  // 127 pixels entre les deux bouts de la jauge.
  constexpr float kSpan = 127.0F;
  CHECK(ro::ui::BasicInfoFill(1, 2, kSpan) == 63.0F);
  CHECK(ro::ui::BasicInfoFill(2, 2, kSpan) == kSpan);
  CHECK(ro::ui::BasicInfoFill(0, 2, kSpan) == 0.0F);
  CHECK(ro::ui::BasicInfoFill(5, 0, kSpan) == 0.0F);
}

TEST_CASE("Basic Info : le poids rougit au seuil du serveur, une fois dit", "[ui][basic_info]") {
  CHECK(ro::ui::BasicInfoOverweight(500, 1000, 50));
  CHECK_FALSE(ro::ui::BasicInfoOverweight(499, 1000, 50));
  // ⚠ SANS SEUIL, JAMAIS : le jeu, lui, le montrait rouge dès la connexion.
  CHECK_FALSE(ro::ui::BasicInfoOverweight(999, 1000, 0));
}

TEST_CASE("Basic Info : le zeny se groupe par milliers, à la virgule", "[ui][basic_info]") {
  CHECK(ro::ui::BasicInfoGrouped(0) == "0");
  CHECK(ro::ui::BasicInfoGrouped(999) == "999");
  CHECK(ro::ui::BasicInfoGrouped(1'000) == "1,000");
  CHECK(ro::ui::BasicInfoGrouped(2'147'483'647) == "2,147,483,647");
  CHECK(ro::ui::BasicInfoGrouped(-1'234) == "-1,234");
}
