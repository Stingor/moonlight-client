// LA FICHE D'UN OBJET, ÉDITÉE PAR LE STAFF. Voir `ro/ui/item_sheet_editor.h`.

#include "ro/ui/item_sheet_editor.h"

#include <algorithm>
#include <cfloat>
#include <cstring>
#include <string>
#include <string_view>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/core/printed.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/window_bounds.h"
#include "ro/ui/window_name.h"

#include "ui_scale.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

constexpr const char* kTitle = core::TrText("Fiche d'item (staff)");

constexpr float kFirstWidthPixels = 560.0F;
constexpr float kFirstHeightPixels = 520.0F;
constexpr float kLeastWidthPixels = 420.0F;
constexpr float kLeastHeightPixels = 380.0F;
// La colonne des libellés, et le côté d'une icône : le double des 24 pixels
// d'une icône de sac, au plus proche, pour qu'une teinte se juge.
constexpr float kLabelColumnPixels = 96.0F;
constexpr float kIconSidePixels = 48.0F;
constexpr float kIconGapPixels = 12.0F;
// Les lignes que la description laisse sous elle et au-dessus : l'état (au
// plus quatre lignes) et les trois champs d'une ligne. Et sa hauteur la
// moindre, en lignes de texte.
constexpr float kStatusLines = 4.0F;
constexpr float kSingleLineFields = 3.0F;
constexpr float kLeastDescriptionLines = 4.0F;

constexpr ImU32 kGoodColor = IM_COL32(38, 110, 46, 255);
constexpr ImU32 kBadColor = IM_COL32(153, 31, 31, 255);

// Copie un texte dans un tampon de saisie, coupé à sa taille.
template <std::size_t kBytes>
void Fill(std::array<char, kBytes>& into, std::string_view text) {
  const std::size_t kept = std::min(text.size(), kBytes - 1);
  std::memcpy(into.data(), text.data(), kept);
  into[kept] = '\0';
}

void FillFromView(ItemSheetEditorState& state, const ItemSheetEditorView& view) {
  Fill(state.name, view.start.name);
  Fill(state.resource, view.start.resource);
  Fill(state.description, ItemSheetEditorText(view.start.description));
  state.tint_rank = view.start.tint_rank;
  state.filled = true;
}

// Le libellé d'une ligne de champ, dans sa colonne.
void FieldLabel(const char* label) {
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(label);
  ImGui::SameLine(Px(kLabelColumnPixels));
}

void DrawIcon(const SkinImage& icon, const char* caption) {
  ImGui::BeginGroup();
  const float side = Px(kIconSidePixels);
  if (icon.id != 0) {
    ImGui::Image(icon.id, ImVec2(side, side));
  } else {
    ImGui::Dummy(ImVec2(side, side));
  }
  FaintText(caption);
  ImGui::EndGroup();
}

void DrawIcons(const ItemSheetEditorView& view) {
  DrawIcon(view.table_icon, core::Tr("Table"));
  ImGui::SameLine(0.0F, Px(kIconGapPixels));
  DrawIcon(view.preview_icon, core::Tr("Aperçu"));
  if (view.preview_icon.id == 0) {
    ImGui::SameLine(0.0F, Px(kIconGapPixels));
    ImGui::PushStyleColor(ImGuiCol_Text, kBadColor);
    ImGui::TextWrapped("%s", core::Tr("L'installation n'a pas d'icône sous cette ressource."));
    ImGui::PopStyleColor();
  }
}

void DrawFields(ItemSheetEditorState& state, const ItemSheetEditorView& view, const SkinArt& art,
                float description_height) {
  FieldLabel(core::Tr("Nom"));
  ImGui::SetNextItemWidth(-FLT_MIN);
  ImGui::InputText("##nom", state.name.data(), state.name.size());

  FieldLabel(core::Tr("Ressource"));
  ImGui::SetNextItemWidth(-FLT_MIN);
  ImGui::InputText("##ressource", state.resource.data(), state.resource.size());
  if (ImGui::IsItemHovered()) {
    SkinTooltip(
        core::Tr("Le nom du fichier de l'icône et de l'illustration de collection, sans "
                 "extension. Toutes les cartes portent celui du dos de carte générique."));
  }

  FieldLabel(core::Tr("Teinte d'icône"));
  ImGui::SetNextItemWidth(-FLT_MIN);
  static_cast<void>(SkinCombo("##teinte", &state.tint_rank, view.tint_labels, art));
  if (ImGui::IsItemHovered()) {
    SkinTooltip(
        core::Tr("La couleur posée sur l'icône au chargement, à la luminance de chaque "
                 "pixel. Aucun fichier de l'installation n'est réécrit."));
  }

  FieldLabel(core::Tr("Description"));
  ImGui::InputTextMultiline("##description", state.description.data(), state.description.size(),
                            ImVec2(-FLT_MIN, description_height));
}

