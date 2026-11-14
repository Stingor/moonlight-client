// Les fenêtres de la méta survivors — Église, Kafra, Album, records, l'accueil
// et l'écran de fin qui la montrent : chacune se dessine d'une vue vide comme
// d'une vue pleine, un achat refusé ne rend aucune intention, « Tout
// rembourser » attend sa confirmation, une carte non trouvée cache son
// illustration, et le HUD qui montre les charges ne lit toujours rien.

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/skin_art.h"
#include "ro/ui/skin_look.h"
#include "ro/ui/survivors_album_window.h"
#include "ro/ui/survivors_church_window.h"
#include "ro/ui/survivors_hud.h"
#include "ro/ui/survivors_kafra_window.h"
#include "ro/ui/survivors_prepare_window.h"
#include "ro/ui/survivors_records_window.h"
#include "ro/ui/survivors_shop.h"
#include "ro/ui/survivors_summary_window.h"
#include "ro/ui/ui_context.h"

namespace {

using ro::ui::SkinArt;
using ro::ui::SurvivorsShopRow;

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr int kSettleFrames = 3;
constexpr float kSweepStep = 10.0F;
constexpr float kBetweenClicksSeconds = 0.5F;
constexpr ImTextureID kSomeTexture = 7;
constexpr ImTextureID kIllustration = 11;
constexpr ImTextureID kCardBack = 12;
constexpr std::uint64_t kRichZeny = 1'000'000;
constexpr std::uint64_t kPoorZeny = 10;
const SkinArt kNoArt;

[[nodiscard]] int VerticesOf(const std::function<void()>& draw) {
  ro::ui::UiContext ui;
  int vertices = 0;
  for (int frame = 0; frame < kSettleFrames; ++frame) {
    ui.BeginFrame(kViewport, kFrameSeconds);
    draw();
    ui.EndFrame();
    vertices = ImGui::GetDrawData() != nullptr ? ImGui::GetDrawData()->TotalVtxCount : 0;
  }
  return vertices;
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

// Vrai si une image de `draw` a posé la texture `texture`. ⚠ PAS `GetTexID()`,
// qui assertionne sur la police qu'aucun moteur n'a chargée dans un banc.
[[nodiscard]] bool PaintsTexture(const std::function<void()>& draw, ImTextureID texture) {
  ro::ui::UiContext ui;
  bool painted = false;
  for (int frame = 0; frame < kSettleFrames; ++frame) {
    ui.BeginFrame(kViewport, kFrameSeconds);
    draw();
    ui.EndFrame();
    const ImDrawData* data = ImGui::GetDrawData();
    if (data == nullptr) continue;
    for (const ImDrawList* list : data->CmdLists) {
      for (const ImDrawCmd& command : list->CmdBuffer) {
        if (command.TexRef._TexData == nullptr && command.TexRef._TexID == texture) {
          painted = true;
        }
      }
    }
  }
  return painted;
}

// Le corps de l'Album dans une fenêtre à lui : le Codex le dessine dans son
// onglet Cartes, essayé par `survivors_codex_window_test`.
constexpr ImVec2 kAlbumAloneSize(480.0F, 600.0F);

void DrawAlbumAlone(ro::ui::SurvivorsAlbumState& state, const ro::ui::SurvivorsAlbumView& view) {
  ImGui::SetNextWindowSize(kAlbumAloneSize);
  if (ImGui::Begin("###survivors_album_alone")) ro::ui::DrawSurvivorsAlbumBody(state, view);
  ImGui::End();
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

const std::array<SurvivorsShopRow, 2> kBlessingRows = {
    SurvivorsShopRow{.key = "blessing",
                     .icon = kSomeTexture,
                     .name = "Blessing",
                     .description = "Dégâts de tous les skills augmentés.",
                     .rank = 2,
                     .most_rank = 5,
                     .effect_now = 6,
                     .effect_next = 9,
                     .next_price = 400},
    SurvivorsShopRow{
        .key = "resurrection", .name = "Resurrection", .rank = 1, .most_rank = 1, .effect_now = 1},
};

// Une ligne seule à acheter, au prix dit.
[[nodiscard]] std::array<SurvivorsShopRow, 1> OneRow(std::uint64_t price) {
  return {SurvivorsShopRow{
      .key = "gloria", .name = "Gloria", .most_rank = 3, .effect_next = 5, .next_price = price}};
}

}  // namespace

TEST_CASE("Survivors méta : chaque fenêtre se dessine, vue vide ou pleine", "[ui][survivors]") {
  const int empty = VerticesOf([] {});

  ro::ui::SurvivorsShopState shop{.open = true};
  const ro::ui::SurvivorsShopView shop_full{
      .rows = kBlessingRows, .zeny = kRichZeny, .refundable_zeny = 1'200};
  const int church_empty = VerticesOf([&] { (void)DrawSurvivorsChurchWindow(shop, {}, kNoArt); });
  CHECK(church_empty > empty);
  CHECK(VerticesOf([&] { (void)DrawSurvivorsChurchWindow(shop, shop_full, kNoArt); }) >
        church_empty);
  const int kafra_empty = VerticesOf([&] { (void)DrawSurvivorsKafraWindow(shop, {}, kNoArt); });
  CHECK(kafra_empty > empty);
  const std::array<SurvivorsShopRow, 1> kafra_rows = {
      SurvivorsShopRow{.key = "magnifier", .name = "Magnifier", .most_rank = 3, .next_price = 300}};
  CHECK(VerticesOf([&] {
          (void)DrawSurvivorsKafraWindow(shop, {.rows = kafra_rows, .zeny = kRichZeny}, kNoArt);
        }) > kafra_empty);

  ro::ui::SurvivorsAlbumState album{.detail = 0};
  const std::array<ro::ui::SurvivorsAlbumEntry, 2> cards = {
      ro::ui::SurvivorsAlbumEntry{.illustration = kIllustration,
                                  .illustration_size = ImVec2(75.0F, 100.0F),
                                  .name = "Poring Card",
                                  .found = 3,
                                  .first_found_day = 20'000},
      ro::ui::SurvivorsAlbumEntry{.name = "Golem Card", .dropped_by = "Golem"}};
  const std::array<ro::ui::SurvivorsFeatEntry, 2> feats = {
      ro::ui::SurvivorsFeatEntry{.key = "sight",
                                 .name = "Sight",
                                 .measure = ro::ui::SurvivorsFeatMeasure::kSurvivedMs,
                                 .progress = 192'000,
                                 .threshold = 300'000},
      ro::ui::SurvivorsFeatEntry{.key = "thunderstorm", .name = "Thunderstorm", .soon = true}};
  const int album_empty = VerticesOf([&] { DrawAlbumAlone(album, {}); });
  CHECK(album_empty > empty);
  CHECK(VerticesOf([&] {
          DrawAlbumAlone(album, {.cards = cards, .card_back = kCardBack, .feats = feats});
        }) > album_empty);

  const std::array<ro::ui::SurvivorsRecordRow, 2> runs = {
      ro::ui::SurvivorsRecordRow{.score = 12'000,
                                 .survived_ms = 900'000,
                                 .level = 31,
                                 .outcome = ro::ui::SurvivorsEndOutcome::kVictory,
                                 .latest = true},
      ro::ui::SurvivorsRecordRow{.score = 3'000, .older_rules = true}};
  for (const ro::ui::SurvivorsRecordsTab tab :
       {ro::ui::SurvivorsRecordsTab::kFree, ro::ui::SurvivorsRecordsTab::kChallenge}) {
    ro::ui::SurvivorsRecordsState records{.open = true, .tab = tab};
    const int records_empty = VerticesOf([&] { DrawSurvivorsRecordsWindow(records, {}, kNoArt); });
    CHECK(records_empty > empty);
    CHECK(VerticesOf([&] {
            DrawSurvivorsRecordsWindow(
                records, {.free_runs = runs, .week_runs = runs, .past_weeks = runs}, kNoArt);
          }) > records_empty);
  }

  ro::ui::SurvivorsPrepareState prepare{.open = true};
  const int prepare_plain =
      VerticesOf([&] { (void)DrawSurvivorsPrepareWindow(prepare, {}, kNoArt); });
  for (const ro::ui::SurvivorsBoardTab board :
       {ro::ui::SurvivorsBoardTab::kFree, ro::ui::SurvivorsBoardTab::kChallenge}) {
    ro::ui::SurvivorsPrepareView with_meta;
    with_meta.meta = ro::ui::SurvivorsPrepareMeta{.board = board,
                                                  .zeny = 5'678,
                                                  .most_cursed_water = 5,
                                                  .week_year = 2026,
                                                  .week_number = 43};
    CHECK(VerticesOf([&] { (void)DrawSurvivorsPrepareWindow(prepare, with_meta, kNoArt); }) >
          prepare_plain);
  }

  const std::array<ro::ui::SurvivorsDroppedCardRow, 1> dropped = {
      ro::ui::SurvivorsDroppedCardRow{.illustration = kSomeTexture, .first_time = true}};
  const std::array<ro::ui::SurvivorsTrophyRow, 1> trophies = {
      ro::ui::SurvivorsTrophyRow{.icon = kSomeTexture, .name = "Première carte"}};
  ro::ui::SurvivorsSummaryView with_credit;
  with_credit.credit = ro::ui::SurvivorsCreditRows{.zeny_gained = 1'234,
                                                   .zeny_balance = 5'678,
                                                   .record = ro::ui::SurvivorsRecordNews::kPlaced,
                                                   .record_rank = 1,
                                                   .dropped = dropped,
                                                   .feats = feats};
  with_credit.trophies = trophies;
  CHECK(VerticesOf([&] { (void)DrawSurvivorsSummaryWindow(with_credit, kNoArt); }) >
        VerticesOf([&] { (void)DrawSurvivorsSummaryWindow({}, kNoArt); }));
}

TEST_CASE("Survivors méta : l'écran de fin montre ses succès même sans crédit", "[ui][survivors]") {
  const std::array<ro::ui::SurvivorsTrophyRow, 1> trophies = {
      ro::ui::SurvivorsTrophyRow{.icon = kSomeTexture, .name = "Première carte"}};
  ro::ui::SurvivorsSummaryView summary;
  CHECK(TextOf([&] { (void)DrawSurvivorsSummaryWindow(summary, kNoArt); }).find("Première carte") ==
        std::string::npos);
  summary.trophies = trophies;
  CHECK(TextOf([&] { (void)DrawSurvivorsSummaryWindow(summary, kNoArt); }).find("Première carte") !=
        std::string::npos);
}

TEST_CASE("Survivors méta : l'exploit neuf et l'ancien se disent tous deux atteints",
          "[ui][survivors]") {
  std::array<ro::ui::SurvivorsFeatEntry, 1> feats = {ro::ui::SurvivorsFeatEntry{
      .key = "sight", .name = "Sight", .threshold = 300'000, .reached = true}};
  ro::ui::SurvivorsSummaryView summary;
  summary.credit = ro::ui::SurvivorsCreditRows{.feats = feats};
  const auto draw = [&] { (void)DrawSurvivorsSummaryWindow(summary, kNoArt); };
  CHECK(TextOf(draw).find("Exploit atteint : Sight") != std::string::npos);
  feats[0].fresh = true;
  CHECK(TextOf(draw).find("Exploit atteint : Sight") != std::string::npos);
}

namespace {

// L'art de l'ascenseur seul, sous une texture à lui : la voir posée, c'est
// voir l'ascenseur peint.
constexpr ImTextureID kScrollTexture = 41;
constexpr float kScrollPiecePixels = 13.0F;
// Bien plus de lignes que la hauteur fixe de la liste n'en montre.
constexpr std::size_t kManyRows = 40;

[[nodiscard]] SkinArt ScrollArt() {
  SkinArt art;
  for (const ro::render::SkinPiece piece :
       {ro::render::SkinPiece::kScrollUp, ro::render::SkinPiece::kScrollMiddle,
        ro::render::SkinPiece::kScrollDown, ro::render::SkinPiece::kScrollGrabTop,
        ro::render::SkinPiece::kScrollGrabMiddle, ro::render::SkinPiece::kScrollGrabBottom}) {
    art.Set(piece,
            ro::ui::SkinImage{
                .id = kScrollTexture, .width = kScrollPiecePixels, .height = kScrollPiecePixels});
  }
  return art;
}

}  // namespace

TEST_CASE("Survivors méta : une liste plus haute que son cadre peint son ascenseur",
          "[ui][survivors]") {
  const SkinArt art = ScrollArt();
  std::vector<ro::ui::SurvivorsFoeRow> foes(2,
                                            ro::ui::SurvivorsFoeRow{.name = "Poring", .kills = 3});
  ro::ui::SurvivorsSummaryView summary;
  summary.foes = foes;
  const auto draw_summary = [&] { (void)DrawSurvivorsSummaryWindow(summary, art); };
  CHECK_FALSE(PaintsTexture(draw_summary, kScrollTexture));
  foes.resize(kManyRows, ro::ui::SurvivorsFoeRow{.name = "Poring", .kills = 3});
  summary.foes = foes;
  CHECK(PaintsTexture(draw_summary, kScrollTexture));

  // L'Église et la Kafra : la liste défile entre le solde et le pied.
  std::vector<SurvivorsShopRow> rows(kManyRows, OneRow(400)[0]);
  ro::ui::SurvivorsShopState shop{.open = true};
  CHECK(PaintsTexture(
      [&] { (void)DrawSurvivorsChurchWindow(shop, {.rows = rows, .zeny = kRichZeny}, art); },
      kScrollTexture));
}

TEST_CASE("Survivors méta : l'or des fenêtres se lit sur leur corps clair", "[ui][survivors]") {
  // Le seuil d'un texte ordinaire : record, carte neuve et exploit sont des
  // lignes de texte fin, pas des titres.
  constexpr float kBodyTextContrast = 4.5F;
  CHECK(ro::ui::ContrastRatio(ro::ui::kSkinGoldTextColor, ro::ui::kSkinBodyColor) >=
        kBodyTextContrast);
}

TEST_CASE("Survivors méta : une boutique fermée oublie la confirmation de remboursement",
          "[ui][survivors]") {
  ro::ui::SurvivorsShopState state{.open = true, .confirming_refund = true};
  const ro::ui::SurvivorsShopView view{
      .rows = std::span(kBlessingRows).last(1), .zeny = 0, .refundable_zeny = 12'340};
  CHECK(TextOf([&] { (void)DrawSurvivorsChurchWindow(state, view, kNoArt); }).find("Rendre") !=
        std::string::npos);
  // Fermée par Échap, qui ne touche que `open`, puis rouverte comme l'ouvre
  // `app` : aucun « Oui » à cliquer par mégarde.
  state.open = false;
  state.Open();
  CHECK_FALSE(state.confirming_refund);
  CHECK(TextOf([&] { (void)DrawSurvivorsChurchWindow(state, view, kNoArt); }).find("Rendre") ==
        std::string::npos);
}

TEST_CASE("Survivors méta : un achat refusé ne rend aucune intention", "[ui][survivors]") {
  CHECK(ro::ui::RefusalOf(OneRow(400)[0], kPoorZeny) ==
        ro::ui::SurvivorsShopRefusal::kNotEnoughZeny);
  CHECK(ro::ui::RefusalOf(kBlessingRows[1], kRichZeny) == ro::ui::SurvivorsShopRefusal::kMostRank);
  CHECK_FALSE(ro::ui::RefusalOf(OneRow(400)[0], 400).has_value());

  const std::array<SurvivorsShopRow, 1> row = OneRow(400);
  ro::ui::SurvivorsShopState state;
  ro::ui::SurvivorsShopView view{.rows = row, .zeny = kPoorZeny};
  const auto buys = [&]() {
    state.open = true;
    return DrawSurvivorsChurchWindow(state, view, kNoArt).buy_row.has_value();
  };
  CHECK_FALSE(SweepClicks("###survivors_church", buys));
  CHECK_FALSE(PressKey(ImGuiKey_Enter, buys));
  // Le solde suffit : le même balayage trouve « Bénir », et Entrée achète.
  view.zeny = kRichZeny;
  CHECK(SweepClicks("###survivors_church", buys));
  CHECK(PressKey(ImGuiKey_Enter, buys));

  // La Kafra suit la même règle.
  const std::array<SurvivorsShopRow, 1> kafra = {SurvivorsShopRow{
      .key = "fly_wing", .name = "Fly Wing", .rank = 3, .most_rank = 3, .effect_now = 3}};
  const auto kafra_buys = [&]() {
    state.open = true;
    return DrawSurvivorsKafraWindow(state, {.rows = kafra, .zeny = kRichZeny}, kNoArt)
        .buy_row.has_value();
  };
  CHECK_FALSE(SweepClicks("###survivors_kafra", kafra_buys));
}

TEST_CASE("Survivors méta : « Tout rembourser » ne rend rien avant la confirmation",
          "[ui][survivors]") {
  ro::ui::SurvivorsShopState state;
  const ro::ui::SurvivorsShopView view{
      .rows = std::span(kBlessingRows).last(1), .zeny = 0, .refundable_zeny = 12'340};
  // La confirmation effacée à chaque image : aucun clic ne rembourse.
  CHECK_FALSE(SweepClicks("###survivors_church", [&]() {
    state.open = true;
    state.confirming_refund = false;
    return DrawSurvivorsChurchWindow(state, view, kNoArt).refund_all;
  }));
  // Un clic sur le bouton demande la confirmation, sans rien rendre.
  bool refunded = false;
  CHECK(SweepClicks("###survivors_church", [&]() {
    state.open = true;
    refunded = DrawSurvivorsChurchWindow(state, view, kNoArt).refund_all || refunded;
    return state.confirming_refund;
  }));
  CHECK_FALSE(refunded);
  // La confirmation posée, « Oui » rembourse.
  CHECK(SweepClicks("###survivors_church", [&]() {
    state.open = true;
    return DrawSurvivorsChurchWindow(state, view, kNoArt).refund_all;
  }));
  CHECK_FALSE(state.confirming_refund);
  CHECK(TextOf([&] {
          state.open = true;
          state.confirming_refund = true;
          (void)DrawSurvivorsChurchWindow(state, view, kNoArt);
        }).find("Rendre 12,340 Zeny ?") != std::string::npos);
}

TEST_CASE("Survivors méta : une carte non trouvée cache son illustration", "[ui][survivors]") {
  std::array<ro::ui::SurvivorsAlbumEntry, 1> cards = {
      ro::ui::SurvivorsAlbumEntry{.illustration = kIllustration,
                                  .illustration_size = ImVec2(75.0F, 100.0F),
                                  .name = "Golem Card",
                                  .dropped_by = "Golem"}};
  ro::ui::SurvivorsAlbumState state{.detail = 0};
  const auto draw = [&] { DrawAlbumAlone(state, {.cards = cards, .card_back = kCardBack}); };
  CHECK_FALSE(PaintsTexture(draw, kIllustration));
  CHECK(PaintsTexture(draw, kCardBack));
  CHECK(TextOf(draw).find("Tombe de : Golem") != std::string::npos);
  cards[0].found = 1;
  CHECK(PaintsTexture(draw, kIllustration));
}

TEST_CASE("Survivors méta : l'onglet du défi montre sa semaine", "[ui][survivors]") {
  CHECK(std::string(ro::ui::SurvivorsWeekText(2026, 43).data()) == "Semaine 43 de 2026");
  const ro::ui::SurvivorsRecordsView view{.week_year = 2026, .week_number = 43};
  ro::ui::SurvivorsRecordsState state{.open = true, .tab = ro::ui::SurvivorsRecordsTab::kFree};
  const auto draw = [&] { DrawSurvivorsRecordsWindow(state, view, kNoArt); };
  CHECK(TextOf(draw).find("Semaine 43") == std::string::npos);
  state.tab = ro::ui::SurvivorsRecordsTab::kChallenge;
  CHECK(TextOf(draw).find("Semaine 43 de 2026") != std::string::npos);
}

TEST_CASE("Survivors méta : l'accueil rend ses intentions, l'écran de fin son Église",
          "[ui][survivors]") {
  ro::ui::SurvivorsPrepareState prepare{.open = true};
  ro::ui::SurvivorsPrepareView view;
  view.meta = ro::ui::SurvivorsPrepareMeta{.most_cursed_water = 5};
  const auto draw = [&](const std::function<bool(const ro::ui::SurvivorsPrepareAsk&)>& wanted) {
    return [&, wanted]() {
      prepare.open = true;
      return wanted(DrawSurvivorsPrepareWindow(prepare, view, kNoArt));
    };
  };
  CHECK(SweepClicks("###survivors_prepare", draw([](const auto& asked) {
                      return asked.choose_board == ro::ui::SurvivorsBoardTab::kChallenge;
                    })));
  CHECK(SweepClicks("###survivors_prepare",
                    draw([](const auto& asked) { return asked.choose_cursed_water.has_value(); })));
  CHECK(SweepClicks("###survivors_prepare", draw([](const auto& asked) {
                      return asked.open_place == ro::ui::SurvivorsMetaPlace::kRecords;
                    })));

  ro::ui::SurvivorsSummaryView summary{.accepts_input = true};
  const auto church = [&]() { return DrawSurvivorsSummaryWindow(summary, kNoArt).church; };
  CHECK_FALSE(SweepClicks("###survivors_summary", church));
  summary.credit = ro::ui::SurvivorsCreditRows{.zeny_gained = 1'234, .zeny_balance = 5'678};
  CHECK(TextOf([&] { (void)church(); }).find("+1,234 Zeny (solde 5,678)") != std::string::npos);
  CHECK(SweepClicks("###survivors_summary", church));
}

TEST_CASE("Survivors méta : la partie libre choisit son arène, le défi et la classée la nomment",
          "[ui][survivors]") {
  constexpr std::uint16_t kFieldArena = 1;
  constexpr std::uint16_t kPlainsArena = 2;
  const std::array<ro::ui::SurvivorsArenaChoice, 2> arenas{{
      {.arena_id = kPlainsArena, .name = "Prairie de Midgard"},
      {.arena_id = kFieldArena, .name = "Champ d'essai"},
  }};
  ro::ui::SurvivorsPrepareState prepare{.open = true};
  ro::ui::SurvivorsPrepareView view;
  view.meta = ro::ui::SurvivorsPrepareMeta{.free_arenas = arenas,
                                           .free_arena_id = kPlainsArena,
                                           .board_arena_name = "Prairie de Midgard"};
  const auto asks_arena = [&](std::optional<std::uint16_t> wanted) {
    return [&, wanted]() {
      prepare.open = true;
      const ro::ui::SurvivorsPrepareAsk asked = DrawSurvivorsPrepareWindow(prepare, view, kNoArt);
      return wanted.has_value() ? asked.choose_arena_id == wanted
                                : asked.choose_arena_id.has_value();
    };
  };
  const auto text = [&] { return TextOf([&] { (void)asks_arena(std::nullopt)(); }); };

  // LA PARTIE LIBRE : les deux arènes, la retenue ne se redemande pas.
  CHECK(text().find("Champ d'essai") != std::string::npos);
  CHECK(SweepClicks("###survivors_prepare", asks_arena(kFieldArena)));
  CHECK_FALSE(SweepClicks("###survivors_prepare", asks_arena(kPlainsArena)));

  // LE DÉFI ET LA PARTIE CLASSÉE : leur arène nommée, aucun choix.
  view.meta->board = ro::ui::SurvivorsBoardTab::kChallenge;
  CHECK(text().find("Arène : Prairie de Midgard") != std::string::npos);
  CHECK(text().find("Champ d'essai") == std::string::npos);
  CHECK_FALSE(SweepClicks("###survivors_prepare", asks_arena(std::nullopt)));
  view.meta->board = ro::ui::SurvivorsBoardTab::kRanked;
  view.meta->ranking = ro::ui::SurvivorsPrepareRanking{};
  CHECK(text().find("Arène : Prairie de Midgard") != std::string::npos);
  CHECK(text().find("Champ d'essai") == std::string::npos);
  CHECK_FALSE(SweepClicks("###survivors_prepare", asks_arena(std::nullopt)));

  // UNE SEULE ARÈNE PROPOSÉE se nomme, sans choix.
  view.meta->board = ro::ui::SurvivorsBoardTab::kFree;
  view.meta->free_arenas = std::span(arenas).first(1);
  CHECK(text().find("Arène : Prairie de Midgard") != std::string::npos);
  CHECK_FALSE(SweepClicks("###survivors_prepare", asks_arena(std::nullopt)));
}

TEST_CASE("Survivors méta : le HUD montre charges et vies sans lire d'entrée", "[ui][survivors]") {
  const std::array<ro::ui::SurvivorsHudCharge, 3> charges = {
      ro::ui::SurvivorsHudCharge{.icon = kSomeTexture, .count = 2},
      ro::ui::SurvivorsHudCharge{.count = 0}, ro::ui::SurvivorsHudCharge{.count = 1}};
  ro::ui::SurvivorsHudView view;
  const int plain = VerticesOf([&] { ro::ui::DrawSurvivorsHud(view); });
  view.charges = charges;
  view.lives = 1;
  view.life_icon = kSomeTexture;
  view.cursed_water = 3;
  CHECK(VerticesOf([&] { ro::ui::DrawSurvivorsHud(view); }) > plain);

  view.challenge = true;
  ro::ui::UiContext ui;
  for (const ImGuiKey key : {ImGuiKey_1, ImGuiKey_R, ImGuiKey_Escape, ImGuiKey_Enter}) {
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

TEST_CASE("Survivors méta : la classe se choisit dans tous les onglets, jamais une verrouillée",
          "[ui][survivors]") {
  constexpr std::uint16_t kMageClass = 1;
  constexpr std::uint16_t kKnightClass = 2;
  std::array<ro::ui::SurvivorsClassChoice, 2> classes{{
      {.class_id = kMageClass, .name = "Magicien"},
      {.class_id = kKnightClass,
       .name = "Chevalier",
       .locked = true,
       .unlock_hint = "Chevalier : une partie tenue 10:00 le débloque."},
  }};
  ro::ui::SurvivorsPrepareState prepare{.open = true};
  ro::ui::SurvivorsPrepareView view;
  view.meta = ro::ui::SurvivorsPrepareMeta{
      .ranking = ro::ui::SurvivorsPrepareRanking{}, .classes = classes, .class_id = kMageClass};
  const auto asks_class = [&](std::uint16_t wanted) {
    return [&, wanted]() {
      prepare.open = true;
      return DrawSurvivorsPrepareWindow(prepare, view, kNoArt).choose_class_id == wanted;
    };
  };
  const auto text = [&] { return TextOf([&] { (void)asks_class(kMageClass)(); }); };
  for (const ro::ui::SurvivorsBoardTab tab :
       {ro::ui::SurvivorsBoardTab::kFree, ro::ui::SurvivorsBoardTab::kChallenge,
        ro::ui::SurvivorsBoardTab::kRanked}) {
    view.meta->board = tab;
    classes[1].locked = true;
    const std::string locked = text();
    CHECK(locked.find("Magicien") != std::string::npos);
    CHECK(locked.find("une partie tenue 10:00") != std::string::npos);
    CHECK_FALSE(SweepClicks("###survivors_prepare", asks_class(kKnightClass)));
    classes[1].locked = false;
    CHECK(text().find("une partie tenue 10:00") == std::string::npos);
    CHECK(SweepClicks("###survivors_prepare", asks_class(kKnightClass)));
    CHECK_FALSE(SweepClicks("###survivors_prepare", asks_class(kMageClass)));
  }
}
