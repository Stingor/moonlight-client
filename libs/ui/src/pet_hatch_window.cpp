// LA LISTE D'ÉCLOSION. Voir l'en-tête.

#include "ro/ui/pet_hatch_window.h"

#include <algorithm>
#include <cfloat>
#include <string>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

namespace ro::ui {
namespace {

constexpr ImU32 kLabelColor = IM_COL32(89, 89, 107, 255);
constexpr ImU32 kValueColor = IM_COL32(26, 26, 33, 255);
constexpr ImU32 kRedColor = IM_COL32(153, 31, 31, 255);

// La largeur d'une rangée d'œuf, dont tout le contenu dérive — et la plus
// étroite de la fenêtre.
constexpr float kRowWidthPixels = 250.0F;
// La place de la première ouverture, rabattue dans l'écran.
constexpr float kSpawnXPixels = 760.0F;
constexpr float kSpawnYPixels = 412.0F;
constexpr float kSpawnHeightPixels = 200.0F;
constexpr float kRowGapPixels = 4.0F;
constexpr float kHatchButtonPixels = 110.0F;
constexpr float kCancelButtonPixels = 80.0F;

constexpr const char* kWindowId = "###pet_hatch_window";

[[nodiscard]] float Px(float pixels) { return pixels * ImGui::GetStyle().FontScaleMain; }

void TextColored(ImU32 color, const char* text) {
  ImGui::PushStyleColor(ImGuiCol_Text, color);
  ImGui::TextUnformatted(text);
  ImGui::PopStyleColor();
}

const char* RankText(PetIntimacyLabel label) {
  switch (label) {
    case PetIntimacyLabel::kHateWithPassion:
      return core::Tr("Haine profonde");
    case PetIntimacyLabel::kHate:
      return core::Tr("Haine");
    case PetIntimacyLabel::kAwkward:
      return core::Tr("Distant");
    case PetIntimacyLabel::kShy:
      return core::Tr("Timide");
    case PetIntimacyLabel::kNeutral:
      return core::Tr("Neutre");
    case PetIntimacyLabel::kCordial:
      return core::Tr("Cordial");
    case PetIntimacyLabel::kLoyal:
      return core::Tr("Loyal");
    case PetIntimacyLabel::kUnknown:
      break;
  }
  return core::Tr("Inconnue");
}

// Une rangée : l'icône, le nom à choisir, et sous lui ce que l'œuf contient.
void DrawRow(PetHatchWindowState& state, const PetEggRowView& row, int rank,
             PetHatchWindowAction& action) {
  ImGui::PushID(rank);
  const float side = ImGui::GetTextLineHeight();
  if (row.icon.usable()) {
    ImGui::Image(row.icon.id, ImVec2(side, side));
  } else {
    ImGui::Dummy(ImVec2(side, side));
  }
  ImGui::SameLine(0.0F, ImGui::GetStyle().ItemInnerSpacing.x);
  const std::string label = (row.name.empty() ? std::string("?") : row.name) + "##egg";
  // UNE CELLULE ET NON UN LIEN : le clic gauche choisit l'œuf. La description
  // passe par le clic droit.
  ImGui::PushStyleColor(ImGuiCol_Text, kValueColor);
  if (ImGui::Selectable(label.c_str(), state.selected_slot == row.client_slot, 0,
                        ImVec2(Px(kRowWidthPixels) - side, 0.0F))) {
    state.selected_slot = row.client_slot;
  }
  ImGui::PopStyleColor();
  if (ImGui::IsItemHovered() && row.item_id != 0) {
    action.link.hovered_id = row.item_id;
    action.link.hovering = true;
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
      action.link.gesture = ChatLinkGesture::kDescribe;
      action.link.kind = ChatLinkKind::kItem;
      action.link.item_id = row.item_id;
    }
  }
  ImGui::Indent(side + Px(kRowGapPixels));
  switch (row.state) {
    case PetEggState::kMissing:
      TextColored(kRedColor, core::Tr("Introuvable dans l'inventory."));
      break;
    case PetEggState::kBlank:
      TextColored(kRedColor, core::Tr("Œuf vierge : rien à éclore."));
      break;
    case PetEggState::kInhabited: {
      std::string facts = row.intimacy.has_value() ? RankText(*row.intimacy) : "";
      if (row.renamed) {
        facts += "  ·  ";
        facts += core::Tr("déjà renommé");
      }
      TextColored(kLabelColor, facts.c_str());
      break;
    }
  }
  ImGui::Unindent(side + Px(kRowGapPixels));
  ImGui::PopID();
}

}  // namespace

