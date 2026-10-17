#include "ro/ui/preset_tab.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <string_view>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/character_window.h"
#include "ro/ui/skin_text.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"

#include "ui_scale.h"

namespace ro::ui {
namespace {

constexpr ImVec4 kNameColor(0.0F, 0.0F, 0.0F, 1.0F);
constexpr ImVec4 kLabelColor(0.35F, 0.35F, 0.42F, 1.0F);
constexpr ImVec4 kRefusalColor(0.80F, 0.20F, 0.20F, 1.0F);

// Le cadre d'une pièce dit sa famille : un chapeau et son costume ont la même
// icône.
constexpr ImU32 kCostumeFrame = IM_COL32(150, 90, 190, 220);
constexpr ImU32 kAmmoFrame = IM_COL32(70, 130, 70, 220);

constexpr float kPieceCellPixels = 30.0F;
constexpr float kPieceGapPixels = 3.0F;
constexpr float kPieceRoundingPixels = 3.0F;
// La marge de l'icône dans sa case, et l'épaisseur d'un cadre de famille.
constexpr float kPieceInsetPixels = 2.0F;
constexpr float kBadgeLiftPixels = 1.0F;

constexpr float kButtonGapPixels = 4.0F;
constexpr float kHotkeyGapPixels = 6.0F;
constexpr float kRowEndPixels = 2.0F;
constexpr float kNameFieldMinimumPixels = 80.0F;
constexpr float kNameFieldGapPixels = 8.0F;

void ColoredText(const ImVec4& color, std::string_view text) {
  ImGui::PushStyleColor(ImGuiCol_Text, color);
  ImGui::TextUnformatted(text.data(), text.data() + text.size());
  ImGui::PopStyleColor();
}

// Une pièce de la rangée : sa case, son icône, son raffinement, son nom au
// survol. Elle ne réagit pas au clic.
void DrawPiece(const PresetPieceView& piece, float side, const SkinLook& look) {
  const float scale = Scale();
  const ImVec2 top_left = ImGui::GetCursorScreenPos();
  ImGui::Dummy(ImVec2(side, side));
  const ImVec2 bottom_right(top_left.x + side, top_left.y + side);
  const float rounding = kPieceRoundingPixels * scale;
  const float inset = kPieceInsetPixels * scale;
  ImDrawList* into = ImGui::GetWindowDrawList();
  into->AddRectFilled(top_left, bottom_right, look.slot, rounding);
  switch (piece.kind) {
    case PresetPieceKind::kCostume:
      into->AddRect(top_left, bottom_right, kCostumeFrame, rounding, 0, inset);
      break;
    case PresetPieceKind::kAmmo:
      into->AddRect(top_left, bottom_right, kAmmoFrame, rounding, 0, inset);
      break;
    case PresetPieceKind::kEquipment:
      into->AddRect(top_left, bottom_right, kEquipSlotBorder, rounding);
      break;
  }
  if (piece.icon.usable()) {
    into->AddImage(piece.icon.id, ImVec2(top_left.x + inset, top_left.y + inset),
                   ImVec2(bottom_right.x - inset, bottom_right.y - inset));
  }
  if (piece.refine_level > 0) {
    std::array<char, 16> badge = {};
    std::snprintf(badge.data(), badge.size(), "+%d", piece.refine_level);
    const ImVec2 size = ImGui::CalcTextSize(badge.data());
    AddTextHalo(
        into,
        ImVec2(bottom_right.x - size.x - inset, bottom_right.y - size.y - kBadgeLiftPixels * scale),
        IM_COL32_BLACK, badge.data(), IM_COL32_WHITE);
  }
  if (!ImGui::IsItemHovered()) {
    return;
  }
  std::string tip(piece.name);
  const char* family = piece.kind == PresetPieceKind::kCostume ? core::Tr("costume")
                       : piece.kind == PresetPieceKind::kAmmo  ? core::Tr("munition")
                                                               : nullptr;
  if (family != nullptr) {
    tip += "\n(";
    tip += family;
    tip += ')';
  }
  SkinTooltip(tip.c_str());
}

void DrawGlobalButtons(const PresetTabView& view, const SkinArt& art, PresetAction& action) {
  const float gap = kButtonGapPixels * Scale();
  if (SkinButton(core::Tr("Tout déséquiper"), art)) {
    action.undress = false;
  }
  SkinTooltip(core::Tr("Retire l'équipement porté (garde les costumes)"));
  ImGui::SameLine(0.0F, gap);
  if (SkinButton(core::Tr("+ costumes"), art)) {
    action.undress = true;
  }
  SkinTooltip(core::Tr("Retire aussi les costumes (têtes + cape)"));
  ImGui::SameLine(0.0F, gap);
  if (SkinButton(core::Tr("Mon style"), art)) {
    action.toggle_style = true;
  }
  // L'infobulle rappelle la touche de l'éditeur, qui se change.
  if (ImGui::IsItemHovered()) {
    std::string tip = core::Tr("Couleurs du corps, coiffure et couleur de cheveux");
    if (!view.style_hotkey.empty()) {
      tip += " (";
      tip += view.style_hotkey;
      tip += ')';
    }
    SkinTooltip(tip.c_str());
  }
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();
}

// La touche d'un preset — son libellé et ses boutons —, ou l'attente d'une
// nouvelle touche, que `app` lit au clavier.
void DrawHotkey(std::size_t rank, const PresetRowView& row, PresetTabState& state,
                const SkinArt& art, PresetAction& action) {
  const float scale = Scale();
  ImGui::AlignTextToFramePadding();
  ColoredText(kLabelColor, core::Tr("Raccourci :"));
  ImGui::SameLine();
  if (state.capturing == static_cast<int>(rank)) {
    ColoredText(kNameColor, core::Tr("appuie sur une touche…  (Échap : annuler)"));
    // Défilée hors de vue, l'attente s'annule : elle avalerait le clavier sans
    // que rien ne le montre.
    if (!ImGui::IsItemVisible()) {
      state.capturing = -1;
      state.refusal[0] = '\0';
    } else if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
      state.capturing = -1;
      state.refusal[0] = '\0';
      action.escape_spent = true;
    } else if (state.refusal[0] != '\0') {
      ColoredText(kRefusalColor, state.refusal.data());
    }
    return;
  }
  ColoredText(kNameColor, row.hotkey.empty() ? std::string_view(core::Tr("(aucun)")) : row.hotkey);
  ImGui::SameLine(0.0F, kHotkeyGapPixels * scale);
  if (SkinButton(core::Tr("Définir"), art)) {
    state.capturing = static_cast<int>(rank);
    state.refusal[0] = '\0';
  }
  if (!row.hotkey.empty()) {
    ImGui::SameLine(0.0F, kButtonGapPixels * scale);
    if (SkinButton(core::Tr("Effacer"), art)) {
      action.clear_hotkey = rank;
    }
  }
  if (!row.taken_by.empty()) {
    std::array<char, kEquipPresetNoteBytes> taken = {};
    std::snprintf(taken.data(), taken.size(),
                  core::Tr("Déjà utilisé par %s — choisis un autre combo"),
                  std::string(row.taken_by).c_str());
    ColoredText(kRefusalColor, taken.data());
  }
}

void DrawRow(std::size_t rank, const PresetRowView& row, PresetTabState& state, const SkinArt& art,
             PresetAction& action) {
  const float scale = Scale();
  ImGui::PushID(static_cast<int>(rank));
  ImGui::AlignTextToFramePadding();
  ColoredText(kNameColor, row.name);
  ImGui::SameLine();
  // « Charger » et « Suppr » au bord droit de la ligne.
  const char* load_label = core::Tr("Charger");
  const char* erase_label = core::Tr("Suppr");
  const float load_width = SkinButtonWidth(load_label, art);
  const float erase_width = SkinButtonWidth(erase_label, art);
  const float room = ImGui::GetContentRegionAvail().x;
  ImGui::SetCursorPosX(
      ImGui::GetCursorPosX() +
      std::max(0.0F, room - load_width - erase_width - (kButtonGapPixels + kRowEndPixels) * scale));
  if (SkinButton(load_label, art, ImVec2(load_width, 0.0F))) {
    action.load = rank;
  }
  SkinTooltip(core::Tr("Rééquipe exactement ce jeu (déséquipe le reste)"));
  ImGui::SameLine(0.0F, kButtonGapPixels * scale);
  if (SkinButton(erase_label, art, ImVec2(erase_width, 0.0F))) {
    action.remove = rank;
  }

  if (row.pieces.empty()) {
    ColoredText(kLabelColor, core::Tr("(vide)"));
  } else {
    const float side = kPieceCellPixels * scale;
    const float gap = kPieceGapPixels * scale;
    const auto per_line = std::max<std::size_t>(
        1, static_cast<std::size_t>((ImGui::GetContentRegionAvail().x + gap) / (side + gap)));
    for (std::size_t piece = 0; piece < row.pieces.size(); ++piece) {
      if (piece % per_line != 0) {
        ImGui::SameLine(0.0F, gap);
      }
      DrawPiece(row.pieces[piece], side, art.look());
    }
  }

  bool ticked = row.with_costumes;
  if (SkinCheckBox(core::Tr("Piloter aussi les costumes"), &ticked, art)) {
    action.costumes = rank;
    action.with_costumes = ticked;
  }
  SkinTooltip(core::Tr(
      "Coché, le preset rétablit exactement l'apparence enregistrée : il remet ses costumes et "
      "retire ceux qui n'en font pas partie.\n\nDécoché (défaut), il ne touche pas aux costumes "
      "portés.\n\nLes costumes portés au moment de l'enregistrement sont gardés dans le preset "
      "quoi qu'il arrive : cocher la case plus tard suffit, sans le ré-enregistrer."));

  DrawHotkey(rank, row, state, art, action);
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::PopID();
}

// Le pied : le nom qu'on tape, le bouton qui enregistre, le compte rendu.
void DrawSaveRow(PresetTabState& state, const PresetTabView& view, const SkinArt& art,
                 PresetAction& action) {
  const float scale = Scale();
  std::array<char, kEquipPresetNoteBytes> caption = {};
  std::snprintf(caption.data(), caption.size(), core::Tr("Enregistrer l'équipement porté (%d/%d)"),
                static_cast<int>(view.presets.size()), static_cast<int>(view.capacity));
  ColoredText(kNameColor, caption.data());
  const char* save_label = core::Tr("Sauver l'actuel");
  const float save_width = SkinButtonWidth(save_label, art);
  ImGui::SetNextItemWidth(
      std::max(kNameFieldMinimumPixels * scale,
               ImGui::GetContentRegionAvail().x - save_width - kNameFieldGapPixels * scale));
  ImGui::InputTextWithHint("##nom_preset", core::Tr("nom du preset"), state.name.data(),
                           state.name.size());
  ImGui::SameLine(0.0F, kButtonGapPixels * scale);
  // Au plafond, un nom déjà pris s'enregistre encore : il remplace son preset.
  const std::string_view typed(state.name.data());
  const bool known = std::ranges::any_of(
      view.presets, [typed](const PresetRowView& row) { return row.name == typed; });
  const bool full = view.presets.size() >= view.capacity && !known;
  ImGui::BeginDisabled(typed.empty() || full);
  if (SkinButton(save_label, art, ImVec2(save_width, 0.0F))) {
    action.save_as = std::string(typed);
    state.name[0] = '\0';
  }
  // Sans `AllowWhenDisabled`, un bouton grisé n'est jamais survolé.
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    SkinTooltipFormat(
        "%s", full
                  ? core::Tr("Limite de 5 presets atteinte (renomme un existant ou supprime-en un)")
                  : core::Tr("Enregistre l'équipement porté actuellement sous ce nom"));
  }
  ImGui::EndDisabled();
  if (state.status[0] != '\0') {
    ColoredText(kLabelColor, state.status.data());
  }
}

}  // namespace

PresetAction DrawPresetTab(PresetTabState& state, const PresetTabView& view, const SkinArt& art) {
  PresetAction action;
  DrawGlobalButtons(view, art, action);
  if (view.presets.empty()) {
    ColoredText(kLabelColor, core::Tr("Aucun preset enregistré pour ce personnage."));
    ImGui::Spacing();
  }
  for (std::size_t rank = 0; rank < view.presets.size(); ++rank) {
    DrawRow(rank, view.presets[rank], state, art, action);
  }
  if (state.capturing >= static_cast<int>(view.presets.size())) {
    state.capturing = -1;
  }
  DrawSaveRow(state, view, art, action);
  return action;
}

}  // namespace ro::ui
