#include "ro/ui/weapon_refine_window.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include <imgui.h>

#include "ro/core/ascii.h"
#include "ro/ui/modal_box.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/window_name.h"

#include "craft_window_parts.h"

namespace ro::ui {
namespace {

using craft_parts::BulletWrapped;
using craft_parts::ColoredText;
using craft_parts::ColoredWrapped;
using craft_parts::Px;

// ── LES MESURES DE LA FENÊTRE, en pixels d'interface ────────────────────────
constexpr float kRefineButtonPixels = 100.0F;
constexpr float kRecastButtonPixels = 150.0F;
constexpr float kCloseButtonPixels = 90.0F;
constexpr float kConfirmButtonPixels = 110.0F;
constexpr float kConfirmWrapPixels = 300.0F;
constexpr float kPlusColumnPixels = 26.0F;
constexpr float kSlotsColumnPixels = 38.0F;
// La colonne des cartes réserve la flèche de tri, qui mesure environ deux tiers
// de la police.
constexpr float kSortArrowEm = 0.65F;
// La liste montre quatre armes.
constexpr int kVisibleRows = 4;
// Le journal montre cinq lignes.
constexpr float kHistoryLines = 5.0F;
// La chance est verte au-delà de ceci, ambre au-delà de cela, rouge en dessous.
constexpr int kSafeChance = 90;
constexpr int kRiskyChance = 50;

constexpr std::size_t kLabelBytes = 192;

[[nodiscard]] ImU32 ChanceColor(int chance) {
  if (chance >= kSafeChance) {
    return PaintingLook().good_text;
  }
  return chance >= kRiskyChance ? PaintingLook().warn_text : PaintingLook().bad_text;
}

// L'ordre des lignes : celui du serveur, ou celui de la colonne choisie.
[[nodiscard]] std::vector<const WeaponRefineRowView*> SortedRows(const WeaponRefineView& view,
                                                                 const ImGuiTableSortSpecs* specs,
                                                                 int plus_column,
                                                                 int slots_column) {
  std::vector<const WeaponRefineRowView*> rows;
  rows.reserve(view.rows.size());
  for (const WeaponRefineRowView& row : view.rows) {
    rows.push_back(&row);
  }
  if (specs == nullptr || specs->SpecsCount == 0) {
    return rows;
  }
  const ImGuiTableColumnSortSpecs& spec = specs->Specs[0];
  const bool ascending = spec.SortDirection == ImGuiSortDirection_Ascending;
  const int column = spec.ColumnIndex;
  std::ranges::sort(rows, [&](const WeaponRefineRowView* left, const WeaponRefineRowView* right) {
    int order = 0;
    if (column == plus_column) {
      order = left->refine - right->refine;
    } else if (column == slots_column) {
      order = left->slots != right->slots ? left->slots - right->slots : left->cards - right->cards;
    } else {
      order = left->bare_name.compare(right->bare_name);
    }
    if (order == 0) {
      order = left->index - right->index;
    }
    return ascending ? order < 0 : order > 0;
  });
  return rows;
}

[[nodiscard]] bool Shown(const WeaponRefineWindowState& state, const WeaponRefineRowView& row) {
  return core::AsciiContainsFolded(row.full_name, state.filter.data());
}

void DrawList(WeaponRefineWindowState& state, const WeaponRefineView& view,
              WeaponRefineWindowAction& action) {
  if (view.show_filter) {
    ImGui::SetNextItemWidth(-1.0F);
    ImGui::InputTextWithHint("##refine_filter", core::Tr("Filtrer…"), state.filter.data(),
                             state.filter.size());
  } else {
    // UN FILTRE INVISIBLE NE MASQUE RIEN : on le vide avec son champ.
    state.filter[0] = '\0';
  }

  const ImGuiStyle& style = ImGui::GetStyle();
  const float icon = Px(craft_parts::kRowIconPixels);
  const float row_height = icon + style.CellPadding.y * 2.0F;
  const float head_height = ImGui::GetTextLineHeight() + style.CellPadding.y * 2.0F;
  const int column_count = view.show_cards ? 4 : 3;
  const ImGuiTableFlags flags = ImGuiTableFlags_Sortable | ImGuiTableFlags_SortTristate |
                                ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
                                ImGuiTableFlags_SizingStretchProp;
  if (!ImGui::BeginTable("refine_items", column_count, flags,
                         ImVec2(0.0F, head_height + row_height * kVisibleRows))) {
    return;
  }
  ImGui::TableSetupScrollFreeze(0, 1);
  int column = 0;
  ImGui::TableSetupColumn("##icon",
                          ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort |
                              ImGuiTableColumnFlags_NoHeaderLabel,
                          icon + Px(2.0F));
  ++column;
  const int plus_column = column++;
  ImGui::TableSetupColumn(
      "+", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending,
      Px(kPlusColumnPixels));
  ImGui::TableSetupColumn(core::Tr("Arme"), ImGuiTableColumnFlags_WidthStretch);
  ++column;
  int slots_column = -1;
  if (view.show_cards) {
    slots_column = column++;
    const char* slots_label = core::Tr("Slots");
    const float wanted = ImGui::CalcTextSize(slots_label).x +
                         std::floor(ImGui::GetFontSize() * kSortArrowEm + style.FramePadding.x) +
                         style.CellPadding.x * 2.0F;
    ImGui::TableSetupColumn(
        slots_label, ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending,
        std::max(Px(kSlotsColumnPixels), wanted));
  }
  ImGui::TableHeadersRow();

  ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs();
  bool sort_changed = false;
  if (specs != nullptr && specs->SpecsDirty) {
    specs->SpecsDirty = false;
    sort_changed = true;
  }
  std::vector<const WeaponRefineRowView*> rows = SortedRows(view, specs, plus_column, slots_column);
  std::erase_if(rows, [&](const WeaponRefineRowView* row) { return !Shown(state, *row); });
  // UN TRI NEUF CHOISIT LA PREMIÈRE LIGNE, et remonte en haut.
  if (sort_changed && !rows.empty()) {
    state.selected = rows.front()->index;
    ImGui::SetScrollY(0.0F);
  }
  action.first_visible =
      rows.empty() ? WeaponRefineWindowAction::kNoRefineRow : rows.front()->index;
  action.selection_visible = std::ranges::any_of(
      rows, [&](const WeaponRefineRowView* row) { return row->index == state.selected; });

  ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0F, 0.5F));
  for (const WeaponRefineRowView* row : rows) {
    ImGui::PushID(row->index);
    ImGui::TableNextRow(ImGuiTableRowFlags_None, row_height);
    ImGui::TableNextColumn();
    if (row->icon.usable()) {
      ImGui::Image(row->icon.id, ImVec2(icon, icon));
    } else {
      ImGui::Dummy(ImVec2(icon, icon));
    }
    ImGui::TableNextColumn();
    if (row->refine > 0) {
      ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (icon - ImGui::GetTextLineHeight()) * 0.5F);
      std::array<char, kLabelBytes> plus{};
      std::snprintf(plus.data(), plus.size(), "+%d", row->refine);
      ColoredText(row->at_cap ? PaintingLook().warn_text : PaintingLook().info_text, plus.data());
    }
    ImGui::TableNextColumn();
    const std::string name(row->bare_name);
    if (ImGui::Selectable(
            name.c_str(), state.selected == row->index,
            ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick,
            ImVec2(0.0F, icon))) {
      state.selected = row->index;
      if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        action.verb = WeaponRefineVerb::kRefine;
        action.index = row->index;
      }
    }
    if (ImGui::IsItemHovered()) {
      action.hovered_slot = row->index;
      action.hovered_id = row->item_id;
      if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        state.selected = row->index;
        action.describe_slot = row->index;
      }
    }
    if (view.show_cards) {
      ImGui::TableNextColumn();
      if (row->slots > 0) {
        std::array<char, kLabelBytes> slots{};
        std::snprintf(slots.data(), slots.size(), "%d/%d", row->cards, row->slots);
        const float width = ImGui::CalcTextSize(slots.data()).x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                             std::max(0.0F, (ImGui::GetContentRegionAvail().x - width) * 0.5F));
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (icon - ImGui::GetTextLineHeight()) * 0.5F);
        ColoredText(PaintingLook().faint_text, slots.data());
      }
    }
    ImGui::PopID();
  }
  ImGui::PopStyleVar();
  ImGui::EndTable();
  action.list_drawn = true;
}

