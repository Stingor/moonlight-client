// La fenêtre « Joueurs en ligne » du staff : son dessin. La logique de la
// liste est dans `staff_roster_rows.cpp`.

#include "ro/ui/staff_roster_window.h"

#include <algorithm>
#include <cfloat>
#include <string>
#include <string_view>
#include <utility>

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

constexpr const char* kTitle = core::TrText("Joueurs en ligne");

// ── LES MESURES ─────────────────────────────────────────────────────────────
constexpr float kFirstWidthPixels = 880.0F;
constexpr float kFirstHeightPixels = 460.0F;
constexpr float kLeastWidthPixels = 520.0F;
constexpr float kLeastHeightPixels = 260.0F;
constexpr float kSearchWidthPixels = 170.0F;
constexpr float kJobColumnPixels = 104.0F;
constexpr float kLevelColumnPixels = 52.0F;
constexpr float kMapColumnPixels = 100.0F;
constexpr float kGuildColumnPixels = 110.0F;
constexpr float kDurationColumnPixels = 64.0F;
constexpr float kActionColumnPixels = 78.0F;
constexpr float kClientColumnPixels = 170.0F;
// Entre le nom d'un joueur et chacune de ses étiquettes.
constexpr float kTagGapPixels = 6.0F;
// Entre deux comptes du bandeau de synthèse.
constexpr float kSummaryGapPixels = 14.0F;
// Les premiers caractères d'une révision suffisent à la reconnaître.
constexpr std::size_t kRevisionShownChars = 9;

// ── LES COULEURS, réglées pour le corps clair des fenêtres du jeu ───────────
constexpr ImU32 kInfoColor = IM_COL32(38, 64, 153, 255);
constexpr ImU32 kWarnColor = IM_COL32(150, 74, 26, 255);
constexpr ImU32 kDangerColor = IM_COL32(153, 31, 31, 255);

constexpr const char* kNothing = "—";

[[nodiscard]] int Rank(StaffRosterColumn column) { return static_cast<int>(column); }

[[nodiscard]] std::string_view SearchOf(const StaffRosterWindowState& state) {
  return std::string_view(state.search.data());
}

[[nodiscard]] const char* ActivityLabel(StaffRosterActivity activity) {
  switch (activity) {
    case StaffRosterActivity::kWalk:
      return core::Tr("Marche");
    case StaffRosterActivity::kFight:
      return core::Tr("Combat");
    case StaffRosterActivity::kItem:
      return core::Tr("Item");
    case StaffRosterActivity::kTalk:
      return core::Tr("Parole");
    case StaffRosterActivity::kGesture:
      return core::Tr("Geste");
    case StaffRosterActivity::kNpc:
      return core::Tr("NPC");
    case StaffRosterActivity::kCommand:
      return core::Tr("Commande");
  }
  return kNothing;
}

[[nodiscard]] const char* ClientLabel(StaffRosterClientKind client) {
  switch (client) {
    case StaffRosterClientKind::kNone:
      return core::Tr("sans poignée de main");
    case StaffRosterClientKind::kOriginal:
      return core::Tr("client d'origine");
    case StaffRosterClientKind::kMoonclient:
      return "moonclient";
    case StaffRosterClientKind::kOther:
      break;
  }
  return core::Tr("autre client");
}

[[nodiscard]] ImU32 ClientColor(const StaffRosterRow& row) {
  if (row.client == StaffRosterClientKind::kNone) {
    return kDangerColor;
  }
  if (row.client_outdated || row.tree_modified) {
    return kWarnColor;
  }
  return ImGui::GetColorU32(ImGuiCol_Text);
}

[[nodiscard]] std::string MapText(std::string_view map, int instance_id) {
  if (instance_id == 0) {
    return std::string(map);
  }
  return core::Printed("%.*s #%d", static_cast<int>(map.size()), map.data(), instance_id);
}

// Le client en une cellule : son nom et sa révision, suivie d'un astérisque
// quand l'arbre était modifié. La révision dit la version même quand la mise à
// jour est passée par le miroir sans changer le patch ; le patch ne la remplace
// que pour un client qui ne déclare pas de révision.
[[nodiscard]] std::string ClientCellText(const StaffRosterRow& row) {
  std::string text = ClientLabel(row.client);
  if (row.client == StaffRosterClientKind::kNone) {
    return text;
  }
  if (!row.revision.empty()) {
    text += ' ';
    text += row.revision.substr(0, kRevisionShownChars);
    if (row.tree_modified) {
      text += '*';
    }
  } else if (row.patch_index >= 0) {
    text += core::Printed(" p%d", row.patch_index);
  }
  return text;
}

