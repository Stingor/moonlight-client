// Les fenêtres du classement survivors, sans GPU : le tableau mondial vide,
// plein, avec le joueur dedans ou épinglé en pied ; « Actualiser » grisé
// pendant sa cadence ; l'accueil, l'écran de fin et les records qui ne
// montrent RIEN du classement tant que le serveur ne l'a pas ouvert ; la
// ligne du rang qui passe de l'envoi au verdict ; le HUD « Classée » qui ne
// lit aucune entrée.

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/survivors_hud.h"
#include "ro/ui/survivors_prepare_window.h"
#include "ro/ui/survivors_ranking_window.h"
#include "ro/ui/survivors_records_window.h"
#include "ro/ui/survivors_summary_window.h"
#include "ro/ui/ui_context.h"

namespace {

using ro::ui::SkinArt;
using ro::ui::SurvivorsRankingPage;
using ro::ui::SurvivorsRankingRow;
using ro::ui::SurvivorsRankStatus;

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr int kSettleFrames = 3;
constexpr float kSweepStep = 10.0F;
constexpr float kBetweenClicksSeconds = 0.5F;
constexpr std::size_t kFullBoardRows = 20;
constexpr std::uint64_t kTopScore = 90'000;
constexpr std::uint64_t kScoreStep = 1'000;
constexpr int kOwnRankInside = 7;
constexpr int kOwnRankOutside = 57;
constexpr std::uint64_t kOwnScoreOutside = 31'870;
constexpr int kRuleset = 3;
// Un rectangle plein : quatre sommets.
constexpr int kQuadVertices = 4;
// Deux lignes voisines se touchent, au trait près.
constexpr float kRowSlackPx = 1.0F;
const SkinArt kNoArt;

// Les noms d'essai, tels qu'un serveur les rendrait.
const std::array<std::string, kFullBoardRows> kNames = [] {
  std::array<std::string, kFullBoardRows> names;
  for (std::size_t index = 0; index < names.size(); ++index) {
    names[index] = "Survivant" + std::to_string(index + 1);
  }
  return names;
}();

[[nodiscard]] std::array<SurvivorsRankingRow, kFullBoardRows> FullBoard() {
  std::array<SurvivorsRankingRow, kFullBoardRows> rows;
  for (std::size_t index = 0; index < rows.size(); ++index) {
    rows[index] = SurvivorsRankingRow{.rank = static_cast<int>(index + 1),
                                      .name = kNames[index].c_str(),
                                      .score = kTopScore - kScoreStep * index,
                                      .survived_ms = 60'000,
                                      .level = 20};
  }
  return rows;
}

[[nodiscard]] bool Contains(std::string_view text, std::string_view part) {
  return text.find(part) != std::string_view::npos;
}

// Le texte que `draw` pose à sa dernière image, relevé par le journal d'ImGui.
[[nodiscard]] std::string TextOf(const std::function<void()>& draw) {
  ro::ui::UiContext ui;
  std::string text;
  for (int frame = 0; frame < kSettleFrames; ++frame) {
    ui.BeginFrame(kViewport, kFrameSeconds);
    const bool last = frame + 1 == kSettleFrames;
    if (last) ImGui::LogToBuffer();
    draw();
    if (last) {
      text = ImGui::GetCurrentContext()->LogBuffer.c_str();
      ImGui::LogFinish();
    }
    ui.EndFrame();
  }
  return text;
}

[[nodiscard]] bool Frame(ro::ui::UiContext& ui, const std::function<bool()>& draw) {
  ui.BeginFrame(kViewport, kFrameSeconds);
  const bool asked = draw();
  ui.EndFrame();
  return asked;
}

[[nodiscard]] bool ClickAt(ro::ui::UiContext& ui, ImVec2 at, const std::function<bool()>& draw) {
  ImGuiIO& io = ImGui::GetIO();
  io.AddMousePosEvent(at.x, at.y);
  bool asked = Frame(ui, draw);
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  asked = Frame(ui, draw) || asked;
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  asked = Frame(ui, draw) || asked;
  return asked;
}

// Un clic sur chaque point d'une grille couvrant le corps de la fenêtre : vrai
// si l'un d'eux a rendu l'intention guettée.
[[nodiscard]] bool SweepClicks(const char* window_id, const std::function<bool()>& draw) {
  ro::ui::UiContext ui;
  for (int frame = 0; frame < kSettleFrames; ++frame) {
    (void)Frame(ui, draw);
  }
  const ImGuiWindow* window = ImGui::FindWindowByName(window_id);
  REQUIRE(window != nullptr);
  const ImRect bounds = window->InnerRect;
  for (float y = bounds.Min.y; y < bounds.Max.y; y += kSweepStep) {
    for (float x = bounds.Min.x; x < bounds.Max.x; x += kSweepStep) {
      if (ClickAt(ui, ImVec2(x, y), draw)) return true;
      // Une image longue entre deux clics : deux clics proches font un
      // double-clic.
      ui.BeginFrame(kViewport, kBetweenClicksSeconds);
      (void)draw();
      ui.EndFrame();
    }
  }
  return false;
}

// Une touche tenue le temps d'une image ; vrai si une image a rendu
// l'intention.
[[nodiscard]] bool PressKey(ImGuiKey key, const std::function<bool()>& draw) {
  ro::ui::UiContext ui;
  bool asked = false;
  for (int frame = 0; frame < kSettleFrames; ++frame) {
    asked = Frame(ui, draw) || asked;
  }
  ImGui::GetIO().AddKeyEvent(key, true);
  asked = Frame(ui, draw) || asked;
  ImGui::GetIO().AddKeyEvent(key, false);
  asked = Frame(ui, draw) || asked;
  return asked;
}

// Les bandes de fond que `draw` pose à sa dernière image dans la teinte de la
// ligne du joueur, une par ligne surlignée.
[[nodiscard]] std::vector<ImRect> OwnRowBandsOf(const std::function<void()>& draw) {
  ro::ui::UiContext ui;
  for (int frame = 0; frame < kSettleFrames; ++frame) {
    ui.BeginFrame(kViewport, kFrameSeconds);
    draw();
    ui.EndFrame();
  }
  std::vector<ImRect> bands;
  for (const ImGuiWindow* window : ImGui::GetCurrentContext()->Windows) {
    const ImVector<ImDrawVert>& vertices = window->DrawList->VtxBuffer;
    for (int index = 0; index + kQuadVertices <= vertices.Size; ++index) {
      if (vertices[index].col != ro::ui::kSurvivorsOwnRowBack) continue;
      ImRect band(vertices[index].pos, vertices[index].pos);
      for (int corner = 1; corner < kQuadVertices; ++corner) band.Add(vertices[index + corner].pos);
      index += kQuadVertices - 1;
      // Les deux fonds d'une ligne se posent sur le même rectangle.
      const bool seen = std::ranges::any_of(bands, [&band](const ImRect& other) {
        return other.Min.x == band.Min.x && other.Min.y == band.Min.y &&
               other.Max.x == band.Max.x && other.Max.y == band.Max.y;
      });
      if (!seen) bands.push_back(band);
    }
  }
  return bands;
}

// Les sommets que le HUD ajoute à la liste de fond.
[[nodiscard]] int HudVerticesOf(const ro::ui::SurvivorsHudView& view) {
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

TEST_CASE("Classement survivors : le tableau vide, puis en attente de sa page", "[ui][survivors]") {
  ro::ui::SurvivorsRankingState state{.open = true};
  ro::ui::SurvivorsRankingView view{.ruleset = kRuleset};
  view.ranked = SurvivorsRankingPage{.received = true};
  const auto draw = [&] { (void)DrawSurvivorsRankingWindow(state, view, kNoArt); };
  const std::string empty = TextOf(draw);
  CHECK(Contains(empty, "Aucune partie classée."));
  CHECK(Contains(empty, "Vous n'êtes pas encore classé."));
  CHECK(Contains(empty, "Règles 3"));
  CHECK(ro::ui::SurvivorsOwnPlaceOf(view.ranked) == ro::ui::SurvivorsOwnPlace::kUnranked);
  view.ranked = SurvivorsRankingPage{};
  const std::string waiting = TextOf(draw);
  CHECK(Contains(waiting, "Demande au serveur…"));
  CHECK_FALSE(Contains(waiting, "Vous n'êtes pas encore classé."));
}

TEST_CASE("Classement survivors : vingt lignes, le joueur dedans puis épinglé en pied",
          "[ui][survivors]") {
  const std::array<SurvivorsRankingRow, kFullBoardRows> rows = FullBoard();
  ro::ui::SurvivorsRankingState state{.open = true};
  ro::ui::SurvivorsRankingView view;
  view.ranked = SurvivorsRankingPage{.received = true,
                                     .rows = rows,
                                     .own_rank = kOwnRankInside,
                                     .own_score = rows[kOwnRankInside - 1].score,
                                     .age_s = 12};
  const auto draw = [&] { (void)DrawSurvivorsRankingWindow(state, view, kNoArt); };
  const std::string inside = TextOf(draw);
  CHECK(Contains(inside, kNames.front()));
  CHECK(Contains(inside, kNames.back()));
  CHECK(Contains(inside, "20e"));
  CHECK(Contains(inside, "il y a 12 s"));
  CHECK_FALSE(Contains(inside, "Vous :"));
  CHECK(ro::ui::SurvivorsOwnPlaceOf(view.ranked) == ro::ui::SurvivorsOwnPlace::kInTable);
  // Sa ligne est surlignée : une bande, à la hauteur de la septième ligne.
  const std::vector<ImRect> inside_bands = OwnRowBandsOf(draw);
  REQUIRE(inside_bands.size() == 1);
  // La bande suit le rang : un rang plus bas, elle descend d'une ligne.
  view.ranked.own_rank = kOwnRankInside + 1;
  const std::vector<ImRect> lower_bands = OwnRowBandsOf(draw);
  REQUIRE(lower_bands.size() == 1);
  CHECK(lower_bands.front().Min.y >= inside_bands.front().Max.y - kRowSlackPx);

  view.ranked.own_rank = kOwnRankOutside;
  view.ranked.own_score = kOwnScoreOutside;
  CHECK(ro::ui::SurvivorsOwnPlaceOf(view.ranked) == ro::ui::SurvivorsOwnPlace::kPinned);
  CHECK(Contains(TextOf(draw), "Vous : 57e — 31,870"));
  CHECK(OwnRowBandsOf(draw).empty());
}

TEST_CASE("Classement survivors : « Actualiser » grisé ne rend rien, Tab change d'onglet",
          "[ui][survivors]") {
  ro::ui::SurvivorsRankingState state{.open = true};
  ro::ui::SurvivorsRankingView view;
  view.ranked = SurvivorsRankingPage{.received = true};
  view.refresh_ready = false;
  const auto refresh = [&] {
    state.open = true;
    return DrawSurvivorsRankingWindow(state, view, kNoArt).refresh;
  };
  CHECK_FALSE(SweepClicks("###survivors_ranking", refresh));
  view.refresh_ready = true;
  CHECK(SweepClicks("###survivors_ranking", refresh));

  state = ro::ui::SurvivorsRankingState{.open = true};
  view.week_year = 2026;
  view.week_number = 44;
  const auto tab = [&] { return DrawSurvivorsRankingWindow(state, view, kNoArt).tab_changed; };
  CHECK(PressKey(ImGuiKey_Tab, tab));
  CHECK(state.tab == ro::ui::SurvivorsRankingTab::kChallenge);
  CHECK(Contains(TextOf([&] { (void)tab(); }), "Semaine 44 de 2026"));
}

TEST_CASE("Classement survivors : l'accueil sans service n'en montre rien", "[ui][survivors]") {
  ro::ui::SurvivorsPrepareState prepare{.open = true};
  ro::ui::SurvivorsPrepareView view;
  view.meta = ro::ui::SurvivorsPrepareMeta{};
  const auto draw = [&] {
    prepare.open = true;
    return DrawSurvivorsPrepareWindow(prepare, view, kNoArt);
  };
  const auto ranking_asked = [&] {
    const ro::ui::SurvivorsPrepareAsk asked = draw();
    return asked.open_ranking || asked.choose_board == ro::ui::SurvivorsBoardTab::kRanked;
  };
  for (const ro::ui::SurvivorsBoardTab board :
       {ro::ui::SurvivorsBoardTab::kFree, ro::ui::SurvivorsBoardTab::kChallenge,
        ro::ui::SurvivorsBoardTab::kRanked}) {
    view.meta->board = board;
    const std::string text = TextOf([&] { (void)draw(); });
    CHECK_FALSE(Contains(text, "Partie classée"));
    CHECK_FALSE(Contains(text, "Classement"));
    CHECK_FALSE(Contains(text, "Classé si le serveur répond"));
    CHECK_FALSE(Contains(text, "en attente"));
    CHECK_FALSE(Contains(text, "Graine tirée par le serveur"));
  }
  view.meta->board = ro::ui::SurvivorsBoardTab::kChallenge;
  CHECK_FALSE(SweepClicks("###survivors_prepare", ranking_asked));
  CHECK_FALSE(PressKey(ImGuiKey_Tab,
                       [&] { return draw().choose_board == ro::ui::SurvivorsBoardTab::kRanked; }));
}

TEST_CASE("Classement survivors : l'accueil service ouvert offre l'onglet et le compteur",
          "[ui][survivors]") {
  const std::array<ro::ui::SurvivorsPendingRow, 1> pending = {
      ro::ui::SurvivorsPendingRow{.character = "Ærith", .ended_at_utc_s = 1'792'886'400}};
  ro::ui::SurvivorsPrepareState prepare{.open = true};
  ro::ui::SurvivorsPrepareView view;
  view.meta =
      ro::ui::SurvivorsPrepareMeta{.board = ro::ui::SurvivorsBoardTab::kChallenge,
                                   .ranking = ro::ui::SurvivorsPrepareRanking{.pending = pending}};
  const auto draw = [&] {
    prepare.open = true;
    return DrawSurvivorsPrepareWindow(prepare, view, kNoArt);
  };
  const std::string challenge = TextOf([&] { (void)draw(); });
  CHECK(Contains(challenge, "Partie classée"));
  CHECK(Contains(challenge, "Classement en ligne"));
  CHECK(Contains(challenge, "Classé si le serveur répond."));
  CHECK(Contains(challenge, "1 résultat(s) en attente d'envoi"));
  CHECK(SweepClicks("###survivors_prepare", [&] { return draw().open_ranking; }));
  CHECK(SweepClicks("###survivors_prepare",
                    [&] { return draw().choose_board == ro::ui::SurvivorsBoardTab::kRanked; }));
  CHECK(PressKey(ImGuiKey_Tab,
                 [&] { return draw().choose_board == ro::ui::SurvivorsBoardTab::kRanked; }));

  view.meta->board = ro::ui::SurvivorsBoardTab::kRanked;
  // Sans page, l'accueil n'affirme rien du rang.
  const std::string unknown = TextOf([&] { (void)draw(); });
  CHECK(Contains(unknown, "Votre rang s'affiche à l'ouverture du classement."));
  CHECK_FALSE(Contains(unknown, "Vous n'êtes pas encore classé."));
  view.meta->ranking->best_known = true;
  CHECK(Contains(TextOf([&] { (void)draw(); }), "Vous n'êtes pas encore classé."));
  view.meta->ranking->best_rank = 14;
  view.meta->ranking->best_score = 48'210;
  const std::string ranked = TextOf([&] { (void)draw(); });
  CHECK(Contains(ranked, "Graine tirée par le serveur."));
  CHECK(Contains(ranked, "Votre meilleur : 14e, 48,210"));
  CHECK(SweepClicks("###survivors_prepare", [&] { return draw().start; }));

  // Un onglet classé grisé : la partie ne part pas, sa raison se dit.
  view.meta->ranking->refusal = "Partie classée seulement en ville.";
  CHECK_FALSE(SweepClicks("###survivors_prepare", [&] { return draw().start; }));
  CHECK(Contains(TextOf([&] { (void)draw(); }), "Partie classée seulement en ville."));
}

TEST_CASE("Classement survivors : l'écran de fin passe de l'envoi au rang", "[ui][survivors]") {
  ro::ui::SurvivorsSummaryView summary{.accepts_input = true};
  const auto draw = [&] { return DrawSurvivorsSummaryWindow(summary, kNoArt); };
  const std::string closed = TextOf([&] { (void)draw(); });
  CHECK_FALSE(Contains(closed, "Partie libre"));
  CHECK_FALSE(Contains(closed, "Partie classée"));
  CHECK_FALSE(Contains(closed, "Classement"));

  summary.ranking = ro::ui::SurvivorsSummaryRanking{.ranked = false};
  const std::string free = TextOf([&] { (void)draw(); });
  CHECK(Contains(free, "Partie libre"));
  CHECK_FALSE(Contains(free, "Classement"));
  CHECK_FALSE(SweepClicks("###survivors_summary", [&] { return draw().open_ranking.has_value(); }));

  summary.ranking =
      ro::ui::SurvivorsSummaryRanking{.ranked = true,
                                      .tab = ro::ui::SurvivorsRankingTab::kChallenge,
                                      .standing = {.status = SurvivorsRankStatus::kPending}};
  const std::string pending = TextOf([&] { (void)draw(); });
  CHECK(Contains(pending, "Partie classée"));
  CHECK(Contains(pending, "En attente du serveur"));
  // Le tableau ne peut pas s'ouvrir : pas de bouton.
  CHECK_FALSE(SweepClicks("###survivors_summary", [&] { return draw().open_ranking.has_value(); }));

  summary.ranking->board_reachable = true;
  summary.ranking->server_score = 48'210;
  summary.ranking->standing = {.status = SurvivorsRankStatus::kPersonalBest, .rank = 14};
  const std::string placed = TextOf([&] { (void)draw(); });
  CHECK(Contains(placed, "Score : 48,210"));
  CHECK(Contains(placed, "Classement : 14e — meilleur personnel !"));
  CHECK(SweepClicks("###survivors_summary", [&] {
    return draw().open_ranking == ro::ui::SurvivorsRankingTab::kChallenge;
  }));
}

TEST_CASE("Classement survivors : une phrase par code", "[ui][survivors]") {
  std::set<std::string> refusals;
  for (const ro::ui::SurvivorsTicketRefusal refusal :
       {ro::ui::SurvivorsTicketRefusal::kPlace, ro::ui::SurvivorsTicketRefusal::kUnlinked,
        ro::ui::SurvivorsTicketRefusal::kRules, ro::ui::SurvivorsTicketRefusal::kTooSoon,
        ro::ui::SurvivorsTicketRefusal::kSilent, ro::ui::SurvivorsTicketRefusal::kChallengeSeed,
        ro::ui::SurvivorsTicketRefusal::kUnknown}) {
    refusals.insert(ro::ui::SurvivorsTicketRefusalText(refusal));
  }
  CHECK(refusals.size() == 7);
  CHECK(std::string(ro::ui::SurvivorsTicketRefusalText(ro::ui::SurvivorsTicketRefusal::kSilent)) ==
        "Partie libre : le serveur n'a pas répondu.");

  std::set<std::string> standings;
  for (const SurvivorsRankStatus status :
       {SurvivorsRankStatus::kSending, SurvivorsRankStatus::kPending,
        SurvivorsRankStatus::kAccepted, SurvivorsRankStatus::kPersonalBest,
        SurvivorsRankStatus::kLost, SurvivorsRankStatus::kImplausible,
        SurvivorsRankStatus::kRefused}) {
    standings.insert(
        ro::ui::SurvivorsStandingText({.status = status, .rank = 31, .best_score = 48'210}).data());
  }
  CHECK(standings.size() == 7);
  CHECK(
      std::string(ro::ui::SurvivorsStandingText(
                      {.status = SurvivorsRankStatus::kAccepted, .rank = 31, .best_score = 48'210})
                      .data()) == "Classement : 31e (votre meilleur : 48,210)");
  CHECK(std::string(
            ro::ui::SurvivorsStandingText({.status = SurvivorsRankStatus::kSending}).data()) ==
        "Envoi au serveur…");
}

TEST_CASE("Classement survivors : l'onglet local des parties classées seulement s'il est montré",
          "[ui][survivors]") {
  ro::ui::SurvivorsRecordsState state{.open = true, .tab = ro::ui::SurvivorsRecordsTab::kRanked};
  ro::ui::SurvivorsRecordsView view;
  const auto draw = [&] { DrawSurvivorsRecordsWindow(state, view, kNoArt); };
  CHECK_FALSE(Contains(TextOf(draw), "Parties classées"));
  CHECK(state.tab == ro::ui::SurvivorsRecordsTab::kFree);
  state.tab = ro::ui::SurvivorsRecordsTab::kChallenge;
  CHECK_FALSE(PressKey(ImGuiKey_Tab, [&] {
    draw();
    return state.tab == ro::ui::SurvivorsRecordsTab::kRanked;
  }));

  view.ranked_shown = true;
  state.tab = ro::ui::SurvivorsRecordsTab::kChallenge;
  CHECK(Contains(TextOf(draw), "Parties classées"));
  CHECK(PressKey(ImGuiKey_Tab, [&] {
    draw();
    return state.tab == ro::ui::SurvivorsRecordsTab::kRanked;
  }));
}

TEST_CASE("Classement survivors : le HUD « Classée » ne lit aucune entrée", "[ui][survivors]") {
  ro::ui::SurvivorsHudView view;
  const int unmarked = HudVerticesOf(view);
  view.ranked = true;
  CHECK(HudVerticesOf(view) > unmarked);

  ro::ui::UiContext ui;
  for (const ImGuiKey key : {ImGuiKey_Tab, ImGuiKey_Escape, ImGuiKey_Enter}) {
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

TEST_CASE("Classement survivors : le tableau et les records nomment leur classe",
          "[ui][survivors]") {
  constexpr const char* kClassLine = "Classe : Chevalier";
  ro::ui::SurvivorsRankingState state{.open = true};
  ro::ui::SurvivorsRankingView view{.ruleset = kRuleset};
  const auto draw = [&] { (void)DrawSurvivorsRankingWindow(state, view, kNoArt); };
  CHECK_FALSE(Contains(TextOf(draw), "Classe :"));
  view.class_name = "Chevalier";
  CHECK(Contains(TextOf(draw), kClassLine));

  // Une seule classe se nomme, dans tous les onglets.
  const std::array<ro::ui::SurvivorsClassChoice, 1> alone{{{.class_id = 2, .name = "Chevalier"}}};
  ro::ui::SurvivorsRecordsState records{.open = true};
  ro::ui::SurvivorsRecordsView records_view{.classes = alone, .class_id = 2, .ranked_shown = true};
  const auto draw_records = [&] { DrawSurvivorsRecordsWindow(records, records_view, kNoArt); };
  for (const ro::ui::SurvivorsRecordsTab tab :
       {ro::ui::SurvivorsRecordsTab::kFree, ro::ui::SurvivorsRecordsTab::kChallenge,
        ro::ui::SurvivorsRecordsTab::kRanked}) {
    records.tab = tab;
    CHECK(Contains(TextOf(draw_records), kClassLine));
  }
}

TEST_CASE("Records survivors : l'en-tête propose chaque classe, sans rien choisir seul",
          "[ui][survivors]") {
  const std::array<ro::ui::SurvivorsClassChoice, 3> classes{{
      {.class_id = 1, .name = "Magicien"},
      {.class_id = 2, .name = "Chevalier"},
      {.class_id = 3, .name = "Voleur", .locked = true},
  }};
  ro::ui::SurvivorsRecordsState records{.open = true, .class_id = 2};
  const ro::ui::SurvivorsRecordsView records_view{.classes = classes, .class_id = 2};
  const std::string text =
      TextOf([&] { DrawSurvivorsRecordsWindow(records, records_view, kNoArt); });
  CHECK(Contains(text, "Magicien"));
  CHECK(Contains(text, "Chevalier"));
  CHECK(Contains(text, "Voleur"));
  CHECK_FALSE(Contains(text, "Classe : "));
  CHECK(records.class_id == 2);
}

TEST_CASE("Classement survivors : l'en-tête propose chaque classe classée, sans rien choisir seul",
          "[ui][survivors]") {
  const std::array<ro::ui::SurvivorsClassChoice, 2> classes{{
      {.class_id = 1, .name = "Magicien"},
      {.class_id = 2, .name = "Chevalier"},
  }};
  ro::ui::SurvivorsRankingState state{.open = true};
  const ro::ui::SurvivorsRankingView view{
      .class_name = "Chevalier", .classes = classes, .class_id = 2, .ruleset = kRuleset};
  const std::string text = TextOf([&] { (void)DrawSurvivorsRankingWindow(state, view, kNoArt); });
  CHECK(Contains(text, "Magicien"));
  CHECK(Contains(text, "Chevalier"));
  CHECK_FALSE(Contains(text, "Classe : "));
  CHECK(state.class_id == 0);
}
