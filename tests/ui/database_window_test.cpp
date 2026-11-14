// LA FENÊTRE « BASE DE DONNÉES », sur de vraies images ImGui : le fil des
// fiches et ses deux flèches, l'ouverture sur une fiche, la liste dont un clic
// visite la ligne et dont les autres gestes sont ceux de tout objet ou de tout
// monstre, la fiche d'un objet — celle de la description — et ses onglets, ses
// liens vers ses monstres, le bouton « Marché », la fiche de monstre que le
// panneau de droite emprunte, et le texte des listes de filtre fermées.

#include "ro/ui/database_window.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/skin_pieces.h"
#include "ro/ui/ui_context.h"

using ro::ui::ChatLinkGesture;
using ro::ui::ChatLinkKind;
using ro::ui::DatabaseAnswer;
using ro::ui::DatabaseHistory;
using ro::ui::DatabaseKind;
using ro::ui::DatabaseTarget;
using ro::ui::DatabaseView;
using ro::ui::DatabaseWindowState;
using ro::ui::DescriptionFetch;
using ro::ui::ItemSheetTab;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 900.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr float kPlainPiecePixels = 8.0F;
constexpr int kSettleFrames = 4;
constexpr float kSweepStepPixels = 3.0F;
constexpr float kRowInsetPixels = 20.0F;
constexpr ImTextureID kSkinTexture = 1;

constexpr std::uint32_t kPoring = 1002;
constexpr std::uint32_t kBaphomet = 1039;
constexpr std::uint32_t kJellopy = 909;
constexpr std::uint32_t kSword = 1101;

constexpr DatabaseTarget kJellopySheet{.kind = DatabaseKind::kItem, .id = kJellopy};
constexpr DatabaseTarget kSwordSheet{.kind = DatabaseKind::kItem, .id = kSword};
constexpr DatabaseTarget kPoringSheet{.kind = DatabaseKind::kMonster, .id = kPoring};

[[nodiscard]] ro::ui::SkinArt PlainArt() {
  ro::ui::SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<ro::render::SkinPiece>(rank),
            ro::ui::SkinImage{
                .id = kSkinTexture, .width = kPlainPiecePixels, .height = kPlainPiecePixels});
  }
  return art;
}

struct DatabaseProbe {
  ro::ui::UiContext ui;
  ro::ui::SkinArt art = PlainArt();
  DatabaseWindowState state;
  ro::ui::MonsterSheetWindowState monster_state;
  std::array<ro::ui::DatabaseRowView, 2> rows = {{
      {.id = kJellopy, .name = "Jellopy", .note = "Divers"},
      {.id = kSword, .name = "Sword", .note = "Arme"},
  }};
  std::array<ro::ui::ItemSheetFact, 1> facts = {{{.label = "Poids", .value = "1"}}};
  std::array<ro::ui::DescriptionDropRow, 1> ordinary = {{
      {.monster_id = kPoring, .name = "Poring", .rate_hundredths = 7000},
  }};
  std::array<ro::ui::DescriptionDropRow, 1> boss = {{
      {.monster_id = kBaphomet,
       .name = "Baphomet",
       .boss_rank = 2,
       .mvp_reward = true,
       .rate_hundredths = 100},
  }};
  // LA FICHE D'UN OBJET : celle de la description, telle que la base la
  // remplit — faits, sources de repli, scripts de la base.
  ro::ui::DescriptionView item;
  ro::ui::MonsterSheetView monster;
  DatabaseView view;

