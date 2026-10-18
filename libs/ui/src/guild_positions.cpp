// LE SOUS-ONGLET « POSTES » : nom, droits et part d'expérience des vingt postes.
//
// Le maître édite tout d'un coup et envoie le lot ; les autres voient la grille
// en lecture seule. Tant qu'une saisie est en cours, la copie éditée ne suit
// plus les paquets reçus — sinon la frappe serait écrasée par le
// rafraîchissement suivant.

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "ro/core/i18n.h"
#include "ro/ui/skin_widgets.h"

#include "guild_panes.h"
#include "ui_scale.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

constexpr int kPositionColumnCount = 6;
constexpr float kNumberColumnPixels = 22.0F;
constexpr float kInviteColumnPixels = 54.0F;
constexpr float kExpelColumnPixels = 60.0F;
constexpr float kStorageColumnPixels = 60.0F;
constexpr float kPayColumnPixels = 96.0F;
constexpr float kRowsLeastPixels = 80.0F;
constexpr float kButtonsSlackPixels = 4.0F;

[[nodiscard]] bool SameAsLive(const GuildPositionDraft& draft, const GuildPositionView& live) {
  return draft.may_invite == live.may_invite && draft.may_expel == live.may_expel &&
         draft.may_storage == live.may_storage && draft.pay_rate == live.pay_rate &&
         BufferText(draft.name) == live.name;
}

void Resync(GuildTabState& state, const GuildView& view) {
  for (std::size_t index = 0; index < state.positions.size(); ++index) {
    GuildPositionDraft& draft = state.positions[index];
    if (index >= view.positions.size()) {
      draft = GuildPositionDraft();
      continue;
    }
    const GuildPositionView& live = view.positions[index];
    CopyIntoBuffer(draft.name, live.name);
    draft.may_invite = live.may_invite;
    draft.may_expel = live.may_expel;
    draft.may_storage = live.may_storage;
    draft.pay_rate = live.pay_rate;
  }
}

// Une case de droit : cochable pour le maître, « oui » ou « - » sinon.
void DrawRight(bool editable, bool& right, const SkinArt& art, bool& edited) {
  if (editable) {
    if (SkinCheckBox("##droit", &right, art)) {
      edited = true;
    }
    return;
  }
  ColoredText(kGuildBlackColor, right ? core::Tr("oui") : "-");
}

}  // namespace

