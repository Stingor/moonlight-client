// LE PRESSE-PAPIERS DE L'ÉDITEUR DE CARTE : la plage d'heures en texte, et les
// menus du clic droit qui copient et collent — un champ à trois valeurs, la
// plage d'heures, une ligne de liste.

#include <array>
#include <cstddef>
#include <functional>
#include <string>
#include <variant>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/ui/hour_range_text.h"
#include "ro/ui/map_editor_effects_panel.h"
#include "ro/ui/map_editor_row_menu.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/triple_clipboard.h"
#include "ro/ui/wheel_drag.h"

using Catch::Approx;
using ro::ui::HourRange;
using ro::ui::HourRangeText;
using ro::ui::ParseHourRange;

TEST_CASE("Éditeur : une plage d'heures s'écrit en texte, à la minute", "[ui][mapedit]") {
  CHECK(HourRangeText(HourRange{.from_hour = 20.0F, .to_hour = 6.0F}) == "20:00-06:00");
  CHECK(HourRangeText(HourRange{.from_hour = 0.0F, .to_hour = 24.0F}) == "00:00-24:00");
  CHECK(HourRangeText(HourRange{.from_hour = 21.25F, .to_hour = 5.75F}) == "21:15-05:45");
  // Une borne glissée hors du quart d'heure s'arrondit à la minute.
  CHECK(HourRangeText(HourRange{.from_hour = 7.0F + (59.6F / 60.0F), .to_hour = 8.0F}) ==
        "08:00-08:00");
}

TEST_CASE("Éditeur : une plage d'heures se relit, et fait l'aller-retour", "[ui][mapedit]") {
  for (const HourRange range : {HourRange{.from_hour = 20.0F, .to_hour = 6.0F},
                                HourRange{.from_hour = 0.0F, .to_hour = 24.0F},
                                HourRange{.from_hour = 18.75F, .to_hour = 23.5F}}) {
    const auto read = ParseHourRange(HourRangeText(range));
    REQUIRE(read.has_value());
    CHECK(read->from_hour == Approx(range.from_hour));
    CHECK(read->to_hour == Approx(range.to_hour));
  }
  // Minuit s'écrit 0 ou 24.
  const auto midnight = ParseHourRange("00:00-24:00");
  REQUIRE(midnight.has_value());
  CHECK(midnight->from_hour == 0.0F);
  CHECK(midnight->to_hour == 24.0F);
}

TEST_CASE("Éditeur : la plage d'heures se lit avec tolérance", "[ui][mapedit]") {
  const auto spaced = ParseHourRange("  20:30 - 6:15 \n");
  REQUIRE(spaced.has_value());
  CHECK(spaced->from_hour == Approx(20.5F));
  CHECK(spaced->to_hour == Approx(6.25F));
  const auto letters = ParseHourRange("20h-6h30");
  REQUIRE(letters.has_value());
  CHECK(letters->from_hour == Approx(20.0F));
  CHECK(letters->to_hour == Approx(6.5F));
  const auto bare = ParseHourRange("22–4");
  REQUIRE(bare.has_value());
  CHECK(bare->from_hour == Approx(22.0F));
  CHECK(bare->to_hour == Approx(4.0F));
  CHECK(ParseHourRange("19:00—07:00").has_value());
}

TEST_CASE("Éditeur : une plage d'heures hors du jour est refusée", "[ui][mapedit]") {
  CHECK_FALSE(ParseHourRange("").has_value());
  CHECK_FALSE(ParseHourRange("20:00").has_value());
  CHECK_FALSE(ParseHourRange("25:00-06:00").has_value());
  CHECK_FALSE(ParseHourRange("24:30-06:00").has_value());
  CHECK_FALSE(ParseHourRange("20:60-06:00").has_value());
  CHECK_FALSE(ParseHourRange("20:5-06:00").has_value());
  CHECK_FALSE(ParseHourRange("20:-06:00").has_value());
  CHECK_FALSE(ParseHourRange("-5-6").has_value());
  CHECK_FALSE(ParseHourRange("20:00-06:00-08:00").has_value());
  CHECK_FALSE(ParseHourRange("vingt-six").has_value());
  CHECK_FALSE(ParseHourRange("150.10 113.73 -4.00").has_value());
}