  DatabaseProbe() {
    item.item_id = kJellopy;
    item.name = "Jellopy";
    item.in_database = true;
    item.facts = facts;
    item.server_tabs = true;
    item.drops_ordinary = {
        .fetch = DescriptionFetch::kReady, .rows = ordinary, .from_database = true};
    item.drops_boss = {.fetch = DescriptionFetch::kReady, .rows = boss, .from_database = true};
    item.scripts = {.fetch = DescriptionFetch::kReady, .from_database = true};
    item.sellers_phase = ro::ui::ItemSheetSellersPhase::kReady;
    view.rows = rows;
    view.summary = "2 objets";
    view.item = &item;
    monster.phase = ro::ui::MonsterSheetPhase::kReady;
    monster.shown_id = kPoring;
    monster.name = "Poring";
    ro::ui::OpenDatabaseOn(state, kJellopySheet);
  }

  DatabaseAnswer Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    const DatabaseAnswer answer = ro::ui::DrawDatabaseWindow(state, view, monster_state, art);
    ui.EndFrame();
    return answer;
  }

  void Settle() {
    for (int frame = 0; frame < kSettleFrames; ++frame) {
      static_cast<void>(Frame());
    }
  }

  // Un clic de ce bouton là où la souris se trouve : l'enfoncement et le
  // relâchement réunis. Ce que l'un ou l'autre a demandé est rendu.
  DatabaseAnswer Click(ImGuiMouseButton button = ImGuiMouseButton_Left) {
    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseButtonEvent(button, true);
    const DatabaseAnswer pressed = Frame();
    io.AddMouseButtonEvent(button, false);
    DatabaseAnswer released = Frame();
    released.item.search_market = released.item.search_market || pressed.item.search_market;
    released.item.report_bug = released.item.report_bug || pressed.item.report_bug;
    released.monster.report_bug = released.monster.report_bug || pressed.monster.report_bug;
    return released;
  }

  // Le même clic, ce modificateur tenu.
  DatabaseAnswer ClickWith(ImGuiKey modifier, ImGuiMouseButton button) {
    ImGuiIO& io = ImGui::GetIO();
    io.AddKeyEvent(modifier, true);
    static_cast<void>(Frame());
    DatabaseAnswer answer = Click(button);
    io.AddKeyEvent(modifier, false);
    static_cast<void>(Frame());
    return answer;
  }
};

[[nodiscard]] ImGuiWindow* WindowNamed(std::string_view part) {
  for (ImGuiWindow* window : GImGui->Windows) {
    const std::string_view name(window->Name);
    if (window->WasActive && name.find(part) != std::string_view::npos) {
      return window;
    }
  }
  return nullptr;
}

// L'identifiant d'un élément posé dans une table d'une fenêtre : la table pousse
// le sien, la ligne s'y nomme.
[[nodiscard]] ImGuiID IdInTable(const ImGuiWindow* window, const char* table, const char* label) {
  const ImGuiID table_id = ImHashStr(table, 0, window->ID);
  return ImHashStr(label, 0, table_id);
}

// La souris balaie la fenêtre, de haut en bas puis de gauche à droite, jusqu'à
// ce que `reached` le dise. Rend vrai quand elle y est.
[[nodiscard]] bool SweepUntil(DatabaseProbe& probe, const ImGuiWindow* window, bool across,
                              const std::function<bool(const DatabaseAnswer&)>& reached) {
  const float right =
      across ? window->Pos.x + window->Size.x : window->Pos.x + kRowInsetPixels + 1.0F;
  for (float x = window->Pos.x + kRowInsetPixels; x < right; x += kSweepStepPixels * 4.0F) {
    for (float y = window->Pos.y; y < window->Pos.y + window->Size.y; y += kSweepStepPixels) {
      ImGui::GetIO().AddMousePosEvent(x, y);
      if (reached(probe.Frame())) {
        return true;
      }
    }
  }
  return false;
}

// La souris jusqu'à survoler l'élément `wanted`.
[[nodiscard]] bool HoverOver(DatabaseProbe& probe, const ImGuiWindow* window, ImGuiID wanted,
                             bool across) {
  return SweepUntil(probe, window, across,
                    [wanted](const DatabaseAnswer&) { return GImGui->HoveredId == wanted; });
}

// ── LES ONGLETS DE LA FICHE D'UN OBJET ──────────────────────────────────────

