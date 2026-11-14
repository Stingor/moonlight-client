// LA MOLETTE SUR UNE LISTE DÉROULANTE FERMÉE : une entrée par cran, et rien au
// passage quand on parcourt une page.

#include <array>

#include <imgui.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/skin_widgets.h"
#include "ro/ui/ui_context.h"

using ro::ui::SkinArt;
using ro::ui::SkinCombo;
using ro::ui::SkinComboStepped;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
// Plus que le délai de pose d'une demi-seconde près : trente images.
constexpr int kSettleFrames = 30;
// Plus que le verrou de salve d'ImGui, qui dure sept dixièmes.
constexpr int kBurstOverFrames = 60;
// Entre deux crans d'une même salve.
constexpr int kFramesBetweenNotches = 10;
constexpr int kNotchesInBurst = 6;
// Un point sur le champ de la liste, loin de sa flèche : la fenêtre est au
// coin, et le champ commence sous sa barre de titre.
constexpr float kOnTheFieldX = 200.0F;
constexpr float kOnTheFieldY = 36.0F;
// Dans le champ, sous son coin haut-gauche.
constexpr float kIntoFieldX = 100.0F;
constexpr float kIntoFieldY = 8.0F;
constexpr float kPageHeight = 300.0F;
constexpr float kFarBelow = 2000.0F;
constexpr float kFarAbove = 1000.0F;

constexpr auto kItems = std::to_array<const char*>({"un", "deux", "trois"});

// Une image, dessinée par `draw`.
template <typename Draw>
void Frame(ro::ui::UiContext& ui, Draw&& draw) {
  ui.BeginFrame(kViewport, kFrameSeconds);
  draw();
  ui.EndFrame();
}

template <typename Draw>
void Frames(ro::ui::UiContext& ui, int count, Draw&& draw) {
  for (int frame = 0; frame < count; ++frame) {
    Frame(ui, draw);
  }
}

}  // namespace

TEST_CASE("Liste : un cran vise le voisin, borné aux deux bouts", "[ui][widgets][combo]") {
  CHECK(SkinComboStepped(1, 1, 3) == 2);
  CHECK(SkinComboStepped(1, -1, 3) == 0);
  CHECK(SkinComboStepped(2, 1, 3) == 2);
  CHECK(SkinComboStepped(0, -1, 3) == 0);
  CHECK(SkinComboStepped(1, 0, 3) == 1);
  // Sans rang choisi, le premier cran vise la première entrée.
  CHECK(SkinComboStepped(-1, 1, 3) == 0);
  CHECK(SkinComboStepped(-1, -1, 3) == 0);
  CHECK(SkinComboStepped(-1, 1, 0) == -1);
}

TEST_CASE("Liste : la molette fait défiler une liste fermée, une entrée par cran",
          "[ui][widgets][combo]") {
  ro::ui::UiContext ui;
  const SkinArt art;
  int chosen = 0;
  bool changed = false;
  ImGuiIO& io = ImGui::GetIO();
  const auto draw = [&art, &chosen, &changed]() {
    ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
    ImGui::SetNextWindowSize(ImVec2(kViewport.width_px, kViewport.height_px));
    ImGui::Begin("essai");
    changed = SkinCombo("##liste", &chosen, kItems, art) || changed;
    ImGui::End();
  };

  Frame(ui, draw);
  io.AddMousePosEvent(kOnTheFieldX, kOnTheFieldY);
  Frames(ui, kSettleFrames, draw);

  // Vers le bas : l'entrée d'après.
  io.AddMouseWheelEvent(0.0F, -1.0F);
  Frame(ui, draw);
  CHECK(chosen == 1);
  CHECK(changed);
  io.AddMouseWheelEvent(0.0F, -1.0F);
  Frame(ui, draw);
  CHECK(chosen == 2);

  // En butée, un cran ne change rien — et ne le dit pas.
  changed = false;
  io.AddMouseWheelEvent(0.0F, -1.0F);
  Frame(ui, draw);
  CHECK(chosen == 2);
  CHECK_FALSE(changed);

  // Vers le haut : l'entrée d'avant.
  io.AddMouseWheelEvent(0.0F, 1.0F);
  Frame(ui, draw);
  CHECK(chosen == 1);
}

TEST_CASE("Liste : un cran à peine arrivé dessus ne règle rien", "[ui][widgets][combo]") {
  // 🔴 LE DÉLAI DE POSE : parcourir une page de réglages à la molette fait
  // passer le curseur sur des listes, et aucune ne doit changer au passage.
  ro::ui::UiContext ui;
  const SkinArt art;
  int chosen = 0;
  ImGuiIO& io = ImGui::GetIO();
  const auto draw = [&art, &chosen]() {
    ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
    ImGui::SetNextWindowSize(ImVec2(kViewport.width_px, kViewport.height_px));
    ImGui::Begin("essai");
    static_cast<void>(SkinCombo("##liste", &chosen, kItems, art));
    ImGui::End();
  };

  Frame(ui, draw);
  io.AddMousePosEvent(kOnTheFieldX, kOnTheFieldY);
  Frame(ui, draw);
  io.AddMouseWheelEvent(0.0F, -1.0F);
  Frame(ui, draw);
  CHECK(chosen == 0);
}