// LES TROIS MINERAIS, en liens qui reviennent à la ligne s'ils ne tiennent pas.
void DrawOreLinks(const WeaponRefineView& view, WeaponRefineWindowAction& action) {
  ImGui::SeparatorText(core::Tr("Minerais"));
  const float right_edge = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
  const float inner = ImGui::GetStyle().ItemInnerSpacing.x;
  bool first = true;
  for (std::size_t rank = 0; rank < view.ores.size(); ++rank) {
    const RefineOreView& ore = view.ores[rank];
    std::array<char, kLabelBytes> label{};
    if (ore.name.empty()) {
      std::snprintf(label.data(), label.size(), "#%u %d", static_cast<unsigned>(ore.item_id),
                    ore.owned);
    } else {
      std::snprintf(label.data(), label.size(), "%.*s %d", static_cast<int>(ore.name.size()),
                    ore.name.data(), ore.owned);
    }
    const float width =
        Px(craft_parts::kLinkIconPixels) + inner + ImGui::CalcTextSize(label.data()).x;
    if (!first) {
      ImGui::SameLine();
      if (ImGui::GetCursorScreenPos().x + width > right_edge) {
        ImGui::NewLine();
      }
    }
    first = false;
    bool hovered = false;
    std::array<char, kLabelBytes> id{};
    std::snprintf(id.data(), id.size(), "ore%zu", rank);
    if (craft_parts::ItemLink(id.data(), ore.icon, craft_parts::kLinkIconPixels, label.data(),
                              ore.owned > 0 ? PaintingLook().good_text : PaintingLook().bad_text,
                              &hovered)) {
      action.describe_item_id = ore.item_id;
    }
    if (hovered && view.hover_preview) {
      action.hovered_id = ore.item_id;
      action.hovered_slot = 0;
    }
  }
}

