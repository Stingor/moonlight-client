#include "ro/ui/player_report_window.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <string>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/core/text.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

namespace ro::ui {
namespace {

[[nodiscard]] float Px(float pixels) { return pixels * ImGui::GetStyle().FontScaleMain; }

// ── LES MESURES ─────────────────────────────────────────────────────────────
constexpr float kFirstWidthPixels = 380.0F;
constexpr float kFirstHeightPixels = 320.0F;
constexpr float kLeastWidthPixels = 300.0F;
constexpr float kLeastHeightPixels = 260.0F;
// Ce que la saisie laisse au moins sous elle : le compteur et l'avertissement.
constexpr float kBelowMessageLines = 3.0F;
// La saisie ne rapetisse pas sous deux lignes.
constexpr float kLeastMessageLines = 2.0F;

// LE MOTIF : deux boutons radio, comme chez le client d'origine.
void DrawMotives(PlayerReportWindowState& state, const SkinArt& art) {
  if (SkinRadio(core::Tr("Programme illégal soupçonné"),
                state.motive == PlayerReportMotive::kIllegalProgram, art)) {
    state.motive = PlayerReportMotive::kIllegalProgram;
  }
  if (SkinRadio(core::Tr("Bug ou abus du système"), state.motive == PlayerReportMotive::kBugAbuse,
                art)) {
    state.motive = PlayerReportMotive::kBugAbuse;
  }
}

// LE TEXTE LIBRE, son compte, et l'avertissement du client d'origine.
void DrawMessage(PlayerReportWindowState& state, const SkinArt& art, std::size_t typed) {
  ImGui::TextDisabled("%s", core::Tr("Saisissez le contenu du signalement."));
  const float below = (ImGui::GetTextLineHeightWithSpacing() * kBelowMessageLines) +
                      SkinFooterHeight(art) + ImGui::GetStyle().ItemSpacing.y;
  const float height = std::max(ImGui::GetTextLineHeight() * kLeastMessageLines,
                                ImGui::GetContentRegionAvail().y - below);
  ImGui::InputTextMultiline("##signalement_texte", state.message.data(), state.message.size(),
                            ImVec2(-FLT_MIN, height));
  const bool too_long = typed > kPlayerReportMessageMostChars;
  if (too_long) {
    ImGui::PushStyleColor(ImGuiCol_Text, PaintingLook().bad_text);
  }
  ImGui::TextDisabled(core::Tr("%zu / %zu caractères"), typed, kPlayerReportMessageMostChars);
  if (too_long) {
    ImGui::PopStyleColor();
  }
  ImGui::PushStyleColor(ImGuiCol_Text, PaintingLook().bad_text);
  ImGui::TextWrapped("%s", core::Tr("Un usage abusif de cette fonction peut être sanctionné."));
  ImGui::PopStyleColor();
}

// LE PIED : « Envoyer » à gauche, « Annuler » à droite, du côté de la poignée.
void DrawFooter(PlayerReportWindowState& state, const SkinArt& art, bool can_send,
                PlayerReportAnswer& answer) {
  const std::array<const char*, 2> labels = {core::Tr("Envoyer"), core::Tr("Annuler")};
  const float width = SkinWidestButton(labels, art);
  ImGui::BeginDisabled(!can_send);
  if (SkinButton(labels[0], art, ImVec2(width, 0.0F))) {
    answer.send = true;
    state.open = false;
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                       std::max(0.0F, ImGui::GetContentRegionAvail().x - width));
  if (SkinButton(labels[1], art, ImVec2(width, 0.0F))) {
    state.open = false;
  }
}

}  // namespace

std::size_t Utf8CharCount(std::string_view utf8) noexcept {
  std::size_t count = 0;
  for (std::size_t at = 0; at < utf8.size();) {
    static_cast<void>(core::NextCodePoint(utf8, at));
    ++count;
  }
  return count;
}

PlayerReportAnswer DrawPlayerReportWindow(PlayerReportWindowState& state, const SkinArt& art) {
  PlayerReportAnswer answer;
  if (!state.open) {
    return answer;
  }
  if (state.raise) {
    state.raise = false;
    ImGui::SetNextWindowFocus();
  }
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_FirstUseEver,
                          ImVec2(0.5F, 0.5F));
  ImGui::SetNextWindowSize(ImVec2(Px(kFirstWidthPixels), Px(kFirstHeightPixels)),
                           ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints(ImVec2(Px(kLeastWidthPixels), Px(kLeastHeightPixels)),
                                      ImVec2(FLT_MAX, FLT_MAX));
  const std::string name =
      std::string(core::Tr("Signaler un joueur")) + "###" + kPlayerReportWindowId;
  constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
                                      ImGuiWindowFlags_NoScrollWithMouse;
  if (BeginSkinnedWindow(name.c_str(), &state.open, art, SkinnedTitle::kBar, kFlags) &&
      state.open) {
    ImGui::TextWrapped(core::Tr("Joueur signalé : %s"), state.target_name.c_str());
    ImGui::Separator();
    DrawMotives(state, art);
    ImGui::Spacing();
    const std::size_t typed = Utf8CharCount(state.message.data());
    DrawMessage(state, art, typed);
    DrawFooter(state, art, typed <= kPlayerReportMessageMostChars, answer);
  }
  EndSkinnedWindow();
  return answer;
}

}  // namespace ro::ui
