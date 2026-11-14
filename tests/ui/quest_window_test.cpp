// LE JOURNAL DES QUÊTES, dessiné hors écran : une vue vide, un catalogue
// manquant, une quête complète au détail ; le clic sur une ligne, sur le
// cadenas, sur la case de suivi, sur « Y aller », l'onglet imposé par `app`,
// et « Tout oublier » confirmé par sa boîte.

#include "ro/ui/quest_window.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/skin_pieces.h"
#include "ro/ui/ui_context.h"

using ro::ui::QuestArt;
using ro::ui::QuestGroupView;
using ro::ui::QuestHistoryView;
using ro::ui::QuestHuntView;
using ro::ui::QuestRewardView;
using ro::ui::QuestRowView;
using ro::ui::QuestTab;
using ro::ui::QuestWindowAction;
using ro::ui::QuestWindowState;
using ro::ui::QuestWindowView;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr float kPlainPiecePixels = 8.0F;
constexpr int kSettleFrames = 4;
constexpr float kScanStep = 3.0F;
constexpr const char* kWindow = "quest_log";
constexpr std::uint32_t kPoringQuest = 1001;
constexpr std::uint32_t kLetterQuest = 1002;
constexpr std::uint32_t kPoring = 1002;
constexpr std::uint32_t kJellopy = 909;
constexpr std::uint32_t kPoster = 99;

// Une ligne de la liste, en pixels d'art : sa hauteur, puis le cadenas et la
// case « suivie » comptés depuis son bord droit, à mi-hauteur de la seconde
// ligne.
constexpr float kRowHeight = 46.0F;
constexpr float kLockFromRight = 18.0F;
constexpr float kTrackFromRight = 41.0F;
constexpr float kControlsMiddle = 30.0F;
constexpr float kIntoTitle = 60.0F;

[[nodiscard]] ro::ui::SkinArt PlainArt() {
  ro::ui::SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<ro::render::SkinPiece>(rank),
            ro::ui::SkinImage{.id = 1, .width = kPlainPiecePixels, .height = kPlainPiecePixels});
  }
  return art;
}

struct QuestProbe {
  ro::ui::UiContext ui;
  ro::ui::SkinArt skin = PlainArt();
  QuestArt art;
  QuestWindowState state;
  std::array<QuestHuntView, 2> hunts = {
      QuestHuntView{.label = "Poring Lv.1~Lv.10", .killed = 7, .total = 10, .mob_id = kPoring},
      QuestHuntView{.label = "Monstres de la plaine", .killed = 4, .total = 4, .done = true}};
  std::vector<std::string> description = {
      "^FF0000Attention^000000 : les Porings mangent tout.",
      "Parlez à <NAVI>Rubalkabara<INFO>prt_fild03,365,255,0,000,0</INFO></NAVI>."};
  std::array<QuestRewardView, 1> rewards = {
      QuestRewardView{.item_id = kJellopy, .name = "Jellopy", .count = 20}};
  std::array<QuestRowView, 2> rows;
  std::array<QuestGroupView, 1> groups;
  std::array<QuestHistoryView, 1> history = {
      QuestHistoryView{.quest_id = 900, .title = "Ancienne quête", .left_at = "2026/10/06 16:40"}};
  std::array<ro::ui::QuestCategoryView, 2> categories = {
      ro::ui::QuestCategoryView{.key = "all", .label = "Toutes"},
      ro::ui::QuestCategoryView{.key = "normal", .label = "Épisodes"}};
  QuestWindowView view;

  QuestProbe() {
    state.open = true;
    rows[0] =
        QuestRowView{.quest_id = kPoringQuest,
                     .title = "Chasse aux Porings",
                     .summary = "Débarrassez la plaine",
                     .fresh = true,
                     .time_left = "2 h 13 min",
                     .urgent = true,
                     .progress = 0.8F,
                     .hunts = hunts,
                     .description = description,
                     .deadline = "2026/10/07 18:00",
                     .npc = ro::ui::QuestNpcView{.label = "Rubalkabara, prt_fild03 (365, 255)",
                                                 .map_name = "prt_fild03",
                                                 .cell_x = 365,
                                                 .cell_y = 255},
                     .reward_base_exp = "1 500",
                     .reward_job_exp = "900",
                     .reward_items = rewards};
    rows[1] = QuestRowView{.quest_id = kLetterQuest, .title = "La lettre", .active = false};
    groups[0] = QuestGroupView{.key = "all", .rows = rows};
    recommended[0] =
        ro::ui::QuestRecommendedView{.entry_id = kPoster,
                                     .title = "Épisode 15 : Phantasmagorica",
                                     .summary = "Une science qui dépasse Juperos",
                                     .fresh = true,
                                     .npc = ro::ui::QuestNpcView{.label = "moc_para01 (38, 175)",
                                                                 .map_name = "moc_para01",
                                                                 .cell_x = 38,
                                                                 .cell_y = 175},
                                     .blocks = blocks};
    view.tab_counts = {1, 1, 1, 1, 0};
  }

