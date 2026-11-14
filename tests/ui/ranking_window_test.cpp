// LE TABLEAU DES CLASSEMENTS, dessiné hors jeu : dix lignes et les points du
// joueur, « récupération » tant qu'un nom manque, et l'onglet cliqué qui devient
// la demande de son classement.

#include "ro/ui/ranking_window.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/ui_context.h"

using ro::ui::RankingRowView;
using ro::ui::RankingTab;
using ro::ui::RankingWindowAction;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr int kSettleFrames = 3;
constexpr std::uint32_t kOwnPoints = 4321;
constexpr std::uint32_t kFirstPoints = 987;
// Dix rangs, et le dernier sans nom.
constexpr std::size_t kRowCount = 10;
constexpr std::size_t kNamelessRank = kRowCount - 1;

struct RankingProbe {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  ro::ui::RankingWindowState state;
  std::array<std::string, kRowCount> names{};
  std::array<RankingRowView, kRowCount> rows{};
  ro::ui::RankingView view;
  std::string logged;

  RankingProbe() {
    for (std::size_t rank = 0; rank < rows.size(); ++rank) {
      names[rank] = "Joueur" + std::to_string(rank + 1);
      rows[rank] = RankingRowView{.name = names[rank],
                                  .points = kFirstPoints - static_cast<std::uint32_t>(rank)};
    }
    rows[kNamelessRank].name = {};
    view =
        ro::ui::RankingView{.tab = RankingTab::kBlacksmith, .rows = rows, .own_points = kOwnPoints};
    state.open = true;
  }

  RankingWindowAction Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    ImGui::LogToBuffer();
    RankingWindowAction action = ro::ui::DrawRankingWindow(state, view, art);
    logged = ImGui::GetCurrentContext()->LogBuffer.c_str();
    ImGui::LogFinish();
    ui.EndFrame();
    return action;
  }

  void Settle() {
    for (int frame = 0; frame < kSettleFrames; ++frame) {
      static_cast<void>(Frame());
    }
  }

  [[nodiscard]] bool Says(std::string_view part) const {
    return logged.find(part) != std::string::npos;
  }
};

}  // namespace

TEST_CASE("Tableau des classements : dix lignes et mes points", "[ui][ranking]") {
  RankingProbe probe;
  probe.Settle();
  for (std::size_t rank = 0; rank < kNamelessRank; ++rank) {
    CHECK(probe.Says(probe.names[rank]));
  }
  CHECK(probe.Says(std::to_string(kFirstPoints)));
  CHECK(probe.Says(std::to_string(kRowCount)));
  // La place sans nom se dit comme dans la conversation.
  CHECK(probe.Says("Sans nom"));
  CHECK(probe.Says("Mes points"));
  CHECK(probe.Says(std::to_string(kOwnPoints)));
  CHECK_FALSE(probe.Says("Récupération du classement"));
}

TEST_CASE("Tableau des classements : en attente tant qu'un nom manque", "[ui][ranking]") {
  RankingProbe probe;
  probe.view.waiting_names = true;
  probe.Settle();
  CHECK(probe.Says("Récupération du classement"));
  CHECK_FALSE(probe.Says(probe.names[0]));
  CHECK_FALSE(probe.Says("Mes points"));
}

TEST_CASE("Tableau des classements : l'onglet cliqué demande son classement", "[ui][ranking]") {
  RankingProbe probe;
  probe.Settle();
  // Imposé par `app`, l'onglet ne passe pas pour un clic.
  CHECK_FALSE(probe.Frame().requested);

  ImGuiContext& context = *ImGui::GetCurrentContext();
  REQUIRE(context.TabBars.GetAliveCount() == 1);
  ImGuiTabBar* bar = context.TabBars.GetByIndex(0);
  REQUIRE(bar != nullptr);
  const ImGuiTabItem* taekwon = nullptr;
  for (ImGuiTabItem& tab : bar->Tabs) {
    if (std::string_view(ImGui::TabBarGetTabName(bar, &tab)) == "Taekwon") {
      taekwon = &tab;
    }
  }
  REQUIRE(taekwon != nullptr);
  ImGuiIO& io = ImGui::GetIO();
  io.AddMousePosEvent(bar->BarRect.Min.x + taekwon->Offset + taekwon->Width / 2.0F,
                      (bar->BarRect.Min.y + bar->BarRect.Max.y) / 2.0F);
  static_cast<void>(probe.Frame());
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  RankingWindowAction pressed = probe.Frame();
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  RankingWindowAction released = probe.Frame();
  const RankingWindowAction asked = pressed.requested ? pressed : released;
  REQUIRE(asked.requested);
  CHECK(asked.requested_tab == RankingTab::kTaekwon);
  CHECK(probe.state.tab == RankingTab::kTaekwon);
  // UNE SEULE DEMANDE : l'image suivante ne redemande rien.
  CHECK_FALSE(probe.Frame().requested);
  // La vue porte encore le classement des forgerons : l'onglet attend sa
  // réponse au lieu de montrer des lignes qui ne sont pas les siennes.
  CHECK(probe.Says("Récupération du classement"));
  CHECK_FALSE(probe.Says(probe.names[0]));
}
