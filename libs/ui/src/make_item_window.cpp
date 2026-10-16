#include "ro/ui/make_item_window.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cfloat>
#include <cstdio>
#include <string>
#include <vector>

#include <imgui.h>

#include "ro/ui/list_keys.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

#include "craft_window_parts.h"

namespace ro::ui {
namespace {

using craft_parts::BulletWrapped;
using craft_parts::ColoredText;
using craft_parts::ColoredWrapped;
using craft_parts::Px;

// ── LES MESURES, en pixels d'interface ──────────────────────────────────────
constexpr float kWindowPixels = 320.0F;
constexpr float kButtonPixels = 78.0F;
constexpr float kIconColumnPixels = 28.0F;
constexpr float kOwnedColumnPixels = 58.0F;
constexpr float kCraftableColumnPixels = 60.0F;
constexpr float kBatchFieldPixels = 56.0F;
constexpr float kMaterialIconPixels = 18.0F;
constexpr float kHistoryPixels = 90.0F;
// La liste montre de trois à douze lignes, selon ce qu'elle porte.
constexpr int kLeastShownRows = 3;
constexpr int kMostShownRows = 12;

constexpr int kIdColumn = 0;
constexpr int kNameColumn = 1;
constexpr int kOwnedColumn = 2;
constexpr int kCraftableColumn = 3;

constexpr std::size_t kLabelBytes = 192;

void DrawLine(const CraftTextLine& line) {
  const std::string text(line.text);
  if (line.wrapped) {
    ColoredWrapped(PaintingLook().Tone(line.tone), text.c_str());
  } else {
    ColoredText(PaintingLook().Tone(line.tone), text.c_str());
  }
  if (!line.suffix.empty()) {
    ImGui::SameLine();
    const std::string suffix(line.suffix);
    ColoredText(PaintingLook().Tone(line.suffix_tone), suffix.c_str());
  }
  if (line.help_title != nullptr) {
    ImGui::SameLine();
    SkinHelpMarker(line.help_title, line.help_lines);
  }
}

void DrawLines(std::span<const CraftTextLine> lines) {
  for (const CraftTextLine& line : lines) {
    DrawLine(line);
  }
}

[[nodiscard]] bool Shown(const MakeItemWindowState& state, const MakeItemRowView& row) {
  const std::string_view filter(state.filter.data());
  if (filter.empty()) {
    return true;
  }
  // LE NOM ET L'IDENTIFIANT : « Elemental Converter » se lit quatre fois, seul
  // le numéro les sépare.
  std::array<char, kLabelBytes> searched{};
  std::snprintf(searched.data(), searched.size(), "%.*s %u", static_cast<int>(row.name.size()),
                row.name.data(), static_cast<unsigned>(row.item_id));
  return craft_parts::ContainsFolded(searched.data(), filter);
}

[[nodiscard]] int CompareNames(std::string_view left, std::string_view right) {
  const std::size_t common = std::min(left.size(), right.size());
  for (std::size_t at = 0; at < common; ++at) {
    const int a = std::tolower(static_cast<unsigned char>(left[at]));
    const int b = std::tolower(static_cast<unsigned char>(right[at]));
    if (a != b) {
      return a - b;
    }
  }
  return static_cast<int>(left.size()) - static_cast<int>(right.size());
}

[[nodiscard]] std::vector<const MakeItemRowView*> SortedRows(const MakeItemWindowState& state,
                                                             const MakeItemView& view,
                                                             const ImGuiTableSortSpecs* specs) {
  std::vector<const MakeItemRowView*> rows;
  for (const MakeItemRowView& row : view.rows) {
    if (Shown(state, row)) {
      rows.push_back(&row);
    }
  }
  if (specs == nullptr || specs->SpecsCount == 0) {
    return rows;
  }
  const ImGuiTableColumnSortSpecs& spec = specs->Specs[0];
  const bool ascending = spec.SortDirection == ImGuiSortDirection_Ascending;
  std::ranges::sort(rows, [&](const MakeItemRowView* left, const MakeItemRowView* right) {
    int order = 0;
    switch (spec.ColumnIndex) {
      case kNameColumn:
        order = CompareNames(left->name, right->name);
        break;
      case kOwnedColumn:
        order = left->owned - right->owned;
        break;
      case kCraftableColumn:
        order = left->craftable - right->craftable;
        break;
      default:
        break;
    }
    if (order == 0) {
      order = left->item_id < right->item_id ? -1 : (left->item_id > right->item_id ? 1 : 0);
    }
    return ascending ? order < 0 : order > 0;
  });
  return rows;
}

void DrawEmpty(const MakeItemView& view) {
  if (!view.cant_make.empty()) {
    const std::string said(view.cant_make);
    ColoredWrapped(PaintingLook().warn_text, said.c_str());
  }
  ImGui::Spacing();
  ImGui::TextWrapped("%s", core::Tr("Le serveur n'a proposé aucun produit. Causes possibles :"));
  BulletWrapped(core::Tr("il te manque un matériau (ou la quantité) ;"));
  BulletWrapped(
      core::Tr("ton inventory est plein — le serveur refuse ce qu'il ne peut pas te "
               "remettre ;"));
  BulletWrapped(core::Tr("le niveau de skill requis n'est pas atteint."));
  ImGui::Spacing();
  if (view.stale_lines.empty()) {
    return;
  }
  ImGui::SeparatorText(core::Tr("Ce que tu pouvais faire juste avant"));
  for (const MakeStaleLineView& line : view.stale_lines) {
    const std::string text(line.text);
    ColoredWrapped(PaintingLook().faint_text, text.c_str());
  }
  if (view.stale_more > 0) {
    std::array<char, kLabelBytes> more{};
    std::snprintf(more.data(), more.size(), core::Tr("(+%d autres)"), view.stale_more);
    ColoredText(PaintingLook().faint_text, more.data());
  }
  ImGui::Spacing();
}

void DrawList(MakeItemWindowState& state, const MakeItemView& view, MakeItemWindowAction& action) {
  if (view.empty_list && view.rows.empty()) {
    DrawEmpty(view);
    return;
  }
  if (view.rows.empty()) {
    return;
  }
  if (view.show_filter) {
    // ⚠ JAMAIS DE FOCUS D'OFFICE : la fenêtre s'ouvre sur un geste de JEU, et
    // prendre le clavier confisquerait les touches de déplacement.
    ImGui::PushItemWidth(-1.0F);
    ImGui::InputTextWithHint("##makefilter", core::Tr("Filtrer…"), state.filter.data(),
                             state.filter.size());
    ImGui::PopItemWidth();
  } else {
    state.filter[0] = '\0';
  }

  const ImGuiStyle& style = ImGui::GetStyle();
  const float icon = Px(craft_parts::kRowIconPixels);
  const float row_height = icon + style.CellPadding.y * 2.0F;
  const int shown = std::clamp(static_cast<int>(view.rows.size()), kLeastShownRows, kMostShownRows);
  const float height = ImGui::GetFrameHeight() + static_cast<float>(shown) * row_height;
  const bool show_craftable = view.show_owned && !view.arrow_list;
  const int columns = 2 + (view.show_owned ? 1 : 0) + (show_craftable ? 1 : 0);
  const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                                ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable |
                                ImGuiTableFlags_SortTristate | ImGuiTableFlags_SizingStretchProp;
  if (!ImGui::BeginTable("##makelist", columns, flags, ImVec2(0.0F, height))) {
    return;
  }
  ImGui::TableSetupScrollFreeze(0, 1);
  ImGui::TableSetupColumn("##icone", ImGuiTableColumnFlags_WidthFixed, Px(kIconColumnPixels));
  ImGui::TableSetupColumn(core::Tr("Produit"),
                          ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthStretch);
  if (view.show_owned) {
    ImGui::TableSetupColumn(core::Tr("Possédé"), ImGuiTableColumnFlags_WidthFixed,
                            Px(kOwnedColumnPixels));
  }
  if (show_craftable) {
    ImGui::TableSetupColumn(core::Tr("Faisable"), ImGuiTableColumnFlags_WidthFixed,
                            Px(kCraftableColumnPixels));
  }
  ImGui::TableHeadersRow();
  ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs();
  if (specs != nullptr) {
    specs->SpecsDirty = false;
  }
  const std::vector<const MakeItemRowView*> rows = SortedRows(state, view, specs);
  action.selection_visible = std::ranges::any_of(
      rows, [&](const MakeItemRowView* row) { return row->item_id == state.selected; });

  for (const MakeItemRowView* row : rows) {
    ImGui::PushID(static_cast<int>(row->item_id));
    ImGui::TableNextRow(ImGuiTableRowFlags_None, row_height);
    ImGui::TableNextColumn();
    if (row->icon.usable()) {
      ImGui::Image(row->icon.id, ImVec2(icon, icon));
    } else {
      ImGui::Dummy(ImVec2(icon, icon));
    }
    ImGui::TableNextColumn();
    const std::string name(row->name);
    if (ImGui::Selectable(
            name.c_str(), state.selected == row->item_id,
            ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick,
            ImVec2(0.0F, icon))) {
      state.selected = row->item_id;
      action.selection_visible = true;
      if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        action.verb = MakeItemVerb::kMake;
      }
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
      action.hovered_id = row->item_id;
      if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        action.describe_item_id = row->item_id;
      }
    }
    if (view.show_owned) {
      ImGui::TableNextColumn();
      if (row->owned > 0) {
        ImGui::Text("%d", row->owned);
      } else {
        ColoredText(PaintingLook().faint_text, "0");
      }
    }
    if (show_craftable) {
      ImGui::TableNextColumn();
      if (row->craftable > 0) {
        ImGui::Text("%d", row->craftable);
      } else {
        ColoredText(PaintingLook().faint_text, "—");
      }
    }
    ImGui::PopID();
  }
  ImGui::EndTable();
}