// La barre d'onglets de la fiche d'un objet, telle que la dernière image l'a
// posée.
[[nodiscard]] ImGuiTabBar* ItemTabBar() {
  const ImGuiWindow* const sheet = WindowNamed("db_sheet_child");
  if (sheet == nullptr) {
    return nullptr;
  }
  return GImGui->TabBars.GetByKey(ImHashStr("##item_sheet_tabs", 0, sheet->ID));
}

// L'onglet de la dernière image dont le libellé finit par `suffix` ; nul sinon.
[[nodiscard]] ImGuiTabItem* TabEndingWith(ImGuiTabBar* bar, std::string_view suffix) {
  for (ImGuiTabItem& tab : bar->Tabs) {
    if (tab.LastFrameVisible == bar->CurrFrameVisible &&
        std::string_view(ImGui::TabBarGetTabName(bar, &tab)).ends_with(suffix)) {
      return &tab;
    }
  }
  return nullptr;
}

[[nodiscard]] std::string VisibleTab(ImGuiTabBar* bar) {
  ImGuiTabItem* const tab = ImGui::TabBarFindTabByID(bar, bar->VisibleTabId);
  return tab != nullptr ? ImGui::TabBarGetTabName(bar, tab) : std::string();
}

// Un clic sur l'onglet dont le libellé finit par `suffix` ; rend ce que la
// fenêtre a répondu.
DatabaseAnswer ClickTab(DatabaseProbe& probe, std::string_view suffix) {
  ImGuiTabBar* const bar = ItemTabBar();
  REQUIRE(bar != nullptr);
  const ImGuiTabItem* const tab = TabEndingWith(bar, suffix);
  REQUIRE(tab != nullptr);
  const ImGuiWindow* const sheet = WindowNamed("db_sheet_child");
  REQUIRE(HoverOver(probe, sheet, tab->ID, true));
  DatabaseAnswer answer = probe.Click();
  probe.Settle();
  return answer;
}

// La souris sur la ligne de la liste qui porte ce libellé.
void HoverRow(DatabaseProbe& probe, const char* label) {
  const ImGuiWindow* list = WindowNamed("db_list_child");
  REQUIRE(list != nullptr);
  REQUIRE(HoverOver(probe, list, IdInTable(list, "##db_rows", label), false));
}

}  // namespace

TEST_CASE("Base de données : le fil des fiches se parcourt comme un navigateur", "[ui][database]") {
  DatabaseHistory history;
  CHECK_FALSE(history.current().has_value());
  CHECK_FALSE(history.can_go_back());
  history.Visit(kJellopySheet);
  history.Visit(kSwordSheet);
  history.Visit(kPoringSheet);
  // La même fiche ne s'empile pas deux fois.
  history.Visit(kPoringSheet);
  CHECK(history.current() == kPoringSheet);
  REQUIRE(history.Back());
  REQUIRE(history.Back());
  CHECK(history.current() == kJellopySheet);
  CHECK_FALSE(history.Back());
  CHECK(history.Forward());
  CHECK(history.current() == kSwordSheet);
  // Une fiche ouverte au milieu du fil en coupe la suite.
  history.Visit(kJellopySheet);
  CHECK_FALSE(history.can_go_forward());
  CHECK(history.Back());
  CHECK(history.current() == kSwordSheet);
}

TEST_CASE("Base de données : le fil ne retient que les fiches les plus récentes",
          "[ui][database]") {
  DatabaseHistory history;
  for (std::uint32_t id = 1; id <= ro::ui::kDatabaseHistoryMost + 10; ++id) {
    history.Visit(DatabaseTarget{.kind = DatabaseKind::kItem, .id = id});
  }
  std::size_t steps = 0;
  while (history.Back()) {
    ++steps;
  }
  CHECK(steps == ro::ui::kDatabaseHistoryMost - 1);
  CHECK(history.current()->id == 11);
}