// Le client en toutes lettres, pour la bulle.
[[nodiscard]] std::string ClientDetailText(const StaffRosterRow& row) {
  std::string text = ClientLabel(row.client);
  if (row.client == StaffRosterClientKind::kNone) {
    return text;
  }
  text += ", ";
  text += row.patch_index >= 0 ? core::Printed(core::Tr("patch %d"), row.patch_index)
                               : std::string(core::Tr("jamais patché"));
  if (!row.revision.empty()) {
    text += ", ";
    text +=
        core::Printed(core::Tr("révision %s"), row.revision.substr(0, kRevisionShownChars).c_str());
  }
  if (row.tree_modified) {
    text += ", ";
    text += core::Tr("arbre modifié");
  }
  if (row.client_outdated) {
    text += ", ";
    text += core::Tr("en retard");
  } else if (row.other_revision) {
    text += ", ";
    text += core::Tr("autre révision que la vôtre");
  }
  return text;
}

// Ce que le joueur fait là, d'un mot par état ; vide quand il n'y a rien à dire.
[[nodiscard]] std::string StateText(const StaffRosterRow& row) {
  std::string text;
  const auto add = [&text](bool on, const char* word) {
    if (!on) {
      return;
    }
    if (!text.empty()) {
      text += ", ";
    }
    text += word;
  };
  add(row.offline_shop, core::Tr("hors ligne"));
  add(row.vending, core::Tr("échoppe de vente"));
  add(row.buying, core::Tr("échoppe d'achat"));
  add(row.chat_room, core::Tr("salon"));
  add(row.trading, core::Tr("échange"));
  add(row.npc_dialog, core::Tr("dialogue de NPC"));
  add(row.storage_open, core::Tr("storage ouvert"));
  add(row.dead, core::Tr("mort"));
  add(row.sitting, core::Tr("assis"));
  add(row.hidden, core::Tr("caché"));
  add(row.away, core::Tr("absent"));
  return text;
}

[[nodiscard]] std::string JoinedNames(const std::vector<std::string_view>& names) {
  std::string text;
  for (const std::string_view name : names) {
    if (!text.empty()) {
      text += ", ";
    }
    text += name;
  }
  return text;
}

// « libellé : valeur », le libellé en retrait.
void Pair(const char* label, std::string_view value) {
  FaintText(label);
  ImGui::SameLine();
  Unformatted(value);
}

void DrawRowTooltip(const StaffRosterView& view, std::size_t rank) {
  if (!BeginSkinTooltip()) {
    return;
  }
  const StaffRosterRow& row = view.rows[rank];
  Unformatted(row.name);
  if (!row.job_name.empty()) {
    ImGui::SameLine();
    FaintText(core::Printed("%s %d/%d", row.job_name.c_str(), row.base_level, row.job_level));
  }
  Pair(core::Tr("AID / CID"), core::Printed("%u / %u", row.account_id, row.char_id));
  Pair(core::Tr("Carte"), core::Printed("%s (%d, %d)", MapText(row.map, row.instance_id).c_str(),
                                        row.cell_x, row.cell_y));
  if (!row.guild.empty()) {
    Pair(core::Tr("Guilde"), row.guild);
  }
  if (!row.party.empty()) {
    Pair(core::Tr("Groupe"), row.party);
  }
  if (const std::string state = StateText(row); !state.empty()) {
    Pair(core::Tr("État"), state);
  }
  ImGui::Separator();
  Pair(core::Tr("Session du compte"), StaffRosterDurationText(row.online_s));
  Pair(core::Tr("Sur ce personnage"), StaffRosterDurationText(row.character_s));
  if (!row.offline_shop) {
    Pair(core::Tr("Inactif depuis"), StaffRosterDurationText(row.idle_s));
  }
  for (std::size_t kind = 0; kind < row.action_age_s.size(); ++kind) {
    if (const std::optional<std::uint32_t>& age_s = row.action_age_s[kind]; age_s.has_value()) {
      Pair(ActivityLabel(static_cast<StaffRosterActivity>(kind)),
           core::Printed(core::Tr("il y a %s"), StaffRosterDurationText(*age_s).c_str()));
    }
  }
  ImGui::Separator();
  Pair(core::Tr("Client"), ClientDetailText(row));
  if (const std::vector<std::string_view> mates = MachineMates(view.rows, rank); !mates.empty()) {
    Pair(core::Tr("Même machine"), JoinedNames(mates));
  }
  if (const std::vector<std::string_view> mates = AccountMates(view.rows, rank); !mates.empty()) {
    Pair(core::Tr("Même compte Moonlight"), JoinedNames(mates));
  }
  if (row.cheat_clock > 0 || row.cheat_cadence > 0 || row.cheat_client > 0) {
    Pair(core::Tr("Indices de triche"),
         core::Printed(core::Tr("horloge %d, cadence %d, détecteur %d"), row.cheat_clock,
                       row.cheat_cadence, row.cheat_client));
  }
  if (row.mute_minutes > 0) {
    Pair(core::Tr("Muet"), core::Printed(core::Tr("encore %d min"), row.mute_minutes));
  }
  if (row.jailed) {
    ColoredText(kDangerColor, core::Tr("En prison"));
  }
  EndSkinTooltip();
}

