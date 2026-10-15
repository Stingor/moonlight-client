// La fenêtre « Base de données » : onglets, liste, filtres et fil des fiches.
// Voir l'en-tête ; la fiche d'un objet est dans `database_item_sheet.cpp`.

#include "ro/ui/database_window.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <limits>
#include <string>

#include <imgui.h>

#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

#include "database_parts.h"

namespace ro::ui {

// La place du texte d'une liste de filtre fermée : un nom et une entrée.
constexpr std::size_t kFilterShownBytes = 128;

std::string DatabaseFilterShown(const char* filter_name, int chosen,
                                std::span<const char* const> values) {
  std::array<char, kFilterShownBytes> shown{};
  std::snprintf(shown.data(), shown.size(), core::Tr("%s - %s"), filter_name,
                DatabaseFilterChoice(chosen, values));
  return shown.data();
}

namespace {

[[nodiscard]] float Scale() { return ImGui::GetStyle().FontScaleMain; }

constexpr float kWindowWidthPixels = 940.0F;
constexpr float kWindowHeightPixels = 600.0F;
constexpr float kLeastWidthPixels = 640.0F;
constexpr float kLeastHeightPixels = 360.0F;
// La liste à gauche, à sa première ouverture ; la fiche prend le reste.
constexpr float kListPanePixels = 300.0F;
constexpr float kLevelFieldPixels = 90.0F;
constexpr float kNoteColumnPixels = 80.0F;
constexpr int kListColumns = 2;
constexpr int kSplitColumns = 2;
constexpr std::size_t kLineBytes = 64;
// La place du pied, sous la liste.
constexpr float kFooterLines = 1.0F;
constexpr const char* kItemMenuId = "##database_item_menu";

void TextOf(std::string_view text) {
  ImGui::TextUnformatted(text.data(), text.data() + text.size());
}

// UNE LISTE DE FILTRE : « tous », puis la table. Fermée, elle se nomme devant
// son choix. Rend vrai sur un changement.
template <std::size_t Count>
bool FilterCombo(const char* id, const char* filter_name, int* chosen,
                 const std::array<const char*, Count>& labels, const SkinArt& art) {
  std::array<const char*, Count + 1> entries{};
  entries[0] = core::TrText("Tous");
  std::ranges::copy(labels, entries.begin() + 1);
  const std::string closed = DatabaseFilterShown(filter_name, *chosen, labels);
  return SkinCombo(id, chosen, entries, art, closed.c_str());
}

template <typename Entry, std::size_t Count>
std::array<const char*, Count> LabelsOf(const std::array<Entry, Count>& entries) {
  std::array<const char*, Count> labels{};
  std::ranges::transform(entries, labels.begin(), [](const Entry& entry) { return entry.label; });
  return labels;
}

void LevelBounds(const char* id_least, const char* id_most, int* least, int* most,
                 const SkinArt& art) {
  ImGui::AlignTextToFramePadding();
  TextOf(core::Tr("Niveau"));
  ImGui::SameLine();
  ImGui::SetNextItemWidth(kLevelFieldPixels * Scale());
  static_cast<void>(SkinInputInt(id_least, least, 0, kDatabaseHighestLevel, 1, 0, art));
  ImGui::SameLine();
  ImGui::TextUnformatted("~");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(kLevelFieldPixels * Scale());
  static_cast<void>(SkinInputInt(id_most, most, 0, kDatabaseHighestLevel, 1, 0, art));
}

void DrawFilters(DatabaseWindowState& state, const SkinArt& art) {
  DatabaseFilters& filters = state.filters;
  const float full = ImGui::GetContentRegionAvail().x;
  if (state.tab == kDatabaseItemTab) {
    ImGui::SetNextItemWidth(full);
    static_cast<void>(FilterCombo("##db_item_type", core::Tr("Type"), &filters.item_type,
                                  LabelsOf(kDatabaseItemTypes), art));
    ImGui::SetNextItemWidth(full);
    static_cast<void>(FilterCombo("##db_item_place", core::Tr("Emplacement"), &filters.item_place,
                                  LabelsOf(kDatabaseItemPlaces), art));
    LevelBounds("##db_item_level_min", "##db_item_level_max", &filters.item_level_min,
                &filters.item_level_max, art);
    return;
  }
  const float half = (full - ImGui::GetStyle().ItemSpacing.x) * 0.5F;
  ImGui::SetNextItemWidth(half);
  static_cast<void>(FilterCombo("##db_race", core::Tr("Race"), &filters.race, kDatabaseRaces, art));
  ImGui::SameLine();
  ImGui::SetNextItemWidth(half);
  static_cast<void>(
      FilterCombo("##db_element", core::Tr("Élément"), &filters.element, kDatabaseElements, art));
  ImGui::SetNextItemWidth(half);
  static_cast<void>(
      FilterCombo("##db_size", core::Tr("Taille"), &filters.size, kDatabaseSizes, art));
  ImGui::SameLine();
  ImGui::SetNextItemWidth(half);
  static_cast<void>(FilterCombo("##db_rank", core::Tr("Rang"), &filters.rank, kDatabaseRanks, art));
  LevelBounds("##db_level_min", "##db_level_max", &filters.level_min, &filters.level_max, art);
}

// LA LISTE : un clic visite la fiche de la ligne.
void DrawList(DatabaseWindowState& state, const DatabaseView& view) {
  const DatabaseKind kind =
      state.tab == kDatabaseItemTab ? DatabaseKind::kItem : DatabaseKind::kMonster;
  const std::optional<DatabaseTarget> shown = state.history.current();
  const float height =
      std::max(ImGui::GetFrameHeight(), ImGui::GetContentRegionAvail().y -
                                            kFooterLines * ImGui::GetTextLineHeightWithSpacing());
  if (!ImGui::BeginTable("##db_rows", kListColumns,
                         ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
                             ImGuiTableFlags_BordersOuter | ImGuiTableFlags_SizingStretchProp,
                         ImVec2(0.0F, height))) {
    return;
  }
  ImGui::TableSetupColumn("##db_name", ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn("##db_note", ImGuiTableColumnFlags_WidthFixed,
                          kNoteColumnPixels * Scale());
  int shown_row = -1;
  if (state.scroll_to_shown && shown.has_value() && shown->kind == kind) {
    const auto found = std::ranges::find(view.rows, shown->id, &DatabaseRowView::id);
    if (found != view.rows.end()) {
      shown_row = static_cast<int>(found - view.rows.begin());
      ImGui::SetScrollY(static_cast<float>(shown_row) * ImGui::GetTextLineHeightWithSpacing());
    }
    state.scroll_to_shown = false;
  }
  ImGuiListClipper clipper;
  clipper.Begin(static_cast<int>(view.rows.size()));
  while (clipper.Step()) {
    for (int rank = clipper.DisplayStart; rank < clipper.DisplayEnd; ++rank) {
      const DatabaseRowView& row = view.rows[static_cast<std::size_t>(rank)];
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      std::array<char, kLineBytes> label{};
      std::snprintf(label.data(), label.size(), "#%u %.*s##db_row%d", static_cast<unsigned>(row.id),
                    static_cast<int>(row.name.size()), row.name.data(), rank);
      const bool selected = shown.has_value() && shown->kind == kind && shown->id == row.id;
      if (ImGui::Selectable(label.data(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
        state.history.Visit(DatabaseTarget{.kind = kind, .id = row.id});
      }
      ImGui::TableNextColumn();
      ImGui::TextDisabled("%.*s", static_cast<int>(row.note.size()), row.note.data());
    }
  }
  ImGui::EndTable();
  TextOf(view.summary);
}

void DrawListPane(DatabaseWindowState& state, const DatabaseView& view, const SkinArt& art) {
  std::array<char, kDatabaseQueryBytes>& query =
      state.tab == kDatabaseItemTab ? state.item_query : state.monster_query;
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
  ImGui::InputTextWithHint("##db_query", core::Tr("Nom ou ID"), query.data(), query.size());
  DrawFilters(state, art);
  ImGui::Separator();
  DrawList(state, view);
}

// LA BARRE DU HAUT : les deux onglets, puis ◀ ▶.
void DrawTabs(DatabaseWindowState& state, const SkinArt& art) {
  if (BeginSkinTabBar("##db_tabs", art)) {
    const std::array<std::pair<int, const char*>, 2> tabs = {{
        {kDatabaseItemTab, core::Tr("Objets")},
        {kDatabaseMonsterTab, core::Tr("Monstres")},
    }};
    for (const auto& [tab, label] : tabs) {
      const ImGuiTabItemFlags flags =
          state.select_tab && tab == state.tab ? ImGuiTabItemFlags_SetSelected : 0;
      if (ImGui::BeginTabItem(label, nullptr, flags)) {
        // ⚠ LE CHOIX D'IMGUI NE COMPTE PAS tant que l'onglet voulu n'est pas
        // montré : il n'obéit qu'à l'image d'après.
        if (state.select_tab) {
          state.select_tab = tab != state.tab;
        } else {
          state.tab = tab;
        }
        ImGui::EndTabItem();
      }
    }
    EndSkinTabBar();
  }
  ImGui::BeginDisabled(!state.history.can_go_back());
  if (SkinSmallButton("<##db_back", art) && state.history.Back()) {
    state.scroll_to_shown = true;
  }
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    SkinTooltip(core::Tr("Fiche précédente"));
  }
  ImGui::SameLine();
  ImGui::BeginDisabled(!state.history.can_go_forward());
  if (SkinSmallButton(">##db_forward", art) && state.history.Forward()) {
    state.scroll_to_shown = true;
  }
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    SkinTooltip(core::Tr("Fiche suivante"));
  }
}

void DrawSheetPane(DatabaseWindowState& state, const DatabaseView& view,
                   MonsterSheetWindowState& monster_state, const SkinArt& art,
                   DatabaseAnswer& answer) {
  if (view.item != nullptr) {
    database::DrawItemSheet(state, *view.item, art, state.item_menu, answer);
  } else if (view.monster != nullptr) {
    answer.monster = DrawMonsterSheetBody(monster_state, *view.monster, art);
  } else {
    ImGui::TextDisabled("%s", core::Tr("Choisissez un item ou un monstre dans la liste."));
  }
}

}  // namespace

void DatabaseHistory::Visit(DatabaseTarget target) {
  if (const std::optional<DatabaseTarget> shown = current();
      shown.has_value() && *shown == target) {
    return;
  }
  if (!visited_.empty()) {
    visited_.resize(at_ + 1);
  }
  visited_.push_back(target);
  if (visited_.size() > kDatabaseHistoryMost) {
    visited_.erase(visited_.begin());
  }
  at_ = visited_.size() - 1;
}

bool DatabaseHistory::Back() noexcept {
  if (!can_go_back()) {
    return false;
  }
  --at_;
  return true;
}

bool DatabaseHistory::Forward() noexcept {
  if (!can_go_forward()) {
    return false;
  }
  ++at_;
  return true;
}

std::optional<DatabaseTarget> DatabaseHistory::current() const {
  if (visited_.empty()) {
    return std::nullopt;
  }
  return visited_[at_];
}

void OpenDatabaseOn(DatabaseWindowState& state, std::optional<DatabaseTarget> target) {
  state.open = true;
  state.raise = true;
  if (!target.has_value()) {
    return;
  }
  state.history.Visit(*target);
  state.tab = target->kind == DatabaseKind::kItem ? kDatabaseItemTab : kDatabaseMonsterTab;
  state.select_tab = true;
  state.scroll_to_shown = true;
}

DatabaseAnswer DrawDatabaseWindow(DatabaseWindowState& state, const DatabaseView& view,
                                  MonsterSheetWindowState& monster_state, const SkinArt& art) {
  DatabaseAnswer answer;
  if (!state.open) {
    return answer;
  }
  const float scale = Scale();
  if (state.raise) {
    ImGui::SetNextWindowFocus();
    state.raise = false;
  }
  ImGui::SetNextWindowSize(ImVec2(kWindowWidthPixels * scale, kWindowHeightPixels * scale),
                           ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints(
      ImVec2(kLeastWidthPixels * scale, kLeastHeightPixels * scale),
      ImVec2(std::numeric_limits<float>::max(), std::numeric_limits<float>::max()));
  const std::string title = std::string(core::Tr("Base de données")) + kDatabaseWindowId;
  if (BeginSkinnedWindow(title.c_str(), &state.open, art, SkinnedTitle::kBar,
                         ImGuiWindowFlags_NoCollapse) &&
      state.open) {
    DrawTabs(state, art);
    if (view.loading) {
      TextOf(core::Tr("Lecture de la base de données…"));
    } else if (!view.status.empty()) {
      ImGui::TextWrapped("%.*s", static_cast<int>(view.status.size()), view.status.data());
    } else if (ImGui::BeginTable("##db_split", kSplitColumns,
                                 ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV,
                                 ImGui::GetContentRegionAvail())) {
      ImGui::TableSetupColumn("##db_list_pane", ImGuiTableColumnFlags_WidthFixed,
                              kListPanePixels * scale);
      ImGui::TableSetupColumn("##db_sheet_pane", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      if (ImGui::BeginChild("##db_list_child")) {
        DrawListPane(state, view, art);
      }
      ImGui::EndChild();
      ImGui::TableNextColumn();
      if (ImGui::BeginChild("##db_sheet_child")) {
        DrawSheetPane(state, view, monster_state, art, answer);
      }
      ImGui::EndChild();
      ImGui::EndTable();
    }
    // Le menu de l'objet de la fiche, HORS de la table et des panneaux : une
    // cellule change la pile d'identifiants.
    if (view.item != nullptr) {
      DrawItemLinkMenu(kItemMenuId, state.item_menu, answer.item, view.item->vote_shop);
    }
  }
  EndSkinnedWindow();
  return answer;
}

}  // namespace ro::ui
