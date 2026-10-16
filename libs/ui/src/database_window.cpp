// La fenêtre « Base de données » : onglets, liste, filtres et fil des fiches.
// Voir l'en-tête ; la fiche d'un objet est celle de la description
// (`item_sheet.cpp`).

#include "ro/ui/database_window.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <limits>
#include <string>

#include <imgui.h>

#include "ro/ui/list_keys.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/trade_widgets.h"

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
constexpr const char* kListMenuId = "##database_list_menu";

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

// LES GESTES DE TOUT OBJET OU MONSTRE sur la ligne qu'on vient de poser : le
// survol, Maj + clic, le clic droit qui arme le menu, Ctrl + clic droit.
void NoteRowGestures(DatabaseWindowState& state, DatabaseKind kind, const DatabaseRowView& row,
                     DatabaseAnswer& answer) {
  TradeItemGesture gesture;
  AddLastItemGesture(gesture);
  const ChatLinkKind link_kind =
      kind == DatabaseKind::kItem ? ChatLinkKind::kItem : ChatLinkKind::kMonster;
  if (gesture.hovered) {
    answer.list.hovered_id = row.id;
    answer.list.hovered_kind = link_kind;
    answer.list.hovering = true;
  }
  if (gesture.menu || gesture.describe) {
    state.list_menu.armed = gesture.menu;
    state.list_menu.describe_at_once = gesture.describe;
    state.list_menu.kind = link_kind;
    state.list_menu.item_id = row.id;
    state.list_menu.token = 0;
    state.list_menu.label = std::string(row.name);
  }
  if (gesture.repost) {
    answer.list.gesture = ChatLinkGesture::kRepost;
    answer.list.kind = link_kind;
    answer.list.item_id = row.id;
  }
}

// La ligne montrée dans la liste, ou `kListNoEntry`.
[[nodiscard]] int ShownRow(const DatabaseView& view, DatabaseKind kind,
                           const std::optional<DatabaseTarget>& shown) {
  if (!shown.has_value() || shown->kind != kind) {
    return kListNoEntry;
  }
  const auto found = std::ranges::find(view.rows, shown->id, &DatabaseRowView::id);
  return found != view.rows.end() ? static_cast<int>(found - view.rows.begin()) : kListNoEntry;
}

// LE CLAVIER DE LA LISTE, fenêtre au premier plan : la fiche suit la ligne
// choisie. Rend la ligne où il mène, ou `kListNoEntry` sans touche.
[[nodiscard]] int StepByKeys(DatabaseWindowState& state, const DatabaseView& view,
                             DatabaseKind kind, bool query_active) {
  // ⚠ PAS PAR-DESSUS UNE LISTE DÉROULÉE : elle a ses propres touches.
  if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows |
                              ImGuiFocusedFlags_NoPopupHierarchy)) {
    return kListNoEntry;
  }
  // Une autre saisie garde toutes ses touches ; celle de la recherche laisse
  // les flèches à la liste.
  const bool typing = ImGui::GetIO().WantTextInput;
  if (typing && !query_active) {
    return kListNoEntry;
  }
  const ListKeyMove move = PressedListKey(/*arrows_only=*/typing);
  if (move == ListKeyMove::kNone) {
    return kListNoEntry;
  }
  const float row_pitch = ImGui::GetTextLineHeight() + 2.0F * ImGui::GetStyle().CellPadding.y;
  const int target =
      ListKeyTarget(ShownRow(view, kind, state.history.current()),
                    static_cast<int>(view.rows.size()), VisibleListRows(row_pitch), move);
  if (target == kListNoEntry) {
    return kListNoEntry;
  }
  state.history.Step(
      DatabaseTarget{.kind = kind, .id = view.rows[static_cast<std::size_t>(target)].id});
  return target;
}

// LA LISTE : un clic visite la fiche de la ligne ; les autres gestes sont ceux
// de tout objet ou monstre. Elle défile sous l'ascenseur du jeu.
void DrawList(DatabaseWindowState& state, const DatabaseView& view, const SkinArt& art,
              bool query_active, DatabaseAnswer& answer) {
  const DatabaseKind kind =
      state.tab == kDatabaseItemTab ? DatabaseKind::kItem : DatabaseKind::kMonster;
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
  // Lu DANS la table : c'est sa fenêtre intérieure qui défile.
  const int keyed_row = StepByKeys(state, view, kind, query_active);
  const std::optional<DatabaseTarget> shown = state.history.current();
  if (state.scroll_to_shown && shown.has_value() && shown->kind == kind) {
    if (const int shown_row = ShownRow(view, kind, shown); shown_row != kListNoEntry) {
      ImGui::SetScrollY(static_cast<float>(shown_row) * ImGui::GetTextLineHeightWithSpacing());
    }
    state.scroll_to_shown = false;
  }
  ImGuiListClipper clipper;
  clipper.Begin(static_cast<int>(view.rows.size()));
  // La ligne qu'une touche vient de choisir se pose même hors de la vue, pour
  // que la liste la rejoigne.
  if (keyed_row != kListNoEntry) {
    clipper.IncludeItemByIndex(keyed_row);
  }
  while (clipper.Step()) {
    for (int rank = clipper.DisplayStart; rank < clipper.DisplayEnd; ++rank) {
      const DatabaseRowView& row = view.rows[static_cast<std::size_t>(rank)];
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      std::array<char, kLineBytes> label{};
      std::snprintf(label.data(), label.size(), "#%u %.*s##db_row%d", static_cast<unsigned>(row.id),
                    static_cast<int>(row.name.size()), row.name.data(), rank);
      const bool selected = shown.has_value() && shown->kind == kind && shown->id == row.id;
      // ⚠ MAJ + CLIC POSE LE LIEN, il ne visite pas : c'est le geste de tout lien.
      if (ImGui::Selectable(label.data(), selected, ImGuiSelectableFlags_SpanAllColumns) &&
          !ImGui::GetIO().KeyShift) {
        state.history.Visit(DatabaseTarget{.kind = kind, .id = row.id});
      }
      if (rank == keyed_row) {
        KeepLastItemInView();
      }
      NoteRowGestures(state, kind, row, answer);
      ImGui::TableNextColumn();
      ImGui::TextDisabled("%.*s", static_cast<int>(row.note.size()), row.note.data());
    }
  }
  PaintWindowScrollbar(art);
  ImGui::EndTable();
  TextOf(view.summary);
}

