// UNE CONVERSATION 1:1 : sa saisie est de la même espèce que la barre de la
// conversation — la palette, les pastilles de lien —, mais sa palette n'envoie
// jamais d'émotion du jeu : seuls les voisins la verraient, jamais le
// correspondant.

#include "ro/ui/whisper_window.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/chat_input.h"
#include "ro/ui/chat_strip.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/ui_context.h"

#include "support/emote_palette_bench.h"

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr std::string_view kPeer = "Ana";
constexpr std::uint16_t kSmileId = 1;
constexpr std::uint16_t kNamelessId = 2;
constexpr std::uint32_t kAppleId = 512;
constexpr std::uint32_t kAppleToken = 3;

struct WhisperBench {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  ro::ui::WhisperWindowState state;
  std::array<ro::ui::ChatEmotion, 1> smile{
      ro::ui::ChatEmotion{.id = kSmileId, .name = "smile", .chat_token = "smile"}};
  // Une émoticone que le chat ne sait pas nommer.
  std::array<ro::ui::ChatEmotion, 1> nameless{
      ro::ui::ChatEmotion{.id = kNamelessId, .name = "pnj"}};
  std::array<ro::ui::ChatInputLink, 1> links{
      ro::ui::ChatInputLink{.label = "[Pomme]",
                            .kind = ro::ui::ChatLinkKind::kItem,
                            .item_id = kAppleId,
                            .token = kAppleToken}};
  ro::ui::WhisperWindowView view;
  ro::ui::WhisperWindowAction action;

  WhisperBench() {
    view.peer = kPeer;
    view.title = "With Ana";
    view.emotions = smile;
  }

  void Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    action = ro::ui::DrawWhisperWindow(state, view, art);
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

  void Type(std::string_view text) {
    state.input.text.fill('\0');
    std::ranges::copy(text, state.input.text.begin());
  }
  [[nodiscard]] std::string_view Typed() const { return state.input.text.data(); }

  [[nodiscard]] static ImGuiWindow* Window() {
    return ImGui::FindWindowByName(ro::ui::WhisperWindowId(kPeer).c_str());
  }
  // Le milieu de la rangée de saisie, la dernière de la fenêtre.
  [[nodiscard]] static float RowY() {
    return Window()->DC.CursorMaxPos.y - ImGui::GetFrameHeight() * 0.5F;
  }

  // Cherche le bouton de la palette et le clique.
  [[nodiscard]] bool OpenPalette() {
    ImGuiWindow* window = Window();
    REQUIRE(window != nullptr);
    if (!ro::test::SweepRowFor(window->GetID(ro::test::kEmoteButtonId), RowY(), window->Pos.x,
                               window->Pos.x + window->Size.x, Drawer())) {
      return false;
    }
    ro::test::ClickHere(false, Drawer());
    Settle();
    return !GImGui->OpenPopupStack.empty();
  }
};

}  // namespace

TEST_CASE("Conversation 1:1 : un clic sur une émoticone ENVOIE son nom au correspondant",
          "[ui][chat][whisper]") {
  WhisperBench bench;
  bench.Type("salut");
  bench.Settle();
  REQUIRE(bench.OpenPalette());
  REQUIRE(ro::test::AimAtFirstEmoteCell(kSmileId, bench.Drawer()));
  ro::test::ClickHere(false, bench.Drawer());
  // 🔴 LE `:nom:` PART, PAS L'ÉMOTION DE ZONE : celle-ci ne serait vue que des
  // voisins.
  CHECK(bench.action.chose_emotion < 0);
  REQUIRE(bench.action.said.has_value());
  CHECK(*bench.action.said == ":smile:");
  bench.Settle();
  // La phrase en cours attend, intacte.
  CHECK(bench.Typed() == "salut");
  CHECK(GImGui->OpenPopupStack.empty());
}

TEST_CASE("Conversation 1:1 : dans une saisie vide, pas d'espace devant", "[ui][chat][whisper]") {
  WhisperBench bench;
  bench.Settle();
  REQUIRE(bench.OpenPalette());
  REQUIRE(ro::test::AimAtFirstEmoteCell(kSmileId, bench.Drawer()));
  ro::test::ClickHere(true, bench.Drawer());
  bench.Settle();
  CHECK(bench.Typed() == ":smile:");
  CHECK(bench.action.chose_emotion < 0);
}

TEST_CASE("Conversation 1:1 : une émoticone sans nom est inopérante", "[ui][chat][whisper]") {
  WhisperBench bench;
  bench.view.emotions = bench.nameless;
  bench.Type("salut");
  bench.Settle();
  REQUIRE(bench.OpenPalette());
  static_cast<void>(ro::test::AimAtFirstEmoteCell(kNamelessId, bench.Drawer()));
  ro::test::ClickHere(false, bench.Drawer());
  bench.Settle();
  CHECK(bench.action.chose_emotion < 0);
  CHECK(bench.Typed() == "salut");
}

TEST_CASE("Conversation 1:1 : liaison fermée, la palette ne s'ouvre pas", "[ui][chat][whisper]") {
  WhisperBench bench;
  bench.view.can_send = false;
  bench.Settle();
  ImGuiWindow* window = WhisperBench::Window();
  REQUIRE(window != nullptr);
  // ⚠ LE BOUTON GRISÉ N'EST MÊME PAS SURVOLABLE : on clique à sa place, au
  // bout droit de la rangée.
  const float button_x = window->DC.CursorMaxPos.x - ImGui::GetFrameHeight() * 0.5F;
  ImGui::GetIO().AddMousePosEvent(button_x, WhisperBench::RowY());
  bench.Settle();
  ro::test::ClickHere(false, bench.Drawer());
  bench.Settle();
  CHECK(GImGui->OpenPopupStack.empty());
}

TEST_CASE("Conversation 1:1 : une pastille survolée rend son lien et le correspondant",
          "[ui][chat][whisper]") {
  WhisperBench bench;
  bench.view.input_links = bench.links;
  bench.Type("voici [Pomme] !");
  bench.Settle();
  const ImGuiWindow* window = WhisperBench::Window();
  REQUIRE(window != nullptr);
  bool found = false;
  for (float x = window->Pos.x; x < window->Pos.x + window->Size.x && !found;
       x += ro::test::kPaletteSweepStepPixels) {
    ImGui::GetIO().AddMousePosEvent(x, WhisperBench::RowY());
    bench.Frame();
    found = bench.action.input_link.hovering;
  }
  REQUIRE(found);
  CHECK(bench.action.input_link.hovered_id == kAppleId);
  CHECK(bench.action.input_link.hovered_token == kAppleToken);
  CHECK(bench.action.input_link.from_input);
  // 🔴 DE QUELLE SAISIE : chaque conversation a ses propres liens posés.
  CHECK(bench.action.input_link.input_peer == kPeer);
}
