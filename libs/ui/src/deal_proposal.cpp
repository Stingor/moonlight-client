// LA PROPOSITION D'ÉCHANGE : la boîte qui demande « Accepter » ou « Refuser »,
// ou qui dit qu'on attend la réponse à la sienne.

#include <string>

#include "ro/core/i18n.h"
#include "ro/ui/deal_window.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

namespace ro::ui {
namespace {

[[nodiscard]] float Scale() { return ImGui::GetStyle().FontScaleMain; }

constexpr float kProposalWidthPixels = 300.0F;
constexpr float kAnswerButtonPixels = 130.0F;

}  // namespace

DealProposalAction DrawDealProposal(DealProposalState& state, const DealProposalView& view,
                                    const SkinArt& art) {
  DealProposalAction action;
  // UNE PROPOSITION NEUVE ROUVRE LA BOÎTE, même refermée sur la précédente.
  if (view.seq != state.seen) {
    state.seen = view.seq;
    state.open = true;
  }
  if (!state.open) {
    return action;
  }
  const float scale = Scale();
  // ⚠ AU CENTRE À SON APPARITION : elle interrompt, donc elle tombe sous les
  // yeux ; le joueur peut ensuite la déplacer.
  const ImGuiViewport* screen = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(screen->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5F, 0.5F));
  ImGui::SetNextWindowSize(ImVec2(kProposalWidthPixels * scale, 0.0F), ImGuiCond_Appearing);
  const std::string title =
      std::string(core::Tr("Demande d'échange")) + "###" + kDealProposalWindowId;
  const bool body_shown = BeginSkinnedWindow(
      title.c_str(), &state.open, art, SkinnedTitle::kBar,
      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);
  if (body_shown && state.open) {
    ImGui::TextWrapped("%.*s", static_cast<int>(view.text.size()), view.text.data());
    if (!view.incoming) {
      ImGui::TextDisabled("%s", core::Tr("En attente de sa réponse…"));
    }
    ImGui::Spacing();
    const ImVec2 size(kAnswerButtonPixels * scale, 0.0F);
    if (view.answered) {
      ImGui::BeginDisabled();
    }
    if (view.incoming) {
      if (SkinButton(core::Tr("Accepter"), art, size) && !view.answered) {
        action.accept = true;
      }
      ImGui::SameLine();
      if (SkinButton(core::Tr("Refuser"), art, size) && !view.answered) {
        action.refuse = true;
      }
    } else if (SkinButton(core::Tr("Annuler"), art, size) && !view.answered) {
      action.refuse = true;
    }
    if (view.answered) {
      ImGui::EndDisabled();
    }
  }
  EndSkinnedWindow();
  // 🔴 LA CROIX REFUSE, comme Échap que la pile baisse dans `state.open` : une
  // proposition qu'on écarte sans répondre laisserait l'autre attendre.
  if (!state.open && !view.answered) {
    action.accept = false;
    action.refuse = true;
  }
  return action;
}

}  // namespace ro::ui
