// LA FENÊTRE DE SIGNALEMENT, dessinée hors jeu.
//
// Ce qui se vérifie sans écran : fermée elle ne dessine rien, ouverte elle se
// dessine sans rien envoyer, et le compte des caractères suit l'UTF-8.

#include "ro/ui/player_report_window.h"

#include <cstddef>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/skin_pieces.h"
#include "ro/ui/ui_context.h"

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr float kPlainPiecePixels = 8.0F;
constexpr const char* kWindowName = "###signalement_joueur";

[[nodiscard]] ro::ui::SkinArt PlainArt() {
  ro::ui::SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<ro::render::SkinPiece>(rank),
            ro::ui::SkinImage{.id = 1, .width = kPlainPiecePixels, .height = kPlainPiecePixels});
  }
  return art;
}

struct ReportProbe {
  ro::ui::UiContext ui;
  ro::ui::SkinArt art = PlainArt();
  ro::ui::PlayerReportWindowState state;

  ReportProbe() {
    state.open = true;
    state.target_name = "Bob";
  }

  ro::ui::PlayerReportAnswer Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    const ro::ui::PlayerReportAnswer answer = ro::ui::DrawPlayerReportWindow(state, art);
    ui.EndFrame();
    return answer;
  }
};

}  // namespace

TEST_CASE("Fenêtre de signalement : fermée, rien ne se dessine", "[ui][player_report]") {
  ReportProbe probe;
  probe.state.open = false;
  for (int frame = 0; frame < 3; ++frame) {
    CHECK_FALSE(probe.Frame().send);
  }
  CHECK(ImGui::FindWindowByName(kWindowName) == nullptr);
}

TEST_CASE("Fenêtre de signalement : ouverte, elle se dessine sans rien envoyer",
          "[ui][player_report]") {
  ReportProbe probe;
  for (int frame = 0; frame < 3; ++frame) {
    CHECK_FALSE(probe.Frame().send);
  }
  CHECK(ImGui::FindWindowByName(kWindowName) != nullptr);
  CHECK(probe.state.open);
  CHECK_FALSE(probe.state.raise);
  // Un texte trop long se dessine aussi : seul l'envoi se grise.
  const std::string too_long(ro::ui::kPlayerReportMessageMostChars + 1, 'a');
  too_long.copy(probe.state.message.data(), too_long.size());
  CHECK_FALSE(probe.Frame().send);
}

TEST_CASE("Fenêtre de signalement : un caractère accentué compte pour un", "[ui][player_report]") {
  CHECK(ro::ui::Utf8CharCount("") == 0);
  CHECK(ro::ui::Utf8CharCount("abc") == 3);
  CHECK(ro::ui::Utf8CharCount("été") == 3);
  CHECK(ro::ui::Utf8CharCount("€") == 1);
  // Le tampon tient cent caractères de quatre octets, et le zéro final.
  CHECK(ro::ui::kPlayerReportMessageBufferBytes >
        ro::ui::kPlayerReportMessageMostChars * ro::ui::kUtf8MostBytesPerChar);
}
