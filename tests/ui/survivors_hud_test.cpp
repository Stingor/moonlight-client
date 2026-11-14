// Le HUD d'une partie de « Survivants de Midgard » : le minuteur, la place des
// flèches de bord, et un dessin qui ajoute des sommets sans rien prendre au
// clavier.

#include "ro/ui/survivors_hud.h"

#include <array>
#include <cmath>
#include <numbers>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/ui/ui_context.h"

namespace {

using ro::ui::SurvivorsHudSlot;
using ro::ui::SurvivorsHudView;

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr std::uint64_t kMinuteMs = 60'000;
constexpr float kInset = 20.0F;
const ImVec2 kFrameMin(0.0F, 0.0F);
const ImVec2 kFrameMax(kViewport.width_px, kViewport.height_px);
// Un identifiant de texture quelconque : la liste de dessin ne le lit pas.
constexpr ImTextureID kSomeTexture = 7;

// Les sommets que le HUD ajoute à la liste de fond.
[[nodiscard]] int VerticesOf(const SurvivorsHudView& view) {
  ro::ui::UiContext ui;
  ui.BeginFrame(kViewport, kFrameSeconds);
  const ImDrawList* list = ImGui::GetBackgroundDrawList();
  const int before = list->VtxBuffer.Size;
  ro::ui::DrawSurvivorsHud(view);
  const int after = list->VtxBuffer.Size;
  ui.EndFrame();
  return after - before;
}

}  // namespace

TEST_CASE("HUD survivors : la classe d'un allié suit son nom, quand elle se nomme",
          "[ui][survivors]") {
  // La couleur de la classe : seul son texte la porte dans ce HUD.
  constexpr ImU32 kClassColor = IM_COL32(240, 200, 120, 255);
  std::array<ro::ui::SurvivorsHudAlly, 1> allies = {
      ro::ui::SurvivorsHudAlly{.name = "Sting", .class_name = "Archer"}};
  SurvivorsHudView view;
  view.allies = allies;
  const auto class_vertices = [&view] {
    ro::ui::UiContext ui;
    ui.BeginFrame(kViewport, kFrameSeconds);
    const ImDrawList* list = ImGui::GetBackgroundDrawList();
    ro::ui::DrawSurvivorsHud(view);
    int count = 0;
    for (const ImDrawVert& vertex : list->VtxBuffer) {
      if (vertex.col == kClassColor) ++count;
    }
    ui.EndFrame();
    return count;
  };
  CHECK(class_vertices() > 0);
  allies[0].class_name = "";
  CHECK(class_vertices() == 0);
}

TEST_CASE("HUD survivors : la case d'une posture qui dure se cerne", "[ui][survivors]") {
  std::array<SurvivorsHudSlot, 1> slots = {SurvivorsHudSlot{.icon = kSomeTexture, .level = 3}};
  SurvivorsHudView view;
  view.skills = slots;
  // La couleur du cerne : seul un sommet du cerne la porte dans ce HUD.
  constexpr ImU32 kActiveEdge = IM_COL32(255, 210, 90, 255);
  const auto edge_vertices = [&view] {
    ro::ui::UiContext ui;
    ui.BeginFrame(kViewport, kFrameSeconds);
    const ImDrawList* list = ImGui::GetBackgroundDrawList();
    ro::ui::DrawSurvivorsHud(view);
    int count = 0;
    for (const ImDrawVert& vertex : list->VtxBuffer) {
      if (vertex.col == kActiveEdge) ++count;
    }
    ui.EndFrame();
    return count;
  };
  CHECK(edge_vertices() == 0);
  slots[0].active = true;
  CHECK(edge_vertices() > 0);
}

