// LE RAPPORT DE BUG : la boîte de saisie et l'accusé. Voir l'ADR 0052.

#include "ro/ui/bug_report_window.h"

#include <algorithm>
#include <cfloat>
#include <cstring>
#include <string>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/modal_box.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

namespace ro::ui {
namespace {

// ⚠ `###` : l'identité survit à la traduction du titre, et l'ouverture et le
// dessin doivent porter EXACTEMENT la même chaîne.
constexpr const char* kBugReportModalId = "###bug_report";

// La cadence du client, trois secondes. ⚠ CE N'EST PAS CELLE QUI FAIT FOI : le
// serveur en refuse un par trente secondes et par compte, et l'accusé le dit.
constexpr double kSendPauseSeconds = 3.0;
// La boîte, en largeur ; la saisie, en lignes.
constexpr float kModalWidthPixels = 420.0F;
constexpr float kMessageLines = 5.0F;
constexpr float kButtonWidthPixels = 120.0F;

// L'ACCUSÉ : cinq secondes, un fondu d'entrée bref et un de sortie sur le
// dernier quart.
constexpr double kAckSeconds = 5.0;
constexpr double kAckFadeInSeconds = 0.15;
constexpr double kAckFadeOutShare = 0.75;
// Sa boîte : centrée, dans le tiers supérieur de l'écran.
constexpr float kAckScreenHeightShare = 0.15F;
constexpr float kAckSlidePixels = 10.0F;
constexpr float kAckTitleScale = 1.45F;
constexpr float kAckBodyScale = 1.10F;
constexpr float kAckPaddingPixels = 18.0F;
constexpr float kAckGapPixels = 7.0F;
constexpr float kAckAccentWidthPixels = 7.0F;
constexpr float kAckRounding = 9.0F;
constexpr float kAckEdgeWidth = 1.5F;
constexpr ImVec2 kAckShadowOffset(5.0F, 6.0F);
constexpr int kAckShadowAlpha = 110;
constexpr int kAckFillAlpha = 248;
constexpr int kAckEdgeAlpha = 45;
constexpr ImU32 kAckFillRgb = IM_COL32(28, 30, 35, 0);
constexpr ImU32 kAckTextRgb = IM_COL32(236, 236, 236, 0);
constexpr ImU32 kAckSavedRgb = IM_COL32(90, 205, 105, 0);
constexpr ImU32 kAckTooSoonRgb = IM_COL32(238, 190, 90, 0);
constexpr ImU32 kAckFailedRgb = IM_COL32(232, 100, 100, 0);
constexpr int kOpaque = 255;

[[nodiscard]] std::string ModalTitle() {
  return std::string(core::Tr("Signaler un bug")) + kBugReportModalId;
}

[[nodiscard]] ImU32 WithAlpha(ImU32 rgb, int alpha) {
  return rgb | (static_cast<ImU32>(std::clamp(alpha, 0, kOpaque)) << IM_COL32_A_SHIFT);
}

void DrawAck(BugReportWindowState& state) {
  const double now = ImGui::GetTime();
  if (state.ack_fresh) {
    state.ack_fresh = false;
    state.ack_at_s = now;
  }
  if (state.ack_status < 0) {
    return;
  }
  const double elapsed = now - state.ack_at_s;
  if (elapsed > kAckSeconds) {
    state.ack_status = -1;
    return;
  }
  const double fade_out_from = kAckSeconds * kAckFadeOutShare;
  double alpha = 1.0;
  if (elapsed < kAckFadeInSeconds) {
    alpha = elapsed / kAckFadeInSeconds;
  } else if (elapsed > fade_out_from) {
    alpha = 1.0 - (elapsed - fade_out_from) / (kAckSeconds - fade_out_from);
  }
  alpha = std::clamp(alpha, 0.0, 1.0);
  const float shown = static_cast<float>(alpha);

  const char* title = core::Tr("Échec de l'envoi");
  const char* text = core::Tr("Le rapport n'a pas pu être enregistré (erreur serveur).");
  ImU32 accent = kAckFailedRgb;
  if (state.ack_status == kBugReportSaved) {
    title = core::Tr("Rapport envoyé");
    text = core::Tr("Merci ! Ton rapport de bug a bien été enregistré.");
    accent = kAckSavedRgb;
  } else if (state.ack_status == kBugReportTooSoon) {
    title = core::Tr("Trop rapide");
    text = core::Tr("Patiente un instant avant d'envoyer un nouveau rapport.");
    accent = kAckTooSoonRgb;
  }

  ImDrawList* const into = ImGui::GetForegroundDrawList();
  ImFont* const font = ImGui::GetFont();
  const float title_size = ImGui::GetFontSize() * kAckTitleScale;
  const float body_size = ImGui::GetFontSize() * kAckBodyScale;
  const ImVec2 title_extent = font->CalcTextSizeA(title_size, FLT_MAX, 0.0F, title);
  const ImVec2 body_extent = font->CalcTextSizeA(body_size, FLT_MAX, 0.0F, text);
  const float width =
      kAckAccentWidthPixels + 2.0F * kAckPaddingPixels + std::max(title_extent.x, body_extent.x);
  const float height = 2.0F * kAckPaddingPixels + title_extent.y + kAckGapPixels + body_extent.y;
  const ImVec2 screen = ImGui::GetMainViewport()->Size;
  const ImVec2 top_left((screen.x - width) * 0.5F,
                        screen.y * kAckScreenHeightShare - (1.0F - shown) * kAckSlidePixels);
  const ImVec2 bottom_right(top_left.x + width, top_left.y + height);
  const auto faded = [shown](int alpha_at_full) {
    return static_cast<int>(static_cast<float>(alpha_at_full) * shown);
  };

  into->AddRectFilled(
      ImVec2(top_left.x + kAckShadowOffset.x, top_left.y + kAckShadowOffset.y),
      ImVec2(bottom_right.x + kAckShadowOffset.x, bottom_right.y + kAckShadowOffset.y),
      WithAlpha(0, faded(kAckShadowAlpha)), kAckRounding);
  into->AddRectFilled(top_left, bottom_right, WithAlpha(kAckFillRgb, faded(kAckFillAlpha)),
                      kAckRounding);
  into->AddRect(top_left, bottom_right, WithAlpha(IM_COL32(255, 255, 255, 0), faded(kAckEdgeAlpha)),
                kAckRounding, 0, kAckEdgeWidth);
  const ImU32 accent_ink = WithAlpha(accent, faded(kOpaque));
  into->AddRectFilled(top_left, ImVec2(top_left.x + kAckAccentWidthPixels, bottom_right.y),
                      accent_ink, kAckRounding, ImDrawFlags_RoundCornersLeft);
  const float text_x = top_left.x + kAckAccentWidthPixels + kAckPaddingPixels;
  into->AddText(font, title_size, ImVec2(text_x, top_left.y + kAckPaddingPixels), accent_ink,
                title);
  into->AddText(font, body_size,
                ImVec2(text_x, top_left.y + kAckPaddingPixels + title_extent.y + kAckGapPixels),
                WithAlpha(kAckTextRgb, faded(kOpaque)), text);
}

}  // namespace

void PoseBugReportButton(bool shown) {
  if (!shown) {
    return;
  }
  SetNextWindowTitleAction(core::Tr("Signaler un bug"),
                           core::Tr("Envoyer un rapport de bug à l'équipe (le contexte est "
                                    "joint automatiquement)"),
                           true);
}

BugReportAction DrawBugReportWindow(BugReportWindowState& state, const SkinArt& art) {
  BugReportAction action;
  if (state.asked) {
    state.asked = false;
    if (!state.open) {
      state.open = true;
      state.message.fill('\0');
      AskModal(ModalTitle().c_str());
    }
  }
  DrawAck(state);
  if (!state.open) {
    return action;
  }
  ImGui::SetNextWindowSize(ImVec2(kModalWidthPixels, 0.0F), ImGuiCond_Appearing);
  if (!BeginSkinnedModal(ModalTitle().c_str(), art, ImGuiWindowFlags_None)) {
    // Fermée par Échap ou par ImGui : elle se rouvrira à la prochaine demande.
    state.open = false;
    return action;
  }
  ImGui::TextWrapped("%s", core::Tr("Décris brièvement le problème. Le contexte ci-dessous "
                                    "est joint automatiquement à ton rapport."));
  ImGui::Separator();
  ImGui::TextDisabled("%s", core::Tr("Contexte"));
  ImGui::TextWrapped("%s", state.context_label.c_str());
  ImGui::Spacing();
  ImGui::TextDisabled("%s", core::Tr("Ton message"));
  ImGui::InputTextMultiline("##message_rapport", state.message.data(), state.message.size(),
                            ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * kMessageLines));
  ImGui::TextDisabled(core::Tr("%zu / %zu caractères"), std::strlen(state.message.data()),
                      kBugReportMessageBytes);
  ImGui::Separator();

  const bool has_message = state.message[0] != '\0';
  const bool paused =
      state.sent_at_s >= 0.0 && ImGui::GetTime() - state.sent_at_s < kSendPauseSeconds;
  ImGui::BeginDisabled(!has_message || paused);
  if (SkinButton(core::Tr("Envoyer"), art, ImVec2(kButtonWidthPixels, 0.0F))) {
    action.send = true;
    action.message = state.message.data();
    state.sent_at_s = ImGui::GetTime();
    state.open = false;
    ImGui::CloseCurrentPopup();
  }
  ImGui::EndDisabled();
  // ⚠ SURVOLÉ MÊME GRISÉ : c'est là qu'il faut dire pourquoi il l'est.
  if (paused) {
    SkinTooltip(core::Tr("Patiente quelques secondes avant un nouveau rapport."));
  }
  ImGui::SameLine();
  if (SkinButton(core::Tr("Annuler"), art, ImVec2(kButtonWidthPixels, 0.0F))) {
    state.open = false;
    ImGui::CloseCurrentPopup();
  }
  EndSkinnedModal();
  return action;
}

}  // namespace ro::ui
