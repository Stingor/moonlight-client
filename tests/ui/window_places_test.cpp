// LA PLACE VOULUE ET LA PLACE AFFICHÉE : ancrage, changement d'écran, échelle,
// disposition posée à chaud.

#include "ro/ui/window_places.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/ui_context.h"
#include "ro/ui/window_bounds.h"

namespace {

constexpr ro::ui::Viewport kWide{.width_px = 1920.0F, .height_px = 1080.0F};
constexpr ro::ui::Viewport kNarrow{.width_px = 1024.0F, .height_px = 768.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;

constexpr const char* kWindowName = "Inventaire###inventory";
constexpr const char* kWindowKey = "inventory";
// ⚠ DES NOMBRES QUI NE TOMBENT PAS JUSTE : à une place multiple du facteur, la
// troncature ne mordrait pas et l'essai passerait pour de mauvaises raisons.
constexpr float kWidth = 301.0F;
constexpr float kHeight = 203.0F;
// Collée près du bord droit et du bas de l'écran large.
constexpr float kRightGap = 17.0F;
constexpr float kBottomGap = 23.0F;
constexpr float kOtherScale = 1.37F;

void Draw(ro::ui::UiContext& ui, const ro::ui::Viewport& screen, ImVec2 placed = ImVec2(-1, -1),
          ImVec2 size = ImVec2(kWidth, kHeight)) {
  ui.BeginFrame(screen, kFrameSeconds);
  if (placed.x >= 0.0F) {
    ImGui::SetNextWindowPos(placed);
    ImGui::SetNextWindowSize(size);
  }
  ImGui::Begin(kWindowName);
  ImGui::TextUnformatted("du contenu");
  ImGui::End();
  ui.EndFrame();
}

[[nodiscard]] ImGuiWindow* Found() { return ImGui::FindWindowByName(kWindowName); }

// La fenêtre posée par le joueur dans le coin bas-droit de l'écran large.
void PlaceBottomRight(ro::ui::UiContext& ui) {
  Draw(ui, kWide,
       ImVec2(kWide.width_px - kRightGap - kWidth, kWide.height_px - kBottomGap - kHeight));
  Draw(ui, kWide);
}

}  // namespace

TEST_CASE("Ancrage : le tiers où tombe le centre choisit le bord", "[ui][ancrage]") {
  using ro::ui::AnchorSide;
  const ro::ui::WindowPlace left_top =
      ro::ui::PlaceFromRect({.x_px = 10.0F, .y_px = 20.0F, .width_px = 100.0F, .height_px = 50.0F},
                            1920.0F, 1080.0F, 1.0F);
  CHECK(left_top.across == AnchorSide::kStart);
  CHECK(left_top.down == AnchorSide::kStart);
  CHECK(left_top.offset_x == 10.0F);

  const ro::ui::WindowPlace right_bottom = ro::ui::PlaceFromRect(
      {.x_px = 1800.0F, .y_px = 1000.0F, .width_px = 100.0F, .height_px = 50.0F}, 1920.0F, 1080.0F,
      1.0F);
  CHECK(right_bottom.across == AnchorSide::kEnd);
  CHECK(right_bottom.down == AnchorSide::kEnd);
  CHECK(right_bottom.offset_x == 20.0F);
  CHECK(right_bottom.offset_y == 30.0F);

  const ro::ui::WindowPlace middle = ro::ui::PlaceFromRect(
      {.x_px = 910.0F, .y_px = 500.0F, .width_px = 100.0F, .height_px = 80.0F}, 1920.0F, 1080.0F,
      1.0F);
  CHECK(middle.across == AnchorSide::kCenter);
  CHECK(middle.offset_x == 0.0F);
}

TEST_CASE("Ancrage : un rectangle se retrouve à l'identique", "[ui][ancrage]") {
  const ro::ui::ScreenRect rect{
      .x_px = 1517.0F, .y_px = 811.0F, .width_px = 301.0F, .height_px = 203.0F};
  const ro::ui::WindowPlace place = ro::ui::PlaceFromRect(rect, 1920.0F, 1080.0F, kOtherScale);
  const ro::ui::ScreenRect back = ro::ui::RectFromPlace(place, 1920.0F, 1080.0F, kOtherScale);
  CHECK(back.x_px == rect.x_px);
  CHECK(back.y_px == rect.y_px);
}

TEST_CASE("Résolution : une fenêtre ancrée à droite suit le bord droit", "[ui][ancrage]") {
  ro::ui::UiContext ui;
  PlaceBottomRight(ui);
  Draw(ui, kNarrow);
  Draw(ui, kNarrow);
  ImGuiWindow* window = Found();
  REQUIRE(window != nullptr);
  // MÊME DISTANCE AUX BORDS sur le petit écran : la fenêtre ne s'est pas
  // simplement rabattue contre le bord, elle a gardé son écart.
  CHECK(window->Pos.x == kNarrow.width_px - kRightGap - kWidth);
  CHECK(window->Pos.y == kNarrow.height_px - kBottomGap - kHeight);
}

TEST_CASE("Résolution : un aller-retour rend la place de départ", "[ui][ancrage]") {
  ro::ui::UiContext ui;
  // PLUS HAUTE QUE LE PETIT ÉCRAN NE LE PERMET : c'est le cas qui se perdait,
  // la borne d'écran écrasait la taille retenue.
  const ImVec2 start(1300.0F, 61.0F);
  const ImVec2 tall(kWidth, 950.0F);
  Draw(ui, kWide, start, tall);
  Draw(ui, kWide);
  REQUIRE(Found()->SizeFull.y == tall.y);

  Draw(ui, kNarrow);
  Draw(ui, kNarrow);
  CHECK(Found()->SizeFull.y <= kNarrow.height_px * ro::ui::kMaxScreenFraction);
  CHECK(Found()->Pos.y + Found()->Size.y <= kNarrow.height_px);

  Draw(ui, kWide);
  Draw(ui, kWide);
  CHECK(Found()->Pos.x == start.x);
  CHECK(Found()->Pos.y == start.y);
  CHECK(Found()->SizeFull.y == tall.y);
}

TEST_CASE("Échelle : un aller-retour rend la place au pixel près", "[ui][echelle]") {
  ro::ui::UiContext ui;
  PlaceBottomRight(ui);
  const ImVec2 before = Found()->Pos;
  const ImVec2 size_before = Found()->SizeFull;

  ui.SetInterfaceScale(kOtherScale);
  Draw(ui, kWide);
  CHECK(Found()->SizeFull.x > size_before.x);
  ui.SetInterfaceScale(1.0F);
  Draw(ui, kWide);

  CHECK(Found()->Pos.x == before.x);
  CHECK(Found()->Pos.y == before.y);
  CHECK(Found()->SizeFull.x == size_before.x);
  CHECK(Found()->SizeFull.y == size_before.y);
}

TEST_CASE("Disposition : la fenêtre déplacée change la place voulue", "[ui][ancrage]") {
  ro::ui::UiContext ui;
  const std::uint64_t at_start = ui.layout_revision();
  PlaceBottomRight(ui);
  CHECK(ui.layout_revision() != at_start);

  const ro::ui::LayoutProfile layout = ui.CurrentLayout();
  const auto found = layout.places.find(kWindowKey);
  REQUIRE(found != layout.places.end());
  CHECK(found->second.across == ro::ui::AnchorSide::kEnd);
  CHECK(found->second.offset_x == kRightGap);

  // Rien ne bouge : la place voulue non plus.
  const std::uint64_t settled = ui.layout_revision();
  Draw(ui, kWide);
  Draw(ui, kWide);
  CHECK(ui.layout_revision() == settled);
}

TEST_CASE("Disposition : posée à chaud, elle déplace la fenêtre ouverte", "[ui][ancrage]") {
  ro::ui::UiContext ui;
  PlaceBottomRight(ui);

  ro::ui::LayoutProfile other;
  other.places.emplace(
      kWindowKey,
      ro::ui::WindowPlace{.offset_x = 40.0F, .offset_y = 50.0F, .width = 200.0F, .height = 150.0F});
  ui.UseLayout(other);
  Draw(ui, kWide);
  CHECK(Found()->Pos.x == 40.0F);
  CHECK(Found()->Pos.y == 50.0F);
  CHECK(Found()->SizeFull.x == 200.0F);
}

TEST_CASE("Disposition : une fenêtre pas encore née naît à sa place", "[ui][ancrage]") {
  ro::ui::UiContext ui;
  Draw(ui, kWide);  // une image sans elle : l'écran existe

  ro::ui::LayoutProfile layout;
  layout.places.emplace("plus_tard", ro::ui::WindowPlace{.across = ro::ui::AnchorSide::kEnd,
                                                         .offset_x = 30.0F,
                                                         .offset_y = 60.0F,
                                                         .width = 250.0F,
                                                         .height = 120.0F});
  ui.UseLayout(layout);
  Draw(ui, kWide);  // la passe prépare sa place

  ui.BeginFrame(kWide, kFrameSeconds);
  ImGui::SetNextWindowPos(ImVec2(5.0F, 5.0F), ImGuiCond_FirstUseEver);
  ImGui::Begin("Plus tard###plus_tard");
  ImGui::End();
  // ⚠ DÈS SA PREMIÈRE IMAGE, sans passer par un défaut : le défaut de
  // l'appelant ne vaut que pour une fenêtre sans place connue.
  ImGuiWindow* born = ImGui::FindWindowByName("Plus tard###plus_tard");
  REQUIRE(born != nullptr);
  CHECK(born->Pos.x == kWide.width_px - 30.0F - 250.0F);
  CHECK(born->Pos.y == 60.0F);
  ui.EndFrame();
}

TEST_CASE("Disposition : la place que l'appelant donne à l'apparition gagne", "[ui][ancrage]") {
  ro::ui::UiContext ui;
  Draw(ui, kWide);

  ro::ui::LayoutProfile layout;
  layout.places.emplace(
      "sous_le_curseur",
      ro::ui::WindowPlace{.offset_x = 30.0F, .offset_y = 60.0F, .width = 250.0F, .height = 120.0F});
  ui.UseLayout(layout);
  Draw(ui, kWide);

  // NÉE SOUS LE CURSEUR, ailleurs que la place préparée : elle y reste.
  const ImVec2 under_pointer(700.0F, 400.0F);
  for (int frame = 0; frame < 3; ++frame) {
    ui.BeginFrame(kWide, kFrameSeconds);
    ImGui::SetNextWindowPos(under_pointer, ImGuiCond_Appearing);
    ImGui::Begin("Description###sous_le_curseur");
    ImGui::End();
    ui.EndFrame();
  }
  ImGuiWindow* born = ImGui::FindWindowByName("Description###sous_le_curseur");
  REQUIRE(born != nullptr);
  CHECK(born->Pos.x == under_pointer.x);
  CHECK(born->Pos.y == under_pointer.y);
}

TEST_CASE("Échelle : une fenêtre à taille verrouillée la suit aussi", "[ui][echelle]") {
  // Le verrou du joueur pose `NoResize` : il fige le GESTE, pas l'échelle.
  ro::ui::UiContext ui;
  for (int frame = 0; frame < 2; ++frame) {
    ui.BeginFrame(kWide, kFrameSeconds);
    if (frame == 0) {
      ImGui::SetNextWindowPos(ImVec2(100.0F, 100.0F));
      ImGui::SetNextWindowSize(ImVec2(kWidth, kHeight));
    }
    ImGui::Begin("verrouillee", nullptr, ImGuiWindowFlags_NoResize);
    ImGui::End();
    ui.EndFrame();
  }
  ui.SetInterfaceScale(2.0F);
  ImGuiWindow* window = ImGui::FindWindowByName("verrouillee");
  REQUIRE(window != nullptr);
  CHECK(window->SizeFull.x == kWidth * 2.0F);
}

// ── LES BORDS : une fenêtre posée contre un bord y reste ───────────────────

namespace {

constexpr ro::ui::Viewport kHuge{.width_px = 2560.0F, .height_px = 1440.0F};
constexpr float kTopGap = 100.0F;
constexpr float kGrowth = 120.0F;
constexpr float kShrink = 80.0F;
// Plus large que les deux tiers de l'écran : collée à droite, son centre tombe
// au milieu de l'écran.
constexpr float kBroad = 1400.0F;
constexpr int kImages = 3;
// La poignée bas-droite, saisie en retrait du coin, puis tirée au-delà du
// bord droit en quelques images.
constexpr float kInsideGrip = 3.0F;
constexpr ImVec2 kPastRightEdge{2100.0F, 760.0F};
constexpr int kDragSteps = 4;

// Une image où l'appelant impose la taille, sans toucher à la position.
void DrawSized(ro::ui::UiContext& ui, ImVec2 size) {
  ui.BeginFrame(kWide, kFrameSeconds);
  ImGui::SetNextWindowSize(size);
  ImGui::Begin(kWindowName);
  ImGui::TextUnformatted("du contenu");
  ImGui::End();
  ui.EndFrame();
}

[[nodiscard]] float RightGap(const ro::ui::Viewport& screen) {
  return screen.width_px - (Found()->Pos.x + Found()->SizeFull.x);
}

}  // namespace

TEST_CASE("Bords : collée à droite, elle y reste quand sa taille change", "[ui][ancrage]") {
  ro::ui::UiContext ui;
  Draw(ui, kWide, ImVec2(kWide.width_px - kWidth, kTopGap));
  Draw(ui, kWide);
  REQUIRE(RightGap(kWide) == 0.0F);

  // Son appelant la grandit, puis la rapetisse : ni le joueur ni la passe.
  for (int image = 0; image < kImages; ++image) {
    DrawSized(ui, ImVec2(kWidth + kGrowth, kHeight));
  }
  CHECK(RightGap(kWide) == 0.0F);
  for (int image = 0; image < kImages; ++image) {
    DrawSized(ui, ImVec2(kWidth - kShrink, kHeight));
  }
  CHECK(Found()->SizeFull.x == kWidth - kShrink);
  CHECK(RightGap(kWide) == 0.0F);
  CHECK(Found()->Pos.y == kTopGap);
}

TEST_CASE("Bords : une grande fenêtre collée à droite garde son bord quand l'écran change",
          "[ui][ancrage]") {
  ro::ui::UiContext ui;
  Draw(ui, kWide, ImVec2(kWide.width_px - kBroad, kTopGap), ImVec2(kBroad, kHeight));
  Draw(ui, kWide);
  REQUIRE(RightGap(kWide) == 0.0F);

  Draw(ui, kHuge);
  Draw(ui, kHuge);
  CHECK(Found()->SizeFull.x == kBroad);
  CHECK(RightGap(kHuge) == 0.0F);

  Draw(ui, kWide);
  Draw(ui, kWide);
  CHECK(RightGap(kWide) == 0.0F);
}

TEST_CASE("Bords : la poignée tirée au-delà du bord ne fait pas sauter la fenêtre",
          "[ui][bornes]") {
  // ⚠ UNE FENÊTRE OUVERTE PAR `Begin` NU, sans la contrainte d'écran : c'est la
  // passe seule qui doit tenir le geste.
  ro::ui::UiContext ui;
  const ImVec2 start(1500.0F, 500.0F);
  Draw(ui, kWide, start);
  Draw(ui, kWide);

  ImGuiIO& io = ImGui::GetIO();
  const ImVec2 grip(start.x + kWidth - kInsideGrip, start.y + kHeight - kInsideGrip);
  io.MousePos = grip;
  io.MouseDown[0] = false;
  Draw(ui, kWide);
  io.MouseDown[0] = true;
  Draw(ui, kWide);
  for (int step = 1; step <= kDragSteps; ++step) {
    const float done = static_cast<float>(step) / static_cast<float>(kDragSteps);
    io.MousePos = ImVec2(grip.x + (kPastRightEdge.x - grip.x) * done,
                         grip.y + (kPastRightEdge.y - grip.y) * done);
    Draw(ui, kWide);
    // LE COIN QUE LE GESTE NE TIRE PAS NE BOUGE PAS, à aucune image.
    CHECK(Found()->Pos.x == start.x);
    CHECK(Found()->Pos.y == start.y);
  }
  io.MouseDown[0] = false;
  Draw(ui, kWide);
  Draw(ui, kWide);
  CHECK(Found()->Pos.x == start.x);
  CHECK(Found()->Pos.y == start.y);
  CHECK(RightGap(kWide) == 0.0F);
}

TEST_CASE("Disposition : posée à chaud, elle remplace la précédente", "[ui][ancrage]") {
  // Une fenêtre que la nouvelle disposition ne nomme pas reprend la place que
  // son code lui donne à la première ouverture.
  ro::ui::UiContext ui;
  const ImVec2 home(120.0F, 90.0F);
  const auto frame = [&](ImVec2 forced) {
    ui.BeginFrame(kWide, kFrameSeconds);
    if (forced.x >= 0.0F) {
      ImGui::SetNextWindowPos(forced);
    } else {
      ImGui::SetNextWindowPos(home, ImGuiCond_FirstUseEver);
    }
    ImGui::SetNextWindowSize(ImVec2(kWidth, kHeight), ImGuiCond_FirstUseEver);
    ImGui::Begin("chez_elle");
    ImGui::End();
    ui.EndFrame();
  };
  frame(ImVec2(900.0F, 500.0F));  // le joueur l'a posée ailleurs
  frame(ImVec2(-1.0F, -1.0F));
  REQUIRE(ImGui::FindWindowByName("chez_elle")->Pos.x == 900.0F);

  ui.UseLayout(ro::ui::LayoutProfile{});
  for (int image = 0; image < 3; ++image) {
    frame(ImVec2(-1.0F, -1.0F));
  }
  CHECK(ImGui::FindWindowByName("chez_elle")->Pos.x == home.x);
  CHECK(ImGui::FindWindowByName("chez_elle")->Pos.y == home.y);
}

TEST_CASE("Bords : une fenêtre à la taille de son contenu garde sa barre de titre",
          "[ui][ancrage]") {
  // 🔴 LE DÉFAUT VU EN JEU : l'éditeur de couleurs, posé dans le bas de l'écran,
  // remontait sa barre de titre à chaque onglet plus long, et la redescendait
  // au suivant.
  ro::ui::UiContext ui;
  constexpr const char* kAuto = "Éditeur###editeur_auto";
  constexpr int kFewLines = 3;
  // Assez pour changer de hauteur, pas assez pour sortir de l'écran.
  constexpr int kManyLines = 20;
  constexpr ImVec2 kNearBottomRight{1500.0F, 600.0F};
  const auto frame = [&ui](int lines, ImVec2 placed) {
    ui.BeginFrame(kWide, kFrameSeconds);
    if (placed.x >= 0.0F) {
      ImGui::SetNextWindowPos(placed);
    }
    ImGui::Begin(kAuto, nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    for (int line = 0; line < lines; ++line) {
      ImGui::TextUnformatted("une ligne");
    }
    ImGui::End();
    ui.EndFrame();
  };
  frame(kFewLines, kNearBottomRight);
  for (int image = 0; image < kImages; ++image) {
    frame(kFewLines, ImVec2(-1.0F, -1.0F));
  }
  const ImVec2 top = ImGui::FindWindowByName(kAuto)->Pos;

  for (int image = 0; image < kImages; ++image) {
    frame(kManyLines, ImVec2(-1.0F, -1.0F));
  }
  CHECK(ImGui::FindWindowByName(kAuto)->Pos.y == top.y);
  for (int image = 0; image < kImages; ++image) {
    frame(kFewLines, ImVec2(-1.0F, -1.0F));
  }
  CHECK(ImGui::FindWindowByName(kAuto)->Pos.x == top.x);
  CHECK(ImGui::FindWindowByName(kAuto)->Pos.y == top.y);
}
