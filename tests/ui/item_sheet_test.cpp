// LA FICHE D'UN OBJET, dans la fenêtre de description : ses onglets selon ce
// que l'objet porte, et l'onglet retenu qui revient avec l'objet qui l'a.

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/skin_pieces.h"
#include "ro/ui/description_window.h"
#include "ro/ui/ui_context.h"

using ro::ui::DescriptionFetch;
using ro::ui::ItemSheetTab;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr float kPlainPiecePixels = 8.0F;
constexpr int kSettleFrames = 3;
constexpr std::uint32_t kOldCardAlbum = 616;
constexpr std::uint32_t kPoringCard = 4001;
constexpr std::uint32_t kPoring = 1002;

[[nodiscard]] ro::ui::SkinArt PlainArt() {
  ro::ui::SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<ro::render::SkinPiece>(rank),
            ro::ui::SkinImage{.id = 1, .width = kPlainPiecePixels, .height = kPlainPiecePixels});
  }
  return art;
}

struct SheetProbe {
  ro::ui::UiContext ui;
  ro::ui::SkinArt art = PlainArt();
  ro::ui::DescriptionWindowState state;
  std::array<ro::ui::DescriptionOddsRow, 1> odds = {ro::ui::DescriptionOddsRow{
      .id = kPoringCard, .name = "Poring Card", .percent = 100.0, .guaranteed = true}};
  std::array<ro::ui::DescriptionDropRow, 1> drops = {
      ro::ui::DescriptionDropRow{.monster_id = kPoring, .name = "Poring", .rate_hundredths = 1}};
  ro::ui::DescriptionView view;

  SheetProbe() {
    state.open = true;
    state.item_id = kOldCardAlbum;
    view.item_id = kOldCardAlbum;
    view.name = "Old Card Album";
    view.odds = odds;
  }

  ro::ui::DescriptionWindowAction Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    ro::ui::DescriptionWindowAction action = ro::ui::DrawDescriptionWindow(state, view, art);
    ui.EndFrame();
    return action;
  }

  void Settle() {
    for (int frame = 0; frame < kSettleFrames; ++frame) {
      static_cast<void>(Frame());
    }
  }
};

// La barre d'onglets de la fiche, dans la fenêtre de description ; nulle quand
// la dernière image n'en a pas posé.
[[nodiscard]] ImGuiTabBar* SheetTabBar() {
  const ImGuiWindow* const window = ImGui::FindWindowByName("###description");
  if (window == nullptr) {
    return nullptr;
  }
  ImGuiTabBar* const bar = GImGui->TabBars.GetByKey(ImHashStr("##item_sheet_tabs", 0, window->ID));
  return bar != nullptr && bar->CurrFrameVisible == GImGui->FrameCount ? bar : nullptr;
}

// L'onglet de la dernière image dont le libellé finit par `suffix` ; nul sinon.
[[nodiscard]] ImGuiTabItem* SheetTab(std::string_view suffix) {
  ImGuiTabBar* const bar = SheetTabBar();
  if (bar == nullptr) {
    return nullptr;
  }
  for (ImGuiTabItem& tab : bar->Tabs) {
    if (tab.LastFrameVisible == bar->CurrFrameVisible &&
        std::string_view(ImGui::TabBarGetTabName(bar, &tab)).ends_with(suffix)) {
      return &tab;
    }
  }
  return nullptr;
}

[[nodiscard]] std::string VisibleSheetTab() {
  ImGuiTabBar* const bar = SheetTabBar();
  if (bar == nullptr) {
    return {};
  }
  ImGuiTabItem* const tab = ImGui::TabBarFindTabByID(bar, bar->VisibleTabId);
  return tab != nullptr ? ImGui::TabBarGetTabName(bar, tab) : std::string();
}

}  // namespace