TEST_CASE("Base de données : ouvrir sur une fiche montre son onglet", "[ui][database]") {
  DatabaseWindowState state;
  ro::ui::OpenDatabaseOn(state, kPoringSheet);
  CHECK(state.open);
  CHECK(state.raise);
  CHECK(state.tab == ro::ui::kDatabaseMonsterTab);
  CHECK(state.select_tab);
  CHECK(state.scroll_to_shown);
  CHECK(state.history.current() == kPoringSheet);
  // Sans fiche, la fenêtre s'ouvre et garde ce qu'elle montrait.
  ro::ui::OpenDatabaseOn(state, std::nullopt);
  CHECK(state.history.current() == kPoringSheet);
}

TEST_CASE("Base de données : un clic dans la liste visite la ligne", "[ui][database]") {
  DatabaseProbe probe;
  probe.Settle();
  HoverRow(probe, "#1101 Sword##db_row1");
  static_cast<void>(probe.Click());
  CHECK(probe.state.history.current() == kSwordSheet);
  CHECK(probe.state.history.can_go_back());
}

// ── LES GESTES DE TOUT OBJET SUR UNE LIGNE DE LA LISTE ──────────────────────

TEST_CASE("Base de données : le clic droit sur une ligne ouvre le menu des liens",
          "[ui][database]") {
  DatabaseProbe probe;
  probe.Settle();
  HoverRow(probe, "#1101 Sword##db_row1");
  const DatabaseAnswer hovered = probe.Frame();
  CHECK(hovered.list.hovered_id == kSword);
  CHECK(hovered.list.hovered_kind == ChatLinkKind::kItem);
  static_cast<void>(probe.Click(ImGuiMouseButton_Right));
  // LE MENU COMMUN DES LIENS, celui du sac et du chat, sur l'objet de la ligne.
  const ImGuiWindow* const window = WindowNamed("###database");
  REQUIRE(window != nullptr);
  CHECK(ImGui::IsPopupOpen(ImHashStr("##database_list_menu", 0, window->ID), ImGuiPopupFlags_None));
  CHECK(probe.state.list_menu.kind == ChatLinkKind::kItem);
  CHECK(probe.state.list_menu.item_id == kSword);
  CHECK(probe.state.list_menu.label == "Sword");
  // Le clic droit ne visite pas.
  CHECK(probe.state.history.current() == kJellopySheet);
}

TEST_CASE("Base de données : Maj + clic et Ctrl + clic droit sont ceux de tout objet",
          "[ui][database]") {
  DatabaseProbe probe;
  probe.Settle();
  HoverRow(probe, "#1101 Sword##db_row1");
  // MAJ + CLIC : le lien posé dans la saisie — l'intention de toute cellule
  // d'objet —, et la ligne n'est pas visitée.
  const DatabaseAnswer reposted = probe.ClickWith(ImGuiMod_Shift, ImGuiMouseButton_Left);
  CHECK(reposted.list.gesture == ChatLinkGesture::kRepost);
  CHECK(reposted.list.kind == ChatLinkKind::kItem);
  CHECK(reposted.list.item_id == kSword);
  CHECK(probe.state.history.current() == kJellopySheet);
  // CTRL + CLIC DROIT : la description, sans passer par le menu.
  const DatabaseAnswer described = probe.ClickWith(ImGuiMod_Ctrl, ImGuiMouseButton_Right);
  CHECK(described.list.gesture == ChatLinkGesture::kDescribe);
  CHECK(described.list.kind == ChatLinkKind::kItem);
  CHECK(described.list.item_id == kSword);
}