  // Une vue peuplée : un groupe sans en-tête, deux lignes.
  void Fill() {
    view.groups = groups;
    view.history = history;
    view.recommended = recommended;
    view.categories = categories;
  }

  std::vector<std::string> blocks = {"Rejoignez le groupe Eden.\nVenez vite !",
                                     "^3275f0Conditions^000000\nNiveau de base 140 ou plus"};
  std::array<ro::ui::QuestRecommendedView, 1> recommended;

  QuestWindowAction Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    QuestWindowAction action = ro::ui::DrawQuestWindow(state, view, art, skin);
    ui.EndFrame();
    return action;
  }

  void Settle() {
    for (int frame = 0; frame < kSettleFrames; ++frame) {
      static_cast<void>(Frame());
    }
  }
};

[[nodiscard]] ImGuiWindow* Window() { return ImGui::FindWindowByName(kWindow); }

// L'enfant de la fenêtre dont le nom porte `id`.
[[nodiscard]] ImGuiWindow* Child(std::string_view id) {
  for (ImGuiWindow* window : GImGui->Windows) {
    if (window->ParentWindow == Window() &&
        std::string_view(window->Name).find(id) != std::string_view::npos) {
      return window;
    }
  }
  return nullptr;
}

[[nodiscard]] bool Empty(const QuestWindowAction& action) {
  return !action.selected && !action.read_recommended && !action.toggle_active &&
         !action.toggle_tracked && !action.guide_to && action.open_monster == 0 &&
         action.item.gesture == ro::ui::ChatLinkGesture::kNothing &&
         action.followed_address.empty() && !action.forget && !action.forget_all &&
         !action.show_tracker && !action.open_settings && !action.report_bug;
}

struct Click {
  QuestWindowAction pressed;
  QuestWindowAction released;
};

Click ClickAt(QuestProbe& probe, ImVec2 at) {
  ImGuiIO& io = ImGui::GetIO();
  io.AddMousePosEvent(at.x, at.y);
  static_cast<void>(probe.Frame());
  Click click;
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  click.pressed = probe.Frame();
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  click.released = probe.Frame();
  static_cast<void>(probe.Frame());
  return click;
}

// Balaie une fenêtre jusqu'à survoler l'élément `id`.
[[nodiscard]] std::optional<ImVec2> FindHovered(QuestProbe& probe, const ImGuiWindow& window,
                                                ImGuiID id) {
  ImGuiIO& io = ImGui::GetIO();
  for (float y = window.Pos.y + 1.0F; y < window.Pos.y + window.Size.y; y += kScanStep) {
    for (float x = window.Pos.x + 1.0F; x < window.Pos.x + window.Size.x; x += kScanStep * 2.0F) {
      io.AddMousePosEvent(x, y);
      static_cast<void>(probe.Frame());
      if (GImGui->HoveredId == id) {
        return ImVec2(x, y);
      }
    }
  }
  return std::nullopt;
}

// Un point d'une ligne de la liste : `rank` rangs plus bas, `from_right`
// pixels d'art depuis le bord droit (zéro : depuis la gauche, `x`).
[[nodiscard]] ImVec2 RowPoint(std::size_t rank, float x, float y, bool from_right) {
  const ImGuiWindow* list = Child("##quetes_liste");
  REQUIRE(list != nullptr);
  const float scale = ImGui::GetStyle().FontScaleMain;
  const float top = list->InnerRect.Min.y + (kRowHeight * static_cast<float>(rank) + y) * scale;
  const float left =
      from_right ? list->InnerRect.Max.x - x * scale : list->InnerRect.Min.x + x * scale;
  return {left, top};
}

}  // namespace

TEST_CASE("Quêtes : une vue vide se dessine sans rien demander", "[ui][quetes]") {
  QuestProbe probe;
  for (int frame = 0; frame < kSettleFrames; ++frame) {
    CHECK(Empty(probe.Frame()));
  }
  REQUIRE(Window() != nullptr);
  CHECK(Child("##quetes_liste") != nullptr);
  CHECK(Child("##quetes_detail") != nullptr);

  // Fermée, rien n'est soumis.
  QuestProbe closed;
  closed.state.open = false;
  closed.Settle();
  const ImGuiWindow* window = Window();
  CHECK_FALSE((window != nullptr && window->Active));
}

TEST_CASE("Quêtes : un catalogue manquant se dit, sur chaque onglet", "[ui][quetes]") {
  QuestProbe probe;
  probe.view.catalog_problem = "Le catalogue des quêtes manque (data\\yaml_files\\quests.yml).";
  probe.Fill();
  for (const QuestTab tab : ro::ui::kQuestTabOrder) {
    probe.state.tab = tab;
    probe.view.tab = tab;
    for (int frame = 0; frame < kSettleFrames; ++frame) {
      CHECK(Empty(probe.Frame()));
    }
    CHECK(probe.state.tab == tab);
  }
}

