// Les fenêtres de la file : « en attente », « terrain prêt » et la boîte de
// résultat. Voir `entry_queue_parts.h`.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <string>
#include <string_view>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/bug_report_window.h"
#include "ro/ui/modal_box.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/window_name.h"

#include "entry_queue_parts.h"
#include "ui_popup.h"
#include "ui_strings.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

constexpr const char* kStandbyTitle = core::TrText("File du champ de bataille");
// La boîte de résultat n'a pas de place à garder : son identité reste ici.
constexpr const char* kNoticeId = "entry_queue_notice";

// L'écart entre une étiquette et sa valeur, en hauteurs de police.
constexpr float kLabelGapFonts = 0.75F;
// SOUS CE DÉLAI, LE COMPTE À REBOURS PASSE AU ROUGE : il reste juste le temps
// de lire et de cliquer.
constexpr int kHurrySeconds = 5;
constexpr ImVec4 kHurryColor(0.80F, 0.10F, 0.10F, 1.0F);

constexpr std::size_t kTextCapacity = 256;
using Text = std::array<char, kTextCapacity>;

// POURQUOI « Annuler l'inscription » est éteint ; nul s'il peut partir.
//
// ⚠ PENDANT L'INVITATION, LE SERVEUR IGNORE L'ANNULATION : le bouton s'éteint
// plutôt que d'envoyer une demande qui ne sera jamais suivie d'effet.
[[nodiscard]] const char* CancelBlocker(const EntryQueueView& view) {
  if (!view.link_ready) {
    return EntryQueueLinkPendingText();
  }
  if (view.invited || view.accepted) {
    return core::Tr("Le terrain est prêt : entrez ou refusez.");
  }
  if (!view.queued) {
    return core::Tr("Inscription envoyée, en attente du serveur…");
  }
  return nullptr;
}

// Les trois lignes de la file : le terrain, l'effectif, l'attente.
void DrawStandbyFacts(const EntryQueueView& view) {
  const std::array<const char*, 3> labels = {core::Tr("Terrain"), core::Tr("Joueurs en file"),
                                             core::Tr("Attente")};
  float widest = 0.0F;
  for (const char* label : labels) {
    widest = std::max(widest, ImGui::CalcTextSize(label).x);
  }
  const float column = ImGui::GetCursorPosX() + widest + ImGui::GetFontSize() * kLabelGapFonts;

  ImGui::TextUnformatted(labels[0]);
  ImGui::SameLine(column);
  Unformatted(view.queued_name);

  ImGui::TextUnformatted(labels[1]);
  ImGui::SameLine(column);
  // ⚠ UN TIRET TANT QUE L'EFFECTIF N'EST PAS CONNU : un zéro se lirait « la
  // file est vide ».
  if (view.count >= 0) {
    ImGui::Text("%d", view.count);
  } else {
    ImGui::TextUnformatted("-");
  }

  ImGui::TextUnformatted(labels[2]);
  ImGui::SameLine(column);
  ImGui::TextUnformatted(FormatQueueWait(view.waited_s).c_str());
}

}  // namespace

EntryQueueAction DrawEntryQueueStandby(EntryQueueWindowState& state, const EntryQueueView& view,
                                       const SkinArt& art) {
  EntryQueueAction action;
  // Hors file et sans demande en vol, il n'y a plus rien à attendre.
  if (!view.queued && !view.applying) {
    state.standby_open = false;
    return action;
  }
  const ImGuiViewport* screen = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(screen->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5F, 0.5F));
  static WindowName name(kStandbyTitle, kEntryQueueStandbyId);
  PoseBugReportButton(state.bug_report);
  const bool begun =
      BeginSkinnedWindow(name.Get(), &state.standby_open, art, SkinnedTitle::kBar,
                         ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);
  if (!begun) {
    EndSkinnedWindow();
    return action;
  }
  action.report_bug = state.bug_report && TitleActionClicked(kEntryQueueBugReportRank);
  action.field_rank = state.selected;

  if (view.queued) {
    DrawStandbyFacts(view);
  } else {
    ImGui::TextUnformatted(core::Tr("Inscription envoyée, en attente du serveur…"));
  }
  if (view.accepted) {
    ImGui::TextDisabled("%s", core::Tr("Entrée demandée, en attente du serveur…"));
  } else if (view.invited) {
    ImGui::TextUnformatted(core::Tr("Le terrain est prêt !"));
    ImGui::SameLine();
    // La modale de l'invitation, congédiée ou recouverte, se redemande d'ici.
    if (SkinSmallButton(core::Tr("Répondre"), art)) {
      state.asked_ready = false;
    }
  }
  ImGui::Separator();

  const char* blocker = CancelBlocker(view);
  ImGui::BeginDisabled(blocker != nullptr);
  if (SkinButton(core::Tr("Annuler l'inscription"), art)) {
    AskEntryQueueVerb(state, EntryQueueVerb::kCancel);
  }
  ImGui::EndDisabled();
  SkinTooltip(blocker);
  ImGui::SameLine();
  if (SkinButton(core::Tr("Fermer (rester en file)"), art)) {
    state.standby_open = false;
  }
  SkinTooltip(
      core::Tr("L'icône Champ de bataille du menu rouvre cette fenêtre tant que vous êtes en "
               "file."));

  // 🔴 QUITTER LA FILE SE CONFIRME : un clic de travers perdrait des minutes
  // d'attente. Et la réponse se rejuge au « oui », l'invitation ayant pu
  // arriver pendant que la boîte attendait.
  if (state.confirming == EntryQueueVerb::kCancel) {
    Text question{};
    std::snprintf(question.data(), question.size(), core::Tr("Quitter la file de « %s » ?"),
                  Owned(view.queued_name).c_str());
    if (ConfirmEntryQueueVerb(state, EntryQueueVerb::kCancel, question.data(),
                              core::Tr("Quitter la file"), core::Tr("Rester"), art) &&
        CancelBlocker(view) == nullptr) {
      action.verb = EntryQueueVerb::kCancel;
    }
  }
  EndSkinnedWindow();
  return action;
}