TEST_CASE("HUD survivors : le minuteur en minutes et secondes", "[ui][survivors]") {
  CHECK(std::string_view(ro::ui::SurvivorsClockText(0).data()) == "00:00");
  CHECK(std::string_view(ro::ui::SurvivorsClockText(kMinuteMs + 1'999).data()) == "01:01");
  CHECK(std::string_view(ro::ui::SurvivorsClockText(15 * kMinuteMs).data()) == "15:00");
  // Au-delà d'une heure, les minutes continuent.
  CHECK(std::string_view(ro::ui::SurvivorsClockText(61 * kMinuteMs).data()) == "61:00");
}

TEST_CASE("HUD survivors : aucune flèche pour une cible dans le cadre", "[ui][survivors]") {
  CHECK_FALSE(ro::ui::PlaceEdgeArrow(kFrameMin, kFrameMax, kInset, ImVec2(640.0F, 360.0F)));
  // Dans la marge rentrée, la cible est hors du cadre utile : flèche.
  CHECK(ro::ui::PlaceEdgeArrow(kFrameMin, kFrameMax, kInset, ImVec2(5.0F, 360.0F)));
}

TEST_CASE("HUD survivors : la flèche se pose au bord, vers la cible", "[ui][survivors]") {
  const auto right = ro::ui::PlaceEdgeArrow(kFrameMin, kFrameMax, kInset, ImVec2(5'000.0F, 360.0F));
  REQUIRE(right.has_value());
  CHECK(right->tip.x == Catch::Approx(kViewport.width_px - kInset));
  CHECK(right->tip.y == Catch::Approx(360.0F));
  CHECK(right->angle_radians == Catch::Approx(0.0F).margin(1e-6));

  const auto above = ro::ui::PlaceEdgeArrow(kFrameMin, kFrameMax, kInset, ImVec2(640.0F, -900.0F));
  REQUIRE(above.has_value());
  CHECK(above->tip.y == Catch::Approx(kInset));
  CHECK(above->tip.x == Catch::Approx(640.0F));
  CHECK(above->angle_radians == Catch::Approx(-std::numbers::pi_v<float> / 2.0F));

  // En diagonale, le bord le plus proche sur la droite arrête la flèche.
  const auto corner =
      ro::ui::PlaceEdgeArrow(kFrameMin, kFrameMax, kInset, ImVec2(2'000.0F, 2'000.0F));
  REQUIRE(corner.has_value());
  CHECK(corner->tip.y == Catch::Approx(kViewport.height_px - kInset));
  CHECK(corner->tip.x < kViewport.width_px - kInset);
}

TEST_CASE("HUD survivors : la flèche sort de la fenêtre qui la couvrirait", "[ui][survivors]") {
  // Une chatbox en bas à gauche, et une cible loin vers le sud-ouest.
  constexpr float kClearance = 10.0F;
  const std::array<ro::ui::SurvivorsHudRect, 1> chat = {
      ro::ui::SurvivorsHudRect{.min = ImVec2(0.0F, 460.0F), .max = ImVec2(420.0F, 720.0F)}};
  const ImVec2 target(-2'000.0F, 2'000.0F);
  const auto bare = ro::ui::PlaceEdgeArrow(kFrameMin, kFrameMax, kInset, target);
  const auto clear = ro::ui::PlaceEdgeArrow(kFrameMin, kFrameMax, kInset, target, chat, kClearance);
  REQUIRE(bare.has_value());
  REQUIRE(clear.has_value());
  // Sans la fenêtre, la pointe tomberait dedans ; avec elle, elle s'arrête à
  // son bord gonflé, sur la même droite.
  CHECK(bare->tip.y > chat[0].min.y);
  CHECK((clear->tip.x > chat[0].max.x + kClearance - 0.5F ||
         clear->tip.y < chat[0].min.y - kClearance + 0.5F));
  CHECK(clear->angle_radians == Catch::Approx(bare->angle_radians));

  // Une cible dans le cadre, mais sous la fenêtre : une flèche au bord de la
  // fenêtre, alors que sans fenêtre il n'y en aurait pas.
  const ImVec2 under(200.0F, 600.0F);
  CHECK_FALSE(ro::ui::PlaceEdgeArrow(kFrameMin, kFrameMax, kInset, under));
  const auto hidden = ro::ui::PlaceEdgeArrow(kFrameMin, kFrameMax, kInset, under, chat, kClearance);
  REQUIRE(hidden.has_value());
  CHECK((hidden->tip.x > chat[0].max.x + kClearance - 0.5F ||
         hidden->tip.y < chat[0].min.y - kClearance + 0.5F));
}

TEST_CASE("HUD survivors : vue minimale et vue pleine ajoutent des sommets", "[ui][survivors]") {
  const int minimal = VerticesOf(SurvivorsHudView{});
  CHECK(minimal > 0);

  const std::array<SurvivorsHudSlot, 3> skills = {
      SurvivorsHudSlot{.icon = kSomeTexture, .level = 3},
      SurvivorsHudSlot{.icon = kSomeTexture, .level = 1},
      SurvivorsHudSlot{.icon = 0, .level = 8},
  };
  const std::array<SurvivorsHudSlot, 2> cards = {
      SurvivorsHudSlot{.icon = kSomeTexture, .level = 2},
      SurvivorsHudSlot{.icon = kSomeTexture, .level = 5},
  };
  const std::array<ro::ui::SurvivorsEdgeTarget, 2> targets = {
      ro::ui::SurvivorsEdgeTarget{.screen_px = ImVec2(-300.0F, 100.0F),
                                  .kind = ro::ui::SurvivorsEdgeKind::kBoss},
      ro::ui::SurvivorsEdgeTarget{.screen_px = ImVec2(900.0F, 2'000.0F),
                                  .kind = ro::ui::SurvivorsEdgeKind::kChest},
  };
  SurvivorsHudView full;
  full.level = 27;
  full.exp_fill = 0.6F;
  full.clock_ms = 14 * kMinuteMs;
  full.kills = 4'321;
  full.zeny = 987;
  full.skills = skills;
  full.cards = cards;
  full.boss_name = "Baphomet";
  full.boss_fill = 0.4F;
  full.edge_targets = targets;
  full.unread_whispers = 2;
  full.hero_feet_px = ImVec2(640.0F, 380.0F);
  full.hero_fill = 0.3F;
  CHECK(VerticesOf(full) > minimal);
}

TEST_CASE("HUD survivors : la flèche de la rencontre porte son icône", "[ui][survivors]") {
  SurvivorsHudView view;
  const std::array<ro::ui::SurvivorsEdgeTarget, 1> bare = {ro::ui::SurvivorsEdgeTarget{
      .screen_px = ImVec2(-300.0F, 100.0F), .kind = ro::ui::SurvivorsEdgeKind::kMeeting}};
  view.edge_targets = bare;
  const int arrow_only = VerticesOf(view);
  const std::array<ro::ui::SurvivorsEdgeTarget, 1> marked = {
      ro::ui::SurvivorsEdgeTarget{.screen_px = ImVec2(-300.0F, 100.0F),
                                  .kind = ro::ui::SurvivorsEdgeKind::kMeeting,
                                  .icon = kSomeTexture}};
  view.edge_targets = marked;
  CHECK(VerticesOf(view) > arrow_only);
  // En vue, ni flèche ni icône.
  const int nothing = VerticesOf(SurvivorsHudView{});
  const std::array<ro::ui::SurvivorsEdgeTarget, 1> seen = {
      ro::ui::SurvivorsEdgeTarget{.screen_px = ImVec2(640.0F, 360.0F),
                                  .kind = ro::ui::SurvivorsEdgeKind::kMeeting,
                                  .icon = kSomeTexture}};
  view.edge_targets = seen;
  CHECK(VerticesOf(view) == nothing);
}

TEST_CASE("HUD survivors : la vignette d'une carte ramassée glisse puis repart",
          "[ui][survivors]") {
  CHECK(ro::ui::SurvivorsCardSlide(0.0F) == 0.0F);
  CHECK(ro::ui::SurvivorsCardSlide(0.05F) == Catch::Approx(0.5F));
  CHECK(ro::ui::SurvivorsCardSlide(0.5F) == 1.0F);
  CHECK(ro::ui::SurvivorsCardSlide(0.95F) == Catch::Approx(0.5F));
  CHECK(ro::ui::SurvivorsCardSlide(1.0F) == 0.0F);

  // Une illustration au milieu de sa vie se dessine ; sans texture, rien.
  const int without = VerticesOf(SurvivorsHudView{});
  SurvivorsHudView shown;
  shown.found_card = ro::ui::SurvivorsHudCard{
      .illustration = kSomeTexture, .size = ImVec2(320.0F, 440.0F), .fraction = 0.5F};
  CHECK(VerticesOf(shown) > without);
  shown.found_card.illustration = 0;
  CHECK(VerticesOf(shown) == without);
}

TEST_CASE("HUD survivors : une case se trouve sous le curseur", "[ui][survivors]") {
  const std::array<ro::ui::SurvivorsHudSlot, 2> slots{ro::ui::SurvivorsHudSlot{.name = "premier"},
                                                      ro::ui::SurvivorsHudSlot{.name = "second"}};
  constexpr ImVec2 kCorner(10.0F, 20.0F);
  constexpr float kInsideFirst = 15.0F;
  constexpr float kBetweenTheTwo = 43.0F;
  constexpr float kInsideSecond = 50.0F;
  constexpr float kPastTheRow = 90.0F;
  constexpr float kOnTheRow = 25.0F;
  constexpr float kUnderTheRow = 60.0F;
  const auto at = [&](float x, float y) {
    return ro::ui::SurvivorsSlotAt(slots, kCorner, ImVec2(x, y));
  };
  CHECK(at(kInsideFirst, kOnTheRow) == slots.data());
  CHECK(at(kBetweenTheTwo, kOnTheRow) == nullptr);
  CHECK(at(kInsideSecond, kOnTheRow) == slots.data() + 1);
  CHECK(at(kPastTheRow, kOnTheRow) == nullptr);
  CHECK(at(kInsideFirst, kUnderTheRow) == nullptr);
}

TEST_CASE("HUD survivors : l'infobulle d'une case ne prend pas la souris", "[ui][survivors]") {
  const std::array<ro::ui::SurvivorsHudSlot, 1> slots{
      ro::ui::SurvivorsHudSlot{.level = 3, .name = "Bash", .description = "Un coup."}};
  SurvivorsHudView view;
  view.skills = slots;
  ro::ui::UiContext ui;
  // Le curseur va sur la première case, sous la barre et le niveau.
  constexpr ImVec2 kOnFirstSlot(20.0F, 45.0F);
  for (int frame = 0; frame < 3; ++frame) {
    ImGui::GetIO().AddMousePosEvent(kOnFirstSlot.x, kOnFirstSlot.y);
    ui.BeginFrame(kViewport, kFrameSeconds);
    ro::ui::DrawSurvivorsHud(view);
    ui.EndFrame();
    CHECK_FALSE(ImGui::GetIO().WantCaptureMouse);
  }
}

TEST_CASE("HUD survivors : il ne lit aucune entrée clavier", "[ui][survivors]") {
  ro::ui::UiContext ui;
  SurvivorsHudView view;
  view.boss_name = "Baphomet";
  view.hero_feet_px = ImVec2(640.0F, 380.0F);
  ui.BeginFrame(kViewport, kFrameSeconds);
  ro::ui::DrawSurvivorsHud(view);
  ui.EndFrame();
  for (const ImGuiKey key : {ImGuiKey_1, ImGuiKey_Escape, ImGuiKey_W, ImGuiKey_Space}) {
    ImGui::GetIO().AddKeyEvent(key, true);
    ui.BeginFrame(kViewport, kFrameSeconds);
    ro::ui::DrawSurvivorsHud(view);
    CHECK(ImGui::GetKeyOwner(key) == ImGuiKeyOwner_NoOwner);
    ui.EndFrame();
    ImGui::GetIO().AddKeyEvent(key, false);
    CHECK_FALSE(ui.gate().keyboard_to_ui);
    CHECK_FALSE(ui.gate().mouse_to_ui);
  }
  CHECK(ImGui::GetActiveID() == 0);
}