void DrawSessionOver() {
  ColoredWrapped(PaintingLook().faint_text,
                 core::Tr("Session terminée : le serveur n'autorise qu'un refine par "
                          "lancement du skill."));
}

void DrawFooter(WeaponRefineWindowState& state, const WeaponRefineView& view, const SkinArt& art,
                WeaponRefineWindowAction& action, bool selection_visible) {
  DrawOreLinks(view, action);

  // ── LE PLAFOND, et la chance sur la même ligne ─────────────────────────────
  if (view.cap > 0) {
    std::array<char, kLabelBytes> cap{};
    std::snprintf(cap.data(), cap.size(), core::Tr("Plafond : +%d"), view.cap);
    ColoredText(PaintingLook().faint_text, cap.data());
    ImGui::SameLine();
    SkinHelpMarker(
        core::Tr("Le niveau appris du skill Upgrade Weapon EST le plafond : le "
                 "serveur refuse toute arme déjà à ce refine (et jamais au-delà de "
                 "+10)."));
  }
  if (view.chance >= 0) {
    ImGui::SameLine();
    std::array<char, kLabelBytes> chance{};
    std::snprintf(chance.data(), chance.size(), core::Tr("· Chances : %d %%"), view.chance);
    ColoredText(ChanceColor(view.chance), chance.data());
    ImGui::SameLine();
    static constexpr std::array<const char*, 3> kChanceHelp = {
        core::TrText("Le serveur calcule per = Rate/100 + (job_level - 50) / 2, puis réussit si "
                     "per > rnd()%100. Comme le tirage est uniforme sur 0..99, la probabilité "
                     "vaut exactement per."),
        core::TrText("Le Rate de base vient de la table de refine du serveur (niveau d'arme × "
                     "refine visé)."),
        core::TrText("Un job level inférieur à 50 donne un bonus NÉGATIF : ce n'est pas une "
                     "erreur, le serveur fait bien cela."),
    };
    SkinHelpMarker(core::Tr("Probabilité EXACTE de cette tentative, pas une estimation."),
                   kChanceHelp);
  }

  if (view.awaiting) {
    ColoredWrapped(PaintingLook().warn_text,
                   core::Tr("Tentative envoyée — en attente du serveur…"));
    ImGui::Spacing();
  }

  // ── L'ÉTAT DE LA CHAÎNE, un seul message ───────────────────────────────────
  std::array<char, kLabelBytes> chain{};
  switch (view.chain_note) {
    case RefineChainNote::kRefineSoon:
      std::snprintf(chain.data(), chain.size(), core::Tr("Refine automatique imminent… (n° %d)"),
                    view.chain_count);
      ColoredWrapped(PaintingLook().bad_text, chain.data());
      ImGui::Spacing();
      break;
    case RefineChainNote::kChaining:
      std::snprintf(chain.data(), chain.size(), core::Tr("Chaîne automatique… (%d)"),
                    view.chain_count);
      ColoredWrapped(PaintingLook().info_text, chain.data());
      ImGui::Spacing();
      break;
    case RefineChainNote::kRecasting:
      std::snprintf(chain.data(), chain.size(), core::Tr("Relance automatique… (%d)"),
                    view.chain_count);
      ColoredWrapped(PaintingLook().info_text, chain.data());
      ImGui::Spacing();
      break;
    case RefineChainNote::kPaused:
      ColoredWrapped(PaintingLook().warn_text,
                     core::Tr("Chaîne arrêtée. « Chaîne auto » la relance, après confirmation."));
      ImGui::Spacing();
      break;
    case RefineChainNote::kReady:
      ColoredWrapped(PaintingLook().warn_text,
                     core::Tr("Refine automatique coché : « Chaîne auto » le lance pour cette "
                              "fenêtre, après confirmation."));
      ImGui::Spacing();
      break;
    case RefineChainNote::kStopped: {
      const std::string reason(view.stop_reason);
      ColoredWrapped(PaintingLook().warn_text, reason.c_str());
      ImGui::Spacing();
      break;
    }
    case RefineChainNote::kNone:
      break;
  }

  if (!view.last_result.empty() && !view.awaiting) {
    const std::string result(view.last_result);
    ColoredWrapped(PaintingLook().Tone(view.last_result_tone), result.c_str());
    ImGui::Spacing();
  }
  if (view.consumed && !view.awaiting && !view.relaunch_coming) {
    DrawSessionOver();
    ImGui::Spacing();
  }

  // ── LES BOUTONS ─────────────────────────────────────────────────────────────
  if (!view.rows.empty()) {
    const bool busy = view.awaiting || view.consumed;
    if (!selection_visible && !busy) {
      ColoredWrapped(PaintingLook().faint_text, core::Tr("Sélectionne une arme dans la liste."));
      ImGui::Spacing();
    }
    ImGui::BeginDisabled(!selection_visible || busy);
    if (SkinButton(core::Tr("Refine"), art, ImVec2(Px(kRefineButtonPixels), 0.0F))) {
      action.verb = WeaponRefineVerb::kRefine;
      action.index = state.selected;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
  }
  // LA BASCULE DE LA CHAÎNE, au milieu : « Arrêter » tant qu'elle est armée et
  // qu'il lui reste de quoi faire, « Chaîne auto » sur une liste vivante.
  // Sinon la place revient à la relance, sans laquelle rien ne repart.
  const bool live_list = !view.rows.empty() && !view.consumed;
  if (view.chain_armed && (view.chain_running || live_list)) {
    if (SkinButton(core::Tr("Arrêter"), art, ImVec2(Px(kRecastButtonPixels), 0.0F))) {
      action.verb = WeaponRefineVerb::kStop;
    }
    if (ImGui::IsItemHovered()) {
      SkinTooltip(
          core::Tr("Arrête le refine automatique tout de suite, et décoche le réglage. Une "
                   "tentative DÉJÀ envoyée ira à son terme : le serveur a l'arme, elle ne se "
                   "reprend pas. Rien ne repartira ensuite."));
    }
    ImGui::SameLine();
  } else if (live_list) {
    ImGui::BeginDisabled(view.awaiting);
    if (SkinButton(core::Tr("Chaîne auto"), art, ImVec2(Px(kRecastButtonPixels), 0.0F))) {
      state.chain_confirm = true;
      state.chain_confirm_opened = true;
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      SkinTooltip(
          core::Tr("Refine TOUT SEUL, arme après arme, jusqu'à épuisement du SP, du minerai "
                   "ou de la liste — sans confirmation à chaque tentative. Demande une "
                   "confirmation, et ne vaut que pour cette fenêtre : rouverte, elle redemande."));
    }
    ImGui::SameLine();
  } else if (!view.relaunch_coming) {
    ImGui::BeginDisabled(view.awaiting);
    if (SkinButton(core::Tr("Relancer le skill"), art, ImVec2(Px(kRecastButtonPixels), 0.0F))) {
      action.verb = WeaponRefineVerb::kRecast;
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      SkinTooltip(
          core::Tr("Relance le skill Upgrade Weapon pour obtenir une nouvelle liste. "
                   "Le serveur n'autorise QU'UNE tentative par lancement : après chaque "
                   "refine il faut relancer, et c'est ce que fait ce bouton — sans "
                   "repasser par la barre d'action."));
    }
    ImGui::SameLine();
  }
  if (SkinButton(core::Tr("Fermer"), art, ImVec2(Px(kCloseButtonPixels), 0.0F))) {
    action.verb = WeaponRefineVerb::kClose;
  }
}

// LA CONFIRMATION : l'arme, le risque, et ce que la réussite donnerait.
void DrawConfirm(WeaponRefineWindowState& state, const WeaponRefineView& view, const SkinArt& art,
                 WeaponRefineWindowAction& action) {
  const std::string title = std::string(core::Tr("Confirmer le refine")) + "###refine_confirm";
  if (state.confirm_opened) {
    AskModal(title.c_str());
    state.confirm_opened = false;
    state.confirm_frame = ImGui::GetFrameCount();
  }
  if (state.confirm_index == WeaponRefineWindowAction::kNoRefineRow) {
    return;
  }
  const ImVec2 screen = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(screen, ImGuiCond_Appearing, ImVec2(0.5F, 0.5F));
  if (!BeginSkinnedModal(title.c_str(), art)) {
    // Refermée par Échap : c'est un « non ».
    action.verb = WeaponRefineVerb::kDismissConfirm;
    return;
  }
  ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + Px(kConfirmWrapPixels));
  const std::string name = view.confirm_name.empty() ? std::string(core::Tr("(arme inconnue)"))
                                                     : std::string(view.confirm_name);
  ImGui::TextWrapped("%s", name.c_str());
  ImGui::Spacing();
  ColoredText(PaintingLook().bad_text, core::Tr("Un échec DÉTRUIT l'arme."));
  ImGui::TextWrapped(core::Tr("Le minerai est consommé dans tous les cas. En cas de réussite "
                              "l'arme passe de +%d à +%d."),
                     view.confirm_refine, view.confirm_refine + 1);
  ImGui::Spacing();
  ImGui::PopTextWrapPos();
  // ⚠ LA FRAPPE QUI A OUVERT LA MODALE NE LA VALIDE PAS : Entrée ne compte qu'à
  // partir de l'image suivante.
  const bool entered = ImGui::GetFrameCount() > state.confirm_frame &&
                       (ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
                        ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false));
  if (SkinButton(core::Tr("Refine"), art, ImVec2(Px(kConfirmButtonPixels), 0.0F)) || entered) {
    action.verb = WeaponRefineVerb::kConfirm;
    ImGui::CloseCurrentPopup();
  }
  ImGui::SameLine();
  if (SkinButton(core::Tr("Annuler"), art, ImVec2(Px(kConfirmButtonPixels), 0.0F))) {
    action.verb = WeaponRefineVerb::kDismissConfirm;
    ImGui::CloseCurrentPopup();
  }
  EndSkinnedModal();
}