EntryQueueAction DrawEntryQueueReady(EntryQueueWindowState& state, const EntryQueueView& view,
                                     const SkinArt& art) {
  EntryQueueAction action;
  const std::string name = HiddenIdLabel(core::Tr("Terrain prêt"), kEntryQueueReadyId);
  // 🔴 ELLE NE S'OUVRE QUE SUR UNE PILE VIDE : un surgissant déjà ouvert — une
  // confirmation, une saisie — serait fermé par elle. Elle attend donc qu'il se
  // ferme ; le compte à rebours, lui, court déjà dans « en attente ».
  if (!view.invited) {
    state.asked_ready = false;
  } else if (!state.asked_ready && !AnyPopupOpen()) {
    AskModal(name.c_str());
    state.asked_ready = true;
  }
  // ⚠ ÉCHAP NE LA CONGÉDIE PAS : une invitation ne doit pas disparaître d'une
  // touche tapée pour autre chose.
  if (!BeginSkinnedModal(name.c_str(), art, ImGuiWindowFlags_AlwaysAutoResize,
                         ModalEscape::kIgnored)) {
    return action;
  }
  if (!view.invited) {
    // Répondue, expirée ou annulée par le serveur : plus rien à décider.
    ImGui::CloseCurrentPopup();
    EndSkinnedModal();
    return action;
  }
  ImGui::Text(core::Tr("Le terrain « %s » est prêt."), Owned(view.invited_name).c_str());
  const int seconds_left = std::max(0, view.invitation_s_left);
  if (seconds_left > 0) {
    if (seconds_left <= kHurrySeconds) {
      ImGui::PushStyleColor(ImGuiCol_Text, kHurryColor);
    }
    ImGui::Text(core::Tr("Il vous reste %d s pour répondre."), seconds_left);
    if (seconds_left <= kHurrySeconds) {
      ImGui::PopStyleColor();
    }
  } else {
    ImGui::TextUnformatted(core::Tr("L'invitation a expiré."));
  }
  if (view.accepted) {
    ImGui::TextDisabled("%s", core::Tr("Entrée demandée, en attente du serveur…"));
  }
  ImGui::Spacing();

  // ⚠ ÉTEINTS À ZÉRO, UNE FOIS L'ENTRÉE DEMANDÉE, ET HORS LIAISON : une
  // réponse qui ne peut plus rien changer ne part pas.
  const char* blocker = nullptr;
  if (!view.link_ready) {
    blocker = EntryQueueLinkPendingText();
  } else if (view.accepted) {
    blocker = core::Tr("Entrée demandée, en attente du serveur…");
  } else if (seconds_left == 0) {
    blocker = core::Tr("L'invitation a expiré.");
  }
  ImGui::BeginDisabled(blocker != nullptr);
  if (SkinButton(core::Tr("Entrer"), art)) {
    action.verb = EntryQueueVerb::kAccept;
  }
  ImGui::EndDisabled();
  SkinTooltip(blocker);
  ImGui::SameLine();
  ImGui::BeginDisabled(blocker != nullptr);
  if (SkinButton(core::Tr("Refuser"), art)) {
    action.verb = EntryQueueVerb::kDecline;
  }
  ImGui::EndDisabled();
  SkinTooltip(blocker);
  // 🔴 UNE MODALE SANS RÉPONSE POSSIBLE OFFRE TOUJOURS UNE SORTIE : elle bloque
  // toute l'interface, chat compris. Fermée, elle se redemande depuis « en
  // attente » tant que l'invitation court.
  if (blocker != nullptr) {
    ImGui::SameLine();
    if (SkinButton(core::Tr("Fermer"), art)) {
      ImGui::CloseCurrentPopup();
    }
  }
  EndSkinnedModal();
  return action;
}

EntryQueueAction DrawEntryQueueNotice(EntryQueueWindowState& state, const EntryQueueView& view,
                                      const SkinArt& art) {
  EntryQueueAction action;
  const std::string name = HiddenIdLabel(core::Tr("Champ de bataille"), kNoticeId);
  if (view.notice.empty()) {
    state.asked_notice = false;
  } else if (!state.asked_notice && !AnyPopupOpen()) {
    AskModal(name.c_str());
    state.asked_notice = true;
  }
  if (BeginSkinnedModal(name.c_str(), art)) {
    if (view.notice.empty()) {
      ImGui::CloseCurrentPopup();
      EndSkinnedModal();
      return action;
    }
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetFontSize() * kEntryQueueWrapFonts);
    Unformatted(view.notice);
    ImGui::PopTextWrapPos();
    ImGui::Spacing();
    if (SkinButton(core::Tr("Fermer"), art)) {
      ImGui::CloseCurrentPopup();
      action.verb = EntryQueueVerb::kNoticeSeen;
    }
    EndSkinnedModal();
    return action;
  }
  // ⚠ FERMÉE AUTREMENT QUE PAR SON BOUTON — Échap — : le résultat est lu tout
  // de même, et `app` peut l'oublier.
  if (state.asked_notice && !view.notice.empty() && !ImGui::IsPopupOpen(name.c_str())) {
    action.verb = EntryQueueVerb::kNoticeSeen;
  }
  return action;
}

}  // namespace ro::ui
