// LE PANNEAU DE SUIVI, dessiné hors écran : sa hauteur selon ce qui est plié,
// son silence sans entrée, la flèche qui ouvre, le guidage, le menu d'une
// entrée, et les réglages qui rendent un nouvel aspect.

#include "ro/ui/tracker_panel.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/skin_pieces.h"
#include "ro/ui/ui_context.h"

using ro::ui::TrackerEntry;
using ro::ui::TrackerKind;
using ro::ui::TrackerLine;
using ro::ui::TrackerLook;
using ro::ui::TrackerPanelAction;
using ro::ui::TrackerPanelState;
using ro::ui::TrackerSection;
using ro::ui::TrackerTarget;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr float kPlainPiecePixels = 8.0F;
constexpr int kSettleFrames = 4;
constexpr std::uint32_t kPoringHunt = 7001;
constexpr std::uint32_t kLostQuest = 7002;
constexpr std::uint32_t kFirstSteps = 12;

// Les mesures du panneau, en pixels d'art : le haut et le pied, un en-tête de
// section, une entrée pliée, le pas d'une ligne, sa barre, la marge sous une
// entrée dépliée, l'écart entre deux sections, la ligne « +N autres ».
constexpr float kTopAndBottom = 15.0F;
constexpr float kHeader = 18.0F;
constexpr float kFolded = 20.0F;
constexpr float kPitch = 15.0F;
constexpr float kBarRoom = 4.0F;
constexpr float kUnfoldedGap = 5.0F;
constexpr float kSectionGap = 6.0F;
constexpr float kMore = 16.0F;
// La flèche qui ouvre, et le bouton de guidage à sa gauche.
constexpr float kArrowX = 293.0F;
constexpr float kGuideX = 280.0F;
// Le haut de la première entrée sous un en-tête, et un point dans sa ligne.
constexpr float kFirstEntryTop = 28.0F;
constexpr float kIntoLine = 5.0F;

[[nodiscard]] ro::ui::SkinArt PlainArt() {
  ro::ui::SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<ro::render::SkinPiece>(rank),
            ro::ui::SkinImage{.id = 1, .width = kPlainPiecePixels, .height = kPlainPiecePixels});
  }
  return art;
}

struct PanelProbe {
  ro::ui::UiContext ui;
  ro::ui::SkinArt art = PlainArt();
  TrackerPanelState state;
  TrackerLook look;
  std::array<TrackerLine, 2> hunt_lines = {TrackerLine{.text = "Poring 3 / 10", .progress = 0.3F},
                                           TrackerLine{.text = "Lunatic 10 / 10", .reached = true}};
  std::array<TrackerLine, 1> lost_lines = {TrackerLine{.text = "Parler au PNJ"}};
  std::array<TrackerEntry, 2> quests = {
      TrackerEntry{.key = kPoringHunt,
                   .name = "Chasse aux Porings",
                   .badge = "2 h 13 min",
                   .badge_urgent = true,
                   .lines = hunt_lines,
                   .can_guide = true},
      TrackerEntry{.key = kLostQuest, .name = "Le pendentif perdu", .lines = lost_lines}};
  std::array<TrackerEntry, 1> achievements = {
      TrackerEntry{.key = kFirstSteps, .name = "Premiers pas", .completed = true}};
  std::vector<TrackerSection> sections;

  PanelProbe() {
    state.shown = true;
    sections = {
        TrackerSection{.kind = TrackerKind::kQuest, .label = "Quêtes (2)", .entries = quests},
        TrackerSection{
            .kind = TrackerKind::kAchievement, .label = "Succès (1)", .entries = achievements}};
  }

  TrackerPanelAction Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    TrackerPanelAction action =
        ro::ui::DrawTrackerPanel(ro::ui::kTrackerPanelId, state, sections, look, 0.0F, art);
    ui.EndFrame();
    return action;
  }

  void Settle() {
    for (int frame = 0; frame < kSettleFrames; ++frame) {
      static_cast<void>(Frame());
    }
  }

  [[nodiscard]] float Height() const { return ro::ui::TrackerPanelHeight(state, sections); }
};

[[nodiscard]] ImGuiWindow* Panel() { return ImGui::FindWindowByName(ro::ui::kTrackerPanelId); }

// Un point du panneau, en pixels d'art depuis son coin.
[[nodiscard]] ImVec2 PanelPoint(float x, float y) {
  const ImGuiWindow* window = Panel();
  REQUIRE(window != nullptr);
  const float scale = ImGui::GetStyle().FontScaleMain;
  return {window->Pos.x + x * scale, window->Pos.y + y * scale};
}

struct Click {
  TrackerPanelAction pressed;
  TrackerPanelAction released;
};