// LA CONFIRMATION DE LA CHAÎNE : ce qu'elle fera, et ce qu'elle peut détruire.
// ⚠ ENTRÉE NE LA VALIDE PAS : elle lance des tentatives sans confirmation, et
// une touche tenue pour enchaîner les refines ne doit pas l'accepter au passage.
void DrawChainConfirm(WeaponRefineWindowState& state, const SkinArt& art,
                      WeaponRefineWindowAction& action) {
  const std::string title = std::string(core::Tr("Refine automatique")) + "###refine_chain_confirm";
  if (state.chain_confirm_opened) {
    AskModal(title.c_str());
    state.chain_confirm_opened = false;
  }
  if (!state.chain_confirm) {
    return;
  }
  const ImVec2 screen = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(screen, ImGuiCond_Appearing, ImVec2(0.5F, 0.5F));
  if (!BeginSkinnedModal(title.c_str(), art)) {
    // Refermée par Échap : c'est un « non ».
    state.chain_confirm = false;
    return;
  }
  ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + Px(kConfirmWrapPixels));
  ColoredWrapped(PaintingLook().bad_text,
                 core::Tr("Chaque tentative peut DÉTRUIRE l'arme, et elles partent sans "
                          "confirmation. La chaîne joue les armes de la liste jusqu'à épuisement "
                          "du SP."));
  ImGui::Spacing();
  ImGui::TextWrapped("%s", core::Tr("Elle s'arrête seule : plus de SP, plus de minerai, plus "
                                    "d'arme dans la liste, ou refus du serveur. « Arrêter » "
                                    "l'interrompt à tout moment."));
  ImGui::Spacing();
  ImGui::PopTextWrapPos();
  if (SkinButton(core::Tr("Lancer"), art, ImVec2(Px(kConfirmButtonPixels), 0.0F))) {
    action.verb = WeaponRefineVerb::kStartChain;
    state.chain_confirm = false;
    ImGui::CloseCurrentPopup();
  }
  ImGui::SameLine();
  if (SkinButton(core::Tr("Annuler"), art, ImVec2(Px(kConfirmButtonPixels), 0.0F))) {
    state.chain_confirm = false;
    ImGui::CloseCurrentPopup();
  }
  EndSkinnedModal();
}

}  // namespace

