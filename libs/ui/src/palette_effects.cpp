#include "ro/ui/palette_effects.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_widgets.h"

namespace ro::ui {
namespace {

// La largeur des textes d'aide, en tailles de police.
constexpr float kWrapInFonts = 24.0F;

// Les tours de teinte proposés, en degrés : de quoi passer d'une couleur à
// ses voisines et à son opposée sans chercher au curseur.
constexpr std::array<int, 5> kProposedTurns = {-120, -60, 60, 120, 180};

void Wrapped(const char* text) {
  ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetFontSize() * kWrapInFonts);
  ImGui::TextUnformatted(text);
  ImGui::PopTextWrapPos();
}

void Explain(const char* text) {
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    SkinTooltip(text);
  }
}

// Le libellé d'une ligne : l'objet qui pose l'effet, sinon l'effet.
std::string RowLabel(const PaletteEffectRow& row, const PaletteEffectSetting& shown) {
  std::string label = row.item_name.empty() ? row.name : row.item_name;
  if (!row.worn.neutral() || !shown.neutral()) {
    label += " •";
  }
  if (shown != row.worn) {
    label += " *";
  }
  label += "###effet";
  label += std::to_string(row.number);
  return label;
}

// La liste des effets affichés ; un clic en choisit un.
void DrawRows(PaletteEffectsState& state, const PaletteEffectsView& view,
              const PaletteEffectRow& chosen) {
  for (const PaletteEffectRow& row : view.rows) {
    const std::string label = RowLabel(row, EffectDraftOf(state, row));
    ImGui::BeginDisabled(row.blocked != PaletteEffectBlock::kNone);
    if (ImGui::Selectable(label.c_str(), row.number == chosen.number)) {
      state.selected = row.number;
    }
    ImGui::EndDisabled();
    if (row.blocked != PaletteEffectBlock::kNone) {
      Explain(EffectBlockReason(row.blocked));
    } else if (!row.item_name.empty()) {
      Explain(row.name.c_str());
    }
  }
}

// Les trois curseurs, « Assombrir » et les tours proposés. Rend le réglage.
PaletteEffectSetting DrawSetting(const PaletteEffectRow& row, PaletteEffectSetting setting,
                                 const SkinArt& art) {
  static_cast<void>(SkinSliderInt(core::Tr("Teinte"), &setting.hue, -kPaletteEffectHueMost,
                                  kPaletteEffectHueMost, art, "%d°"));
  Explain(core::Tr("Fait tourner toutes les couleurs de l'effet, en degrés."));
  static_cast<void>(SkinSliderInt(core::Tr("Saturation"), &setting.saturation,
                                  -kPaletteEffectPercentMost, kPaletteEffectPercentMost, art,
                                  "%d%%"));
  Explain(core::Tr("Moins, l'effet tourne au gris ; plus, ses couleurs se font vives."));
  // 🔴 UNE LUEUR NE FONCE PAS, ELLE PÂLIT : sous un mélange additif, le
  // curseur ne règle que son intensité, et le dire évite un curseur qui
  // semble cassé.
  const bool intensity = row.additive && !setting.darken;
  static_cast<void>(SkinSliderInt(intensity ? core::Tr("Intensité") : core::Tr("Clarté"),
                                  &setting.lightness, -kPaletteEffectPercentMost,
                                  kPaletteEffectPercentMost, art, "%d%%"));
  Explain(intensity ? core::Tr("Une lueur s'ajoute à ce qui est derrière elle : moins, elle "
                               "pâlit ; elle ne fonce pas.")
                    : core::Tr("Moins, l'effet fonce ; plus, il s'éclaircit."));
  if (row.additive) {
    static_cast<void>(SkinCheckBox(core::Tr("Assombrir"), &setting.darken, art));
    Explain(
        core::Tr("Une aura lumineuse ne fonce pas, elle pâlit. Assombrir la peint comme une "
                 "fumée, qui ne brille plus : elle peut alors foncer."));
  }
  ImGui::TextUnformatted(core::Tr("Proposés :"));
  for (const int turn : kProposedTurns) {
    ImGui::SameLine();
    char label[32] = {};
    std::snprintf(label, sizeof(label), "%+d°###tour%d", turn, turn);
    if (SkinSmallButton(label, art)) {
      setting.hue = turn;
    }
  }
  ImGui::SameLine();
  if (SkinSmallButton(core::Tr("Gris"), art)) {
    setting.saturation = -kPaletteEffectPercentMost;
  }
  return ClampedEffectSetting(setting, row.additive);
}