Click ClickAt(PanelProbe& probe, ImVec2 at, ImGuiMouseButton button = ImGuiMouseButton_Left) {
  ImGuiIO& io = ImGui::GetIO();
  io.AddMousePosEvent(at.x, at.y);
  static_cast<void>(probe.Frame());
  Click click;
  io.AddMouseButtonEvent(button, true);
  click.pressed = probe.Frame();
  io.AddMouseButtonEvent(button, false);
  click.released = probe.Frame();
  return click;
}

[[nodiscard]] bool PopupOpen(const char* popup_id) {
  const ImGuiWindow* window = Panel();
  REQUIRE(window != nullptr);
  const ImGuiID wanted = ImHashStr(popup_id, 0, window->ID);
  return std::ranges::any_of(GImGui->OpenPopupStack,
                             [&](const ImGuiPopupData& popup) { return popup.PopupId == wanted; });
}

}  // namespace

TEST_CASE("Suivi : la hauteur suit les entrées, les lignes et les sections pliées", "[ui][suivi]") {
  PanelProbe probe;
  // Deux en-têtes, trois entrées pliées, l'écart entre les sections.
  const float folded = kTopAndBottom + 2.0F * kHeader + 3.0F * kFolded + kSectionGap;
  CHECK(probe.Height() == folded);

  // La chasse dépliée : son titre, une ligne à barre, une ligne nue, la marge.
  probe.state.expanded.emplace_back(TrackerKind::kQuest, kPoringHunt);
  const float unfolded = kPitch + (kPitch + kBarRoom) + kPitch + kUnfoldedGap;
  CHECK(probe.Height() == folded - kFolded + unfolded);

  // LE MÊME IDENTIFIANT DANS L'AUTRE GENRE ne déplie rien ici.
  probe.state.expanded.clear();
  probe.state.expanded.emplace_back(TrackerKind::kAchievement, kPoringHunt);
  CHECK(probe.Height() == folded);

  // La section des quêtes pliée : son en-tête seul reste.
  probe.state.folded_sections.push_back(TrackerKind::kQuest);
  CHECK(probe.Height() == folded - 2.0F * kFolded);

  // Ce qui est caché s'annonce sur une ligne de plus.
  probe.state.folded_sections.clear();
  probe.sections[0].more = "+3 autres";
  CHECK(probe.Height() == folded + kMore);

  // Une section sans en-tête n'en paie pas la hauteur ; une section vide ne
  // compte pas du tout.
  probe.sections[0].more = {};
  probe.sections[0].label = {};
  probe.sections[1].entries = {};
  CHECK(probe.Height() == kTopAndBottom + 2.0F * kFolded);
}

TEST_CASE("Suivi : sans entrée, ou caché, rien ne se dessine", "[ui][suivi]") {
  PanelProbe probe;
  for (TrackerSection& section : probe.sections) {
    section.entries = {};
  }
  probe.Settle();
  CHECK(Panel() == nullptr);

  PanelProbe hidden;
  hidden.state.shown = false;
  hidden.Settle();
  CHECK(Panel() == nullptr);
}

TEST_CASE("Suivi : la flèche ouvre, l'épingle guide, le titre déplie", "[ui][suivi]") {
  PanelProbe probe;
  probe.Settle();
  REQUIRE(Panel() != nullptr);
  const Click open = ClickAt(probe, PanelPoint(kArrowX, kFirstEntryTop + kIntoLine));
  REQUIRE(open.released.open.has_value());
  CHECK(*open.released.open == TrackerTarget{.kind = TrackerKind::kQuest, .key = kPoringHunt});
  CHECK_FALSE(open.released.guide.has_value());

  const Click guide = ClickAt(probe, PanelPoint(kGuideX, kFirstEntryTop + kIntoLine));
  REQUIRE(guide.released.guide.has_value());
  CHECK(guide.released.guide->key == kPoringHunt);
  CHECK_FALSE(guide.released.open.has_value());

  // La seconde entrée n'a pas de lieu : pas d'épingle sous la souris.
  const Click none = ClickAt(probe, PanelPoint(kGuideX, kFirstEntryTop + kFolded + kIntoLine));
  CHECK_FALSE(none.released.guide.has_value());

  static_cast<void>(ClickAt(probe, PanelPoint(kGuideX / 4.0F, kFirstEntryTop + kIntoLine)));
  CHECK(probe.state.expanded ==
        std::vector<std::pair<TrackerKind, std::uint32_t>>{{TrackerKind::kQuest, kPoringHunt}});
}

TEST_CASE("Suivi : l'en-tête d'une section la replie", "[ui][suivi]") {
  PanelProbe probe;
  probe.Settle();
  const float before = Panel()->Size.y;
  static_cast<void>(ClickAt(probe, PanelPoint(kIntoLine * 2.0F, kTopAndBottom)));
  probe.Settle();
  CHECK(probe.state.folded_sections == std::vector<TrackerKind>{TrackerKind::kQuest});
  CHECK(Panel()->Size.y < before);
}

