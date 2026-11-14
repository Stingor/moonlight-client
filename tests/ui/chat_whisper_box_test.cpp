// LA LISTE DES DERNIERS CORRESPONDANTS, DÉROULÉE SOUS LA BOX « PSEUDO » : elle
// s'ouvre à la prise du clavier, le laisse à la box, et ses touches ne font
// rien d'autre que ce qu'elles disent.

#include <array>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/chat_input.h"
#include "ro/ui/chat_whisper_list.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/ui_context.h"

namespace {

using ro::ui::ChatRecentWhisper;
using ro::ui::ChatWhisperWay;

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr float kWindowWidth = 420.0F;
constexpr float kRoomForInput = 200.0F;
// Un point qui tombe DANS la box « Pseudo » : elle ouvre la rangée, à gauche.
constexpr float kInTheBox = 40.0F;
constexpr float kOnTheRow = 32.0F;
// Un point loin de tout, dans le décor.
constexpr float kFarAway = 900.0F;
// Le nom de la fenêtre de la liste, tel que la box la dessine.
constexpr const char* kListWindow = "##correspondants";
const ro::ui::SkinArt kNoArt;

const std::array<ChatRecentWhisper, 3> kNames = {
    ChatRecentWhisper{.name = "Bob", .way = ChatWhisperWay::kIncoming},
    ChatRecentWhisper{.name = "Élodie", .way = ChatWhisperWay::kOutgoing},
    ChatRecentWhisper{.name = "Robert", .way = ChatWhisperWay::kOutgoing}};

// Une barre de saisie, image après image, avec une phrase déjà tapée : c'est
// elle qu'Entrée enverrait.
struct Bench {
  ro::ui::UiContext ui;
  ro::ui::ChatInputState state;
  ro::ui::ChatLinePreview preview;
  ro::ui::ChatInputAsk asked;
  std::optional<std::string> said;

  Bench() {
    std::snprintf(state.text.data(), state.text.size(), "%s", "salut");
    preview.target = "Alentour";
    preview.recent_whispers = kNames;
    Frame();
  }

  void Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
    ImGui::SetNextWindowSize(ImVec2(kWindowWidth, kViewport.height_px));
    ImGui::Begin("essai");
    static_cast<void>(ro::ui::ChatInputHeight(state, preview, kRoomForInput, kNoArt));
    asked = ro::ui::ChatInputAsk{};
    said = ro::ui::DrawChatInput(state, preview, &asked, kNoArt);
    ImGui::End();
    ui.EndFrame();
  }

  void Click(float x, float y) {
    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent(x, y);
    io.AddMouseButtonEvent(0, true);
    Frame();
    io.AddMouseButtonEvent(0, false);
    Frame();
  }

  // Une touche pressée : l'image où elle descend dit ce qu'elle a fait.
  void Press(ImGuiKey key) {
    ImGuiIO& io = ImGui::GetIO();
    io.AddKeyEvent(key, true);
    Frame();
    pressed_said = said;
    pressed_asked = asked;
    io.AddKeyEvent(key, false);
    Frame();
  }

  void Type(std::string_view letters) {
    for (const char letter : letters) {
      ImGui::GetIO().AddInputCharacter(static_cast<unsigned char>(letter));
      Frame();
    }
  }

  [[nodiscard]] static bool ListShown() {
    const ImGuiWindow* list = ImGui::FindWindowByName(kListWindow);
    return list != nullptr && list->Active;
  }

  [[nodiscard]] std::string_view whisper_to() const { return state.whisper_to.data(); }

  std::optional<std::string> pressed_said;
  ro::ui::ChatInputAsk pressed_asked;
};

}  // namespace

TEST_CASE("Box « Pseudo » : la liste se déroule à la prise du clavier", "[ui][chuchotement]") {
  Bench bench;
  CHECK_FALSE(Bench::ListShown());
  bench.Click(kInTheBox, kOnTheRow);
  CHECK(bench.state.whisper_list.open);
  CHECK(Bench::ListShown());
  // ⚠ LE CLAVIER EST RESTÉ À LA BOX : on y tape, et le filtre suit.
  bench.Type("rob");
  CHECK(bench.whisper_to() == "rob");
  CHECK(Bench::ListShown());
  // Aucun nom ne répond : la liste se replie.
  bench.Type("zz");
  CHECK_FALSE(Bench::ListShown());
}