TEST_CASE("Quêtes : une quête complète se détaille, et « Y aller » guide vers son PNJ",
          "[ui][quetes]") {
  QuestProbe probe;
  probe.Fill();
  probe.state.selected = kPoringQuest;
  probe.view.selected = &probe.rows[0];
  for (int frame = 0; frame < kSettleFrames; ++frame) {
    CHECK(Empty(probe.Frame()));
  }
  ImGuiWindow* detail = Child("##quetes_detail");
  REQUIRE(detail != nullptr);
  const std::optional<ImVec2> go = FindHovered(probe, *detail, detail->GetID("Y aller"));
  REQUIRE(go.has_value());
  const Click click = ClickAt(probe, *go);
  const std::optional<ro::ui::RichPlace>& guide =
      click.released.guide_to.has_value() ? click.released.guide_to : click.pressed.guide_to;
  REQUIRE(guide.has_value());
  CHECK(guide->map == "prt_fild03");
  CHECK(guide->x == 365);
  CHECK(guide->y == 255);

  // L'historique ne montre que le titre et la description : pas de PNJ.
  probe.state.tab = QuestTab::kHistory;
  probe.view.tab = QuestTab::kHistory;
  probe.Settle();
  CHECK_FALSE(FindHovered(probe, *Child("##quetes_detail"), detail->GetID("Y aller")).has_value());
}

TEST_CASE("Quêtes : un clic sur une ligne la choisit", "[ui][quetes]") {
  QuestProbe probe;
  probe.Fill();
  probe.Settle();
  const Click click = ClickAt(probe, RowPoint(1, kIntoTitle, kRowHeight * 0.5F, false));
  CHECK(click.released.selected == std::optional<std::uint32_t>(kLetterQuest));
  CHECK(probe.state.selected == kLetterQuest);
  CHECK_FALSE(click.released.toggle_active.has_value());
}

TEST_CASE("Quêtes : le cadenas suspend, la case suit, sans choisir la ligne", "[ui][quetes]") {
  QuestProbe probe;
  probe.Fill();
  probe.Settle();
  const Click lock = ClickAt(probe, RowPoint(0, kLockFromRight, kControlsMiddle, true));
  CHECK(lock.released.toggle_active == std::optional<std::uint32_t>(kPoringQuest));
  CHECK_FALSE(lock.released.selected.has_value());
  CHECK(probe.state.selected == 0);

  // La case dessinée de l'art.
  probe.art.track_on = ro::ui::SkinImage{.id = 2, .width = 14.0F, .height = 14.0F};
  probe.art.track_off = ro::ui::SkinImage{.id = 3, .width = 14.0F, .height = 14.0F};
  probe.Settle();
  const Click track = ClickAt(probe, RowPoint(1, kTrackFromRight, kControlsMiddle, true));
  CHECK(track.released.toggle_tracked == std::optional<std::uint32_t>(kLetterQuest));
  CHECK_FALSE(track.released.selected.has_value());
}

TEST_CASE("Quêtes : un onglet posé par app s'impose et tient", "[ui][quetes]") {
  QuestProbe probe;
  probe.Fill();
  probe.Settle();
  probe.state.tab = QuestTab::kSuspended;
  probe.Settle();
  CHECK(probe.state.tab == QuestTab::kSuspended);
  const ImGuiTabBar* bar = nullptr;
  for (int rank = 0; rank < GImGui->TabBars.GetMapSize(); ++rank) {
    if (const ImGuiTabBar* found = GImGui->TabBars.TryGetMapData(rank); found != nullptr) {
      bar = found;
    }
  }
  REQUIRE(bar != nullptr);
  CHECK(bar->SelectedTabId == ImHashStr("###quetes_suspendues", 0, bar->ID));
}

TEST_CASE("Quêtes : une affiche recommandée s'ouvre, et son PNJ se rejoint", "[ui][quetes]") {
  QuestProbe probe;
  probe.Fill();
  probe.state.tab = QuestTab::kRecommended;
  probe.view.tab = QuestTab::kRecommended;
  probe.Settle();
  CHECK(probe.state.tab == QuestTab::kRecommended);
  const Click click = ClickAt(probe, RowPoint(0, kIntoTitle, kRowHeight * 0.5F, false));
  CHECK(click.released.read_recommended == std::optional<std::uint32_t>(kPoster));
  CHECK(probe.state.selected_recommended == kPoster);
  // Ce n'est pas une quête : la sélection des quêtes ne bouge pas.
  CHECK_FALSE(click.released.selected.has_value());
  CHECK(probe.state.selected == 0);

  probe.view.selected_recommended = &probe.recommended[0];
  probe.Settle();
  ImGuiWindow* detail = Child("##quetes_detail");
  REQUIRE(detail != nullptr);
  const std::optional<ImVec2> go = FindHovered(probe, *detail, detail->GetID("Y aller"));
  REQUIRE(go.has_value());
  const Click went = ClickAt(probe, *go);
  const std::optional<ro::ui::RichPlace>& guide =
      went.released.guide_to.has_value() ? went.released.guide_to : went.pressed.guide_to;
  REQUIRE(guide.has_value());
  CHECK(guide->map == "moc_para01");
  CHECK(guide->y == 175);
}