void OpenPetHatchWindow(PetHatchWindowState& state) {
  state.open = true;
  state.focus_asked = true;
  state.selected_slot.reset();
}

PetHatchWindowAction DrawPetHatchWindow(PetHatchWindowState& state, const PetHatchWindowView& view,
                                        const SkinArt& art) {
  PetHatchWindowAction action;
  if (!state.open) {
    return action;
  }
  if (state.focus_asked) {
    state.focus_asked = false;
    ImGui::SetNextWindowFocus();
  }
  const ImVec2 screen = ImGui::GetIO().DisplaySize;
  ImGui::SetNextWindowPos(
      ImVec2(std::min(Px(kSpawnXPixels), std::max(0.0F, screen.x - Px(kRowWidthPixels))),
             std::min(Px(kSpawnYPixels), std::max(0.0F, screen.y - Px(kSpawnHeightPixels)))),
      ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints(ImVec2(Px(kRowWidthPixels), 0.0F), ImVec2(FLT_MAX, FLT_MAX));
  std::string title = core::Tr("Éclore un œuf");
  title += kWindowId;
  const bool begun = BeginSkinnedWindow(
      title.c_str(), &state.open, art, SkinnedTitle::kBar,
      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);
  if (begun) {
    // Le serveur envoie la liste même vide.
    if (view.rows.empty()) {
      TextColored(kLabelColor, core::Tr("Aucun œuf de familier dans l'inventory."));
    }
    const PetEggRowView* chosen = nullptr;
    int rank = 0;
    for (const PetEggRowView& row : view.rows) {
      DrawRow(state, row, rank++, action);
      if (state.selected_slot == row.client_slot) {
        chosen = &row;
      }
    }

    // ── LES REFUS, DITS AVANT LE CLIC ───────────────────────────────────────
    // Liste vide : la ligne au-dessus l'a dit, le bouton reste seulement grisé.
    const char* refusal = nullptr;
    bool refused = false;
    if (chosen == nullptr) {
      refused = true;
      refusal = view.rows.empty() ? nullptr : core::Tr("Choisis l'œuf à faire éclore.");
    } else if (chosen->state == PetEggState::kMissing) {
      refused = true;
      refusal = core::Tr("Cet œuf n'est plus dans l'inventory.");
    } else if (chosen->state == PetEggState::kBlank) {
      refused = true;
      refusal = core::Tr("Cet œuf est vide : le serveur refusera de l'éclore.");
    }

    ImGui::Separator();
    if (refusal != nullptr) {
      ImGui::PushStyleColor(ImGuiCol_Text, kLabelColor);
      ImGui::TextWrapped("%s", refusal);
      ImGui::PopStyleColor();
    }
    ImGui::BeginDisabled(refused);
    std::string hatch = core::Tr("Faire éclore");
    hatch += "##pet_hatch_ok";
    if (SkinSmallButton(hatch.c_str(), art, ImVec2(Px(kHatchButtonPixels), 0.0F)) &&
        chosen != nullptr) {
      action.hatch = chosen->client_slot;
      state.open = false;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    // FERMER SANS RÉPONDRE n'envoie rien, comme l'annulation du jeu.
    std::string cancel = core::Tr("Annuler");
    cancel += "##pet_hatch_cancel";
    if (SkinSmallButton(cancel.c_str(), art, ImVec2(Px(kCancelButtonPixels), 0.0F))) {
      state.open = false;
    }
  }
  EndSkinnedWindow();
  return action;
}

}  // namespace ro::ui