TEST_CASE("Box « Pseudo » : ↓ puis Entrée prend le nom, et n'envoie pas la ligne",
          "[ui][chuchotement]") {
  Bench bench;
  bench.Click(kInTheBox, kOnTheRow);
  bench.Press(ImGuiKey_DownArrow);
  bench.Press(ImGuiKey_DownArrow);
  bench.Press(ImGuiKey_Enter);
  // 🔴 LA LIGNE EST RESTÉE : Entrée appartenait à la liste.
  CHECK_FALSE(bench.pressed_said.has_value());
  CHECK(std::string_view(bench.state.text.data()) == "salut");
  CHECK(bench.whisper_to() == "Élodie");
  // Le clavier part à la phrase, et la liste se replie.
  CHECK_FALSE(bench.state.focus_on_whisper);
  CHECK_FALSE(Bench::ListShown());
}

TEST_CASE("Box « Pseudo » : Tab prend le nom surligné", "[ui][chuchotement]") {
  Bench bench;
  bench.Click(kInTheBox, kOnTheRow);
  bench.Press(ImGuiKey_UpArrow);
  bench.Press(ImGuiKey_Tab);
  CHECK(bench.whisper_to() == "Robert");
  CHECK_FALSE(bench.pressed_said.has_value());
}

TEST_CASE("Box « Pseudo » : sans nom surligné, Entrée envoie comme avant", "[ui][chuchotement]") {
  Bench bench;
  bench.Click(kInTheBox, kOnTheRow);
  REQUIRE(Bench::ListShown());
  bench.Press(ImGuiKey_Enter);
  CHECK(bench.pressed_said == "salut");
  CHECK(bench.whisper_to().empty());
  // ⚠ LA BOX QUI REPREND LE CLAVIER APRÈS L'ENVOI NE REDÉROULE PAS SA LISTE.
  bench.Frame();
  CHECK_FALSE(Bench::ListShown());
}

TEST_CASE("Box « Pseudo » : Échap ferme la liste et rien d'autre", "[ui][chuchotement]") {
  Bench bench;
  // En battle mode, Échap qui quitte la box replie la barre : c'est ce qu'il
  // ne doit PAS faire tant que la liste est déroulée.
  bench.state.battle_mode = true;
  bench.state.unfolded = true;
  bench.Click(kInTheBox, kOnTheRow);
  REQUIRE(Bench::ListShown());
  bench.Press(ImGuiKey_Escape);
  CHECK_FALSE(Bench::ListShown());
  CHECK_FALSE(bench.pressed_asked.folded);
  CHECK(bench.state.unfolded);
  CHECK(bench.state.whisper_list.box_was_active);
  // La seconde frappe, liste fermée, a son rôle ordinaire.
  bench.Press(ImGuiKey_Escape);
  CHECK(bench.pressed_asked.folded);
}

TEST_CASE("Box « Pseudo » : un clic sur un nom le prend", "[ui][chuchotement]") {
  Bench bench;
  bench.Click(kInTheBox, kOnTheRow);
  const ImGuiWindow* list = ImGui::FindWindowByName(kListWindow);
  REQUIRE(list != nullptr);
  // La première rangée, au milieu de sa hauteur.
  const ImVec2 first_row(
      list->Pos.x + list->Size.x * 0.5F,
      list->Pos.y + ImGui::GetStyle().WindowPadding.y + ImGui::GetTextLineHeight() * 0.5F);
  bench.Click(first_row.x, first_row.y);
  CHECK(bench.whisper_to() == "Bob");
  CHECK_FALSE(bench.said.has_value());
}

TEST_CASE("Box « Pseudo » : un clic ailleurs replie la liste", "[ui][chuchotement]") {
  Bench bench;
  bench.Click(kInTheBox, kOnTheRow);
  REQUIRE(Bench::ListShown());
  bench.Click(kFarAway, kFarAway);
  CHECK_FALSE(bench.state.whisper_list.open);
  CHECK_FALSE(Bench::ListShown());
}
