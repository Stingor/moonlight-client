#include <array>
#include <cstdio>
#include <string>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/chat_room_window.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

#include "chat_room_parts.h"
#include "ui_scale.h"

namespace ro::ui {
namespace {

using chat_room::Owned;
using chat_room::Typed;

constexpr float kWidthPixels = 300.0F;
constexpr float kEyeFonts = 2.2F;

void Counter(std::size_t used, std::size_t most) {
  std::array<char, chat_room::kCounterBytes> text{};
  std::snprintf(text.data(), text.size(), "%zu/%zu", used, most);
  ImGui::PushStyleColor(ImGuiCol_Text, used > most
                                           ? ImGui::ColorConvertU32ToFloat4(PaintingLook().bad_text)
                                           : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
  ImGui::TextUnformatted(text.data());
  ImGui::PopStyleColor();
}

}  // namespace

// L'INVITE DU SALON PRIVÉ. Le client d'origine la tient sur une ligne, coupe
// son propre libellé et se FERME sur un refus, obligeant à recliquer
// l'enseigne pour retenter : celle-ci garde le motif dedans, et reste.
ChatRoomPasswordAction DrawChatRoomPasswordWindow(ChatRoomPasswordState& state,
                                                  const ChatRoomPasswordView& view,
                                                  const SkinArt& art) {
  ChatRoomPasswordAction action;
  if (!state.open) {
    state.was_open = false;
    return action;
  }
  const float scale = Scale();
  const bool opening = !state.was_open;
  state.was_open = true;
  if (opening) {
    state.input.fill('\0');
    state.reveal = false;
    ImGui::SetNextWindowFocus();
  }
  const ImGuiViewport* screen = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(screen->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5F, 0.5F));
  ImGui::SetNextWindowSize(ImVec2(kWidthPixels * scale, 0.0F), ImGuiCond_Always);
  const std::string name = Owned(view.words.password_label) + "###" + kChatRoomPasswordWindowId;
  bool open = true;
  const bool body = BeginSkinnedWindow(name.c_str(), &open, art, SkinnedTitle::kBar,
                                       ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
  if (body && open) {
    ImGui::PushTextWrapPos(0.0F);
    ImGui::TextUnformatted(Owned(view.words.enter_password).c_str());
    ImGui::PopTextWrapPos();
    ImGui::Spacing();

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetFontSize() * kEyeFonts);
    if (opening) {
      ImGui::SetKeyboardFocusHere();
    }
    const bool submitted = ImGui::InputText(
        "##chatroom_password_prompt", state.input.data(), state.input.size(),
        (state.reveal ? 0 : ImGuiInputTextFlags_Password) | ImGuiInputTextFlags_EnterReturnsTrue);
    action.edited = ImGui::IsItemEdited();
    ImGui::SameLine();
    if (SkinSmallButton(state.reveal ? "*##chatroom_prompt_eye" : "o##chatroom_prompt_eye", art)) {
      state.reveal = !state.reveal;
    }
    SkinTooltip(core::Tr("Afficher / masquer le mot de passe"));

    const std::size_t typed = chat_room::PasswordBytes(Typed(state.input));
    Counter(typed, view.limits.password_bytes);
    if (!view.error.empty()) {
      ImGui::PushStyleColor(ImGuiCol_Text, PaintingLook().bad_text);
      ImGui::PushTextWrapPos(0.0F);
      ImGui::TextUnformatted(view.error.data(), view.error.data() + view.error.size());
      ImGui::PopTextWrapPos();
      ImGui::PopStyleColor();
    }
    ImGui::Separator();
    const std::array<const char*, 2> labels = {core::Tr("Entrer"), core::Tr("Annuler")};
    const float width = SkinWidestButton(labels, art);
    const bool can_send = !view.waiting && typed > 0 && typed <= view.limits.password_bytes;
    ImGui::BeginDisabled(!can_send);
    const bool go = SkinButton(core::Tr("Entrer"), art, ImVec2(width, 0.0F));
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (SkinButton(core::Tr("Annuler"), art, ImVec2(width, 0.0F))) {
      open = false;
    }
    action.enter = (go || submitted) && can_send;
  }
  EndSkinnedWindow();
  if (!open) {
    state.open = false;
    action = ChatRoomPasswordAction{};
    action.cancelled = true;
  }
  return action;
}

}  // namespace ro::ui