TEST_CASE("Base de données : une ligne de monstre porte les gestes d'un monstre",
          "[ui][database]") {
  DatabaseProbe probe;
  const std::array<ro::ui::DatabaseRowView, 1> monsters = {{
      {.id = kPoring, .name = "Poring", .note = "Niv. 1"},
  }};
  probe.view.rows = monsters;
  probe.state.tab = ro::ui::kDatabaseMonsterTab;
  probe.state.select_tab = true;
  probe.Settle();
  HoverRow(probe, "#1002 Poring##db_row0");
  static_cast<void>(probe.Click(ImGuiMouseButton_Right));
  CHECK(probe.state.list_menu.kind == ChatLinkKind::kMonster);
  CHECK(probe.state.list_menu.item_id == kPoring);
  // Le menu se ferme : un clic ailleurs.
  ImGui::GetIO().AddMousePosEvent(1.0F, 1.0F);
  static_cast<void>(probe.Click());
  HoverRow(probe, "#1002 Poring##db_row0");
  const DatabaseAnswer reposted = probe.ClickWith(ImGuiMod_Shift, ImGuiMouseButton_Left);
  CHECK(reposted.list.gesture == ChatLinkGesture::kRepost);
  CHECK(reposted.list.kind == ChatLinkKind::kMonster);
  CHECK(reposted.list.item_id == kPoring);
}

// ── LA FICHE D'UN OBJET, CELLE DE LA DESCRIPTION ────────────────────────────

TEST_CASE("Base de données : un monstre qui lâche l'objet mène à sa fiche", "[ui][database]") {
  DatabaseProbe probe;
  probe.state.item_sheet.sheet_tab = ItemSheetTab::kMonsters;
  probe.Settle();
  const ImGuiWindow* body = WindowNamed("item_sheet_body");
  REQUIRE(body != nullptr);
  REQUIRE(SweepUntil(probe, body, true, [](const DatabaseAnswer& answer) {
    return answer.item.link.hovered_id == kPoring &&
           answer.item.link.hovered_kind == ChatLinkKind::kMonster;
  }));
  static_cast<void>(probe.Click());
  CHECK(probe.state.history.current() == kPoringSheet);
}

TEST_CASE("Base de données : « Marché » ouvre le Marché sur l'objet", "[ui][database]") {
  DatabaseProbe probe;
  probe.Settle();
  const ImGuiWindow* sheet = WindowNamed("db_sheet_child");
  REQUIRE(sheet != nullptr);
  REQUIRE(HoverOver(probe, sheet, ImHashStr("Marché", 0, sheet->ID), true));
  CHECK(probe.Click().item.search_market);
}

TEST_CASE("Base de données : « Signaler un bug » de la fiche d'un objet, selon le réglage",
          "[ui][database]") {
  DatabaseProbe probe;
  probe.Settle();
  const ImGuiWindow* sheet = WindowNamed("db_sheet_child");
  REQUIRE(sheet != nullptr);
  const ImGuiID button = ImHashStr("Signaler un bug", 0, sheet->ID);
  // Réglage éteint : pas de bouton.
  CHECK_FALSE(HoverOver(probe, sheet, button, true));
  // Allumé : le bouton de l'en-tête rend la demande de la description.
  probe.state.item_sheet.bug_report = true;
  probe.Settle();
  REQUIRE(HoverOver(probe, sheet, button, true));
  CHECK(probe.Click().item.report_bug);
}

TEST_CASE("Base de données : « Signaler un bug » de la fiche d'un monstre, selon le réglage",
          "[ui][database]") {
  DatabaseProbe probe;
  probe.view.item = nullptr;
  probe.view.monster = &probe.monster;
  probe.Settle();
  const ImGuiWindow* sheet = WindowNamed("db_sheet_child");
  REQUIRE(sheet != nullptr);
  const ImGuiID button = ImHashStr("Signaler un bug", 0, sheet->ID);
  // Réglage éteint : pas de bouton.
  CHECK_FALSE(HoverOver(probe, sheet, button, true));
  // Allumé — le réglage de LA fiche de monstre, que le panneau emprunte : le
  // bouton de l'en-tête rend la demande de la fiche.
  probe.monster_state.bug_report = true;
  probe.Settle();
  REQUIRE(HoverOver(probe, sheet, button, true));
  const DatabaseAnswer clicked = probe.Click();
  CHECK(clicked.monster.report_bug);
  CHECK_FALSE(clicked.item.report_bug);
}

