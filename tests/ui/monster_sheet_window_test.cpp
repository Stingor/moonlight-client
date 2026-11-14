// LA FICHE D'UN MONSTRE, dessinée hors jeu.
//
// Ce qui se vérifie sans écran : fermée elle ne dessine rien, chaque phase se
// dessine sans rien rendre, le cadre du sprite se relève pour `app`, un clic
// dessus est une chatouille — pas une commande —, une carte d'apparition rend
// ses trois gestes de lieu, et l'aperçu au survol porte sa ligne de pied.

#include "ro/ui/monster_sheet_window.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/skin_pieces.h"
#include "ro/ui/ui_context.h"

using ro::render::SkinPiece;
using ro::ui::MonsterSheetAnswer;
using ro::ui::MonsterSheetPhase;
using ro::ui::MonsterSheetView;
using ro::ui::MonsterSheetWindowState;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr float kPlainPiecePixels = 8.0F;
constexpr std::uint32_t kPoring = 1002;
constexpr std::uint32_t kJellopy = 909;
constexpr std::uint32_t kRuby = 723;
// Le cadre du sprite, en pixels.
constexpr float kSpriteBoxWidthPixels = 116.0F;
// Un point bien à l'intérieur du cadre, depuis son coin.
constexpr float kInsideSpritePixels = 20.0F;
// Un point dans le libellé d'une cellule, depuis son bord gauche.
constexpr float kInsideCellPixels = 10.0F;
// Le pas du balayage de la barre de titre.
constexpr float kSweepStepPixels = 3.0F;

[[nodiscard]] ro::ui::SkinArt PlainArt() {
  ro::ui::SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<SkinPiece>(rank),
            ro::ui::SkinImage{.id = 1, .width = kPlainPiecePixels, .height = kPlainPiecePixels});
  }
  return art;
}

struct SheetProbe {
  ro::ui::UiContext ui;
  ro::ui::SkinArt art = PlainArt();
  MonsterSheetWindowState state;
  std::array<ro::ui::MonsterSheetDropView, 1> drops = {ro::ui::MonsterSheetDropView{
      .item_id = kJellopy, .name = "Jellopy", .rate_hundredths = 7000}};
  std::array<ro::ui::MonsterSheetSpawnView, 1> spawns = {ro::ui::MonsterSheetSpawnView{
      .map_name = "prt_fild08", .label = "Prontera Field (prt_fild08)", .count = 60}};
  MonsterSheetView view;

  SheetProbe() {
    state.open = true;
    view.phase = MonsterSheetPhase::kReady;
    view.shown_id = kPoring;
    view.name = "Poring";
    view.level = 1;
    view.hp = 50;
    view.namesake_count = 3;
    view.namesake_origin = kPoring - 1;
    view.aegis_name = "PORING";
    view.sprite_missing = true;
    view.exp_gain = ro::ui::MonsterSheetExpView{.base_exp = 10, .next_base_exp = 100};
    view.own = ro::ui::MonsterSheetOwnView{.hit = 50, .flee = 40};
    view.drops = drops;
    view.spawns = spawns;
  }

  MonsterSheetAnswer Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    MonsterSheetAnswer answer = ro::ui::DrawMonsterSheetWindow(state, view, art);
    ui.EndFrame();
    return answer;
  }

  void Settle() {
    for (int frame = 0; frame < 4; ++frame) {
      static_cast<void>(Frame());
    }
  }
};

}  // namespace

TEST_CASE("Fiche de monstre : fermée, rien ne se dessine", "[ui][monster_sheet]") {
  SheetProbe probe;
  probe.state.open = false;
  probe.Settle();
  CHECK(ImGui::FindWindowByName("###monster_sheet") == nullptr);
}

TEST_CASE("Fiche de monstre : chaque phase se dessine sans rien demander", "[ui][monster_sheet]") {
  SheetProbe probe;
  for (const MonsterSheetPhase phase :
       {MonsterSheetPhase::kAsking, MonsterSheetPhase::kUnknown, MonsterSheetPhase::kReady}) {
    probe.view.phase = phase;
    for (int frame = 0; frame < 3; ++frame) {
      const MonsterSheetAnswer answer = probe.Frame();
      CHECK_FALSE(answer.poked);
      CHECK(answer.open_monster == 0);
      CHECK(answer.guide_map.empty());
      CHECK(answer.link_map.empty());
      CHECK(answer.item.gesture == ro::ui::ChatLinkGesture::kNothing);
    }
  }
  CHECK(ImGui::FindWindowByName("###monster_sheet") != nullptr);
}

TEST_CASE("Fiche de monstre : le titre porte le nom, une fois la fiche arrivée",
          "[ui][monster_sheet]") {
  SheetProbe probe;
  probe.view.phase = MonsterSheetPhase::kAsking;
  static_cast<void>(probe.Frame());
  CHECK(probe.state.title.find("Poring") == std::string::npos);
  probe.view.phase = MonsterSheetPhase::kReady;
  static_cast<void>(probe.Frame());
  CHECK(probe.state.title.starts_with("Poring###"));
}

