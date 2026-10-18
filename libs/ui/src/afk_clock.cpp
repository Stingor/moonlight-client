#include "ro/ui/afk_clock.h"

#include <algorithm>

#include "ui_text.h"

namespace ro::ui {
namespace {

// ⚠ JAMAIS TRADUIT : c'est une clé ImGui, et la changer de langue en ferait une
// autre fenêtre — qui repartirait au coin de l'écran.
constexpr const char* kClockId = "##veille_horloge";

// L'OMBRE PORTÉE, d'un pixel vers le bas et la droite.
//
// 🔴 UNE OMBRE, PAS UN CERNE, et c'est la règle déjà payée par le compteur
// d'images : un cerne demande huit passes et se voit comme un halo, là où une
// ombre d'un pixel suffit à décoller le texte de n'importe quel décor.
constexpr float kShadowOffsetPixels = 1.0F;

// En dessous, la rampe ne se voit plus : on ne peint pas.
constexpr float kFaintEnoughToSkip = 0.01F;

// Une ligne centrée dans la fenêtre, à sa taille, avec son ombre.
void ClockLine(const char* text, float base_font_size, float scale, ImU32 color, bool shadow) {
  // ⚠ LA TAILLE EST DEMANDÉE EN « BASE NON MISE À L'ÉCHELLE » : ImGui lui
  // applique ensuite l'échelle d'interface du joueur, comme à tout le reste.
  // Passer la taille finale ferait une horloge qui ignore ce réglage-là.
  ImGui::PushFont(nullptr, base_font_size * scale);
  const float left = (ImGui::GetWindowWidth() - ImGui::CalcTextSize(text).x) * 0.5F;
  const float top = ImGui::GetCursorPosY();
  if (shadow) {
    // ⚠ L'OMBRE PREND L'ALPHA DU TEXTE : à couleur transparente, elle s'efface
    // avec lui au lieu de rester seule en noir.
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 0, (color >> IM_COL32_A_SHIFT) & 0xFFU));
    ImGui::SetCursorPos(ImVec2(left + kShadowOffsetPixels, top + kShadowOffsetPixels));
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
    ImGui::SetCursorPos(ImVec2(left, top));  // par-dessus, à la bonne place
  } else {
    ImGui::SetCursorPosX(left);
  }
  ColoredText(color, text);
  ImGui::PopFont();
}

}  // namespace

void DrawAfkClock(const AfkClockView& view) {
  if (view.time == nullptr || view.time[0] == '\0' || view.fade <= kFaintEnoughToSkip) {
    return;
  }

  // L'ANCRAGE DONNE À LA FOIS LA PLACE ET LE PIVOT, et c'est ce qui rend les
  // neuf cas identiques : un ancrage à droite colle son BORD DROIT à la marge,
  // sans cas particulier. Une fenêtre auto-dimensionnée dont on ne bougerait
  // que la position déborderait de l'écran dès que le texte s'allonge.
  const ImGuiViewport* screen = ImGui::GetMainViewport();
  const int anchor = std::clamp(view.anchor, 0, kAfkClockAnchorCount - 1);
  const float column = static_cast<float>(anchor % kAfkClockAnchorColumns) * 0.5F;
  const float row = static_cast<float>(anchor / kAfkClockAnchorColumns) * 0.5F;
  const auto margin = static_cast<float>(std::clamp(view.margin, 0, kMostAfkClockMargin));
  ImGui::SetNextWindowPos(
      ImVec2(screen->WorkPos.x + margin + (screen->WorkSize.x - 2.0F * margin) * column,
             screen->WorkPos.y + margin + (screen->WorkSize.y - 2.0F * margin) * row),
      ImGuiCond_Always, ImVec2(column, row));
  ImGui::SetNextWindowBgAlpha(0.0F);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
  // ⚠ `NoInputs` N'EST PAS DÉCORATIF : sans lui, cette fenêtre intercepterait
  // le clic qui met fin à la veille, et la souris ne rendrait jamais la main.
  if (ImGui::Begin(kClockId, nullptr,
                   ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                       ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings |
                       ImGuiWindowFlags_AlwaysAutoResize)) {
    const float base = ImGui::GetStyle().FontSizeBase;
    const float scale = std::clamp(view.scale, kLeastAfkClockScale, kMostAfkClockScale);
    // Le fondu passe par le style global : la couleur du texte le reprend, donc
    // l'ombre s'estompe avec lui sans qu'on ait à la teinter nous-mêmes.
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, std::clamp(view.fade, 0.0F, 1.0F));
    ClockLine(view.time, base, scale, view.color, view.shadow);
    if (view.away != nullptr && view.away[0] != '\0') {
      const auto alpha = static_cast<ImU32>(
          static_cast<float>((view.color >> IM_COL32_A_SHIFT) & 0xFFU) * kAfkAwayOpacity);
      const ImU32 faded = (view.color & ~IM_COL32_A_MASK) | (alpha << IM_COL32_A_SHIFT);
      ClockLine(view.away, base,
                std::clamp(scale * kAfkAwayScale, kLeastAfkClockScale, kMostAfkClockScale), faded,
                view.shadow);
    }
    ImGui::PopStyleVar();
  }
  ImGui::End();
  ImGui::PopStyleVar();
}

}  // namespace ro::ui