namespace {

constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr float kPiecePixels = 8.0F;
constexpr ImVec2 kHostAt(40.0F, 40.0F);
constexpr ImVec2 kHostSize(520.0F, 320.0F);
constexpr int kSettleFrames = 4;
constexpr ro::ui::WheelDrag kPlainDrag{.drag_step = 1.0F, .wheel_step = 1.0F, .format = "%.2f"};

// Un habillage complet : les identifiants ne désignent aucune texture, rien
// n'est peint ici.
[[nodiscard]] ro::ui::SkinArt PlainArt() {
  ro::ui::SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<ro::render::SkinPiece>(rank),
            ro::ui::SkinImage{.id = 1, .width = kPiecePixels, .height = kPiecePixels});
  }
  return art;
}

// UN CONTEXTE IMGUI QUI VIT PLUSIEURS IMAGES, et son presse-papiers à lui :
// l'essai ne touche pas à celui du système.
struct ClipboardBench {
  ImGuiContext* context = ImGui::CreateContext();
  std::string clipboard;
  // Ce que le dernier élément dessiné couvre, à la dernière image.
  ImRect field;

  ClipboardBench() {
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(800.0F, 600.0F);
    io.DeltaTime = kFrameSeconds;
    io.IniFilename = nullptr;
    io.Fonts->AddFontDefault();
    io.Fonts->Build();
    ImGuiPlatformIO& platform = ImGui::GetPlatformIO();
    platform.Platform_ClipboardUserData = &clipboard;
    platform.Platform_SetClipboardTextFn = [](ImGuiContext* /*context*/, const char* text) {
      *static_cast<std::string*>(ImGui::GetPlatformIO().Platform_ClipboardUserData) = text;
    };
    platform.Platform_GetClipboardTextFn = [](ImGuiContext* /*context*/) -> const char* {
      return static_cast<std::string*>(ImGui::GetPlatformIO().Platform_ClipboardUserData)->c_str();
    };
  }
  ~ClipboardBench() { ImGui::DestroyContext(context); }
  ClipboardBench(const ClipboardBench&) = delete;
  ClipboardBench& operator=(const ClipboardBench&) = delete;

  // Une image : `draw` pose ses éléments dans une fenêtre hôte fixe.
  void Frame(const std::function<void(ImRect&)>& draw) {
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(kHostAt);
    ImGui::SetNextWindowSize(kHostSize);
    ImGui::Begin("##hote", nullptr, ImGuiWindowFlags_NoMove);
    draw(field);
    ImGui::End();
    ImGui::EndFrame();
  }
  void Settle(const std::function<void(ImRect&)>& draw) {
    for (int frame = 0; frame < kSettleFrames; ++frame) {
      Frame(draw);
    }
  }
  // Un clic du bouton `button` là où est la souris : l'enfoncement, puis le
  // relâchement, une image chacun.
  void Click(ImGuiMouseButton button, const std::function<void(ImRect&)>& draw) {
    ImGui::GetIO().AddMouseButtonEvent(button, true);
    Frame(draw);
    ImGui::GetIO().AddMouseButtonEvent(button, false);
    Frame(draw);
  }
  void MoveTo(ImVec2 at, const std::function<void(ImRect&)>& draw) {
    ImGui::GetIO().AddMousePosEvent(at.x, at.y);
    Settle(draw);
  }
  // Le clic droit au milieu du champ, puis le menu le temps de se poser.
  void OpenMenu(const std::function<void(ImRect&)>& draw) {
    Settle(draw);
    MoveTo(field.GetCenter(), draw);
    Click(ImGuiMouseButton_Right, draw);
    Settle(draw);
  }
};

// Le menu ouvert, s'il y en a un.
[[nodiscard]] ImGuiWindow* OpenMenuWindow() {
  const ImGuiContext& context = *ImGui::GetCurrentContext();
  if (context.OpenPopupStack.empty()) {
    return nullptr;
  }
  return context.OpenPopupStack.back().Window;
}

