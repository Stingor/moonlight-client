#include "palette_outfits.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <string_view>

#include "ro/core/i18n.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_widgets.h"

namespace ro::ui {
namespace {

// La largeur du nom d'un rangement et celle du champ de code, en largeurs de
// police.
constexpr float kSlotNameInFonts = 9.0F;
constexpr float kCodeFieldInFonts = 7.0F;
constexpr float kHintWrapInFonts = 22.0F;

void Hint(const char* text) {
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    SkinTooltip(text);
  }
}

void Wrapped(const char* text) {
  ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetFontSize() * kHintWrapInFonts);
  ImGui::TextDisabled("%s", text);
  ImGui::PopTextWrapPos();
}

// ── LE CODE COURT ───────────────────────────────────────────────────────────

void DrawShortCode(PaletteEditorState& state, const PaletteEditorView& view, const SkinArt& art,
                   PaletteEditorAction& action) {
  ImGui::BeginDisabled(!view.outfits_can_send || !view.has_outfit);
  if (SkinButton(core::Tr("Publier ma tenue"), art)) {
    action.verb = PaletteEditorVerb::kPublishOutfit;
  }
  ImGui::EndDisabled();
  Hint(view.has_outfit
           ? core::Tr("Le serveur donne un code court à ta tenue validée : le corps et toutes "
                      "tes teintures portées. Qui le colle la voit dans son éditeur.")
           : core::Tr("Valide d'abord un style : seul ce que tu portes se publie."));
  if (view.outfit_code[0] != '\0') {
    ImGui::SameLine();
    ImGui::Text("%s %s", core::Tr("Code :"), view.outfit_code);
    ImGui::SameLine();
    if (SkinSmallButton(core::Tr("Copier"), art)) {
      ImGui::SetClipboardText(view.outfit_code);
    }
  }

  ImGui::SetNextItemWidth(ImGui::GetFontSize() * kCodeFieldInFonts);
  ImGui::InputTextWithHint("##code court", core::Tr("Code court"), state.outfit_code.data(),
                           state.outfit_code.size());
  ImGui::SameLine();
  const bool typed = state.outfit_code[0] != '\0';
  ImGui::BeginDisabled(!typed || !view.outfits_can_send);
  if (SkinButton(core::Tr("Voir cette tenue"), art)) {
    action.verb = PaletteEditorVerb::kFetchOutfit;
    action.pasted = state.outfit_code.data();
  }
  ImGui::EndDisabled();
  Hint(
      core::Tr("La tenue s'ouvre dans l'éditeur comme un aperçu. Rien ne change sur toi tant "
               "que tu ne valides pas."));
}

// ── LA GARDE-ROBE ───────────────────────────────────────────────────────────

// RENOMMER ET PARTAGER UN RANGEMENT PLEIN, sur sa ligne.
//
// 🔴 RENOMMER NE TOUCHE PAS À LA TENUE : c'est celle du rangement qui se range
// à nouveau, sous le nouveau nom — pas ce que le personnage porte.
void DrawSlotGestures(PaletteEditorState& state, const PaletteEditorView& view, const SkinArt& art,
                      PaletteEditorAction& action, int slot, const std::string& name) {
  ImGui::SameLine();
  ImGui::BeginDisabled(!view.outfits_can_send);
  if (SkinSmallButton(core::Tr("Renommer"), art)) {
    state.wardrobe_rename = {};
    std::snprintf(state.wardrobe_rename.data(), state.wardrobe_rename.size(), "%s", name.c_str());
    ImGui::OpenPopup("renommer");
  }
  ImGui::SameLine();
  if (SkinSmallButton(core::Tr("Partager"), art)) {
    action.verb = PaletteEditorVerb::kShareOutfit;
    action.outfit_slot = slot;
  }
  ImGui::EndDisabled();
  Hint(
      core::Tr("Pose un lien vers cette tenue dans ta saisie — à toi d'envoyer. Qui le reçoit "
               "l'essaie d'un clic, sans rien changer sur lui."));
  if (!BeginSkinPopup("renommer")) {
    return;
  }
  ImGui::SetNextItemWidth(ImGui::GetFontSize() * kSlotNameInFonts);
  if (ImGui::IsWindowAppearing()) {
    ImGui::SetKeyboardFocusHere();
  }
  const bool entered =
      ImGui::InputText("##nom", state.wardrobe_rename.data(), state.wardrobe_rename.size(),
                       ImGuiInputTextFlags_EnterReturnsTrue);
  const std::string typed = state.wardrobe_rename.data();
  ImGui::SameLine();
  ImGui::BeginDisabled(typed.empty() || typed == name);
  if ((SkinButton(core::Tr("Renommer"), art) || entered) && !typed.empty() && typed != name) {
    action.verb = PaletteEditorVerb::kRenameOutfit;
    action.outfit_slot = slot;
    action.preset = typed;
    ImGui::CloseCurrentPopup();
  }
  ImGui::EndDisabled();
  EndSkinPopup();
}

