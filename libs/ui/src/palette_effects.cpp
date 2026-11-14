#include "ro/ui/palette_effects.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/render/effect_dye.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_widgets.h"

#include "palette_pieces.h"

namespace ro::ui {
namespace {

// La largeur des textes d'aide, en tailles de police.
constexpr float kWrapInFonts = 24.0F;

// Le côté de la pastille de l'effet, en tailles de police.
constexpr float kSwatchInFonts = 2.0F;
constexpr float kChannelMax = 255.0F;
constexpr float kHalfTurnDegrees = 180.0F;
constexpr float kFullTurnDegrees = 360.0F;
constexpr float kPercent = 100.0F;

std::array<float, 3> Unit(std::array<std::uint8_t, 3> rgb) {
  return {static_cast<float>(rgb[0]) / kChannelMax, static_cast<float>(rgb[1]) / kChannelMax,
          static_cast<float>(rgb[2]) / kChannelMax};
}

std::uint8_t ToByte(float unit) {
  return static_cast<std::uint8_t>(std::lround(std::clamp(unit, 0.0F, 1.0F) * kChannelMax));
}

// Teinte en degrés, saturation et clarté en unités, en TSL : la même que la
// teinture du dessin.
std::array<float, 3> Hsl(std::array<std::uint8_t, 3> rgb) {
  const std::array<float, 3> unit = Unit(rgb);
  const float high = std::max({unit[0], unit[1], unit[2]});
  const float low = std::min({unit[0], unit[1], unit[2]});
  const float lightness = (high + low) * 0.5F;
  const float spread = high - low;
  if (spread <= 0.0F) {
    return {0.0F, 0.0F, lightness};
  }
  const float saturation = lightness > 0.5F ? spread / (2.0F - high - low) : spread / (high + low);
  float sextant = 0.0F;
  if (high == unit[0]) {
    sextant = std::fmod((unit[1] - unit[2]) / spread + 6.0F, 6.0F);
  } else if (high == unit[1]) {
    sextant = (unit[2] - unit[0]) / spread + 2.0F;
  } else {
    sextant = (unit[0] - unit[1]) / spread + 4.0F;
  }
  return {sextant * (kFullTurnDegrees / 6.0F), saturation, lightness};
}

// Le facteur relatif, en pour-cent, qui mène `from` à `to`.
int RelativePercent(float from, float to) {
  if (from <= 0.0F) {
    return 0;
  }
  return static_cast<int>(std::lround((to / from - 1.0F) * kPercent));
}

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

// « RÉGLAGES AVANCÉS », repliés par défaut : les trois curseurs et
// « Assombrir ». Rend le réglage.
PaletteEffectSetting DrawAdvanced(PaletteEffectsState& state, const PaletteEffectRow& row,
                                  PaletteEffectSetting setting, const SkinArt& art) {
  // 🔴 REPLIÉ PAR DÉFAUT ET TOUJOURS REPLIABLE, couleur connue ou non : c'est
  // le joueur qui l'ouvre, et sa main seule le referme.
  ImGui::SetNextItemOpen(state.advanced);
  const std::string label = std::string(core::Tr("Réglages avancés")) + "###reglages_effet";
  const bool open = ImGui::TreeNode(label.c_str());
  state.advanced = open;
  if (!open) {
    return setting;
  }
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
  ImGui::TreePop();
  return ClampedEffectSetting(setting, row.additive);
}

// LA PASTILLE DE L'EFFET : la couleur que le réglage donne, et le nuancier
// qu'elle ouvre, réglages avancés compris. Rend le réglage.
//
// 🔴 LES RÉGLAGES AVANCÉS VIVENT DANS LE NUANCIER : ses curseurs et les leurs
// règlent la même teinture, l'une en couleur visée, l'autre en écarts. Deux
// panneaux côte à côte se liraient comme deux réglages. Sans couleur lue, il
// n'y a pas de pastille, et ils restent seuls dans le panneau.
PaletteEffectSetting DrawSwatch(PaletteEffectsState& state, const PaletteEffectRow& row,
                                PaletteEffectSetting setting, const SkinArt& art) {
  if (!row.base_color.has_value()) {
    ImGui::TextDisabled("%s", row.measuring_color
                                  ? core::Tr("Mesure de la couleur de l'effet…")
                                  : core::Tr("Cet effet ne laisse pas lire sa couleur : règle-le "
                                             "dans les réglages avancés."));
    return DrawAdvanced(state, row, setting, art);
  }
  const std::array<float, 3> shown = Unit(EffectColorOf(*row.base_color, setting));
  const float side = ImGui::GetFontSize() * kSwatchInFonts;
  // ⚠ SANS LE GLISSER NI L'INFOBULLE D'IMGUI, en anglais : on pose la nôtre.
  if (ImGui::ColorButton("##couleur_effet", ImVec4(shown[0], shown[1], shown[2], 1.0F),
                         ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoDragDrop |
                             ImGuiColorEditFlags_NoTooltip,
                         ImVec2(side, side))) {
    ImGui::OpenPopup("nuancier_effet");
  }
  Explain(
      core::Tr("Choisis la couleur de l'effet : la teinte, la saturation et la clarté "
               "suivent."));
  ImGui::SameLine();
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(core::Tr("Couleur de l'effet"));
  if (BeginSkinPopup("nuancier_effet")) {
    std::array<float, 3> picked = shown;
    if (DrawPaletteColorPicker(picked, art)) {
      setting = EffectSettingToward(*row.base_color,
                                    {ToByte(picked[0]), ToByte(picked[1]), ToByte(picked[2])},
                                    setting, row.additive);
    }
    setting = DrawAdvanced(state, row, setting, art);
    EndSkinPopup();
  }
  return setting;
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
  ImGui::SameLine();
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

std::array<std::uint8_t, 3> EffectColorOf(std::array<std::uint8_t, 3> base,
                                          const PaletteEffectSetting& setting) {
  // ⚠ SANS « ASSOMBRIR » : la pastille montre la couleur, et non le mélange qui
  // la pose.
  const render::EffectDye dye =
      render::MakeEffectDye(setting.hue, setting.saturation, setting.lightness, false);
  const std::array<std::uint8_t, 4> dyed = render::DyeEffectTexel(
      {base[0], base[1], base[2], static_cast<std::uint8_t>(kChannelMax)}, dye);
  return {dyed[0], dyed[1], dyed[2]};
}

PaletteEffectSetting EffectSettingToward(std::array<std::uint8_t, 3> base,
                                         std::array<std::uint8_t, 3> wanted,
                                         const PaletteEffectSetting& current, bool additive) {
  const std::array<float, 3> from = Hsl(base);
  const std::array<float, 3> to = Hsl(wanted);
  PaletteEffectSetting setting = current;
  if (from[1] > 0.0F && to[1] > 0.0F) {
    float turn = std::fmod(to[0] - from[0] + kFullTurnDegrees, kFullTurnDegrees);
    if (turn > kHalfTurnDegrees) {
      turn -= kFullTurnDegrees;
    }
    setting.hue = static_cast<int>(std::lround(turn));
  }
  setting.saturation =
      from[1] > 0.0F ? RelativePercent(from[1], to[1]) : (to[1] > 0.0F ? 0 : current.saturation);
  setting.lightness = RelativePercent(from[2], to[2]);
  return ClampedEffectSetting(setting, additive);
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
  const PaletteEffectSetting after = DrawSwatch(state, *chosen, before, art);
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