void DrawRecipe(const MakeItemView& view, MakeItemWindowAction& action) {
  if (!view.recipe_shown) {
    return;
  }
  ImGui::Separator();
  if (view.recipe_unknown) {
    ColoredWrapped(PaintingLook().faint_text,
                   core::Tr("Recette inconnue du client pour ce produit."));
    return;
  }
  const std::string title(view.recipe_title);
  ImGui::SeparatorText(title.c_str());
  DrawLines(view.chance);
  for (std::size_t rank = 0; rank < view.materials.size(); ++rank) {
    const MakeMaterialView& material = view.materials[rank];
    const std::string label(material.label);
    const ImU32 color = material.enough ? PaintingLook().good_text : PaintingLook().bad_text;
    if (material.item_id == 0) {
      ColoredText(color, label.c_str());
      continue;
    }
    std::array<char, kLabelBytes> id{};
    std::snprintf(id.data(), id.size(), "mat%zu", rank);
    bool hovered = false;
    if (craft_parts::ItemLink(id.data(), material.icon, kMaterialIconPixels, label.c_str(), color,
                              &hovered)) {
      action.describe_item_id = material.item_id;
    }
    if (hovered) {
      action.hovered_id = material.item_id;
    }
  }
}

void DrawForgeSlots(const MakeItemView& view, const SkinArt& art, MakeItemWindowAction& action) {
  if (!view.forge_shown) {
    return;
  }
  ImGui::SeparatorText(core::Tr("Matériaux optionnels"));
  const float spacing = ImGui::GetStyle().ItemSpacing.x;
  const float width = (ImGui::GetContentRegionAvail().x - spacing * 2.0F) / kForgeSlotCount;
  for (std::size_t slot = 0; slot < view.forge_slots.size(); ++slot) {
    const ForgeSlotView& forge = view.forge_slots[slot];
    ImGui::PushID(static_cast<int>(slot));
    ImGui::SetNextItemWidth(width);
    const std::string preview =
        forge.chosen == 0 ? std::string(core::Tr("(vide)")) : std::string(forge.chosen_label);
    int step = 0;
    const bool open = BeginSkinCombo("##forgeslot", preview.c_str(), art, &step);
    // Le rang zéro est la case vide, les matériaux suivent.
    const auto at = std::ranges::find(forge.choices, forge.chosen, &ForgeChoiceView::item_id);
    const int rank =
        at == forge.choices.end() ? 0 : static_cast<int>(at - forge.choices.begin()) + 1;
    if (const int to = SkinComboStepped(rank, step, static_cast<int>(forge.choices.size()) + 1);
        to != rank) {
      action.forge_slot = static_cast<int>(slot);
      action.forge_item_id =
          to == 0 ? std::uint32_t{0} : forge.choices[static_cast<std::size_t>(to - 1)].item_id;
    }
    if (open) {
      ComboKeys keys(rank, static_cast<int>(forge.choices.size()) + 1);
      if (keys.Took(0, ImGui::Selectable(core::Tr("(vide)"), forge.chosen == 0))) {
        action.forge_slot = static_cast<int>(slot);
        action.forge_item_id = 0;
      }
      for (std::size_t index = 0; index < forge.choices.size(); ++index) {
        const ForgeChoiceView& choice = forge.choices[index];
        const std::string label(choice.label);
        if (keys.Took(static_cast<int>(index) + 1,
                      ImGui::Selectable(label.c_str(), forge.chosen == choice.item_id))) {
          action.forge_slot = static_cast<int>(slot);
          action.forge_item_id = choice.item_id;
        }
      }
      EndSkinCombo();
    }
    ImGui::PopID();
    if (slot + 1 < view.forge_slots.size()) {
      ImGui::SameLine();
    }
  }
  DrawLines(view.forge_notes);
}