// Le milieu de la première entrée du menu ouvert, et celui de la dernière : le
// menu se taille sur ce qu'il porte.
[[nodiscard]] ImVec2 FirstEntry(const ImGuiWindow& menu) {
  return ImVec2(menu.DC.CursorStartPos.x + ImGui::GetFontSize(),
                menu.DC.CursorStartPos.y + (ImGui::GetFontSize() * 0.5F));
}
[[nodiscard]] ImVec2 LastEntry(const ImGuiWindow& menu) {
  return ImVec2(menu.DC.CursorStartPos.x + ImGui::GetFontSize(),
                menu.Rect().Max.y - menu.WindowPadding.y - (ImGui::GetFontSize() * 0.5F));
}

}  // namespace

TEST_CASE("Éditeur : un champ à trois valeurs n'a plus de boutons, rien sans clic droit",
          "[ui][mapedit]") {
  ClipboardBench bench;
  std::array<float, 3> values{1.0F, 2.0F, 3.0F};
  bool pasted = false;
  ImVec2 cursor_before;
  ImVec2 cursor_after;
  ImRect item_after;
  bench.Settle([&](ImRect& field) {
    static_cast<void>(ro::ui::WheelDragFloat3("Position", values, kPlainDrag));
    field = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    cursor_before = ImGui::GetCursorScreenPos();
    pasted |= ro::ui::TripleClipboardMenu("position", values);
    cursor_after = ImGui::GetCursorScreenPos();
    item_after = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
  });
  // Rien n'a été posé à la suite du champ.
  CHECK(cursor_after.x == cursor_before.x);
  CHECK(cursor_after.y == cursor_before.y);
  CHECK(item_after.Max.x == bench.field.Max.x);
  CHECK_FALSE(pasted);
  CHECK(OpenMenuWindow() == nullptr);
}

TEST_CASE("Éditeur : le clic droit sur un champ à trois valeurs copie et colle", "[ui][mapedit]") {
  ClipboardBench bench;
  std::array<float, 3> values{150.1F, 113.734F, -4.0F};
  bool pasted = false;
  const auto draw = [&](ImRect& field) {
    static_cast<void>(ro::ui::WheelDragFloat3("Position", values, kPlainDrag));
    field = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    pasted |= ro::ui::TripleClipboardMenu("position", values);
  };
  SECTION("Copier") {
    bench.OpenMenu(draw);
    const ImGuiWindow* menu = OpenMenuWindow();
    REQUIRE(menu != nullptr);
    bench.MoveTo(FirstEntry(*menu), draw);
    bench.Click(ImGuiMouseButton_Left, draw);
    CHECK(bench.clipboard == "150.10 113.73 -4.00");
  }
  SECTION("Coller, grisé sans trois nombres") {
    bench.clipboard = "bonjour";
    bench.OpenMenu(draw);
    const ImGuiWindow* menu = OpenMenuWindow();
    REQUIRE(menu != nullptr);
    bench.MoveTo(LastEntry(*menu), draw);
    bench.Click(ImGuiMouseButton_Left, draw);
    CHECK_FALSE(pasted);
    CHECK(values[0] == 150.1F);
  }
  SECTION("Coller, actif avec trois nombres") {
    bench.clipboard = "1, 2; 3";
    bench.OpenMenu(draw);
    const ImGuiWindow* menu = OpenMenuWindow();
    REQUIRE(menu != nullptr);
    bench.MoveTo(LastEntry(*menu), draw);
    bench.Click(ImGuiMouseButton_Left, draw);
    CHECK(pasted);
    CHECK(values == std::array<float, 3>{1.0F, 2.0F, 3.0F});
  }
}

