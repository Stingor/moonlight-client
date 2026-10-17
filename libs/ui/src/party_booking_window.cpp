// La fenêtre de recherche de groupe. Voir l'en-tête.

#include "ro/ui/party_booking_window.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <string>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/bug_report_window.h"
#include "ro/ui/list_keys.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

#include "ui_scale.h"

namespace ro::ui {
namespace {

// Les colonnes de la liste : auteur, niveaux, lieu, métiers, heure.
constexpr int kResultColumns = 5;
// La colonne de l'heure, étroite : « 14:05 ».
constexpr float kPostedColumnPixels = 50.0F;
// Celle des niveaux : « 120~135 ».
constexpr float kLevelsColumnPixels = 70.0F;
// Le pas des boutons d'un champ de niveau : un niveau, puis dix.
constexpr int kLevelStep = 1;
constexpr int kLevelFastStep = 10;

std::string Labelled(const char* text, const char* id) { return std::string(text) + "###" + id; }

// LE NOM D'UN MÉTIER, vide pour un code que la vue ne connaît pas.
std::string_view JobName(const PartyBookingView& view, std::int16_t code) {
  for (const PartyBookingJobGroupView& group : view.job_groups) {
    for (const PartyBookingJobView& job : group.jobs) {
      if (job.code == code) {
        return job.name;
      }
    }
  }
  return {};
}

// LA RÉGION PUIS LE LIEU. Changer de région revient à son premier lieu.
void DrawPlaceCombos(const char* id, int& region, int& place, const PartyBookingView& view,
                     const SkinArt& art) {
  ImGui::PushID(id);
  const bool region_known = region >= 0 && static_cast<std::size_t>(region) < view.regions.size();
  const std::string region_shown =
      region_known ? std::string(view.regions[static_cast<std::size_t>(region)].name)
                   : std::string(core::Tr("Choisir une région"));
  ImGui::SetNextItemWidth(kPartyBookingComboPixels * Scale());
  if (BeginSkinCombo("##region", region_shown.c_str(), art)) {
    ComboKeys keys(region, static_cast<int>(view.regions.size()));
    for (std::size_t rank = 0; rank < view.regions.size(); ++rank) {
      const std::string name(view.regions[rank].name);
      ImGui::PushID(static_cast<int>(rank));
      if (keys.Took(static_cast<int>(rank),
                    ImGui::Selectable(name.c_str(), static_cast<int>(rank) == region))) {
        region = static_cast<int>(rank);
        place = 0;
      }
      ImGui::PopID();
    }
    EndSkinCombo();
  }
  if (region_known) {
    const PartyBookingRegionView& chosen = view.regions[static_cast<std::size_t>(region)];
    const bool place_known = place >= 0 && static_cast<std::size_t>(place) < chosen.places.size();
    const std::string place_shown =
        place_known ? std::string(chosen.places[static_cast<std::size_t>(place)].name)
                    : std::string();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(kPartyBookingComboPixels * Scale());
    if (BeginSkinCombo("##place", place_shown.c_str(), art)) {
      ComboKeys keys(place, static_cast<int>(chosen.places.size()));
      for (std::size_t rank = 0; rank < chosen.places.size(); ++rank) {
        const std::string name(chosen.places[rank].name);
        ImGui::PushID(static_cast<int>(rank));
        if (keys.Took(static_cast<int>(rank),
                      ImGui::Selectable(name.c_str(), static_cast<int>(rank) == place))) {
          place = static_cast<int>(rank);
        }
        ImGui::PopID();
      }
      EndSkinCombo();
    }
  }
  ImGui::PopID();
}

// LE MÉTIER D'UNE RECHERCHE : une seule entrée, rangée sous sa famille.
void DrawJobCombo(PartyBookingWindowState& state, const PartyBookingView& view,
                  const SkinArt& art) {
  const std::string_view chosen = JobName(view, state.search_job);
  const std::string shown =
      chosen.empty() ? std::string(core::Tr("Choisir un job")) : std::string(chosen);
  ImGui::SetNextItemWidth(kPartyBookingComboPixels * Scale());
  if (!BeginSkinCombo("##search_job", shown.c_str(), art)) {
    return;
  }
  // Les rangs courent d'un groupe à l'autre : l'intitulé d'un groupe n'en a pas.
  int job_count = 0;
  int chosen_rank = kListNoEntry;
  for (const PartyBookingJobGroupView& group : view.job_groups) {
    for (const PartyBookingJobView& job : group.jobs) {
      if (job.code == state.search_job) {
        chosen_rank = job_count;
      }
      ++job_count;
    }
  }
  ComboKeys keys(chosen_rank, job_count);
  int rank = 0;
  for (const PartyBookingJobGroupView& group : view.job_groups) {
    const std::string label(group.label);
    ImGui::TextDisabled("%s", label.c_str());
    for (const PartyBookingJobView& job : group.jobs) {
      const std::string name(job.name);
      ImGui::PushID(job.code);
      if (keys.Took(rank, ImGui::Selectable(name.c_str(), job.code == state.search_job))) {
        state.search_job = job.code;
      }
      ImGui::PopID();
      ++rank;
    }
  }
  EndSkinCombo();
}

// LES MÉTIERS COCHÉS D'UNE ANNONCE, six au plus, par famille repliable.
void DrawJobPicker(const char* id, PartyBookingSlots& slots, const PartyBookingView& view,
                   const SkinArt& art) {
  ImGui::PushID(id);
  std::array<char, 96> count = {};
  std::snprintf(count.data(), count.size(), core::Tr("Jobs recherchés : %d / %d"),
                static_cast<int>(PartyBookingSlotsUsed(slots)),
                static_cast<int>(kPartyBookingSlotCount));
  ImGui::TextUnformatted(count.data());
  const bool full = PartyBookingSlotsUsed(slots) == kPartyBookingSlotCount;
  for (const PartyBookingJobGroupView& group : view.job_groups) {
    const std::string label(group.label);
    if (!ImGui::TreeNode(label.c_str())) {
      continue;
    }
    for (const PartyBookingJobView& job : group.jobs) {
      bool ticked = std::find(slots.begin(), slots.end(), job.code) != slots.end();
      const std::string name(job.name);
      ImGui::PushID(job.code);
      ImGui::BeginDisabled(full && !ticked);
      if (SkinCheckBox(name.c_str(), &ticked, art)) {
        static_cast<void>(TogglePartyBookingSlot(slots, job.code));
      }
      ImGui::EndDisabled();
      ImGui::PopID();
    }
    ImGui::TreePop();
  }
  ImGui::PopID();
}

void DrawRowCells(const PartyBookingRowView& row) {
  ImGui::TableNextColumn();
  ImGui::TextUnformatted(row.levels.data(), row.levels.data() + row.levels.size());
  ImGui::TableNextColumn();
  ImGui::TextUnformatted(row.place.data(), row.place.data() + row.place.size());
  ImGui::TableNextColumn();
  ImGui::TextWrapped("%.*s", static_cast<int>(row.jobs.size()), row.jobs.data());
  ImGui::TableNextColumn();
  ImGui::TextUnformatted(row.posted.data(), row.posted.data() + row.posted.size());
}

// LA LISTE DES ANNONCES. Un clic droit sur une ligne propose de chuchoter à son
// auteur ; un double clic ouvre sa conversation.
void DrawResults(const PartyBookingView& view, PartyBookingAction& action) {
  if (view.searching) {
    ImGui::TextUnformatted(core::Tr("Recherche en cours…"));
    return;
  }
  if (!view.searched) {
    ImGui::TextDisabled("%s", core::Tr("Lancez une recherche pour voir les annonces."));
    return;
  }
  if (view.rows.empty()) {
    ImGui::TextDisabled("%s", core::Tr("Aucune annonce ne correspond."));
    return;
  }
  if (!ImGui::BeginTable("##booking_rows", kResultColumns,
                         ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
    return;
  }
  const float scale = Scale();
  ImGui::TableSetupColumn(core::Tr("Auteur"), ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn(core::Tr("Niveaux"), ImGuiTableColumnFlags_WidthFixed,
                          kLevelsColumnPixels * scale);
  ImGui::TableSetupColumn(core::Tr("Lieu"), ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn(core::Tr("Recherche"), ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn(core::Tr("Heure"), ImGuiTableColumnFlags_WidthFixed,
                          kPostedColumnPixels * scale);
  ImGui::TableHeadersRow();
  for (std::size_t rank = 0; rank < view.rows.size(); ++rank) {
    const PartyBookingRowView& row = view.rows[rank];
    const std::string leader(row.leader);
    ImGui::PushID(static_cast<int>(rank));
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    if (ImGui::Selectable(
            leader.c_str(), false,
            ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick) &&
        ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
      action.verb = PartyBookingVerb::kWhisperWindow;
      action.name = leader;
    }
    if (BeginSkinPopupContextItem("##row_menu")) {
      if (ImGui::MenuItem(core::Tr("Chuchoter"))) {
        action.verb = PartyBookingVerb::kWhisper;
        action.name = leader;
      }
      if (ImGui::MenuItem(core::Tr("Ouvrir une conversation"))) {
        action.verb = PartyBookingVerb::kWhisperWindow;
        action.name = leader;
      }
      EndSkinPopup();
    }
    DrawRowCells(row);
    ImGui::PopID();
  }
  ImGui::EndTable();
}

void DrawSearch(PartyBookingWindowState& state, const PartyBookingView& view, const SkinArt& art,
                PartyBookingAction& action) {
  const float scale = Scale();
  ImGui::SetNextItemWidth(kPartyBookingLevelFieldPixels * scale);
  static_cast<void>(SkinInputInt(core::Tr("Niveau (0 : tous)"), &state.search_level, 0,
                                 view.most_level, kLevelStep, kLevelFastStep, art));
  ImGui::TextUnformatted(core::Tr("Filtrer par"));
  ImGui::SameLine();
  if (SkinRadio(core::Tr("Rien"), state.filter == PartyBookingFilter::kNone, art)) {
    state.filter = PartyBookingFilter::kNone;
  }
  ImGui::SameLine();
  if (SkinRadio(core::Tr("Lieu"), state.filter == PartyBookingFilter::kPlace, art)) {
    state.filter = PartyBookingFilter::kPlace;
  }
  ImGui::SameLine();
  if (SkinRadio(core::Tr("Job"), state.filter == PartyBookingFilter::kJob, art)) {
    state.filter = PartyBookingFilter::kJob;
  }
  if (state.filter == PartyBookingFilter::kPlace) {
    DrawPlaceCombos("search_place", state.search_region, state.search_place, view, art);
  } else if (state.filter == PartyBookingFilter::kJob) {
    DrawJobCombo(state, view, art);
  }
  ImGui::BeginDisabled(view.searching);
  if (SkinButton(core::Tr("Rechercher"), art)) {
    action = PartyBookingSearchAsked(state, view);
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(view.searching || !view.more_on_server);
  if (SkinButton(core::Tr("Page suivante"), art)) {
    action.verb = PartyBookingVerb::kNextPage;
  }
  ImGui::EndDisabled();
  DrawResults(view, action);
}

// L'ANNONCE DU JOUEUR : ses métiers se modifient, elle se retire.
void DrawMine(PartyBookingWindowState& state, const PartyBookingView& view, const SkinArt& art,
              PartyBookingAction& action) {
  if (!state.edited_loaded || state.edited_index != view.mine_index) {
    state.edited_jobs = view.mine_jobs;
    state.edited_index = view.mine_index;
    state.edited_loaded = true;
  }
  ImGui::Text("%s : %.*s", core::Tr("Niveaux"), static_cast<int>(view.mine.levels.size()),
              view.mine.levels.data());
  ImGui::Text("%s : %.*s", core::Tr("Lieu"), static_cast<int>(view.mine.place.size()),
              view.mine.place.data());
  ImGui::TextWrapped("%s : %.*s", core::Tr("Recherche"), static_cast<int>(view.mine.jobs.size()),
                     view.mine.jobs.data());
  DrawJobPicker("edit_jobs", state.edited_jobs, view, art);
  ImGui::BeginDisabled(PartyBookingSlotsUsed(state.edited_jobs) == 0);
  if (SkinButton(core::Tr("Modifier les jobs"), art)) {
    action.verb = PartyBookingVerb::kUpdate;
    action.jobs = state.edited_jobs;
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  if (SkinButton(core::Tr("Retirer l'annonce"), art)) {
    action.verb = PartyBookingVerb::kDelete;
  }
}

// LA PUBLICATION : niveau minimal, lieu, métiers.
void DrawRegister(PartyBookingWindowState& state, const PartyBookingView& view, const SkinArt& art,
                  PartyBookingAction& action) {
  state.edited_loaded = false;
  ImGui::SetNextItemWidth(kPartyBookingLevelFieldPixels * Scale());
  static_cast<void>(SkinInputInt(core::Tr("Niveau minimal"), &state.register_level,
                                 view.least_level, view.most_level, kLevelStep, kLevelFastStep,
                                 art));
  DrawPlaceCombos("register_place", state.register_region, state.register_place, view, art);
  DrawJobPicker("register_jobs", state.register_jobs, view, art);
  if (view.registering) {
    ImGui::TextUnformatted(core::Tr("Publication…"));
    return;
  }
  const bool ready = PartyBookingSlotsUsed(state.register_jobs) > 0;
  ImGui::BeginDisabled(!ready);
  if (SkinButton(core::Tr("Publier"), art)) {
    action = PartyBookingRegisterAsked(state, view);
  }
  ImGui::EndDisabled();
  if (!ready) {
    ImGui::TextDisabled("%s", core::Tr("Cochez au moins un job."));
  }
}

}  // namespace

PartyBookingAction DrawPartyBookingWindow(PartyBookingWindowState& state,
                                          const PartyBookingView& view, const SkinArt& art) {
  PartyBookingAction action;
  if (!state.open) {
    state.was_open = false;
    return action;
  }
  if (!state.was_open) {
    state.was_open = true;
    ImGui::SetNextWindowFocus();
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5F, 0.5F));
  }
  const float scale = Scale();
  ImGui::SetNextWindowSize(
      ImVec2(kPartyBookingFirstWidthPixels * scale, kPartyBookingFirstHeightPixels * scale),
      ImGuiCond_FirstUseEver);
  PoseBugReportButton(state.bug_report);
  const bool begun = BeginSkinnedWindow(state.name.Get(), &state.open, art, SkinnedTitle::kBar,
                                        ImGuiWindowFlags_NoCollapse);
  // ⚠ RELU MAINTENANT, POSÉ À LA FIN : un bouton du corps remplace l'intention
  // entière, et l'effacerait.
  const bool report_bug = state.bug_report && TitleActionClicked(0);
  if (!begun) {
    EndSkinnedWindow();
    action.report_bug = report_bug;
    return action;
  }
  if (ImGui::CollapsingHeader(Labelled(core::Tr("Annonces"), "booking_ads").c_str(),
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    DrawSearch(state, view, art, action);
  }
  if (ImGui::CollapsingHeader(Labelled(core::Tr("Mon annonce"), "booking_mine").c_str(),
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    if (view.has_mine) {
      DrawMine(state, view, art, action);
    } else {
      DrawRegister(state, view, art, action);
    }
  }
  EndSkinnedWindow();
  action.report_bug = report_bug;
  return action;
}

}  // namespace ro::ui