// LA SÉRIE : combien de fabrications partent toutes seules.
void DrawBatch(MakeItemWindowState& state, const MakeItemView& view, const SkinArt& art) {
  ImGui::AlignTextToFramePadding();
  ColoredText(PaintingLook().faint_text, core::Tr("Quantité"));
  ImGui::SameLine();
  if (SkinSmallButton("-##batchdec", art) && state.batch_target > kLeastBatch) {
    --state.batch_target;
  }
  ImGui::SameLine();
  ImGui::SetNextItemWidth(Px(kBatchFieldPixels));
  ImGui::InputInt("##batch", &state.batch_target, 0, 0);
  state.batch_target = std::clamp(state.batch_target, kLeastBatch, kMostBatch);
  ImGui::SameLine();
  if (SkinSmallButton("+##batchinc", art) && state.batch_target < kMostBatch) {
    ++state.batch_target;
  }
  ImGui::SameLine();
  if (!view.batch_progress.empty()) {
    const std::string progress(view.batch_progress);
    ColoredText(PaintingLook().warn_text, progress.c_str());
  } else {
    static constexpr std::array<const char*, 4> kBatchHelp = {
        core::TrText("À ne pas confondre avec « relancer automatiquement », qui rouvre la liste "
                     "après chaque fabrication : à 1 avec cette case cochée, une fabrication part "
                     "seule, la liste revient, et le clic suivant est à toi."),
        core::TrText("Le serveur n'accepte AUCUNE quantité : une liste = une fabrication. « ×20 » "
                     "signifie donc vingt tours complets — relance, nouvelle liste, nouvelle "
                     "demande."),
        core::TrText("Il faut par conséquent que la relance automatique correspondante soit "
                     "active, sinon aucune liste ne revient : la ligne Quantité n'apparaît donc "
                     "que si elle l'est."),
        core::TrText("Même effet qu'Entrée maintenue, mais la cible est CHIFFRÉE : on peut lâcher "
                     "le clavier, et ça s'arrête au compte demandé plutôt qu'au moment où l'on "
                     "pense à relâcher."),
    };
    SkinHelpMarker(core::Tr("Combien de fabrications partent TOUTES SEULES."), kBatchHelp);
  }
}