// « Valider », « D'origine », « Retirer la teinture », et ce qui les retient.
void DrawSendButtons(PaletteEffectsState& state, const PaletteEffectsView& view,
                     const PaletteEffectRow& row, const PaletteEffectSetting& setting,
                     const SkinArt& art, PaletteEffectsAction& action) {
  const bool changed = setting != row.worn;
  if (SkinButton(core::Tr("D'origine"), art)) {
    state.drafts[row.number] = PaletteEffectSetting{};
  }
  Explain(core::Tr("Rend à l'effet ses couleurs d'origine, sans rien envoyer."));
  ImGui::SameLine();
  ImGui::BeginDisabled(!changed);
  if (SkinButton(core::Tr("Revenir à ce que je porte"), art)) {
    state.drafts.erase(row.number);
  }
  ImGui::EndDisabled();
  const bool ready = view.connected && view.served && view.atelier_open && view.can_send && changed;
  ImGui::BeginDisabled(!ready);
  if (SkinButton(core::Tr("Valider"), art)) {
    action.validate = row.number;
  }
  ImGui::EndDisabled();
  if (!view.atelier_open) {
    ImGui::TextDisabled("%s", core::Tr("Une teinture d'effet se valide auprès du styliste."));
  } else if (view.send == PaletteEffectSend::kSent) {
    ImGui::TextDisabled("%s", core::Tr("Envoyé : en attente du serveur."));
  } else if (view.send == PaletteEffectSend::kShared && !changed) {
    ImGui::TextDisabled("%s", core::Tr("Partagé : les autres le voient."));
  }
  if (row.worn.neutral()) {
    return;
  }
  ImGui::BeginDisabled(!view.connected || !view.served || !view.atelier_open);
  if (SkinSmallButton(core::Tr("Retirer la teinture de cet effet"), art)) {
    ImGui::OpenPopup("retirer_effet");
  }
  ImGui::EndDisabled();
  Explain(core::Tr("Il retrouve ses couleurs d'origine, pour toi comme pour les autres."));
  if (BeginSkinPopup("retirer_effet")) {
    Wrapped(core::Tr("Retirer la teinture de cet effet ?"));
    if (SkinButton(core::Tr("Retirer"), art)) {
      action.clear = row.number;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (SkinButton(core::Tr("Annuler"), art)) {
      ImGui::CloseCurrentPopup();
    }
    EndSkinPopup();
  }
}

}  // namespace

PaletteEffectSetting EffectDraftOf(const PaletteEffectsState& state, const PaletteEffectRow& row) {
  const auto found = state.drafts.find(row.number);
  return found != state.drafts.end() ? found->second : row.worn;
}

const PaletteEffectRow* SelectedEffect(const PaletteEffectsState& state,
                                       const PaletteEffectsView& view) {
  const PaletteEffectRow* first_dyeable = nullptr;
  for (const PaletteEffectRow& row : view.rows) {
    if (row.blocked != PaletteEffectBlock::kNone) {
      continue;
    }
    if (row.number == state.selected) {
      return &row;
    }
    if (first_dyeable == nullptr) {
      first_dyeable = &row;
    }
  }
  return first_dyeable;
}

PaletteEffectSetting ClampedEffectSetting(PaletteEffectSetting setting, bool additive) {
  setting.hue = std::clamp(setting.hue, -kPaletteEffectHueMost, kPaletteEffectHueMost);
  setting.saturation =
      std::clamp(setting.saturation, -kPaletteEffectPercentMost, kPaletteEffectPercentMost);
  setting.lightness =
      std::clamp(setting.lightness, -kPaletteEffectPercentMost, kPaletteEffectPercentMost);
  setting.darken = setting.darken && additive;
  return setting;
}

const char* EffectBlockReason(PaletteEffectBlock blocked) {
  switch (blocked) {
    case PaletteEffectBlock::kNone:
      return "";
    case PaletteEffectBlock::kChangesBody:
      return core::Tr("Cet effet change ton corps, pas une lumière : il n'a rien à teindre.");
    case PaletteEffectBlock::kVoice:
      return core::Tr("Le micro de la voix ne se teint pas.");
  }
  return "";
}

PaletteEffectsAction DrawPaletteEffects(PaletteEffectsState& state, const PaletteEffectsView& view,
                                        const SkinArt& art) {
  PaletteEffectsAction action;
  if (!view.served) {
    Wrapped(core::Tr("Le serveur ne range pas encore la teinture des effets de chapeau."));
  }
  const PaletteEffectRow* chosen = SelectedEffect(state, view);
  if (view.rows.empty()) {
    ImGui::TextDisabled("%s", core::Tr("Tu n'affiches aucun effet de chapeau."));
    return action;
  }
  if (chosen == nullptr) {
    // Seuls des effets qui ne se teignent pas : la liste dit pourquoi.
    const PaletteEffectRow none{};
    DrawRows(state, view, none);
    return action;
  }
  DrawRows(state, view, *chosen);
  ImGui::Separator();
  const PaletteEffectSetting before = EffectDraftOf(state, *chosen);
  const PaletteEffectSetting after = DrawSetting(*chosen, before, art);
  if (after != before) {
    state.drafts[chosen->number] = after;
  }
  if (!chosen->item_name.empty()) {
    Wrapped(
        core::Tr("Cette teinture appartient à l'item qui pose l'effet : elle le suit à "
                 "l'échange, à la vente, au storage et au courrier."));
  }
  ImGui::Separator();
  DrawSendButtons(state, view, *chosen, EffectDraftOf(state, *chosen), art, action);
  return action;
}

}  // namespace ro::ui
