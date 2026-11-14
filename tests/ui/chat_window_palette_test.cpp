// La palette de la chatbox, dessinée dans la fenêtre entière : ce qu'un clic
// envoie, ce qu'un Maj+clic écrit, et où remonte ce qu'une flottante pioche.

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/chat_input.h"
#include "ro/ui/chat_strip.h"
#include "ro/ui/chat_window.h"
#include "ro/ui/ui_context.h"

#include "support/emote_palette_bench.h"

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
// Aucune pièce d'habillage : la palette doit tenir sur les seuls replis.
const ro::ui::SkinArt kNoArt;

}  // namespace

namespace {

constexpr std::uint16_t kSmileId = 1;
constexpr std::uint16_t kNamelessId = 2;

struct WindowBench {
  ro::ui::UiContext ui;
  ro::ui::ChatWindows windows;
  std::uint32_t group = 0;
  ro::ui::ChatLinePreview preview{.target = "Alentour"};
  std::array<ro::ui::ChatStripTab, 1> tabs{ro::ui::ChatStripTab{.name = "Tout"}};
  std::array<ro::ui::ChatEmotion, 2> emotions{
      ro::ui::ChatEmotion{.id = kSmileId, .name = "smile", .chat_token = "smile"},
      ro::ui::ChatEmotion{.id = kNamelessId, .name = "pnj"}};
  std::span<const ro::ui::ChatEmotion> shown_emotions = emotions;
  std::span<const ro::ui::ChatInputLink> links;
  ro::ui::ChatWindowAction asked;

  void Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    windows.board.strips.clear();
    asked = ro::ui::DrawChatWindow(windows.For(group), ro::ui::ChatAppearance{}, preview, {},
                                   nullptr, tabs, 1, {}, ro::ui::ChatTabLook{}, shown_emotions, {},
                                   windows.board, kNoArt, links);
    ui.EndFrame();
  }
  [[nodiscard]] auto Drawer() {
    return [this] { Frame(); };
  }
  void Settle() {
    for (int frame = 0; frame < ro::test::kPaletteSettleFrames; ++frame) {
      Frame();
    }
  }
  [[nodiscard]] ImGuiWindow* Window() const {
    return ImGui::FindWindowByName(ro::ui::ChatWindowId(group).c_str());
  }

  // Ouvre la palette de la bande et vise la première émoticone.
  void AimAtFirstEmote(std::uint16_t emotion_id) {
    Settle();
    ImGuiWindow* window = Window();
    REQUIRE(window != nullptr);
    const float strip_y =
        window->DC.CursorStartPos.y + (ImGui::GetFontSize() + ro::ui::kChatTabHeightPadding) * 0.5F;
    REQUIRE(ro::test::SweepRowFor(window->GetID(ro::test::kEmoteButtonId), strip_y, window->Pos.x,
                                  window->Pos.x + window->Size.x, Drawer()));
    ro::test::ClickHere(false, Drawer());
    Settle();
    REQUIRE(ro::test::AimAtFirstEmoteCell(emotion_id, Drawer()));
  }
};

}  // namespace

TEST_CASE("Fenêtre détachée : le `:nom:` pioché remonte vers la saisie principale",
          "[ui][chatbox]") {
  // 🔴 UNE FLOTTANTE N'A PAS DE SAISIE : écrit dans son état, il se perdait.
  WindowBench bench;
  bench.group = 7;
  bench.AimAtFirstEmote(kSmileId);
  ro::ui::ChatWindowAction seen;
  ro::test::ClickHere(true, [&bench, &seen] {
    bench.Frame();
    if (!bench.asked.insert_into_main.empty()) {
      seen = bench.asked;
    }
  });
  CHECK(seen.insert_into_main == ":smile:");
  CHECK(seen.chose_emotion < 0);
  // Et la principale, posée par l'appelant, prend l'espace devant.
  ro::ui::ChatInputState main;
  std::ranges::copy(std::string_view("salut"), main.text.begin());
  REQUIRE(ro::ui::AppendPickedToChatInput(main, seen.insert_into_main));
  CHECK(main.pending_insert == " :smile:");
}

TEST_CASE("Bande : Maj+clic sur une émoticone sans nom ne fait rien", "[ui][chatbox]") {
  WindowBench bench;
  bench.shown_emotions = std::span(bench.emotions).subspan(1);
  bench.AimAtFirstEmote(kNamelessId);
  bool sent = false;
  ro::test::ClickHere(true, [&bench, &sent] {
    bench.Frame();
    sent = sent || bench.asked.chose_emotion >= 0;
  });
  bench.Settle();
  // 🔴 NI ENVOYÉE, NI ÉCRITE : le Maj+clic demandait d'écrire, pas d'envoyer.
  CHECK_FALSE(sent);
  CHECK(bench.windows.main.input.pending_insert.empty());
  CHECK(bench.windows.main.input.text[0] == '\0');
}

TEST_CASE("Bande : un clic sur une émoticone ENVOIE son `:nom:`, seul", "[ui][chatbox]") {
  WindowBench bench;
  std::ranges::copy(std::string_view("salut"), bench.windows.main.input.text.begin());
  bench.AimAtFirstEmote(kSmileId);
  ro::ui::ChatWindowAction seen;
  ro::test::ClickHere(false, [&bench, &seen] {
    bench.Frame();
    if (!bench.asked.sent_emote.empty()) {
      seen = bench.asked;
    }
  });
  bench.Settle();
  CHECK(seen.sent_emote == ":smile:");
  // 🔴 NI GESTE DU JEU, NI SAISIE TOUCHÉE : la phrase en cours attend intacte.
  CHECK(seen.chose_emotion < 0);
  CHECK(std::string_view(bench.windows.main.input.text.data()) == "salut");
  CHECK(bench.windows.main.input.pending_insert.empty());
  // C'est un envoi : la palette se referme.
  CHECK(GImGui->OpenPopupStack.empty());
}

TEST_CASE("Bande : sans nom, le clic part en geste du jeu", "[ui][chatbox]") {
  WindowBench bench;
  bench.shown_emotions = std::span(bench.emotions).subspan(1);
  bench.AimAtFirstEmote(kNamelessId);
  int gesture = -1;
  bool written = false;
  ro::test::ClickHere(false, [&bench, &gesture, &written] {
    bench.Frame();
    gesture = std::max(gesture, bench.asked.chose_emotion);
    written = written || !bench.asked.sent_emote.empty();
  });
  CHECK(gesture == kNamelessId);
  CHECK_FALSE(written);
}