TEST_CASE("Base de données : la fiche de monstre se dessine dans le panneau", "[ui][database]") {
  DatabaseProbe probe;
  probe.view.item = nullptr;
  probe.view.monster = &probe.monster;
  probe.Settle();
  // Le corps de LA fiche de monstre, sans sa fenêtre à elle.
  CHECK(WindowNamed(ro::ui::kMonsterSheetWindowId) == nullptr);
  CHECK(WindowNamed("db_sheet_child") != nullptr);
}

TEST_CASE("Base de données : seul l'onglet défile, l'en-tête reste", "[ui][database]") {
  constexpr int kDescriptionLines = 60;
  DatabaseProbe probe;
  std::vector<std::string> lines;
  for (int line = 0; line < kDescriptionLines; ++line) {
    lines.push_back("Heal: 45 ~ 65 HP");
  }
  probe.item.lines = lines;
  probe.Settle();
  const ImGuiWindow* const sheet = WindowNamed("db_sheet_child");
  REQUIRE(sheet != nullptr);
  CHECK(sheet->ScrollMax.y == 0.0F);
  const ImGuiWindow* const body = WindowNamed("item_sheet_body");
  REQUIRE(body != nullptr);
  CHECK(body->ScrollMax.y > 0.0F);
}

TEST_CASE("Base de données : la fiche d'un objet porte les onglets de la description et de la base",
          "[ui][database]") {
  DatabaseProbe probe;
  probe.Settle();
  ImGuiTabBar* bar = ItemTabBar();
  REQUIRE(bar != nullptr);
  // CEUX DE LA DESCRIPTION : la description, les sources, le script, les
  // combos. CEUX DE LA BASE : les faits, les vendeurs.
  for (const std::string_view tab :
       {"###sheet_description", "###sheet_general", "###sheet_monsters", "###sheet_boss",
        "###sheet_script", "###sheet_combos", "###sheet_sellers"}) {
    CHECK(TabEndingWith(bar, tab) != nullptr);
  }
  // Pas de table de tirage : pas de « Probabilités ».
  CHECK(TabEndingWith(bar, "###sheet_odds") == nullptr);
  CHECK(VisibleTab(bar).ends_with("###sheet_description"));
  // Un script vide se grise ; les combos attendent le serveur.
  CHECK_FALSE(ro::ui::ItemSheetTabOffered(ItemSheetTab::kScript, probe.item));
  CHECK(ro::ui::ItemSheetTabOffered(ItemSheetTab::kCombos, probe.item));
  CHECK(std::string(ImGui::TabBarGetTabName(bar, TabEndingWith(bar, "###sheet_sellers"))) ==
        "Vendu chez (0)###sheet_sellers");

  // Hors ligne, ni vendeurs ; sans base, ni faits.
  probe.item.sellers_phase = ro::ui::ItemSheetSellersPhase::kAbsent;
  probe.item.facts = {};
  probe.Settle();
  bar = ItemTabBar();
  CHECK(TabEndingWith(bar, "###sheet_sellers") == nullptr);
  CHECK(TabEndingWith(bar, "###sheet_general") == nullptr);
}

