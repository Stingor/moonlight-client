#include "character_select_amend.h"

#include <array>
#include <cstddef>
#include <cstdio>

#include "ro/core/i18n.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/window_name.h"

#include "ui_scale.h"

namespace ro::ui {
namespace {

// L'ÉTIQUETTE DES RETOUCHES : un bleu clair sur fond nuit, pour ne se
// confondre ni avec le nom ni avec le rouge d'une suppression programmée.
constexpr ImU32 kAmendColor = IM_COL32(175, 225, 255, 255);
constexpr ImU32 kAmendPlateColor = IM_COL32(0, 22, 44, 200);
constexpr float kAmendPaddingPixels = 6.0F;
constexpr float kAmendRounding = 3.0F;
// Le fond de l'étiquette descend sous la ligne de base, comme celui du compte
// à rebours.
constexpr float kAmendPlateDrop = 2.0F;

// La fenêtre du nouveau nom.
constexpr const char* kRenameWindowTitle = core::TrText("Renommer le personnage");
constexpr const char* kRenameWindowId = "character_rename";
constexpr float kNameFieldPoints = 220.0F;
constexpr ImU32 kRefusalColor = IM_COL32(255, 96, 96, 255);
constexpr ImU32 kPendingColor = IM_COL32(216, 209, 115, 255);
constexpr ImU32 kWarningColor = IM_COL32(200, 60, 60, 255);

}  // namespace

void DrawAmendTag(ImDrawList* list, const SeatFrame& seat, const CharacterSlotView& who,
                  float top) {
  if (who.moves_left <= 0 && !who.renamable) {
    return;
  }
  std::array<char, 96> tag = {};
  if (who.moves_left > 0 && who.renamable) {
    std::snprintf(tag.data(), tag.size(), core::Tr("Déplacements : %d · Renommage possible"),
                  who.moves_left);
  } else if (who.moves_left > 0) {
    std::snprintf(tag.data(), tag.size(), core::Tr("Déplacements : %d"), who.moves_left);
  } else {
    std::snprintf(tag.data(), tag.size(), "%s", core::Tr("Renommage possible"));
  }
  const ImVec2 size = ImGui::CalcTextSize(tag.data());
  const float left = seat.centre_x - size.x * 0.5F;
  list->AddRectFilled(ImVec2(left - kAmendPaddingPixels, top + kAmendPlateDrop),
                      ImVec2(left + size.x + kAmendPaddingPixels, top + size.y + 1.0F),
                      kAmendPlateColor, kAmendRounding);
  list->AddText(ImVec2(left, top), kAmendColor, tag.data());
}

bool IsMoveTarget(const CharacterSelectState& state, const CharacterSelectView& view, int slot) {
  return state.moving() && slot != state.moving_slot && slot >= 0 && slot < view.creatable_slots &&
         slot < static_cast<int>(view.slots.size());
}

void MoveTargetTooltip(const CharacterSlotView& who, int slot) {
  if (who.occupied) {
    SkinTooltipFormat(core::Tr("Échanger avec %s (emplacement %d)"), who.name.c_str(), slot + 1);
  } else {
    SkinTooltipFormat(core::Tr("Déplacer ici (emplacement %d)"), slot + 1);
  }
}

std::string MoveBannerText(const CharacterSelectState& state, const CharacterSelectView& view) {
  const std::string& name = state.moving_slot < static_cast<int>(view.slots.size())
                                ? view.slots[static_cast<std::size_t>(state.moving_slot)].name
                                : std::string();
  std::array<char, 192> banner = {};
  if (state.moving_target < 0) {
    std::snprintf(banner.data(), banner.size(),
                  core::Tr("Choisis la nouvelle place de %s. Échap pour annuler."), name.c_str());
  } else {
    std::snprintf(banner.data(), banner.size(),
                  core::Tr("Emplacement %d choisi pour %s : valide pour utiliser un déplacement."),
                  state.moving_target + 1, name.c_str());
  }
  return banner.data();
}

CharacterSelectAction DrawRenameWindow(CharacterSelectState& state, const CharacterSelectView& view,
                                       const SkinArt& art) {
  CharacterSelectAction action;
  if (!state.rename.open()) {
    return action;
  }

  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5F, 0.5F));
  static WindowName name(kRenameWindowTitle, kRenameWindowId);
  if (!BeginSkinnedWindow(name.Get(), nullptr, art, SkinnedTitle::kBar,
                          ImGuiWindowFlags_AlwaysAutoResize)) {
    EndSkinnedWindow();
    return action;
  }

  const float field = kNameFieldPoints * Scale();
  std::array<char, 128> question = {};
  std::snprintf(question.data(), question.size(), core::Tr("Nouveau nom pour « %s » :"),
                state.rename.former_name.c_str());
  ImGui::TextUnformatted(question.data());
  // ⚠ LE PRIX SE DIT AVANT LE GESTE : le renommage se décompte, et le serveur
  // n'en rend pas.
  ImGui::PushStyleColor(ImGuiCol_Text, kWarningColor);
  ImGui::TextUnformatted(core::Tr("Le renommage utilise ton droit au changement de nom."));
  ImGui::PopStyleColor();

  ImGui::SetNextItemWidth(field);
  if (ImGui::IsWindowAppearing()) {
    ImGui::SetKeyboardFocusHere();
  }
  const bool submitted_here =
      ImGui::InputText("##new_name", state.rename.name.data(), state.rename.name.size(),
                       ImGuiInputTextFlags_EnterReturnsTrue);

  ImGui::Spacing();
  if (view.amend_pending) {
    ImGui::PushStyleColor(ImGuiCol_Text, kPendingColor);
    ImGui::TextUnformatted(core::Tr("Renommage en cours…"));
    ImGui::PopStyleColor();
  } else if (!view.rename_refusal.empty()) {
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + field);
    ImGui::PushStyleColor(ImGuiCol_Text, kRefusalColor);
    ImGui::TextUnformatted(view.rename_refusal.c_str());
    ImGui::PopStyleColor();
    ImGui::PopTextWrapPos();
  }

  const bool can_rename = state.rename.name[0] != '\0' && !view.amend_pending;
  ImGui::BeginDisabled(!can_rename);
  if (SkinButton(core::Tr("Renommer"), art) || (submitted_here && can_rename)) {
    action.verb = CharacterSelectVerb::kRename;
    action.character_id = state.rename.character_id;
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  if (SkinButton(core::Tr("Annuler"), art) || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
    action.verb = CharacterSelectVerb::kDropAmend;
    state.rename = CharacterRenameDraft{};
  }

  EndSkinnedWindow();
  return action;
}

}  // namespace ro::ui