void DrawFooter(MakeItemWindowState& state, const MakeItemView& view, const SkinArt& art,
                MakeItemWindowAction& action) {
  ImGui::Separator();
  const ImVec2 button(Px(kButtonPixels), 0.0F);
  if (view.spent) {
    ImGui::BeginDisabled(!view.can_relaunch);
    if (SkinButton(core::Tr("Relancer"), art, button)) {
      action.verb = MakeItemVerb::kRelaunch;
    }
    ImGui::EndDisabled();
    if (!view.relaunch_tip.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      const std::string tip(view.relaunch_tip);
      SkinTooltip(tip.c_str());
    }
  } else {
    ImGui::BeginDisabled(!view.can_make || !action.selection_visible);
    if (SkinButton(core::Tr("Fabriquer"), art, button)) {
      action.verb = MakeItemVerb::kMake;
    }
    ImGui::EndDisabled();
  }
  ImGui::SameLine();
  if (SkinButton(core::Tr("Fermer"), art, button)) {
    action.verb = MakeItemVerb::kClose;
  }
  ImGui::SameLine();
  if (SkinButton(core::Tr("Atlas"), art, button)) {
    action.verb = MakeItemVerb::kOpenAtlas;
  }
  if (ImGui::IsItemHovered()) {
    SkinTooltip(
        core::Tr("Ouvre l'Atlas des recettes : tout ce qui se fabrique sur ce serveur, par "
                 "job, par produit et par matériau — y compris ce que cette liste ne "
                 "propose pas encore."));
  }

  // ── LA QUANTITÉ ─────────────────────────────────────────────────────────────
  if (view.batch_shown) {
    DrawBatch(state, view, art);
  }
  if (view.can_make && !action.selection_visible) {
    ColoredWrapped(PaintingLook().faint_text,
                   core::Tr("Le produit visé est masqué par le filtre."));
  }
  DrawLines(view.footer_notes);
}

}  // namespace