void DrawStatus(const ItemSheetEditorView& view) {
  if (!view.known) {
    FaintText(core::Tr("Cet item n'est pas dans la table : le bon créera sa fiche."));
  }
  if (view.pending) {
    FaintText(core::Tr("Un bon attend le script pour cet item : la saisie le montre."));
  }
  if (view.pending_count > 0) {
    FaintText(core::Printed(core::Tr("Bons en attente : %zu. À porter par "
                                     "apply_item_info_delta.py, puis relancer le client."),
                            view.pending_count)
                  .c_str());
  }
  if (!view.notice.empty()) {
    ImGui::PushStyleColor(ImGuiCol_Text, view.notice_ok ? kGoodColor : kBadColor);
    ImGui::TextWrapped("%.*s", static_cast<int>(view.notice.size()), view.notice.data());
    ImGui::PopStyleColor();
  }
}

void DrawFooter(ItemSheetEditorState& state, const ItemSheetEditorView& view, const SkinArt& art,
                ItemSheetEditorAnswer& answer) {
  if (SkinButton(core::Tr("Enregistrer le bon"), art)) {
    answer.save = true;
  }
  if (ImGui::IsItemHovered()) {
    SkinTooltip(
        core::Tr("Écrit les champs changés dans savedata\\item_info_delta.yml. Le "
                 "client n'en montre rien avant le passage du script."));
  }
  ImGui::SameLine();
  ImGui::BeginDisabled(!view.pending);
  if (SkinButton(core::Tr("Annuler le bon"), art)) {
    answer.clear_pending = true;
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  if (SkinButton(core::Tr("Rétablir"), art)) {
    state.filled = false;
  }
  if (ImGui::IsItemHovered()) {
    SkinTooltip(core::Tr("Oublie la saisie : revient au bon en attente, sinon à la table."));
  }
  ImGui::SameLine();
  const char* const close = core::Tr("Fermer");
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0F, ImGui::GetContentRegionAvail().x -
                                                                   SkinButtonWidth(close, art)));
  if (SkinButton(close, art)) {
    state.open = false;
  }
}

}  // namespace

void OpenItemSheetEditor(ItemSheetEditorState& state, std::uint32_t item_id) {
  state.open = true;
  state.raise = true;
  state.item_id = item_id;
  state.filled = false;
}

std::vector<std::string> ItemSheetEditorLines(std::string_view text) {
  std::vector<std::string> lines;
  if (text.empty()) {
    return lines;
  }
  std::size_t at = 0;
  while (at <= text.size()) {
    const std::size_t end = std::min(text.find('\n', at), text.size());
    std::string_view line = text.substr(at, end - at);
    if (!line.empty() && line.back() == '\r') {
      line.remove_suffix(1);
    }
    lines.emplace_back(line);
    at = end + 1;
  }
  if (!lines.empty() && lines.back().empty()) {
    lines.pop_back();
  }
  return lines;
}

std::string ItemSheetEditorText(std::span<const std::string> lines) {
  std::string text;
  for (std::size_t rank = 0; rank < lines.size(); ++rank) {
    text += rank == 0 ? "" : "\n";
    text += lines[rank];
  }
  return text;
}

ItemSheetEditorAnswer DrawItemSheetEditor(ItemSheetEditorState& state,
                                          const ItemSheetEditorView& view, const SkinArt& art) {
  ItemSheetEditorAnswer answer;
  if (!state.open || view.item_id != state.item_id) {
    return answer;
  }
  if (!state.filled) {
    FillFromView(state, view);
  }
  if (state.raise) {
    state.raise = false;
    ImGui::SetNextWindowFocus();
  }
  // Recomposé au changement de langue seulement.
  static WindowName name(kTitle, kItemSheetEditorId);
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_FirstUseEver,
                          ImVec2(0.5F, 0.5F));
  ImGui::SetNextWindowSize(ImVec2(Px(kFirstWidthPixels), Px(kFirstHeightPixels)),
                           ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints(ImVec2(Px(kLeastWidthPixels), Px(kLeastHeightPixels)),
                                      ImVec2(FLT_MAX, FLT_MAX));
  ConstrainNextWindowToScreen(name.Get());
  constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoCollapse;
  if (BeginSkinnedWindow(name.Get(), &state.open, art, SkinnedTitle::kBar, kFlags) && state.open) {
    ImGui::TextUnformatted(core::Printed(core::Tr("Item %u"), view.item_id).c_str());
    DrawIcons(view);
    ImGui::Separator();
    // La description prend ce qui reste, moins les champs d'une ligne, l'état
    // et le pied.
    const float below = ImGui::GetTextLineHeightWithSpacing() * kStatusLines +
                        ImGui::GetFrameHeightWithSpacing() * kSingleLineFields +
                        SkinFooterHeight(art);
    const float description_height = std::max(ImGui::GetTextLineHeight() * kLeastDescriptionLines,
                                              ImGui::GetContentRegionAvail().y - below);
    DrawFields(state, view, art, description_height);
    DrawStatus(view);
    DrawFooter(state, view, art, answer);
  }
  EndSkinnedWindow();
  return answer;
}

}  // namespace ro::ui