void DrawGuildPositions(GuildTabState& state, const GuildView& view, const SkinArt& art,
                        GuildAction& action) {
  if (!state.positions_editing) {
    Resync(state, view);
  }
  bool any = false;
  for (const GuildPositionView& position : view.positions) {
    any = any || position.has_name || position.has_rights;
  }
  if (!any) {
    ColoredText(kGuildLabelColor, core::Tr("Postes non encore reçus — clic sur « Actualiser »."));
    return;
  }
  std::array<char, 160> line = {};
  if (view.master) {
    std::snprintf(line.data(), line.size(),
                  core::Tr("Nom, droits et part d'exp de chaque poste. Le serveur plafonne la "
                           "part d'exp à %d %%."),
                  kGuildTabPayRateMost);
    ColoredText(kGuildLabelColor, line.data());
  } else {
    ColoredText(kGuildLabelColor, core::Tr("Seul le maître de guilde peut modifier les postes."));
  }

  const auto flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
                     ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY;
  // La place des boutons du bas, pour le maître seul.
  const float reserved = view.master ? SkinFooterHeight(art) + Px(kButtonsSlackPixels) : 0.0F;
  const float rows_height =
      std::max(Px(kRowsLeastPixels), ImGui::GetContentRegionAvail().y - reserved);
  if (ImGui::BeginTable("postes", kPositionColumnCount, flags, ImVec2(0.0F, rows_height))) {
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, Px(kNumberColumnPixels));
    ImGui::TableSetupColumn(core::Tr("Nom"), ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn(core::Tr("Inviter"), ImGuiTableColumnFlags_WidthFixed,
                            Px(kInviteColumnPixels));
    ImGui::TableSetupColumn(core::Tr("Expulser"), ImGuiTableColumnFlags_WidthFixed,
                            Px(kExpelColumnPixels));
    ImGui::TableSetupColumn(core::Tr("Storage"), ImGuiTableColumnFlags_WidthFixed,
                            Px(kStorageColumnPixels));
    ImGui::TableSetupColumn(core::Tr("Part exp"), ImGuiTableColumnFlags_WidthFixed,
                            Px(kPayColumnPixels));
    ImGui::TableHeadersRow();
    for (std::size_t index = 0; index < state.positions.size() && index < view.positions.size();
         ++index) {
      GuildPositionDraft& draft = state.positions[index];
      // ⚠ ÉDITABLE SEULEMENT SI SES DROITS SONT CONNUS : voir la vue.
      const bool editable = view.master && view.positions[index].has_rights;
      bool edited = false;
      ImGui::PushID(static_cast<int>(index));
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ColoredText(kGuildBlackColor, std::to_string(index).c_str());
      ImGui::TableSetColumnIndex(1);
      if (editable) {
        ImGui::SetNextItemWidth(-1.0F);
        edited = ImGui::InputText("##nom", draft.name.data(), draft.name.size());
      } else {
        const std::string_view name = BufferText(draft.name);
        ColoredText(kGuildBlackColor, name.empty() ? "—" : std::string(name).c_str());
      }
      const std::array<bool*, 3> rights = {&draft.may_invite, &draft.may_expel, &draft.may_storage};
      for (std::size_t right = 0; right < rights.size(); ++right) {
        ImGui::TableSetColumnIndex(static_cast<int>(2 + right));
        ImGui::PushID(static_cast<int>(right));
        DrawRight(editable, *rights[right], art, edited);
        ImGui::PopID();
      }
      ImGui::TableSetColumnIndex(5);
      if (editable) {
        ImGui::SetNextItemWidth(-1.0F);
        edited = SkinSliderInt("##part", &draft.pay_rate, 0, kGuildTabPayRateMost, art, "%d %%") ||
                 edited;
      } else {
        std::snprintf(line.data(), line.size(), "%d %%", draft.pay_rate);
        ColoredText(kGuildBlackColor, line.data());
      }
      state.positions_editing = state.positions_editing || edited;
      ImGui::PopID();
    }
    PaintWindowScrollbar(art);
    ImGui::EndTable();
  }
  if (!view.master) {
    return;
  }

  // L'ENVOI : seulement les lignes qui DIFFÈRENT de l'état serveur, et jamais
  // une ligne dont les droits sont inconnus.
  std::vector<GuildPositionEditView> changed;
  for (std::size_t index = 0; index < state.positions.size() && index < view.positions.size();
       ++index) {
    const GuildPositionView& live = view.positions[index];
    const GuildPositionDraft& draft = state.positions[index];
    if (!live.has_rights || SameAsLive(draft, live)) {
      continue;
    }
    changed.push_back(GuildPositionEditView{
        .position = static_cast<std::int32_t>(index),
        .name = std::string(BufferText(draft.name)),
        .may_invite = draft.may_invite,
        .may_expel = draft.may_expel,
        .may_storage = draft.may_storage,
        .pay_rate = std::clamp(draft.pay_rate, 0, kGuildTabPayRateMost),
    });
  }
  ImGui::BeginDisabled(changed.empty());
  if (SkinButton(core::Tr("Enregistrer les postes"), art)) {
    std::snprintf(line.data(), line.size(), core::Tr("%d poste(s) envoyé(s)."),
                  static_cast<int>(changed.size()));
    state.status = line.data();
    action.verb = GuildVerb::kSavePositions;
    action.positions = std::move(changed);
    state.positions_editing = false;
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(!state.positions_editing);
  if (SkinButton(core::Tr("Annuler les modifications"), art)) {
    state.positions_editing = false;
  }
  ImGui::EndDisabled();
}

}  // namespace ro::ui