// À poser juste après l'élément qui tient la ligne : sa bulle, et son menu au
// clic droit.
void RowGestures(const StaffRosterView& view, std::size_t rank, StaffRosterAnswer& answer) {
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup) &&
      ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
    answer.menu_account_id = view.rows[rank].account_id;
  }
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
    DrawRowTooltip(view, rank);
  }
}

// Les étiquettes d'une ligne, à la suite de son nom.
void DrawTags(const StaffRosterRow& row) {
  const auto tag = [](ImU32 color, const char* word) {
    ImGui::SameLine(0.0F, Px(kTagGapPixels));
    ColoredText(color, word);
  };
  const ImU32 faint = ImGui::GetColorU32(ImGuiCol_TextDisabled);
  if (row.staff) {
    tag(kInfoColor, core::Tr("staff"));
  }
  if (row.offline_shop) {
    tag(faint, core::Tr("hors ligne"));
  } else if (row.vending || row.buying) {
    tag(faint, core::Tr("échoppe"));
  }
  if (row.hidden) {
    tag(kInfoColor, core::Tr("caché"));
  }
  if (row.away) {
    tag(faint, core::Tr("absent"));
  }
  if (row.machine_group != 0) {
    tag(kWarnColor, core::Tr("même machine"));
  } else if (row.account_group != 0) {
    tag(kWarnColor, core::Tr("même compte"));
  }
  if (row.cheat_clock > 0 || row.cheat_cadence > 0 || row.cheat_client > 0) {
    tag(kDangerColor, core::Tr("triche"));
  }
  if (row.mute_minutes > 0) {
    tag(kWarnColor, core::Tr("muet"));
  }
  if (row.jailed) {
    tag(kDangerColor, core::Tr("prison"));
  }
}

// Le nom, tenu par une sélection invisible qui prend toute la ligne : un nom ne
// sert ainsi jamais d'identifiant à ImGui.
void DrawNameCell(const StaffRosterView& view, std::size_t rank, StaffRosterAnswer& answer,
                  ImGuiSelectableFlags flags) {
  static_cast<void>(ImGui::Selectable("##ligne", false, flags | ImGuiSelectableFlags_AllowOverlap));
  RowGestures(view, rank, answer);
  ImGui::SameLine(0.0F, 0.0F);
  Unformatted(view.rows[rank].name);
  DrawTags(view.rows[rank]);
}

