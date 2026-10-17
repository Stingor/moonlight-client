#include "ro/ui/talkbox_window.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <string>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/bitmap_button.h"
#include "ro/ui/skinned_window.h"

#include "ui_scale.h"

namespace ro::ui {
namespace {

// Le client écrit la ligne en noir, puis la même en blanc un pixel plus bas et
// plus à droite, dessous : une ombre claire.
constexpr ImU32 kPromptColor = IM_COL32(0, 0, 0, 255);
constexpr ImU32 kPromptShadowColor = IM_COL32(255, 255, 255, 255);
constexpr float kPromptShadowPixels = 1.0F;
// Le texte tapé, sur le fond clair du champ.
constexpr ImU32 kFieldTextColor = IM_COL32(0, 0, 0, 255);
// Le compteur de caractères, en gris, deux pixels sous le champ.
constexpr ImU32 kCounterColor = IM_COL32(110, 110, 110, 255);
constexpr float kCounterDrop = 2.0F;
constexpr std::size_t kCounterBytes = 16;
// Le repère de la hauteur d'écran sur laquelle le client place la fenêtre.
constexpr float kReferenceScreenHeight = 480.0F;
constexpr float kReferenceScreenWidth = 640.0F;

[[nodiscard]] ImVec2 At(ImVec2 corner, ImVec2 art_pixels, float scale) {
  return ImVec2(corner.x + art_pixels.x * scale, corner.y + art_pixels.y * scale);
}

}  // namespace

TalkboxWindowAction DrawTalkboxWindow(TalkboxWindowState& state, const TalkboxView& view,
                                      const SkinArt& art) {
  TalkboxWindowAction action;
  if (!state.open) {
    state.was_open = false;
    return action;
  }
  const float scale = Scale();
  const bool opening = !state.was_open;
  if (opening) {
    state.was_open = true;
    state.text.fill('\0');
    ImGui::SetNextWindowFocus();
    // ⚠ À CHAQUE OUVERTURE, comme le client, qui la recrée : un X pensé pour
    // un écran de 640 de large et recentré, une hauteur rapportée à 480.
    const ImGuiViewport* screen = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(
        ImVec2(screen->Pos.x + (screen->Size.x - kReferenceScreenWidth * scale) * 0.5F +
                   kTalkboxWindowLeft * scale,
               screen->Pos.y + kTalkboxWindowTopOn480 * screen->Size.y / kReferenceScreenHeight -
                   kTalkboxWindowLift * scale),
        ImGuiCond_Always);
  }
  ImGui::SetNextWindowSize(ImVec2(kTalkboxWindowWidth * scale, kTalkboxWindowHeight * scale),
                           ImGuiCond_Always);

  // Le titre change avec la langue, l'identité jamais.
  std::string name(view.title);
  name += "###";
  name += kTalkboxWindowId;
  bool open = true;
  const bool begun =
      BeginSkinnedWindow(name.c_str(), &open, art, SkinnedTitle::kBar,
                         ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  // La croix vaut « Annuler » : rien ne part.
  if (!open) {
    action.cancelled = true;
  }
  if (!begun) {
    EndSkinnedWindow();
    return action;
  }

  const ImVec2 corner = ImGui::GetWindowPos();
  ImDrawList* const into = ImGui::GetWindowDrawList();
  const ImVec2 prompt = At(corner, kTalkboxPromptOrigin, scale);
  const char* const prompt_begin = view.prompt.data();
  const char* const prompt_end = view.prompt.data() + view.prompt.size();
  into->AddText(
      ImVec2(prompt.x + kPromptShadowPixels * scale, prompt.y + kPromptShadowPixels * scale),
      kPromptShadowColor, prompt_begin, prompt_end);
  into->AddText(prompt, kPromptColor, prompt_begin, prompt_end);

  // ── LE CHAMP : il a le clavier dès l'ouverture ─────────────────────────────
  ImGui::SetCursorScreenPos(At(corner, kTalkboxFieldOrigin, scale));
  ImGui::PushStyleColor(ImGuiCol_FrameBg, kTalkboxFieldColor);
  ImGui::PushStyleColor(ImGuiCol_Text, kFieldTextColor);
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0F, 0.0F));
  ImGui::SetNextItemWidth(kTalkboxFieldSize.x * scale);
  if (opening) {
    ImGui::SetKeyboardFocusHere();
  }
  // ⚠ ENTRÉE VAUT « OK » : le champ la remonte à la fenêtre, qui presse son
  // bouton par défaut. Échap, lui, n'a pas de bouton désigné et ne fait rien.
  const std::size_t field_bytes = std::min(view.field_chars, kTalkboxFieldChars) + 1;
  const bool entered = ImGui::InputText("##talkbox_text", state.text.data(), field_bytes,
                                        ImGuiInputTextFlags_EnterReturnsTrue);
  ImGui::PopStyleVar();
  ImGui::PopStyleColor(2);

  // ── CE QUI RESTE À ÉCRIRE, sous le bord droit du champ : le serveur coupe
  // au-delà sans prévenir.
  std::array<char, kCounterBytes> counter{};
  std::snprintf(counter.data(), counter.size(), core::Tr("%d / %d"),
                static_cast<int>(std::strlen(state.text.data())),
                static_cast<int>(field_bytes - 1));
  const ImVec2 field = At(corner, kTalkboxFieldOrigin, scale);
  into->AddText(
      ImVec2(field.x + kTalkboxFieldSize.x * scale - ImGui::CalcTextSize(counter.data()).x,
             field.y + (kTalkboxFieldSize.y + kCounterDrop) * scale),
      kCounterColor, counter.data());

  // ── LES DEUX BOUTONS ───────────────────────────────────────────────────────
  const ImVec2 button(kTalkboxButtonSize.x * scale, kTalkboxButtonSize.y * scale);
  ImGui::SetCursorScreenPos(At(corner, kTalkboxOkOrigin, scale));
  if (BitmapButton("##talkbox_ok", art, kOkFaces, core::Tr("OK"), button) || entered) {
    action.confirmed = std::string(state.text.data());
  }
  ImGui::SetCursorScreenPos(At(corner, kTalkboxCancelOrigin, scale));
  if (BitmapButton("##talkbox_cancel", art, kCancelFaces, core::Tr("Annuler"), button)) {
    action.cancelled = true;
  }
  EndSkinnedWindow();
  return action;
}

}  // namespace ro::ui