TEST_CASE("Base de données : l'onglet cliqué suit d'un objet à l'autre, « Description » à défaut",
          "[ui][database]") {
  DatabaseProbe probe;
  probe.item.scripts.main = "itemheal 45,0;";
  probe.Settle();
  // « Général » d'abord : livré à lui-même, ImGui retomberait sur lui, le
  // dernier choisi avant « Script ».
  static_cast<void>(ClickTab(probe, "###sheet_general"));
  static_cast<void>(ClickTab(probe, "###sheet_script"));
  CHECK(probe.state.item_sheet.sheet_tab == ItemSheetTab::kScript);
  CHECK(VisibleTab(ItemTabBar()).ends_with("###sheet_script"));

  // Un autre objet qui a un script : l'onglet reste.
  probe.item.item_id = kSword;
  probe.item.name = "Sword";
  probe.item.scripts.main = {};
  probe.item.scripts.unequip = "sc_end SC_ENDURE;";
  probe.state.history.Visit(kSwordSheet);
  probe.Settle();
  CHECK(probe.state.item_sheet.sheet_tab == ItemSheetTab::kScript);
  CHECK(VisibleTab(ItemTabBar()).ends_with("###sheet_script"));

  // Un objet sans script : « Description » DÈS LA PREMIÈRE IMAGE, son contenu
  // dessiné, et le choix du joueur intact. L'onglet se grise sans disparaître.
  probe.item.scripts.unequip = {};
  static_cast<void>(probe.Frame());
  ImGuiTabBar* const bar = ItemTabBar();
  CHECK(VisibleTab(bar).ends_with("###sheet_description"));
  CHECK(bar->VisibleTabWasSubmitted);
  probe.Settle();
  CHECK(probe.state.item_sheet.sheet_tab == ItemSheetTab::kScript);
  CHECK(VisibleTab(ItemTabBar()).ends_with("###sheet_description"));
  CHECK(TabEndingWith(ItemTabBar(), "###sheet_script") != nullptr);

  // L'objet suivant qui a un script rouvre « Script ».
  probe.item.scripts.main = "itemheal 45,0;";
  for (int frame = 0; frame < 2; ++frame) {
    static_cast<void>(probe.Frame());
    CHECK(ItemTabBar()->VisibleTabWasSubmitted);
  }
  CHECK(VisibleTab(ItemTabBar()).ends_with("###sheet_script"));
  CHECK(probe.state.item_sheet.sheet_tab == ItemSheetTab::kScript);

  // Un clic sur un autre onglet est un nouveau choix.
  probe.Settle();
  static_cast<void>(ClickTab(probe, "###sheet_general"));
  CHECK(probe.state.item_sheet.sheet_tab == ItemSheetTab::kGeneral);
}

TEST_CASE("Base de données : « Vendu chez » ne demande au Marché qu'onglet ouvert",
          "[ui][database]") {
  DatabaseProbe probe;
  probe.item.sellers_phase = ro::ui::ItemSheetSellersPhase::kAsking;
  probe.Settle();
  CHECK_FALSE(probe.Frame().item.sellers_shown);
  ImGuiTabBar* bar = ItemTabBar();
  REQUIRE(bar != nullptr);
  // Le nombre n'est pas encore su : le titre n'en dit rien.
  CHECK(std::string(ImGui::TabBarGetTabName(bar, TabEndingWith(bar, "###sheet_sellers"))) ==
        "Vendu chez###sheet_sellers");

  static_cast<void>(ClickTab(probe, "###sheet_sellers"));
  CHECK(probe.state.item_sheet.sheet_tab == ItemSheetTab::kSellers);
  CHECK(probe.Frame().item.sellers_shown);

  // La réponse arrivée, le titre compte les vendeurs.
  const std::array<ro::ui::ItemSheetSellerRow, 2> sellers = {{
      {.npc = "Tool Dealer", .place = "prontera", .price = "10 z"},
      {.npc = "Tool Dealer", .place = "geffen", .price = "10 z"},
  }};
  probe.item.sellers_phase = ro::ui::ItemSheetSellersPhase::kReady;
  probe.item.sellers = sellers;
  probe.Settle();
  bar = ItemTabBar();
  CHECK(std::string(ImGui::TabBarGetTabName(bar, TabEndingWith(bar, "###sheet_sellers"))) ==
        "Vendu chez (2)###sheet_sellers");
  CHECK(probe.Frame().item.sellers_shown);

  // Revenu sur « Général », la fiche ne demande plus rien.
  static_cast<void>(ClickTab(probe, "###sheet_general"));
  CHECK_FALSE(probe.Frame().item.sellers_shown);
}