void DrawPlayerRow(const StaffRosterView& view, std::size_t rank, StaffRosterAnswer& answer) {
  const StaffRosterRow& row = view.rows[rank];
  ImGui::PushID(static_cast<int>(rank));
  ImGui::TableNextRow();
  ImGui::TableSetColumnIndex(Rank(StaffRosterColumn::kPlayer));
  DrawNameCell(view, rank, answer, ImGuiSelectableFlags_SpanAllColumns);

  ImGui::TableSetColumnIndex(Rank(StaffRosterColumn::kJob));
  Unformatted(row.job_name.empty() ? std::string_view(kNothing) : std::string_view(row.job_name));
  ImGui::TableSetColumnIndex(Rank(StaffRosterColumn::kLevel));
  ImGui::Text("%d/%d", row.base_level, row.job_level);
  ImGui::TableSetColumnIndex(Rank(StaffRosterColumn::kMap));
  Unformatted(MapText(row.map, row.instance_id));
  ImGui::TableSetColumnIndex(Rank(StaffRosterColumn::kGuild));
  Unformatted(row.guild.empty() ? std::string_view(kNothing) : std::string_view(row.guild));
  ImGui::TableSetColumnIndex(Rank(StaffRosterColumn::kOnline));
  Unformatted(StaffRosterDurationText(row.online_s));

  ImGui::TableSetColumnIndex(Rank(StaffRosterColumn::kIdle));
  if (row.offline_shop) {
    FaintText(kNothing);
  } else if (RowIsIdle(row)) {
    ColoredText(kWarnColor, StaffRosterDurationText(row.idle_s));
  } else {
    Unformatted(StaffRosterDurationText(row.idle_s));
  }

  ImGui::TableSetColumnIndex(Rank(StaffRosterColumn::kAction));
  if (row.offline_shop) {
    FaintText(core::Tr("Échoppe"));
  } else if (const std::optional<StaffRosterActivity> activity = FreshestActivity(row);
             activity.has_value()) {
    ImGui::TextUnformatted(ActivityLabel(*activity));
  } else {
    FaintText(kNothing);
  }

  ImGui::TableSetColumnIndex(Rank(StaffRosterColumn::kClient));
  ColoredText(ClientColor(row), ClientCellText(row));
  ImGui::PopID();
}

void DrawPlayers(StaffRosterWindowState& state, const StaffRosterView& view, float height,
                 const SkinArt& art, StaffRosterAnswer& answer) {
  const auto flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_RowBg |
                     ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV |
                     ImGuiTableFlags_ScrollY;
  if (!ImGui::BeginTable("##joueurs_en_ligne", kStaffRosterColumnCount, flags,
                         ImVec2(0.0F, height))) {
    return;
  }
  // Une durée se lit d'abord par ses plus longues.
  const auto duration_flags =
      ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending;
  ImGui::TableSetupScrollFreeze(0, 1);
  ImGui::TableSetupColumn(core::Tr("Joueur"),
                          ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_DefaultSort);
  ImGui::TableSetupColumn(core::Tr("Classe"), ImGuiTableColumnFlags_WidthFixed,
                          Px(kJobColumnPixels));
  ImGui::TableSetupColumn(core::Tr("Nv"), ImGuiTableColumnFlags_WidthFixed, Px(kLevelColumnPixels));
  ImGui::TableSetupColumn(core::Tr("Carte"), ImGuiTableColumnFlags_WidthFixed,
                          Px(kMapColumnPixels));
  ImGui::TableSetupColumn(core::Tr("Guilde"), ImGuiTableColumnFlags_WidthFixed,
                          Px(kGuildColumnPixels));
  ImGui::TableSetupColumn(core::Tr("Connecté"), duration_flags, Px(kDurationColumnPixels));
  ImGui::TableSetupColumn(core::Tr("Inactif"), duration_flags, Px(kDurationColumnPixels));
  ImGui::TableSetupColumn(core::Tr("Action"), ImGuiTableColumnFlags_WidthFixed,
                          Px(kActionColumnPixels));
  ImGui::TableSetupColumn(core::Tr("Client"), ImGuiTableColumnFlags_WidthFixed,
                          Px(kClientColumnPixels));
  ImGui::TableHeadersRow();

  StaffRosterColumn column = StaffRosterColumn::kPlayer;
  bool descending = false;
  if (ImGuiTableSortSpecs* const specs = ImGui::TableGetSortSpecs(); specs != nullptr) {
    if (specs->SpecsCount > 0) {
      column = static_cast<StaffRosterColumn>(specs->Specs[0].ColumnIndex);
      descending = specs->Specs[0].SortDirection == ImGuiSortDirection_Descending;
    }
    specs->SpecsDirty = false;
  }
  state.shown_order = OrderedRows(view.rows, SearchOf(state), state.filters, column, descending);
  for (const std::size_t rank : state.shown_order) {
    DrawPlayerRow(view, rank, answer);
  }
  PaintWindowScrollbar(art);
  ImGui::EndTable();
}

