// LA RECHERCHE DE GROUPE, dessinée hors jeu : ce qu'elle montre, et
// l'intention que chaque bouton rend — la fenêtre n'envoie rien elle-même.

#include "ro/ui/party_booking_window.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/ui_context.h"

using ro::ui::PartyBookingAction;
using ro::ui::PartyBookingFilter;
using ro::ui::PartyBookingSlots;
using ro::ui::PartyBookingVerb;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 900.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr int kSettleFrames = 3;
constexpr std::int16_t kKnight = 11;
constexpr std::int16_t kPriest = 12;
constexpr std::int16_t kCulvertCode = 0x0101;
constexpr std::int16_t kLabyrinthCode = 0x0102;
constexpr int kMostLevel = 999;

struct BookingProbe {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art;
  ro::ui::PartyBookingWindowState state;
  std::array<ro::ui::PartyBookingPlaceView, 2> places = {
      ro::ui::PartyBookingPlaceView{.name = "Prontera Culvert F1", .map_code = kCulvertCode},
      ro::ui::PartyBookingPlaceView{.name = "Labyrinth Forest F1", .map_code = kLabyrinthCode}};
  std::array<ro::ui::PartyBookingRegionView, 1> regions = {
      ro::ui::PartyBookingRegionView{.name = "Prontera", .places = places}};
  std::array<ro::ui::PartyBookingJobView, 2> jobs = {
      ro::ui::PartyBookingJobView{.code = kKnight, .name = "Knight"},
      ro::ui::PartyBookingJobView{.code = kPriest, .name = "Priest"}};
  std::array<ro::ui::PartyBookingJobGroupView, 1> groups = {
      ro::ui::PartyBookingJobGroupView{.label = "Seconds jobs", .jobs = jobs}};
  std::array<ro::ui::PartyBookingRowView, 1> rows = {
      ro::ui::PartyBookingRowView{.leader = "Alice",
                                  .levels = "50~65",
                                  .place = "Prontera — Prontera Culvert F1",
                                  .jobs = "Knight, Priest",
                                  .posted = "14:05"}};
  ro::ui::PartyBookingView view;
  std::string logged;

  BookingProbe() {
    view.regions = regions;
    view.job_groups = groups;
    view.rows = rows;
    view.searched = true;
    view.least_level = 1;
    view.most_level = kMostLevel;
    state.open = true;
  }

  PartyBookingAction Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    ImGui::LogToBuffer();
    PartyBookingAction action = ro::ui::DrawPartyBookingWindow(state, view, art);
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

  // Presse ce bouton de la fenêtre, et rend l'intention de l'image où il part.
  PartyBookingAction Press(const char* label) {
    ImGuiWindow* window = ImGui::FindWindowByName(state.name.Get());
    REQUIRE(window != nullptr);
    ImGui::ActivateItemByID(window->GetID(label));
    for (int frame = 0; frame < kSettleFrames; ++frame) {
      PartyBookingAction action = Frame();
      if (action.verb != PartyBookingVerb::kNothing) {
        return action;
      }
    }
    return {};
  }

  [[nodiscard]] bool Says(std::string_view part) const {
    return logged.find(part) != std::string::npos;
  }
};

}  // namespace

TEST_CASE("Recherche de groupe : six cases au plus, tassées", "[ui][party_booking]") {
  PartyBookingSlots slots = ro::ui::kPartyBookingNoSlots;
  for (std::int16_t code = 1; code <= 6; ++code) {
    CHECK(ro::ui::TogglePartyBookingSlot(slots, code));
  }
  CHECK(ro::ui::PartyBookingSlotsUsed(slots) == 6);
  // Une septième est refusée, sans rien changer.
  CHECK_FALSE(ro::ui::TogglePartyBookingSlot(slots, kKnight));
  CHECK(ro::ui::PartyBookingSlotsUsed(slots) == 6);
  // Décocher tasse les suivantes.
  CHECK(ro::ui::TogglePartyBookingSlot(slots, 2));
  CHECK(slots == PartyBookingSlots{1, 3, 4, 5, 6, -1});
}