TEST_CASE("Base de données : la même fiche d'objet dans la fenêtre de description",
          "[ui][database]") {
  // 🔴 UN SEUL COMPOSANT : la vue de la base, posée telle quelle dans la
  // fenêtre de description, y porte les mêmes onglets.
  DatabaseProbe probe;
  ro::ui::DescriptionView described = probe.item;
  described.in_database = false;
  ro::ui::DescriptionWindowState window;
  window.open = true;
  window.item_id = kJellopy;
  for (int frame = 0; frame < kSettleFrames; ++frame) {
    probe.ui.BeginFrame(kViewport, kFrameSeconds);
    static_cast<void>(ro::ui::DrawDescriptionWindow(window, described, probe.art));
    probe.ui.EndFrame();
  }
  const ImGuiWindow* const host = ImGui::FindWindowByName("###description");
  REQUIRE(host != nullptr);
  ImGuiTabBar* const bar = GImGui->TabBars.GetByKey(ImHashStr("##item_sheet_tabs", 0, host->ID));
  REQUIRE(bar != nullptr);
  for (const std::string_view tab :
       {"###sheet_description", "###sheet_general", "###sheet_monsters", "###sheet_boss",
        "###sheet_script", "###sheet_combos", "###sheet_sellers"}) {
    CHECK(TabEndingWith(bar, tab) != nullptr);
  }
}

TEST_CASE("Base de données : en lecture ou sans table, ni liste ni fiche", "[ui][database]") {
  DatabaseProbe probe;
  probe.view.loading = true;
  probe.Settle();
  CHECK(WindowNamed("db_list_child") == nullptr);
  probe.view.loading = false;
  probe.view.status = "Table absente.";
  probe.Settle();
  CHECK(WindowNamed("db_list_child") == nullptr);
  probe.view.status = {};
  probe.Settle();
  CHECK(WindowNamed("db_list_child") != nullptr);
}

TEST_CASE("Base de données : une liste de filtre fermée se nomme devant son choix",
          "[ui][database]") {
  std::array<const char*, ro::ui::kDatabaseItemTypes.size()> types{};
  std::ranges::transform(ro::ui::kDatabaseItemTypes, types.begin(),
                         [](const ro::ui::DatabaseItemType& type) { return type.label; });
  // Le rang d'une entrée dans la liste ouverte : « tous » occupe le premier.
  const auto rank_of = [](std::span<const char* const> labels, std::string_view wanted) {
    const auto found = std::ranges::find(labels, wanted);
    REQUIRE(found != labels.end());
    return static_cast<int>(found - labels.begin()) + 1;
  };

  CHECK(ro::ui::DatabaseFilterShown("Type", ro::ui::kDatabaseAnyChoice, types) == "Type - Tous");
  CHECK(ro::ui::DatabaseFilterShown("Type", rank_of(types, "Soins"), types) == "Type - Soins");
  CHECK(ro::ui::DatabaseFilterShown("Race", ro::ui::kDatabaseAnyChoice, ro::ui::kDatabaseRaces) ==
        "Race - Tous");
  CHECK(ro::ui::DatabaseFilterShown("Race", rank_of(ro::ui::kDatabaseRaces, "Démon"),
                                    ro::ui::kDatabaseRaces) == "Race - Démon");
  // Un rang hors de la liste est borné, jamais lu au-delà.
  CHECK(ro::ui::DatabaseFilterShown("Taille", -1, ro::ui::kDatabaseSizes) == "Taille - Tous");
  CHECK(ro::ui::DatabaseFilterShown("Rang", static_cast<int>(ro::ui::kDatabaseRanks.size()) + 1,
                                    ro::ui::kDatabaseRanks) == "Rang - MVP");
}