TEST_CASE("Fiche de monstre : le cadre du sprite se relève, et un clic le titille",
          "[ui][monster_sheet]") {
  SheetProbe probe;
  probe.Settle();
  const float scale = ImGui::GetStyle().FontScaleMain;
  CHECK(probe.state.sprite_box_width == kSpriteBoxWidthPixels * scale);

  const ImGuiWindow* window = ImGui::FindWindowByName("###monster_sheet");
  REQUIRE(window != nullptr);
  const ImVec2 corner(window->ContentRegionRect.Min.x, window->ContentRegionRect.Min.y);
  ImGuiIO& io = ImGui::GetIO();
  io.AddMousePosEvent(corner.x + kInsideSpritePixels, corner.y + kInsideSpritePixels);
  static_cast<void>(probe.Frame());
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  const MonsterSheetAnswer pressed = probe.Frame();
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  static_cast<void>(probe.Frame());
  CHECK(pressed.poked);
}

TEST_CASE("Fiche de monstre : une carte d'apparition se survole, se guide et se linke",
          "[ui][monster_sheet]") {
  SheetProbe probe;
  probe.Settle();
  // L'ONGLET DES APPARITIONS, choisi par la barre elle-même.
  ImGuiTabBar* tabs = nullptr;
  ImGuiTabItem* spawns_tab = nullptr;
  for (int index = 0; index < GImGui->TabBars.GetMapSize(); ++index) {
    if (ImGuiTabBar* bar = GImGui->TabBars.TryGetMapData(index); bar != nullptr) {
      if (ImGuiTabItem* tab = ImGui::TabBarFindTabByID(bar, ImHashStr("###mi_spawns", 0, bar->ID));
          tab != nullptr) {
        tabs = bar;
        spawns_tab = tab;
      }
    }
  }
  REQUIRE(spawns_tab != nullptr);
  ImGui::TabBarQueueFocus(tabs, spawns_tab);
  probe.Settle();

  // LA TABLE DESSINÉE À LA DERNIÈRE IMAGE, et sa première ligne sous l'en-tête.
  const ImGuiTable* table = nullptr;
  for (int index = 0; index < GImGui->Tables.GetMapSize(); ++index) {
    if (const ImGuiTable* found = GImGui->Tables.TryGetMapData(index);
        found != nullptr && found->LastFrameActive == GImGui->FrameCount) {
      table = found;
    }
  }
  REQUIRE(table != nullptr);
  CHECK(tabs->SelectedTabId == spawns_tab->ID);
  const ImGuiStyle& style = ImGui::GetStyle();
  const float row_height = GImGui->FontSize + (2.0F * style.CellPadding.y);
  constexpr float kFirstRowMiddleRows = 1.5F;
  ImGuiIO& io = ImGui::GetIO();
  io.AddMousePosEvent(table->Columns[0].MinX + style.CellPadding.x + kInsideCellPixels,
                      table->OuterRect.Min.y + (kFirstRowMiddleRows * row_height));
  static_cast<void>(probe.Frame());
  const MonsterSheetAnswer hovered = probe.Frame();
  CHECK(hovered.hovered_map == "prt_fild08");
  CHECK(hovered.guide_map.empty());
  CHECK(hovered.link_map.empty());

  // LE CLIC GUIDE.
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  static_cast<void>(probe.Frame());
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  const MonsterSheetAnswer clicked = probe.Frame();
  CHECK(clicked.guide_map == "prt_fild08");
  CHECK(clicked.link_map.empty());

  // LE MAJ+CLIC LINKE, et ne guide pas.
  io.AddKeyEvent(ImGuiMod_Shift, true);
  static_cast<void>(probe.Frame());
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  static_cast<void>(probe.Frame());
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  const MonsterSheetAnswer shifted = probe.Frame();
  io.AddKeyEvent(ImGuiMod_Shift, false);
  static_cast<void>(probe.Frame());
  CHECK(shifted.link_map == "prt_fild08");
  CHECK(shifted.guide_map.empty());
}

