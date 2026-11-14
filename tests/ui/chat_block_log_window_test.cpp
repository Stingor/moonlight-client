// LE JOURNAL DES BLOCAGES DE CHAT, dessiné hors jeu.
//
// Ce qui se vérifie sans écran : fermé il ne dessine rien, chaque phase se
// dessine sans rien redemander, une liste vide aussi.

#include "ro/ui/chat_block_log_window.h"

#include <cstddef>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/skin_pieces.h"
#include "ro/ui/ui_context.h"

using ro::render::SkinPiece;
using ro::ui::ChatBlockLogAnswer;
using ro::ui::ChatBlockLogPhase;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr float kPlainPiecePixels = 8.0F;
constexpr const char* kWindowName = "###journal_blocages_chat";

[[nodiscard]] ro::ui::SkinArt PlainArt() {
  ro::ui::SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<SkinPiece>(rank),
            ro::ui::SkinImage{.id = 1, .width = kPlainPiecePixels, .height = kPlainPiecePixels});
  }
  return art;
}

struct LogProbe {
  ro::ui::UiContext ui;
  ro::ui::SkinArt art = PlainArt();
  ro::ui::ChatBlockLogWindowState state;
  ro::ui::ChatBlockLogView view;

  LogProbe() {
    state.open = true;
    view.subject = "Alice — AID 2000123";
    view.phase = ChatBlockLogPhase::kReady;
    view.rows = {
        {.when = "2026-10-11 16:40", .author = "Garde", .command = "@unmute", .duration = ""},
        {.when = "2026-10-11 15:40",
         .author = "Sentinelle",
         .command = "@mute",
         .duration = "60 min"}};
  }

  ChatBlockLogAnswer Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    const ChatBlockLogAnswer answer = ro::ui::DrawChatBlockLogWindow(state, view, art);
    ui.EndFrame();
    return answer;
  }
};

}  // namespace

TEST_CASE("Journal des blocages : fermé, rien ne se dessine", "[ui][chat_block_log]") {
  LogProbe probe;
  probe.state.open = false;
  for (int frame = 0; frame < 3; ++frame) {
    CHECK_FALSE(probe.Frame().refresh);
  }
  CHECK(ImGui::FindWindowByName(kWindowName) == nullptr);
}

TEST_CASE("Journal des blocages : chaque phase se dessine sans rien redemander",
          "[ui][chat_block_log]") {
  LogProbe probe;
  for (const ChatBlockLogPhase phase :
       {ChatBlockLogPhase::kAsking, ChatBlockLogPhase::kSilent, ChatBlockLogPhase::kDenied,
        ChatBlockLogPhase::kMissing, ChatBlockLogPhase::kLogOff, ChatBlockLogPhase::kReadFailed,
        ChatBlockLogPhase::kReady}) {
    probe.view.phase = phase;
    for (int frame = 0; frame < 3; ++frame) {
      CHECK_FALSE(probe.Frame().refresh);
    }
  }
  CHECK(ImGui::FindWindowByName(kWindowName) != nullptr);
  CHECK(probe.state.open);
}

TEST_CASE("Journal des blocages : une liste vide se dessine aussi", "[ui][chat_block_log]") {
  LogProbe probe;
  probe.view.rows.clear();
  CHECK_FALSE(probe.Frame().refresh);
  CHECK(probe.state.open);
}

TEST_CASE("Journal des blocages : un nouveau joueur remonte la fenêtre", "[ui][chat_block_log]") {
  LogProbe probe;
  probe.state.raise = true;
  static_cast<void>(probe.Frame());
  CHECK_FALSE(probe.state.raise);
}