TEST_CASE("Fiche d'objet : les onglets suivent ce que l'objet porte", "[ui][item_sheet]") {
  SheetProbe probe;
  probe.Settle();
  // Un conteneur hors ligne, sans base : « Description » et « Probabilités ».
  CHECK(SheetTab("###sheet_description") != nullptr);
  CHECK(SheetTab("###sheet_odds") != nullptr);
  CHECK(SheetTab("###sheet_monsters") == nullptr);
  CHECK(SheetTab("###sheet_general") == nullptr);

  // EN LIGNE : les quatre onglets du serveur. Une réponse vide grise le sien,
  // qui reste à sa place.
  probe.view.server_tabs = true;
  probe.view.online = true;
  probe.view.drops_ordinary = {.fetch = DescriptionFetch::kReady, .rows = probe.drops};
  probe.view.drops_boss = {.fetch = DescriptionFetch::kReady};
  probe.view.scripts = {.fetch = DescriptionFetch::kWaiting};
  probe.view.sellers_phase = ro::ui::ItemSheetSellersPhase::kAsking;
  probe.Settle();
  for (const std::string_view tab : {"###sheet_monsters", "###sheet_boss", "###sheet_script",
                                     "###sheet_combos", "###sheet_sellers"}) {
    CHECK(SheetTab(tab) != nullptr);
  }
  CHECK(ro::ui::ItemSheetTabOffered(ItemSheetTab::kMonsters, probe.view));
  CHECK_FALSE(ro::ui::ItemSheetTabOffered(ItemSheetTab::kBoss, probe.view));
  // En attendant la réponse, rien ne se grise.
  CHECK(ro::ui::ItemSheetTabOffered(ItemSheetTab::kScript, probe.view));

  // SANS RIEN D'AUTRE QUE LA DESCRIPTION, pas de barre du tout.
  probe.view = ro::ui::DescriptionView{.item_id = kOldCardAlbum, .name = "Old Card Album"};
  probe.Settle();
  CHECK(SheetTab("###sheet_description") == nullptr);
}

TEST_CASE("Fiche d'objet : « Probabilités » choisi revient au retour sur le conteneur",
          "[ui][item_sheet]") {
  SheetProbe probe;
  probe.state.sheet_tab = ItemSheetTab::kOdds;
  probe.Settle();
  CHECK(VisibleSheetTab().ends_with("###sheet_odds"));
  // Un objet ordinaire, en ligne : « Description » est montré, le choix reste.
  probe.view.odds = {};
  probe.view.server_tabs = true;
  probe.Settle();
  CHECK(VisibleSheetTab().ends_with("###sheet_description"));
  CHECK(probe.state.sheet_tab == ItemSheetTab::kOdds);
  // Le retour au conteneur rouvre « Probabilités ».
  probe.view.odds = probe.odds;
  probe.Settle();
  CHECK(VisibleSheetTab().ends_with("###sheet_odds"));
}

TEST_CASE("Fiche d'objet : le corps qui défile seul ne laisse rien à faire défiler à la fenêtre",
          "[ui][item_sheet]") {
  SheetProbe probe;
  // Un cadre plus épais que la marge de la fenêtre, comme celui du jeu.
  constexpr float kThickFramePixels = 24.0F;
  probe.art.Set(
      ro::render::SkinPiece::kFrameLeft,
      ro::ui::SkinImage{.id = 1, .width = kThickFramePixels, .height = kPlainPiecePixels});
  // Plus de lignes qu'aucune hauteur n'en montre : l'ascenseur est celui du corps.
  constexpr std::size_t kManyRows = 400;
  std::vector<ro::ui::DescriptionOddsRow> rows(kManyRows, probe.odds[0]);
  probe.view.odds = rows;
  probe.state.sheet_tab = ItemSheetTab::kOdds;
  probe.Settle();

  const auto left_to_scroll = [] {
    const ImGuiWindow* const window = ImGui::FindWindowByName("###description");
    REQUIRE(window != nullptr);
    return window->ScrollMax.y;
  };
  CHECK(left_to_scroll() == 0.0F);

  // Le corps s'arrête au-dessus du bas du cadre.
  const ImGuiWindow* const window = ImGui::FindWindowByName("###description");
  REQUIRE(window != nullptr);
  REQUIRE(!window->DC.ChildWindows.empty());
  const ImGuiWindow* const body = window->DC.ChildWindows[0];
  CHECK(body->Pos.y + body->Size.y <= window->Pos.y + window->Size.y - kThickFramePixels);

  // 🔴 QUELLE QUE SOIT SA TAILLE, et après un état où la fenêtre défilait
  // réellement : un objet sans onglets au texte trop long pour elle.
  constexpr float kWidthPixels = 400.0F;
  constexpr std::array<float, 3> kHeightsPixels = {180.0F, 333.0F, 620.0F};
  for (const float height_pixels : kHeightsPixels) {
    ImGui::SetWindowSize("###description", ImVec2(kWidthPixels, height_pixels));
    probe.Settle();
    CHECK(left_to_scroll() == 0.0F);
  }

  const std::vector<std::string> long_text(kManyRows, "Une ligne de description.");
  probe.view.lines = long_text;
  probe.view.odds = {};
  probe.Settle();
  CHECK(left_to_scroll() > 0.0F);
  probe.view.odds = rows;
  probe.Settle();
  CHECK(left_to_scroll() == 0.0F);
}