void DrawWardrobe(PaletteEditorState& state, const PaletteEditorView& view, const SkinArt& art,
                  PaletteEditorAction& action) {
  // UNE SEULE DEMANDE PAR OUVERTURE : la liste revient, et c'est `app` qui la
  // redemande après un rangement.
  if (!view.wardrobe_listed && !state.wardrobe_asked && view.outfits_can_send) {
    action.verb = PaletteEditorVerb::kListWardrobe;
    state.wardrobe_asked = true;
  }
  if (!view.wardrobe_listed) {
    ImGui::TextDisabled("%s", core::Tr("La garde-robe arrive…"));
    return;
  }
  // Le nom d'un rangement neuf : celui du champ des préréglages, sinon un nom
  // tiré du numéro.
  const std::string typed = state.preset_name.data();
  for (int slot = 0; slot < view.wardrobe_slots; ++slot) {
    const auto found = std::ranges::find(view.wardrobe, slot, &PaletteWardrobeSlot::slot);
    const bool filled = found != view.wardrobe.end();
    ImGui::PushID(slot);
    char fallback[32] = {};
    std::snprintf(fallback, sizeof(fallback), core::Tr("Rangement %d"), slot + 1);
    ImGui::AlignTextToFramePadding();
    if (filled) {
      ImGui::TextUnformatted(found->name.empty() ? fallback : found->name.c_str());
    } else {
      ImGui::TextDisabled("%s", core::Tr("(vide)"));
    }
    ImGui::SameLine(ImGui::GetFontSize() * kSlotNameInFonts);
    ImGui::BeginDisabled(!filled);
    if (SkinSmallButton(core::Tr("Reprendre"), art)) {
      action.verb = PaletteEditorVerb::kWearOutfit;
      action.outfit_slot = slot;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!view.outfits_can_send || !view.has_outfit);
    if (SkinSmallButton(core::Tr("Ranger ici"), art)) {
      action.verb = PaletteEditorVerb::kStoreOutfit;
      action.outfit_slot = slot;
      action.preset = typed.empty() ? std::string(fallback) : typed;
    }
    ImGui::EndDisabled();
    Hint(
        core::Tr("Range ta tenue validée sous le nom saisi dans les préréglages. Tous les "
                 "personnages du compte la retrouvent."));
    if (filled) {
      DrawSlotGestures(state, view, art, action, slot, found->name);
      ImGui::SameLine();
      ImGui::BeginDisabled(!view.outfits_can_send);
      if (SkinSmallButton("x", art)) {
        action.verb = PaletteEditorVerb::kClearOutfit;
        action.outfit_slot = slot;
      }
      ImGui::EndDisabled();
      Hint(core::Tr("Vider ce rangement"));
    }
    ImGui::PopID();
  }
}

}  // namespace

void DrawPaletteOutfits(PaletteEditorState& state, const PaletteEditorView& view,
                        const SkinArt& art, PaletteEditorAction& action) {
  ImGui::Separator();
  if (!ImGui::TreeNode(core::Tr("Tenues"))) {
    // Refermée, la section redemandera la liste à sa prochaine ouverture.
    state.wardrobe_asked = false;
    return;
  }
  if (!view.outfits_served) {
    Wrapped(core::Tr("Ce serveur ne garde pas les tenues."));
    ImGui::TreePop();
    return;
  }
  if (view.outfit_status != nullptr) {
    Wrapped(view.outfit_status);
  }
  DrawShortCode(state, view, art, action);
  ImGui::Separator();
  DrawWardrobe(state, view, art, action);
  Wrapped(
      core::Tr("Une tenue reprise ne fait qu'une proposition : valide pour la porter. Les "
               "teintures d'items ne s'appliquent qu'aux items de même apparence que tu "
               "portes."));
  ImGui::TreePop();
}

}  // namespace ro::ui