TEST_CASE("Quêtes : une attente prête se détaille sans rien demander", "[ui][quetes]") {
  QuestProbe probe;
  probe.rows[0].waiting = true;
  probe.rows[0].expired = true;
  probe.rows[0].urgent = false;
  probe.rows[0].time_left = "prête";
  probe.rows[0].hunts = {};
  probe.Fill();
  probe.state.tab = QuestTab::kWaiting;
  probe.view.tab = QuestTab::kWaiting;
  probe.state.selected = kPoringQuest;
  probe.view.selected = &probe.rows[0];
  for (int frame = 0; frame < kSettleFrames; ++frame) {
    CHECK(Empty(probe.Frame()));
  }
  CHECK(probe.state.tab == QuestTab::kWaiting);
}

TEST_CASE("Quêtes : « Tout oublier » ouvre sa boîte, et « oui » l'envoie", "[ui][quetes]") {
  QuestProbe probe;
  probe.Fill();
  probe.state.tab = QuestTab::kHistory;
  probe.view.tab = QuestTab::kHistory;
  probe.Settle();
  ImGuiWindow* window = Window();
  REQUIRE(window != nullptr);
  const std::optional<ImVec2> forget_all =
      FindHovered(probe, *window, window->GetID("Tout oublier"));
  REQUIRE(forget_all.has_value());
  const Click asked = ClickAt(probe, *forget_all);
  CHECK_FALSE(asked.released.forget_all);
  ImGuiWindow* box = ImGui::FindWindowByName("##quetes_tout_oublier");
  REQUIRE(box != nullptr);
  REQUIRE(box->Active);
  const std::optional<ImVec2> yes = FindHovered(probe, *box, box->GetID("Tout oublier"));
  REQUIRE(yes.has_value());
  const Click answered = ClickAt(probe, *yes);
  CHECK((answered.pressed.forget_all || answered.released.forget_all));
}

TEST_CASE("Quêtes : un objet de la description se survole, se décrit et se repose",
          "[ui][quetes]") {
  constexpr std::uint32_t kRedPotion = 501;
  QuestProbe probe;
  probe.description.emplace_back("Apportez une <ITEM>Potion rouge<INFO>501</INFO></ITEM>.");
  probe.rows[0].description = probe.description;
  probe.Fill();
  probe.state.selected = kPoringQuest;
  probe.view.selected = &probe.rows[0];
  probe.Settle();
  const ImGuiWindow* detail = Child("##quetes_detail");
  REQUIRE(detail != nullptr);

  // Le SURVOL demande l'aperçu de l'objet, sans geste.
  ImGuiIO& io = ImGui::GetIO();
  std::optional<ImVec2> over;
  for (float y = detail->Pos.y + 1.0F; y < detail->Pos.y + detail->Size.y && !over;
       y += kScanStep) {
    for (float x = detail->Pos.x + 1.0F; x < detail->Pos.x + detail->Size.x; x += kScanStep) {
      io.AddMousePosEvent(x, y);
      const QuestWindowAction seen = probe.Frame();
      if (seen.item.hovered_id == kRedPotion) {
        CHECK(seen.item.hovering);
        CHECK(seen.item.gesture == ro::ui::ChatLinkGesture::kNothing);
        over = ImVec2(x, y);
        break;
      }
    }
  }
  REQUIRE(over.has_value());

  // Le CLIC le décrit.
  const Click described = ClickAt(probe, *over);
  CHECK(described.released.item.gesture == ro::ui::ChatLinkGesture::kDescribe);
  CHECK(described.released.item.kind == ro::ui::ChatLinkKind::kItem);
  CHECK(described.released.item.item_id == kRedPotion);
  CHECK(described.released.followed_address.empty());

  // MAJ + CLIC le repose dans la saisie.
  io.AddKeyEvent(ImGuiMod_Shift, true);
  const Click reposted = ClickAt(probe, *over);
  io.AddKeyEvent(ImGuiMod_Shift, false);
  CHECK(reposted.released.item.gesture == ro::ui::ChatLinkGesture::kRepost);
  CHECK(reposted.released.item.item_id == kRedPotion);
}