TEST_CASE("Éditeur : le clic droit sur la plage d'heures la copie et la colle", "[ui][mapedit]") {
  ClipboardBench bench;
  const ro::ui::SkinArt art = PlainArt();
  ro::ui::MapEditorHours hours{.chosen = true, .from_hour = 20.0F, .to_hour = 6.0F};
  bool toggled = false;
  const auto draw = [&](ImRect& field) {
    ro::ui::WheelDragResult moved;
    toggled |= ro::ui::LitHoursFields(hours, art, moved);
    field = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
  };
  SECTION("Copier") {
    bench.OpenMenu(draw);
    const ImGuiWindow* menu = OpenMenuWindow();
    REQUIRE(menu != nullptr);
    bench.MoveTo(FirstEntry(*menu), draw);
    bench.Click(ImGuiMouseButton_Left, draw);
    CHECK(bench.clipboard == "20:00-06:00");
    CHECK_FALSE(toggled);
  }
  SECTION("Coller, grisé sans plage") {
    bench.clipboard = "1 2 3";
    bench.OpenMenu(draw);
    const ImGuiWindow* menu = OpenMenuWindow();
    REQUIRE(menu != nullptr);
    bench.MoveTo(LastEntry(*menu), draw);
    bench.Click(ImGuiMouseButton_Left, draw);
    CHECK_FALSE(toggled);
    CHECK(hours.from_hour == 20.0F);
  }
  SECTION("Coller choisit la plage, comme le réglage à la main") {
    hours.chosen = false;
    bench.clipboard = "21:30-05:15";
    bench.OpenMenu(draw);
    const ImGuiWindow* menu = OpenMenuWindow();
    REQUIRE(menu != nullptr);
    bench.MoveTo(LastEntry(*menu), draw);
    bench.Click(ImGuiMouseButton_Left, draw);
    CHECK(toggled);
    CHECK(hours.chosen);
    CHECK(hours.from_hour == Approx(21.5F));
    CHECK(hours.to_hour == Approx(5.25F));
  }
}

namespace {

// Une ligne de liste et son menu. `cells` et `move_to` comme la ligne d'une
// lumière : l'intention de collage porte les cases qu'elle reçoit.
void RowFrame(ImRect& field, std::vector<ro::ui::MapEditorIntent>& intents, bool pastes) {
  static_cast<void>(ImGui::Selectable("Lumière 0##lumiere0"));
  field = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
  ro::ui::MapEditorRowMenu menu{.select = ro::ui::map_editor::SelectLight{0},
                                .cells = std::array<float, 3>{10.0F, 20.0F, 1.0F}};
  if (pastes) {
    menu.move_to = [](const std::array<float, 3>& cells) -> ro::ui::MapEditorIntent {
      return ro::ui::map_editor::AddLight{.position = cells};
    };
  }
  ro::ui::DrawMapEditorRowMenu(menu, intents);
}

}  // namespace

TEST_CASE("Éditeur : « Coller la position » d'une ligne suit le presse-papiers", "[ui][mapedit]") {
  ClipboardBench bench;
  std::vector<ro::ui::MapEditorIntent> intents;
  bool pastes = true;
  const auto draw = [&](ImRect& field) { RowFrame(field, intents, pastes); };
  SECTION("grisé sans trois nombres") { bench.clipboard = "20:00-06:00"; }
  SECTION("grisé sans collage, même avec trois nombres") {
    bench.clipboard = "4 5 6";
    pastes = false;
  }
  SECTION("actif : désigner, puis déplacer") { bench.clipboard = "4 5 6"; }
  bench.OpenMenu(draw);
  const ImGuiWindow* menu = OpenMenuWindow();
  REQUIRE(menu != nullptr);
  bench.MoveTo(LastEntry(*menu), draw);
  intents.clear();
  bench.Click(ImGuiMouseButton_Left, draw);
  if (bench.clipboard == "4 5 6" && pastes) {
    REQUIRE(intents.size() == 2);
    CHECK(std::holds_alternative<ro::ui::map_editor::SelectLight>(intents[0]));
    const auto* moved = std::get_if<ro::ui::map_editor::AddLight>(&intents[1]);
    REQUIRE(moved != nullptr);
    CHECK(moved->position == std::array<float, 3>{4.0F, 5.0F, 6.0F});
  } else {
    CHECK(intents.empty());
  }
}