TEST_CASE("Fiche d'objet : les sources de la base le disent, et un monstre se suit",
          "[ui][item_sheet]") {
  SheetProbe probe;
  probe.view.odds = {};
  probe.view.server_tabs = true;
  probe.view.drops_ordinary = {
      .fetch = DescriptionFetch::kReady, .rows = probe.drops, .from_database = true};
  probe.state.sheet_tab = ItemSheetTab::kMonsters;
  probe.Settle();
  REQUIRE(VisibleSheetTab().ends_with("###sheet_monsters"));
  // La souris balaie le corps de l'onglet jusqu'au lien du monstre.
  const ImGuiWindow* body = nullptr;
  for (ImGuiWindow* window : GImGui->Windows) {
    if (window->WasActive &&
        std::string_view(window->Name).find("item_sheet_body") != std::string_view::npos) {
      body = window;
    }
  }
  REQUIRE(body != nullptr);
  constexpr float kStepPixels = 3.0F;
  bool hovered = false;
  for (float y = body->Pos.y; y < body->Pos.y + body->Size.y && !hovered; y += kStepPixels) {
    for (float x = body->Pos.x; x < body->Pos.x + body->Size.x && !hovered; x += kStepPixels * 4) {
      ImGui::GetIO().AddMousePosEvent(x, y);
      const ro::ui::DescriptionWindowAction action = probe.Frame();
      hovered = action.link.hovered_id == kPoring &&
                action.link.hovered_kind == ro::ui::ChatLinkKind::kMonster;
    }
  }
  REQUIRE(hovered);
  // Le clic gauche demande la fiche du monstre, comme tout lien de monstre.
  ImGuiIO& io = ImGui::GetIO();
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  static_cast<void>(probe.Frame());
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  const ro::ui::DescriptionWindowAction clicked = probe.Frame();
  CHECK(clicked.link.gesture == ro::ui::ChatLinkGesture::kDescribe);
  CHECK(clicked.link.kind == ro::ui::ChatLinkKind::kMonster);
  CHECK(clicked.link.item_id == kPoring);
}

TEST_CASE("Fiche d'objet : « Map Drops » suit ce que la base sait", "[ui][item_sheet]") {
  SheetProbe probe;
  // La base ne connaît pas l'objet : l'onglet n'existe pas.
  probe.Settle();
  CHECK(SheetTab("###sheet_map_drops") == nullptr);

  // Connu, sans ligne : il existe, grisé.
  probe.view.map_drops_known = true;
  probe.Settle();
  CHECK(SheetTab("###sheet_map_drops") != nullptr);
  CHECK_FALSE(ro::ui::ItemSheetTabOffered(ItemSheetTab::kMapDrops, probe.view));

  // Avec une ligne, il s'offre et se dessine.
  const std::array<ro::ui::DescriptionMapDropRow, 1> rows = {
      ro::ui::DescriptionMapDropRow{.monster_id = kPoring,
                                    .monster_name = "Poring",
                                    .map_name = "gef_d01_i",
                                    .map_label = "Geffen Dungeon (gef_d01_i)",
                                    .rate_thousandths = 5,
                                    .random_options = true}};
  probe.view.map_drops = rows;
  probe.state.sheet_tab = ItemSheetTab::kMapDrops;
  probe.Settle();
  CHECK(ro::ui::ItemSheetTabOffered(ItemSheetTab::kMapDrops, probe.view));
  CHECK(probe.state.sheet_tab == ItemSheetTab::kMapDrops);
}