TEST_CASE("Suivi : le clic droit ouvre le menu de l'entrée, ou les réglages", "[ui][suivi]") {
  PanelProbe probe;
  probe.Settle();
  static_cast<void>(ClickAt(probe, PanelPoint(kGuideX / 2.0F, kFirstEntryTop + kIntoLine),
                            ImGuiMouseButton_Right));
  CHECK(PopupOpen("##entree_suivi"));
  CHECK_FALSE(PopupOpen("##reglages_suivi"));

  PanelProbe other;
  other.Settle();
  // Sur l'en-tête : aucune entrée visée, ce sont les réglages.
  static_cast<void>(ClickAt(other, PanelPoint(kGuideX, kTopAndBottom), ImGuiMouseButton_Right));
  CHECK(PopupOpen("##reglages_suivi"));

  // Les clics qui traversent n'ouvrent rien.
  PanelProbe through;
  through.look.click_through = true;
  through.Settle();
  static_cast<void>(ClickAt(through, PanelPoint(kGuideX, kTopAndBottom), ImGuiMouseButton_Right));
  CHECK_FALSE(PopupOpen("##reglages_suivi"));
}

TEST_CASE("Suivi : « Ne plus suivre » du menu d'une entrée la vise", "[ui][suivi]") {
  PanelProbe probe;
  probe.Settle();
  // La seconde entrée : le menu doit viser celle qu'on a cliquée, pas la
  // première.
  static_cast<void>(ClickAt(probe, PanelPoint(kGuideX / 2.0F, kFirstEntryTop + kFolded + kIntoLine),
                            ImGuiMouseButton_Right));
  REQUIRE_FALSE(GImGui->OpenPopupStack.empty());
  ImGuiWindow* menu = GImGui->OpenPopupStack.back().Window;
  REQUIRE(menu != nullptr);
  const ImGuiID untrack_id = menu->GetID("Ne plus suivre");
  ImGuiIO& io = ImGui::GetIO();
  std::optional<ImVec2> found;
  for (float y = menu->Pos.y + 1.0F; y < menu->Pos.y + menu->Size.y && !found; y += 2.0F) {
    io.AddMousePosEvent(menu->Pos.x + menu->Size.x * 0.5F, y);
    static_cast<void>(probe.Frame());
    if (GImGui->HoveredId == untrack_id) {
      found = ImVec2(menu->Pos.x + menu->Size.x * 0.5F, y);
    }
  }
  REQUIRE(found.has_value());
  const Click click = ClickAt(probe, *found);
  const std::optional<TrackerTarget> untrack =
      click.released.untrack.has_value() ? click.released.untrack : click.pressed.untrack;
  REQUIRE(untrack.has_value());
  CHECK(*untrack == TrackerTarget{.kind = TrackerKind::kQuest, .key = kLostQuest});
}

TEST_CASE("Suivi : les réglages rendent l'aspect changé, et rien sans geste", "[ui][suivi]") {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art = PlainArt();
  const TrackerLook look;
  constexpr const char* kBench = "banc_reglages_suivi";
  const auto frame = [&] {
    ui.BeginFrame(kViewport, kFrameSeconds);
    ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
    ImGui::SetNextWindowSize(ImVec2(600.0F, 600.0F));
    ImGui::Begin(kBench);
    const ro::ui::TrackerSettingsAnswer answer = ro::ui::DrawTrackerSettingsItems(look, art);
    ImGui::End();
    ui.EndFrame();
    return answer;
  };
  for (int rank = 0; rank < kSettleFrames; ++rank) {
    const ro::ui::TrackerSettingsAnswer idle = frame();
    CHECK_FALSE(idle.look.has_value());
    CHECK_FALSE(idle.reset_place);
  }
  // La case « Verrouiller la position », cherchée au survol puis cliquée.
  ImGuiWindow* bench = ImGui::FindWindowByName(kBench);
  REQUIRE(bench != nullptr);
  const ImGuiID lock_id = bench->GetID("Verrouiller la position");
  std::optional<ImVec2> found;
  ImGuiIO& io = ImGui::GetIO();
  for (float y = bench->Pos.y + 1.0F; y < bench->Pos.y + bench->Size.y && !found; y += 3.0F) {
    io.AddMousePosEvent(bench->Pos.x + 20.0F, y);
    static_cast<void>(frame());
    if (GImGui->HoveredId == lock_id) {
      found = ImVec2(bench->Pos.x + 20.0F, y);
    }
  }
  REQUIRE(found.has_value());
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  ro::ui::TrackerSettingsAnswer answer = frame();
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  const ro::ui::TrackerSettingsAnswer released = frame();
  if (!answer.look.has_value()) {
    answer = released;
  }
  REQUIRE(answer.look.has_value());
  CHECK(answer.look->locked);
  // Le reste n'a pas bougé.
  TrackerLook expected = look;
  expected.locked = true;
  CHECK(*answer.look == expected);
}