// Le titre d'une carte : son nom, puis ce qui s'y trouve. Ce qui suit les trois
// dièses l'identifie, pour que son repli survive au compte qui change.
[[nodiscard]] std::string MapHeader(const StaffRosterMapGroup& group) {
  const std::string map = MapText(group.map, group.instance_id);
  const std::string counts = group.offline_shops > 0
                                 ? core::Printed(core::Tr("%d en ligne, %d hors ligne"),
                                                 group.players, group.offline_shops)
                                 : core::Printed(core::Tr("%d en ligne"), group.players);
  return map + " — " + counts + "###carte_" + map;
}

void DrawMaps(StaffRosterWindowState& state, const StaffRosterView& view, float height,
              const SkinArt& art, StaffRosterAnswer& answer) {
  if (ImGui::BeginChild("##joueurs_en_ligne_cartes", ImVec2(0.0F, height))) {
    state.shown_order = OrderedRows(view.rows, SearchOf(state), state.filters,
                                    StaffRosterColumn::kPlayer, /*descending=*/false);
    for (const StaffRosterMapGroup& group : MapGroupsOf(view.rows, state.shown_order)) {
      if (!ImGui::CollapsingHeader(MapHeader(group).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        continue;
      }
      ImGui::Indent();
      for (const std::size_t rank : group.rows) {
        const StaffRosterRow& row = view.rows[rank];
        ImGui::PushID(static_cast<int>(rank));
        DrawNameCell(view, rank, answer, ImGuiSelectableFlags_None);
        if (!row.job_name.empty()) {
          ImGui::SameLine(0.0F, Px(kTagGapPixels));
          FaintText(core::Printed("%s %d/%d", row.job_name.c_str(), row.base_level, row.job_level));
        }
        ImGui::PopID();
      }
      ImGui::Unindent();
    }
    PaintWindowScrollbar(art);
  }
  ImGui::EndChild();
}

// Ce qui se dit à la place de la liste, tant qu'il n'y en a pas. Un serveur
// muet connaît encore la commande : elle reste à un clic.
void DrawPhase(const StaffRosterView& view, float height, const SkinArt& art,
               StaffRosterAnswer& answer) {
  if (ImGui::BeginChild("##joueurs_en_ligne_etat", ImVec2(0.0F, height))) {
    switch (view.phase) {
      case StaffRosterPhase::kAsking:
        FaintText(core::Tr("Interrogation du serveur..."));
        break;
      case StaffRosterPhase::kSilent:
        ImGui::TextWrapped("%s", core::Tr("Pas de réponse du serveur."));
        ImGui::TextWrapped("%s", core::Tr("Le serveur de carte ne connaît peut-être pas encore ce "
                                          "paquet : il attend un redémarrage."));
        if (SkinButton(core::Tr("Écrire @users dans la conversation"), art)) {
          answer.say_command = true;
        }
        break;
      case StaffRosterPhase::kDenied:
        ImGui::TextWrapped("%s", core::Tr("Le serveur a refusé : ce compte n'est pas staff."));
        break;
      case StaffRosterPhase::kUnreadable:
        ImGui::TextWrapped("%s", core::Tr("Le serveur a répondu par un statut que ce client ne "
                                          "connaît pas : il est sans doute plus récent que lui."));
        break;
      case StaffRosterPhase::kReady:
        break;
    }
  }
  ImGui::EndChild();
}

void DrawSummary(const StaffRosterView& view) {
  const StaffRosterSummary summary = SummaryOf(view.rows);
  bool first = true;
  const auto count = [&first](const char* label, int value, ImU32 color) {
    if (!std::exchange(first, false)) {
      ImGui::SameLine(0.0F, Px(kSummaryGapPixels));
    }
    FaintText(label);
    ImGui::SameLine();
    ColoredText(color, std::to_string(value));
  };
  const ImU32 ink = ImGui::GetColorU32(ImGuiCol_Text);
  count(core::Tr("En ligne"), summary.players, ink);
  count(core::Tr("Échoppes hors ligne"), summary.offline_shops, ink);
  count(core::Tr("Cartes occupées"), summary.maps, ink);
  count(core::Tr("Clients en retard"), summary.outdated_clients,
        summary.outdated_clients > 0 ? kWarnColor : ink);
  count(core::Tr("À signaux"), summary.flagged, summary.flagged > 0 ? kDangerColor : ink);
}

// La recherche, puis les filtres, qui passent à la ligne quand la place manque.
void DrawFilters(StaffRosterWindowState& state, const SkinArt& art) {
  ImGui::SetNextItemWidth(Px(kSearchWidthPixels));
  ImGui::InputTextWithHint("##joueurs_en_ligne_recherche", core::Tr("Nom, guilde ou carte"),
                           state.search.data(), state.search.size());
  const auto filter = [&art](const char* label, bool* ticked) {
    ImGui::SameLine();
    if (ImGui::GetContentRegionAvail().x < SkinCheckBoxWidth(label, art)) {
      ImGui::NewLine();
    }
    static_cast<void>(SkinCheckBox(label, ticked, art));
  };
  filter(core::Tr("Staff seulement"), &state.filters.staff_only);
  filter(core::Tr("Inactifs"), &state.filters.idle_only);
  filter(core::Tr("Multi-comptes"), &state.filters.shared_only);
  filter(core::Tr("À signaux"), &state.filters.flagged_only);
  filter(core::Tr("Sans les échoppes hors ligne"), &state.filters.hide_offline_shops);
}

void DrawTabs(StaffRosterWindowState& state, const SkinArt& art) {
  if (!BeginSkinTabBar("##joueurs_en_ligne_vues", art)) {
    return;
  }
  const auto tab = [&state](const char* label, const char* id, StaffRosterTab which) {
    const std::string name = std::string(label) + "###" + id;
    if (ImGui::BeginTabItem(name.c_str())) {
      state.tab = which;
      ImGui::EndTabItem();
    }
  };
  tab(core::Tr("Par joueur"), "joueurs", StaffRosterTab::kPlayers);
  tab(core::Tr("Par carte"), "cartes", StaffRosterTab::kMaps);
  EndSkinTabBar();
}

// Le pied : redemander et la cadence à gauche, « Fermer » à droite.
void DrawFooter(StaffRosterWindowState& state, const StaffRosterView& view, const SkinArt& art,
                StaffRosterAnswer& answer) {
  const bool waiting = view.phase == StaffRosterPhase::kAsking || view.refreshing;
  ImGui::BeginDisabled(waiting || !view.can_refresh);
  if (SkinButton(
          view.phase == StaffRosterPhase::kReady ? core::Tr("Actualiser") : core::Tr("Redemander"),
          art)) {
    answer.refresh = true;
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  static_cast<void>(
      SkinCheckBox(core::Printed(core::Tr("Toutes les %u s"), view.refresh_period_s).c_str(),
                   &state.auto_refresh, art));
  ImGui::SameLine();
  if (waiting) {
    FaintText(core::Tr("Interrogation du serveur..."));
  } else if (view.phase == StaffRosterPhase::kReady) {
    FaintText(core::Printed(core::Tr("Liste reçue il y a %s"),
                            StaffRosterDurationText(view.age_s).c_str()));
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

StaffRosterAnswer DrawStaffRosterWindow(StaffRosterWindowState& state, const StaffRosterView& view,
                                        const SkinArt& art) {
  StaffRosterAnswer answer;
  if (!state.open) {
    return answer;
  }
  if (state.raise) {
    state.raise = false;
    ImGui::SetNextWindowFocus();
  }
  // Recomposé au changement de langue seulement.
  static WindowName name(kTitle, kStaffRosterWindowId);
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_FirstUseEver,
                          ImVec2(0.5F, 0.5F));
  ImGui::SetNextWindowSize(ImVec2(Px(kFirstWidthPixels), Px(kFirstHeightPixels)),
                           ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints(ImVec2(Px(kLeastWidthPixels), Px(kLeastHeightPixels)),
                                      ImVec2(FLT_MAX, FLT_MAX));
  ConstrainNextWindowToScreen(name.Get());
  constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
                                      ImGuiWindowFlags_NoScrollWithMouse;
  if (BeginSkinnedWindow(name.Get(), &state.open, art, SkinnedTitle::kBar, kFlags) && state.open) {
    const bool listed = view.phase == StaffRosterPhase::kReady;
    if (listed) {
      DrawSummary(view);
      DrawFilters(state, art);
      DrawTabs(state, art);
    }
    const float body_height =
        std::max(0.0F, ImGui::GetContentRegionAvail().y - SkinFooterHeight(art));
    if (!listed) {
      DrawPhase(view, body_height, art, answer);
    } else if (state.tab == StaffRosterTab::kMaps) {
      DrawMaps(state, view, body_height, art, answer);
    } else {
      DrawPlayers(state, view, body_height, art, answer);
    }
    DrawFooter(state, view, art, answer);
  }
  EndSkinnedWindow();
  return answer;
}

}  // namespace ro::ui
