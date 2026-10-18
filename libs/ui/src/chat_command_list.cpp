// Le dessin de la liste des commandes. Voir l'en-tête.

#include "chat_command_list.h"

#include <algorithm>
#include <string>
#include <string_view>

#include <imgui_internal.h>

#include "ro/ui/skin_tooltip.h"

#include "ui_text.h"

namespace ro::ui {
namespace {

constexpr const char* kListWindow = "##commandes";
// L'écart entre la liste et le haut du champ.
constexpr float kListGap = 2.0F;
constexpr int kRowsShown = static_cast<int>(kChatCommandRowsShown);

// UNE RANGÉE : la graphie tapée, ce que la commande attend, le nom d'origine
// quand c'est un alias qui a répondu, puis la première ligne de l'aide en gris.
// ⚠ L'AIDE SE COUPE À LA LARGEUR, points de suspension compris : la rangée
// entière se lit au survol.
void PaintRow(const ChatCommandHint& hint, const ChatCommandMatch& match, ImVec2 at, float right,
              const SkinLook& look) {
  ImDrawList* draw = ImGui::GetWindowDrawList();
  const float spacing = ImGui::GetStyle().ItemSpacing.x;
  std::string spelled(1, hint.sigil);
  spelled += match.spelled;
  draw->AddText(at, look.text, spelled.c_str());
  float x = at.x + TextWidth(spelled) + spacing;
  std::string faint;
  if (match.by_alias) {
    faint = "→ ";
    faint += hint.sigil;
    faint += hint.name;
  }
  if (!hint.params.empty()) {
    if (!faint.empty()) {
      faint += ' ';
    }
    faint += hint.params;
  }
  if (!faint.empty()) {
    draw->AddText(ImVec2(x, at.y), look.faint_text, faint.c_str());
    x += TextWidth(faint) + spacing;
  }
  const std::string_view help = FirstHelpLine(hint.help);
  if (help.empty() || x >= right) {
    return;
  }
  ImGui::PushStyleColor(ImGuiCol_Text, look.faint_text);
  ImGui::RenderTextEllipsis(draw, ImVec2(x, at.y), ImVec2(right, at.y + ImGui::GetFontSize()),
                            right, help.data(), help.data() + help.size(), nullptr);
  ImGui::PopStyleColor();
}

}  // namespace

std::optional<std::size_t> DrawChatCommandList(ChatCommandListState& state,
                                               const ChatCommandListView& view,
                                               std::span<const ChatCommandHint> hints,
                                               ImVec2 field_corner, float width,
                                               const SkinArt& art) {
  std::optional<std::size_t> picked;
  if (!view.open || view.matches.empty()) {
    return picked;
  }
  const SkinLook& look = art.look();
  const ImGuiStyle& style = ImGui::GetStyle();
  const int count = static_cast<int>(view.matches.size());
  const int shown = std::min(count, kRowsShown);
  const float row_height = ImGui::GetTextLineHeightWithSpacing();
  const float height =
      static_cast<float>(shown) * row_height - style.ItemSpacing.y + 2.0F * style.WindowPadding.y;
  ImGui::SetNextWindowPos(ImVec2(field_corner.x, field_corner.y - kListGap), ImGuiCond_Always,
                          ImVec2(0.0F, 1.0F));
  ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
  ImGui::PushStyleColor(ImGuiCol_WindowBg, look.popup);
  ImGui::PushStyleColor(ImGuiCol_Border, look.border);
  ImGui::PushStyleColor(ImGuiCol_Header, look.selection);
  ImGui::PushStyleColor(ImGuiCol_HeaderHovered, look.SelectionHover());
  ImGui::PushStyleColor(ImGuiCol_HeaderActive, look.SelectionActive());
  ImGui::PushStyleColor(ImGuiCol_Text, look.text);
  constexpr int kPushedColors = 6;
  // ⚠ NI FOCUS À L'APPARITION, NI NAVIGATION : la liste s'ouvre pendant qu'on
  // écrit, et le clavier doit rester au champ.
  constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoDecoration |
                                      ImGuiWindowFlags_NoSavedSettings |
                                      ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
                                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse;
  if (ImGui::Begin(kListWindow, nullptr, kFlags)) {
    // 🔴 DEVANT LA FENÊTRE DE CONVERSATION, à chaque image : celle-ci passe au
    // premier plan dès qu'on clique dans son champ, et couvrirait la liste,
    // posée justement sur elle.
    ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
    // La molette fait défiler, sans toucher au choix.
    if (ImGui::IsWindowHovered() && ImGui::GetIO().MouseWheel != 0.0F) {
      state.first_shown =
          std::clamp(state.first_shown + (ImGui::GetIO().MouseWheel > 0.0F ? -1 : 1), 0,
                     std::max(0, count - kRowsShown));
    }
    const float right = ImGui::GetWindowPos().x + width - style.WindowPadding.x;
    const int last = std::min(count, state.first_shown + kRowsShown);
    for (int row = state.first_shown; row < last; ++row) {
      const ChatCommandMatch& match = view.matches[static_cast<std::size_t>(row)];
      const ChatCommandHint& hint = hints[match.hint];
      ImGui::PushID(row);
      const ImVec2 at = ImGui::GetCursorScreenPos();
      if (ImGui::Selectable("##rangee", row == state.chosen, ImGuiSelectableFlags_SelectOnClick,
                            ImVec2(0.0F, ImGui::GetTextLineHeight()))) {
        picked = static_cast<std::size_t>(row);
      }
      if (ImGui::IsItemHovered() && !hint.help.empty()) {
        SkinTooltipFormat("%.*s", static_cast<int>(hint.help.size()), hint.help.data());
      }
      PaintRow(hint, match, at, right, look);
      ImGui::PopID();
    }
  }
  ImGui::End();
  ImGui::PopStyleColor(kPushedColors);
  return picked;
}

}  // namespace ro::ui