void DrawListPane(DatabaseWindowState& state, const DatabaseView& view, const SkinArt& art,
                  DatabaseAnswer& answer) {
  std::array<char, kDatabaseQueryBytes>& query =
      state.tab == kDatabaseItemTab ? state.item_query : state.monster_query;
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
  ImGui::InputTextWithHint("##db_query", core::Tr("Nom ou ID"), query.data(), query.size());
  const bool query_active = ImGui::IsItemActive();
  DrawFilters(state, art);
  ImGui::Separator();
  DrawList(state, view, art, query_active, answer);
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
    DrawItemSheetBody(state.item_sheet, *view.item, art, ItemSheetHost::kDatabase, answer.item);
  } else if (view.monster != nullptr) {
    answer.monster =
        DrawMonsterSheetBody(monster_state, *view.monster, art, MonsterSheetHost::kDatabase);
  } else {
    ImGui::TextDisabled("%s", core::Tr("Choisissez un item ou un monstre dans la liste."));
  }
}

// UN LIEN SUIVI DANS LA FICHE D'UN OBJET reste dans la base : un autre objet,
// ou un monstre qui le lâche, s'y visite. Le clic sur l'objet MONTRÉ, lui,
// ouvre sa description, comme partout : il ne mènerait nulle part ici.
void FollowSheetLink(DatabaseWindowState& state, const DescriptionView& item, ChatLinkAsk& link) {
  if (link.gesture != ChatLinkGesture::kDescribe || link.item_id == 0) {
    return;
  }
  if (link.kind == ChatLinkKind::kMonster) {
    state.history.Visit(DatabaseTarget{.kind = DatabaseKind::kMonster, .id = link.item_id});
  } else if (link.kind == ChatLinkKind::kItem && link.item_id != item.item_id) {
    state.history.Visit(DatabaseTarget{.kind = DatabaseKind::kItem, .id = link.item_id});
  } else {
    return;
  }
  link.gesture = ChatLinkGesture::kNothing;
}

}  // namespace

void DatabaseHistory::Step(DatabaseTarget target) {
  if (const std::optional<DatabaseTarget> shown = current();
      shown.has_value() && *shown == target) {
    return;
  }
  if (!stepped_ || visited_.empty()) {
    Visit(target);
    stepped_ = true;
    return;
  }
  // Revenu sur la fiche d'où il est parti, le clavier ne la double pas.
  if (at_ > 0 && visited_[at_ - 1] == target) {
    visited_.resize(at_);
    --at_;
    stepped_ = false;
    return;
  }
  // Une suite coupée, comme par une visite.
  visited_.resize(at_ + 1);
  visited_[at_] = target;
}

void DatabaseHistory::Visit(DatabaseTarget target) {
  if (const std::optional<DatabaseTarget> shown = current();
      shown.has_value() && *shown == target) {
    return;
  }
  stepped_ = false;
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
  stepped_ = false;
  return true;
}

bool DatabaseHistory::Forward() noexcept {
  if (!can_go_forward()) {
    return false;
  }
  ++at_;
  stepped_ = false;
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
  // Le pantin de la fiche n'est voulu que tant que sa ligne est survolée, et
  // elle le redit.
  state.item_sheet.doll.wanted = false;
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
        DrawListPane(state, view, art, answer);
        PaintWindowScrollbar(art);
      }
      ImGui::EndChild();
      ImGui::TableNextColumn();
      if (ImGui::BeginChild("##db_sheet_child")) {
        DrawSheetPane(state, view, monster_state, art, answer);
        PaintWindowScrollbar(art);
      }
      ImGui::EndChild();
      ImGui::EndTable();
    }
    // LES MENUS, HORS de la table et des panneaux : une cellule change la pile
    // d'identifiants. Celui d'une ligne de la liste, puis ceux de la fiche.
    DrawItemLinkMenu(kListMenuId, state.list_menu, answer.list, view.vote_shop);
    if (view.item != nullptr) {
      FinishItemSheetLinks(state.item_sheet, *view.item, ItemSheetHost::kDatabase, answer.item);
      FollowSheetLink(state, *view.item, answer.item.link);
    }
  }
  EndSkinnedWindow();
  return answer;
}

}  // namespace ro::ui
