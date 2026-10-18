// La liste des commandes branchée sur le champ. Voir l'en-tête.

#include "chat_input_commands.h"

#include <optional>
#include <string>

#include <imgui_internal.h>

#include "chat_command_list.h"
#include "ui_input.h"
#include "ui_strings.h"

namespace ro::ui {
namespace {

// Le propriétaire des touches que la liste prend au champ.
constexpr const char* kKeysOwner = "##touches_des_commandes";

// La touche pressée cette image, et la prend à tout le reste.
[[nodiscard]] bool TakeKey(ImGuiKey key) {
  if (!ImGui::IsKeyPressed(key, false)) {
    return false;
  }
  ImGui::SetKeyOwner(key, ImGui::GetID(kKeysOwner), ImGuiInputFlags_LockThisFrame);
  return true;
}

[[nodiscard]] ChatCommandListView ViewOf(ChatInputState& state,
                                         std::span<const ChatCommandHint> hints, bool active) {
  ChatCommandListView view = FollowChatCommandLine(state.commands, hints, state.text.data());
  view.open = view.open && active;
  return view;
}

}  // namespace

ChatCommandListView TakeCommandKeysBeforeField(ChatInputState& state,
                                               std::span<const ChatCommandHint> hints,
                                               ImGuiID field) {
  ChatCommandListView view = ViewOf(state, hints, state.was_active);
  if (!view.open) {
    return view;
  }
  if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) &&
      PressChatCommandKey(state.commands, view, hints, state.text.data(), ChatCommandKey::kEscape)
          .consumed) {
    static_cast<void>(TakeKey(ImGuiKey_Escape));
    view.open = false;
    return view;
  }
  // ⚠ ENTRÉE N'EST À LA LISTE QU'APRÈS UN CHOIX À ↑ ↓ : sans choix, elle envoie.
  const bool enter = ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
                     ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false);
  if (!enter || !state.commands.navigated) {
    return view;
  }
  const ChatCommandKeyAnswer answer =
      PressChatCommandKey(state.commands, view, hints, state.text.data(), ChatCommandKey::kEnter);
  if (answer.line.has_value()) {
    static_cast<void>(TakeKey(ImGuiKey_Enter));
    static_cast<void>(TakeKey(ImGuiKey_KeypadEnter));
    CopyBounded(state.text, *answer.line);
    ReloadInputField(field);
    view.open = false;
  }
  return view;
}

ImGuiInputTextFlags CommandFieldFlags(const ChatCommandListView& view) {
  return view.open ? ImGuiInputTextFlags_CallbackCompletion : ImGuiInputTextFlags_None;
}

bool CompleteOnTab(const CommandFieldHooks& hooks, ImGuiInputTextCallbackData* data) {
  if (data->EventFlag != ImGuiInputTextFlags_CallbackCompletion) {
    return false;
  }
  const ChatCommandKeyAnswer answer = PressChatCommandKey(
      hooks.state->commands, *hooks.view, hooks.hints, data->Buf, ChatCommandKey::kTab);
  if (!answer.line.has_value()) {
    return answer.consumed;
  }
  data->DeleteChars(0, data->BufTextLen);
  data->InsertChars(0, answer.line->c_str());
  return true;
}

bool NavigateCommands(const CommandFieldHooks& hooks, bool up) {
  return PressChatCommandKey(hooks.state->commands, *hooks.view, hooks.hints,
                             hooks.state->text.data(),
                             up ? ChatCommandKey::kUp : ChatCommandKey::kDown)
      .consumed;
}

void DrawCommandsOverField(ChatInputState& state, std::span<const ChatCommandHint> hints,
                           bool field_active, ImVec2 field_corner, float width,
                           const SkinArt& art) {
  // ⚠ LA LIGNE DE LA FIN DE L'IMAGE, lettre de cette image comprise : la liste
  // suit la frappe sans retard d'une image.
  const ChatCommandListView view = ViewOf(state, hints, field_active);
  const std::optional<std::size_t> picked =
      DrawChatCommandList(state.commands, view, hints, field_corner, width, art);
  if (!picked.has_value()) {
    return;
  }
  CopyBounded(state.text, PickChatCommand(state.commands, view, hints, *picked));
  // 🔴 LE CHAMP SE RECHARGE À L'IMAGE SUIVANTE, comme pour un emoji pioché : le
  // clic l'a désactivé, et c'est en se réactivant qu'il doit relire son tampon.
  state.commands.reload_field = true;
  state.focus_on_whisper = false;
  state.wants_focus = true;
}

}  // namespace ro::ui