WeaponRefineWindowAction DrawWeaponRefineWindow(WeaponRefineWindowState& state,
                                                const WeaponRefineView& view, const SkinArt& art) {
  WeaponRefineWindowAction action;
  if (!state.open) {
    return action;
  }
  const ImGuiStyle& style = ImGui::GetStyle();
  // LA LARGEUR EST CELLE DE LA RANGÉE DE BOUTONS, la hauteur suit le contenu.
  const float width = style.WindowPadding.x * 2.0F + Px(kRefineButtonPixels) +
                      Px(kRecastButtonPixels) + Px(kCloseButtonPixels) + style.ItemSpacing.x * 2.0F;
  ImGui::SetNextWindowSize(ImVec2(width, 0.0F), ImGuiCond_Always);
  if (state.raise) {
    ImGui::SetNextWindowFocus();
    state.raise = false;
  }
  SetNextWindowBulletClickable(core::Tr("Options du refine"));
  static WindowName name(kWeaponRefineWindowTitle, kWeaponRefineWindowId);
  bool open = true;
  const bool begun =
      BeginSkinnedWindow(name.Get(), &open, art, SkinnedTitle::kBar,
                         ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  action.open_settings = TitleBulletClicked();
  if (begun) {
    if (view.empty_list) {
      ColoredText(PaintingLook().warn_text, core::Tr("Aucune arme refinable."));
      ImGui::Spacing();
      ImGui::TextWrapped("%s", core::Tr("Le serveur ne propose une arme que si TOUT est vrai :"));
      BulletWrapped(core::Tr("elle est identifiée et a un niveau d'arme (1 à 4) ;"));
      BulletWrapped(core::Tr("elle n'est PAS portée (déséquipe-la d'abord) ;"));
      BulletWrapped(core::Tr("son refine est encore sous le plafond de ton skill ;"));
      BulletWrapped(core::Tr("tu as le minerai correspondant à son niveau d'arme."));
      ImGui::Spacing();
    } else if (view.rows.empty()) {
      if (!view.awaiting && view.consumed && !view.relaunch_coming) {
        DrawSessionOver();
      }
      ImGui::Spacing();
    } else {
      if (view.consumed) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, style.Alpha * craft_parts::kStaleAlpha);
      }
      DrawList(state, view, action);
      if (view.consumed) {
        ImGui::PopStyleVar();
      }
    }
    DrawFooter(state, view, art, action, action.selection_visible);
    if (view.show_history && !view.history.empty()) {
      craft_parts::DrawHistory("##refine_history", view.history,
                               ImGui::GetTextLineHeightWithSpacing() * kHistoryLines, view.log_time,
                               true);
    }
    // ENTRÉE AFFINE, si le réglage le veut et que la fenêtre a le clavier. ⚠ PAS
    // PENDANT UNE SAISIE : Entrée appartient alors au filtre.
    if (view.enter_key && action.selection_visible && !ImGui::IsAnyItemActive() &&
        ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) &&
        action.verb == WeaponRefineVerb::kNothing) {
      action.verb = WeaponRefineVerb::kRefine;
      action.index = state.selected;
    }
    DrawConfirm(state, view, art, action);
    DrawChainConfirm(state, art, action);
  }
  EndSkinnedWindow();
  // LA CROIX : même effet que « Fermer ».
  if (!open) {
    action.verb = WeaponRefineVerb::kClose;
  }
  return action;
}

}  // namespace ro::ui