TEST_CASE("Fiche de monstre : la colonne « Type » des drops tient « Récompense MVP »",
          "[ui][monster_sheet]") {
  SheetProbe probe;
  const std::array<ro::ui::MonsterSheetDropView, 2> drops = {
      ro::ui::MonsterSheetDropView{.item_id = kJellopy, .name = "Jellopy", .rate_hundredths = 7000},
      ro::ui::MonsterSheetDropView{
          .item_id = kRuby, .name = "Ruby", .rate_hundredths = 100, .mvp_reward = true}};
  probe.view.drops = drops;
  probe.Settle();
  ImGuiTabBar* tabs = nullptr;
  ImGuiTabItem* drops_tab = nullptr;
  for (int index = 0; index < GImGui->TabBars.GetMapSize(); ++index) {
    if (ImGuiTabBar* bar = GImGui->TabBars.TryGetMapData(index); bar != nullptr) {
      if (ImGuiTabItem* tab = ImGui::TabBarFindTabByID(bar, ImHashStr("###mi_drops", 0, bar->ID));
          tab != nullptr) {
        tabs = bar;
        drops_tab = tab;
      }
    }
  }
  REQUIRE(drops_tab != nullptr);
  ImGui::TabBarQueueFocus(tabs, drops_tab);
  probe.Settle();
  // La table des drops, dessinée à la dernière image : trois colonnes.
  const ImGuiTable* table = nullptr;
  for (int index = 0; index < GImGui->Tables.GetMapSize(); ++index) {
    if (const ImGuiTable* found = GImGui->Tables.TryGetMapData(index);
        found != nullptr && found->LastFrameActive == GImGui->FrameCount &&
        found->ColumnsCount == 3) {
      table = found;
    }
  }
  REQUIRE(table != nullptr);
  // 🔴 LE LIBELLÉ ENTIER TIENT DANS SA COLONNE À L'OUVERTURE, et la colonne se
  // règle à la souris.
  probe.ui.BeginFrame(kViewport, kFrameSeconds);
  const float label_width = ImGui::CalcTextSize("Récompense MVP").x;
  probe.ui.EndFrame();
  CHECK(table->Columns[2].WidthGiven >= label_width);
  CHECK((table->Flags & ImGuiTableFlags_Resizable) != 0);
}

TEST_CASE("Fiche de monstre : « Signaler un bug » dans la barre de titre, selon le réglage",
          "[ui][monster_sheet]") {
  SheetProbe probe;
  probe.Settle();
  const ImGuiWindow* window = ImGui::FindWindowByName("###monster_sheet");
  REQUIRE(window != nullptr);
  const ImGuiID button = ImHashStr("Signaler un bug", 0, window->ID);
  // La souris balaie la barre de titre jusqu'au bouton : vrai quand elle y est.
  const auto hover_button = [&] {
    ImGuiIO& io = ImGui::GetIO();
    for (float x = window->Pos.x; x < window->Pos.x + window->Size.x; x += kSweepStepPixels) {
      io.AddMousePosEvent(x, window->Pos.y + (window->TitleBarHeight * 0.5F));
      static_cast<void>(probe.Frame());
      if (GImGui->HoveredId == button) {
        return true;
      }
    }
    return false;
  };
  // Réglage éteint : pas de bouton.
  CHECK_FALSE(hover_button());
  // Allumé : le bouton de la barre rend la demande.
  probe.state.bug_report = true;
  probe.Settle();
  REQUIRE(hover_button());
  ImGuiIO& io = ImGui::GetIO();
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  const MonsterSheetAnswer pressed = probe.Frame();
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  const MonsterSheetAnswer released = probe.Frame();
  CHECK((pressed.report_bug || released.report_bug));
  // Rien d'autre n'est demandé.
  CHECK_FALSE(released.open_database);
  CHECK_FALSE(released.post_link);
}

TEST_CASE("Aperçu de monstre : la ligne de pied se dit à toute phase", "[ui][monster_sheet]") {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art = PlainArt();
  ro::ui::MonsterSheetPreviewState state;
  ro::ui::MonsterSheetPreview preview;
  preview.shown_id = kPoring;
  preview.name = "Poring";
  constexpr std::string_view kHint = "Maj+clic : lien dans le chat";
  const auto logged_of = [&] {
    ui.BeginFrame(kViewport, kFrameSeconds);
    ImGui::LogToBuffer();
    ImGui::Begin("banc_apercu");
    ro::ui::DrawMonsterSheetPreview(state, preview, art);
    ImGui::End();
    std::string logged = ImGui::GetCurrentContext()->LogBuffer.c_str();
    ImGui::LogFinish();
    ui.EndFrame();
    return logged;
  };
  for (const MonsterSheetPhase phase :
       {MonsterSheetPhase::kAsking, MonsterSheetPhase::kUnknown, MonsterSheetPhase::kReady}) {
    preview.phase = phase;
    preview.hint.clear();
    CHECK(logged_of().find(kHint) == std::string::npos);
    preview.hint = kHint;
    CHECK(logged_of().find(kHint) != std::string::npos);
  }
}

TEST_CASE("Fiche de monstre : l'onglet des map drops n'existe qu'avec des lignes",
          "[ui][monster_sheet]") {
  const auto tab_exists = [] {
    for (int index = 0; index < GImGui->TabBars.GetMapSize(); ++index) {
      if (ImGuiTabBar* bar = GImGui->TabBars.TryGetMapData(index);
          bar != nullptr &&
          ImGui::TabBarFindTabByID(bar, ImHashStr("###mi_map_drops", 0, bar->ID)) != nullptr) {
        return true;
      }
    }
    return false;
  };
  SheetProbe probe;
  probe.Settle();
  CHECK_FALSE(tab_exists());

  const std::array<ro::ui::MonsterSheetMapDropView, 1> map_drops = {
      ro::ui::MonsterSheetMapDropView{.item_id = kJellopy,
                                      .name = "Jellopy",
                                      .map_name = "gef_d01_i",
                                      .map_label = "Geffen Dungeon (gef_d01_i)",
                                      .rate_thousandths = 5}};
  probe.view.map_drops = map_drops;
  probe.Settle();
  CHECK(tab_exists());
}