TEST_CASE("Recherche de groupe : la recherche filtre par lieu OU par métier",
          "[ui][party_booking]") {
  BookingProbe probe;
  probe.state.search_level = 50;
  probe.state.search_region = 0;
  probe.state.search_place = 1;
  probe.state.search_job = kKnight;

  probe.state.filter = PartyBookingFilter::kNone;
  PartyBookingAction action = ro::ui::PartyBookingSearchAsked(probe.state, probe.view);
  CHECK(action.verb == PartyBookingVerb::kSearch);
  CHECK(action.level == 50);
  CHECK(action.map_code == ro::ui::kPartyBookingEverywhere);
  CHECK(action.job == ro::ui::kPartyBookingEmptySlot);

  probe.state.filter = PartyBookingFilter::kPlace;
  action = ro::ui::PartyBookingSearchAsked(probe.state, probe.view);
  CHECK(action.map_code == kLabyrinthCode);
  CHECK(action.job == ro::ui::kPartyBookingEmptySlot);

  probe.state.filter = PartyBookingFilter::kJob;
  action = ro::ui::PartyBookingSearchAsked(probe.state, probe.view);
  CHECK(action.map_code == ro::ui::kPartyBookingEverywhere);
  CHECK(action.job == kKnight);

  // Un rang hors des bornes ne vise aucun lieu.
  CHECK(ro::ui::PartyBookingChosenPlace(probe.view, 3, 0) == ro::ui::kPartyBookingEverywhere);
}

TEST_CASE("Recherche de groupe : la liste montre les annonces, et Rechercher part",
          "[ui][party_booking]") {
  BookingProbe probe;
  probe.Settle();
  CHECK(probe.Says("Alice"));
  CHECK(probe.Says("50~65"));
  CHECK(probe.Says("Prontera Culvert F1"));
  CHECK(probe.Says("Knight, Priest"));
  CHECK(probe.Says("14:05"));
  // Sans annonce à soi, le formulaire de publication.
  CHECK(probe.Says("Publier"));
  CHECK(probe.Says("Cochez au moins un job."));

  probe.state.search_level = 40;
  const PartyBookingAction asked = probe.Press("Rechercher");
  CHECK(asked.verb == PartyBookingVerb::kSearch);
  CHECK(asked.level == 40);
}

TEST_CASE("Recherche de groupe : une recherche en cours, puis une page vide",
          "[ui][party_booking]") {
  BookingProbe probe;
  probe.view.searching = true;
  probe.Settle();
  CHECK(probe.Says("Recherche en cours"));
  CHECK_FALSE(probe.Says("Alice"));

  probe.view.searching = false;
  probe.view.rows = {};
  probe.Settle();
  CHECK(probe.Says("Aucune annonce ne correspond."));
}

TEST_CASE("Recherche de groupe : publier rend niveau, lieu et métiers", "[ui][party_booking]") {
  BookingProbe probe;
  probe.state.register_level = 70;
  probe.state.register_region = 0;
  probe.state.register_place = 0;
  REQUIRE(ro::ui::TogglePartyBookingSlot(probe.state.register_jobs, kPriest));
  probe.Settle();
  const PartyBookingAction asked = probe.Press("Publier");
  CHECK(asked.verb == PartyBookingVerb::kRegister);
  CHECK(asked.level == 70);
  CHECK(asked.map_code == kCulvertCode);
  CHECK(asked.jobs[0] == kPriest);
}

TEST_CASE("Recherche de groupe : mon annonce se modifie et se retire", "[ui][party_booking]") {
  BookingProbe probe;
  probe.view.has_mine = true;
  probe.view.mine = probe.rows[0];
  probe.view.mine_index = 9;
  probe.view.mine_jobs = PartyBookingSlots{kKnight, -1, -1, -1, -1, -1};
  probe.Settle();
  CHECK(probe.Says("Retirer l'annonce"));
  CHECK_FALSE(probe.Says("Publier"));
  // Les métiers à modifier partent de ceux de l'annonce.
  CHECK(probe.state.edited_jobs == probe.view.mine_jobs);

  const PartyBookingAction update = probe.Press("Modifier les jobs");
  CHECK(update.verb == PartyBookingVerb::kUpdate);
  CHECK(update.jobs == probe.view.mine_jobs);

  const PartyBookingAction removal = probe.Press("Retirer l'annonce");
  CHECK(removal.verb == PartyBookingVerb::kDelete);
}