MakeItemWindowAction DrawMakeItemWindow(MakeItemWindowState& state, const MakeItemView& view,
                                        const SkinArt& art) {
  MakeItemWindowAction action;
  if (!state.open) {
    return action;
  }
  const float width = Px(kWindowPixels);
  ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0.0F), ImVec2(width, FLT_MAX));
  if (state.raise) {
    ImGui::SetNextWindowFocus();
    state.raise = false;
  }
  SetNextWindowBulletClickable(core::Tr("Options de fabrication"));
  const std::string title = std::string(view.title) + "###" + kMakeItemWindowId;
  bool open = true;
  const bool begun =
      BeginSkinnedWindow(title.c_str(), &open, art, SkinnedTitle::kBar,
                         ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);
  action.open_settings = TitleBulletClicked();
  if (begun) {
    if (view.stale) {
      ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * craft_parts::kStaleAlpha);
    }
    DrawList(state, view, action);
    DrawRecipe(view, action);
    DrawForgeSlots(view, art, action);
    if (view.stale) {
      ImGui::PopStyleVar();
    }
    DrawFooter(state, view, art, action);
    if (view.show_history && !view.history.empty()) {
      craft_parts::DrawHistory("##makelog", view.history, Px(kHistoryPixels), view.log_time, true);
    }
    // ENTRÉE FABRIQUE, si le réglage le veut — jamais pendant une saisie. ⚠ LE
    // CHAT NE LA VOIT PAS ALORS : la partie la lui retire, voir
    // `CraftStage::ClaimsEnter`.
    if (view.enter_key && !ImGui::IsAnyItemActive() &&
        (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) &&
        action.verb == MakeItemVerb::kNothing) {
      action.verb = MakeItemVerb::kMake;
    }
  }
  EndSkinnedWindow();
  if (!open) {
    action.verb = MakeItemVerb::kClose;
  }
  return action;
}

}  // namespace ro::ui