TEST_CASE("Liste : le cran qu'elle prend ne fait pas défiler la page", "[ui][widgets][combo]") {
  ro::ui::UiContext ui;
  const SkinArt art;
  int chosen = 0;
  float scroll = 0.0F;
  ImGuiIO& io = ImGui::GetIO();
  const auto draw = [&art, &chosen, &scroll]() {
    ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
    ImGui::SetNextWindowSize(ImVec2(kViewport.width_px, kPageHeight));
    ImGui::Begin("page");
    static_cast<void>(SkinCombo("##liste", &chosen, kItems, art));
    ImGui::Dummy(ImVec2(1.0F, kFarBelow));
    scroll = ImGui::GetScrollY();
    ImGui::End();
  };

  Frame(ui, draw);
  io.AddMousePosEvent(kOnTheFieldX, kOnTheFieldY);
  Frames(ui, kSettleFrames, draw);
  io.AddMouseWheelEvent(0.0F, -1.0F);
  Frames(ui, 2, draw);
  CHECK(chosen == 1);
  CHECK(scroll == 0.0F);
}

TEST_CASE("Liste : tant qu'une salve fait défiler la page, la liste ne prend rien",
          "[ui][widgets][combo]") {
  // 🔴 LE VERROU DE SALVE : la page arrive en butée, le curseur s'immobilise sur
  // une liste — et les crans qui suivent appartiennent encore au défilement.
  ro::ui::UiContext ui;
  const SkinArt art;
  int chosen = 0;
  bool to_end = false;
  ImVec2 field;
  ImGuiIO& io = ImGui::GetIO();
  const auto draw = [&art, &chosen, &to_end, &field]() {
    ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
    ImGui::SetNextWindowSize(ImVec2(kViewport.width_px, kPageHeight));
    ImGui::Begin("page");
    if (to_end) {
      ImGui::SetScrollY(ImGui::GetScrollMaxY());
    }
    ImGui::Dummy(ImVec2(1.0F, kFarAbove));
    field = ImGui::GetCursorScreenPos();
    static_cast<void>(SkinCombo("##liste", &chosen, kItems, art));
    ImGui::End();
  };

  // La page déroulée jusqu'en bas, la liste en vue.
  to_end = true;
  Frames(ui, 3, draw);
  to_end = false;
  io.AddMousePosEvent(field.x + kIntoFieldX, field.y + kIntoFieldY);
  Frame(ui, draw);

  // Une salve qui commence avant la pose : elle va à la page, en butée, et
  // continue bien après le délai de pose.
  for (int notch = 0; notch < kNotchesInBurst; ++notch) {
    io.AddMouseWheelEvent(0.0F, -1.0F);
    Frames(ui, kFramesBetweenNotches, draw);
  }
  CHECK(chosen == 0);

  // La salve finie, le cran suivant est à la liste.
  Frames(ui, kBurstOverFrames, draw);
  io.AddMouseWheelEvent(0.0F, -1.0F);
  Frame(ui, draw);
  CHECK(chosen == 1);
}

TEST_CASE("Liste : une salve sur une page qui ne défile pas ne verrouille rien",
          "[ui][widgets][combo]") {
  // Le verrou tient au DÉFILEMENT, pas aux crans : un cran tombé trop tôt sur
  // une page immobile ne doit pas retarder le réglage qui suit la pose.
  ro::ui::UiContext ui;
  const SkinArt art;
  int chosen = 0;
  ImGuiIO& io = ImGui::GetIO();
  const auto draw = [&art, &chosen]() {
    ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
    ImGui::SetNextWindowSize(ImVec2(kViewport.width_px, kViewport.height_px));
    ImGui::Begin("essai");
    static_cast<void>(SkinCombo("##liste", &chosen, kItems, art));
    ImGui::End();
  };

  Frame(ui, draw);
  io.AddMousePosEvent(kOnTheFieldX, kOnTheFieldY);
  Frame(ui, draw);
  io.AddMouseWheelEvent(0.0F, -1.0F);
  Frame(ui, draw);
  REQUIRE(chosen == 0);
  // La pose atteinte, bien avant la fin d'un verrou de salve.
  Frames(ui, kSettleFrames, draw);
  io.AddMouseWheelEvent(0.0F, -1.0F);
  Frame(ui, draw);
  CHECK(chosen == 1);
}
