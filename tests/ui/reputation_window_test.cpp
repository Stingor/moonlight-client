// LA FENÊTRE DE RÉPUTATION, dessinée hors jeu : la jauge d'une faction de part et
// d'autre de zéro, les factions et le total de leur groupe, l'attente des points
// et la table qui manque.

#include "ro/ui/reputation_window.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/ui_context.h"

using ro::ui::ReputationGauge;
using ro::ui::ReputationGaugeOf;
using ro::ui::ReputationRowView;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr int kSettleFrames = 3;
constexpr std::int64_t kWolfLeast = -5000;
constexpr std::int64_t kWolfMost = 5000;

struct ReputationProbe {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  ro::ui::ReputationWindowState state;
  std::array<std::string_view, 1> groups{"Arunafeltz"};
  std::array<ReputationRowView, 2> rows{
      ReputationRowView{
          .name = "Grey Wolf Village", .points = 2300, .least = kWolfLeast, .most = kWolfMost},
      ReputationRowView{.name = "Isgard", .points = -700, .least = -3000, .most = 3000}};
  ro::ui::ReputationView view;
  std::string logged;

  ReputationProbe() {
    view = ro::ui::ReputationView{.groups = groups, .rows = rows};
    state.open = true;
  }

  void Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    ImGui::LogToBuffer();
    ro::ui::DrawReputationWindow(state, view, art);
    logged = ImGui::GetCurrentContext()->LogBuffer.c_str();
    ImGui::LogFinish();
    ui.EndFrame();
  }

  void Settle() {
    for (int frame = 0; frame < kSettleFrames; ++frame) {
      Frame();
    }
  }

  [[nodiscard]] bool Says(std::string_view part) const {
    return logged.find(part) != std::string::npos;
  }
};

}  // namespace

TEST_CASE("Réputation : un palier tous les mille points, vers le haut", "[ui][reputation]") {
  const ReputationGauge gauge = ReputationGaugeOf(2300, kWolfLeast, kWolfMost);
  CHECK_FALSE(gauge.negative);
  CHECK(gauge.steps == 5);
  CHECK(gauge.lit_steps == 2);
  CHECK(gauge.into_step == 300);
  CHECK_FALSE(gauge.full);
}

TEST_CASE("Réputation : la jauge négative compte depuis le minimum", "[ui][reputation]") {
  const ReputationGauge gauge = ReputationGaugeOf(-1700, -3000, 3000);
  CHECK(gauge.negative);
  CHECK(gauge.steps == 3);
  CHECK(gauge.lit_steps == 1);
  CHECK(gauge.into_step == 700);
}

TEST_CASE("Réputation : au maximum, la jauge est pleine et ne déborde pas", "[ui][reputation]") {
  const ReputationGauge at_top = ReputationGaugeOf(kWolfMost, kWolfLeast, kWolfMost);
  CHECK(at_top.full);
  CHECK(at_top.lit_steps == at_top.steps);
  CHECK(at_top.into_step == 0);
  // Des points au-delà de la borne, que la table ne sait pas encore.
  const ReputationGauge beyond = ReputationGaugeOf(kWolfMost + 1234, kWolfLeast, kWolfMost);
  CHECK(beyond.lit_steps == 5);
  CHECK(beyond.full);
  // Zéro point : aucune lampe allumée, rien d'entamé.
  const ReputationGauge zero = ReputationGaugeOf(0, kWolfLeast, kWolfMost);
  CHECK(zero.lit_steps == 0);
  CHECK(zero.into_step == 0);
  CHECK_FALSE(zero.negative);
}

TEST_CASE("Réputation : les factions, leurs points et le palier en cours", "[ui][reputation]") {
  ReputationProbe probe;
  probe.Settle();
  CHECK(probe.Says("Grey Wolf Village"));
  CHECK(probe.Says("2300 P"));
  CHECK(probe.Says("300 / 1000"));
  CHECK(probe.Says("Isgard"));
  CHECK(probe.Says("-700 P"));
  // Sous « toutes les factions », pas de total de groupe.
  CHECK_FALSE(probe.Says("Total du groupe"));
}

TEST_CASE("Réputation : un groupe choisi montre son total", "[ui][reputation]") {
  ReputationProbe probe;
  probe.state.group = 1;
  probe.view.group_points = 1600;
  probe.Settle();
  CHECK(probe.Says("Total du groupe"));
  CHECK(probe.Says("1600 P"));
}

TEST_CASE("Réputation : un groupe que la vue n'a plus revient à « toutes »", "[ui][reputation]") {
  ReputationProbe probe;
  probe.state.group = 4;
  probe.Settle();
  CHECK(probe.state.group == ro::ui::kReputationAllGroups);
}

TEST_CASE("Réputation : l'attente des points, puis la table absente", "[ui][reputation]") {
  ReputationProbe probe;
  probe.view.awaiting_points = true;
  probe.Settle();
  CHECK(probe.Says("Récupération des réputations"));
  CHECK_FALSE(probe.Says("Grey Wolf Village"));

  probe.view.book_missing = true;
  probe.Settle();
  CHECK(probe.Says("La table des réputations manque"));
}
