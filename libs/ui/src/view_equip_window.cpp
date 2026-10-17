#include "ro/ui/view_equip_window.h"

#include <algorithm>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"

#include "ui_scale.h"

namespace ro::ui {
namespace {

// TAILLE FIXE : les dix emplacements tiennent sans défiler,
// et les costumes font défiler la liste dans son cadre. DEUX tailles plutôt
// qu'un redimensionnement : la fenêtre reste juste assez grande pour ce qu'elle
// montre, et cocher « Comparer » n'oblige à rien ajuster.
constexpr float kWindowWidthPixels = 620.0F;
constexpr float kComparedWidthPixels = 900.0F;
constexpr float kWindowHeightPixels = 380.0F;

// Le panneau du pantin, et la rangée du bas qu'il laisse libre.
constexpr float kDollPanePixels = 150.0F;
constexpr float kDollInsetPixels = 8.0F;
constexpr float kBottomRowPixels = 28.0F;
// Ce que les deux boutons d'orientation prennent sous le pantin, et le plus
// petit pantin qu'on dessine encore : une fenêtre réduite au minimum ne doit pas
// donner une hauteur négative.
constexpr float kDollButtonsPixels = 26.0F;
constexpr float kDollLeastPixels = 40.0F;

constexpr float kIconPixels = 20.0F;

// Les boutons alignés à droite : leur marge de libellé, et celle du bord.
constexpr float kRefreshPadPixels = 24.0F;
constexpr float kClosePadPixels = 28.0F;
constexpr float kRightEdgePixels = 16.0F;

// Le vert de « même objet de base » dans la colonne de comparaison.
constexpr ImU32 kSameBaseColor = IM_COL32(70, 120, 70, 255);

// Le nombre d'orientations d'un .ACT.
constexpr int kDirections = 8;

// Des identifiants distincts pour les deux colonnes : elles dessinent la même
// structure au même endroit de la pile d'ids, et sans cela ImGui signale un
// conflit — puis les deux cellules se partagent le même menu.
constexpr int kTheirColumnId = 0;
constexpr int kMyColumnId = 1;
constexpr int kOtherRowIdBase = 1000;
constexpr int kEmptyRowIdBase = 2000;

constexpr const char* kItemMenuId = "##equipement_vu_menu";

void FaintText(const char* text) {
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
  ImGui::TextUnformatted(text);
  ImGui::PopStyleColor();
}

// UNE CELLULE « icône + nom », gestes compris (`ro::ui::ItemLinkCell`), jeton
// compris : la pièce garde son raffinement et ses cartes. Le même rendu pour la
// pièce de la cible et pour la mienne : deux colonnes composées autrement se
// compareraient mal.
void PieceCell(ViewEquipWindowState& state, const ViewEquipPieceView& piece, int id,
               ViewEquipAnswer& answer) {
  ImGui::PushID(id);
  DrawItemLinkCell(ItemLinkCell{.item_id = piece.item_id,
                                .token = piece.token,
                                .text = piece.label,
                                .icon = piece.icon,
                                .icon_side = kIconPixels * Scale(),
                                .damaged = piece.damaged,
                                .color = ImGui::GetColorU32(ImGuiCol_Text)},
                   state.menu, answer.link);
  if (ImGui::IsItemHovered()) {
    answer.hovered_token = piece.token;
  }
  ImGui::PopID();
}

void DrawDollPanel(ViewEquipWindowState& state, const ViewEquipView& view, const SkinArt& art,
                   float width, float height) {
  const float doll_height = std::max(height - kDollButtonsPixels * Scale(), kDollLeastPixels);
  state.doll_width = width;
  state.doll_height = doll_height;

  const ImVec2 origin = ImGui::GetCursorScreenPos();
  if (view.doll.usable()) {
    ImGui::GetWindowDrawList()->AddImage(view.doll.id, origin,
                                         ImVec2(origin.x + width, origin.y + doll_height));
  }
  ImGui::Dummy(ImVec2(width, doll_height));
  // La molette sur le pantin le fait tourner, comme sur l'avatar de la fiche.
  if (ImGui::IsItemHovered()) {
    const float wheel = ImGui::GetIO().MouseWheel;
    if (wheel != 0.0F) {
      state.doll_direction =
          (state.doll_direction + (wheel > 0.0F ? 1 : kDirections - 1)) % kDirections;
    }
  }
  if (!view.doll.usable()) {
    FaintText(core::Tr("(silhouette indisponible)"));
    return;
  }
  // Deux boutons plutôt qu'un glisser : le pantin partage sa zone avec la liste,
  // et un glisser y serait attrapé par la fenêtre.
  const float button = (width - ImGui::GetStyle().ItemSpacing.x) * 0.5F;
  if (SkinButton("<###equipement_vu_gauche", art, ImVec2(button, 0.0F))) {
    state.doll_direction = (state.doll_direction + kDirections - 1) % kDirections;
  }
  ImGui::SameLine();
  if (SkinButton(">###equipement_vu_droite", art, ImVec2(button, 0.0F))) {
    state.doll_direction = (state.doll_direction + 1) % kDirections;
  }
}

void DrawRow(ViewEquipWindowState& state, const ViewEquipRowView& row, int id, bool compare,
             ViewEquipAnswer& answer) {
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  FaintText(row.place);
  ImGui::TableNextColumn();
  ImGui::PushID(id);
  if (row.theirs.has_value()) {
    PieceCell(state, *row.theirs, kTheirColumnId, answer);
  } else if (row.held_by_both_hands) {
    FaintText(core::Tr("(arme à deux mains)"));
  } else {
    FaintText(core::Tr("—"));
  }
  // Une ligne vide chez la cible ne l'est pas forcément chez moi : c'est même le
  // cas le plus parlant de la comparaison.
  if (compare) {
    ImGui::TableNextColumn();
    if (row.mine.has_value()) {
      if (row.same_base) {
        ImGui::PushStyleColor(ImGuiCol_Text, kSameBaseColor);
      }
      PieceCell(state, *row.mine, kMyColumnId, answer);
      if (row.same_base) {
        ImGui::PopStyleColor();
      }
    } else {
      FaintText(core::Tr("—"));
    }
  }
  ImGui::PopID();
}

void DrawSection(ViewEquipWindowState& state, const ViewEquipSectionView& section, int rank,
                 ViewEquipAnswer& answer) {
  if (!ImGui::CollapsingHeader(section.title, ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }
  const bool compare = state.compare && section.comparable;
  ImGui::PushID(rank);
  constexpr ImGuiTableFlags kFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;
  if (ImGui::BeginTable("###equipement_vu_table", compare ? 3 : 2, kFlags)) {
    ImGui::TableSetupColumn("###place", ImGuiTableColumnFlags_WidthFixed,
                            ImGui::CalcTextSize(core::Tr("Accessoire gauche")).x);
    ImGui::TableSetupColumn("###piece", ImGuiTableColumnFlags_WidthStretch);
    // ⛔ PAS D'EN-TÊTE « la sienne / la mienne » : une ligne de plus qui décale
    // tout sans rien apprendre — laquelle est la sienne se voit.
    if (compare) {
      ImGui::TableSetupColumn("###mienne", ImGuiTableColumnFlags_WidthStretch);
    }
    for (std::size_t line = 0; line < section.rows.size(); ++line) {
      const ViewEquipRowView& row = section.rows[line];
      const bool empty = !row.theirs.has_value();
      if (empty && !state.show_empty) {
        continue;
      }
      const int id = static_cast<int>(line) + (section.comparable ? 0 : kOtherRowIdBase) +
                     (empty ? kEmptyRowIdBase : 0);
      DrawRow(state, row, id, compare, answer);
    }
    ImGui::EndTable();
  }
  ImGui::PopID();
}

void DrawPieceList(ViewEquipWindowState& state, const ViewEquipView& view,
                   ViewEquipAnswer& answer) {
  // 🔴 L'OUVERTURE SE FAIT ICI, PAS AU CLIC, et dans le MÊME enfant que les
  // cellules : un surgissant appartient à sa fenêtre, et son identifiant se
  // hache avec la pile d'ids.
  DrawItemLinkMenu(kItemMenuId, state.menu, answer.link, view.link_facts);
  for (std::size_t rank = 0; rank < view.sections.size(); ++rank) {
    DrawSection(state, view.sections[rank], static_cast<int>(rank), answer);
  }
}

// L'en-tête : ce que le paquet dit de la personne, et rien de plus — ni niveau
// ni guilde, ils n'y sont pas.
void DrawHeader(const ViewEquipView& view, const SkinArt& art, ViewEquipAnswer& answer) {
  FaintText(view.sex);
  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
  ImGui::Text(core::Tr("· relevé il y a %d s"), view.age_seconds);
  ImGui::PopStyleColor();
  ImGui::SameLine();

  // LES OUTILS DU STAFF : trois commandes du serveur, envoyées comme des lignes
  // tapées. Le SERVEUR reste la seule autorité, et son verdict revient dans la
  // conversation.
  if (view.staff) {
    FaintText(core::Tr("Cloner :"));
    ImGui::SameLine();
    if (SkinButton(core::Tr("équipement###equipement_vu_clone"), art)) {
      answer.verb = ViewEquipVerb::kCloneEquip;
    }
    SkinTooltip(
        core::Tr("Copie SUR MOI l'équipement de ce joueur (@cloneequip). Le serveur "
                 "refuse sur soi-même."));
    ImGui::SameLine();
    if (SkinButton(core::Tr("stats###equipement_vu_stats"), art)) {
      answer.verb = ViewEquipVerb::kCloneStat;
    }
    SkinTooltip(core::Tr("Remplace MES statistiques par les siennes (@clonestat)."));
    ImGui::SameLine();
    if (SkinButton(core::Tr("job###equipement_vu_job"), art)) {
      answer.verb = ViewEquipVerb::kCloneJob;
    }
    if (ImGui::IsItemHovered()) {
      SkinTooltipWarned(core::Tr("Me donne la MÊME classe (@job).\nUn joueur monté ou déguisé "
                                 "sera refusé : sa classe affichée est une classe factice."),
                        core::Tr("Sur moonlight cette commande enchaîne aussi @blvl 999, @jlvl "
                                 "100, @allskills et @allstats."));
    }
    ImGui::SameLine();
  }

  // Grisé plutôt que masqué quand on ne sait pas à qui redemander : un bouton
  // qui disparaît n'explique rien.
  const float scale = Scale();
  const float refresh = ImGui::CalcTextSize(core::Tr("Actualiser")).x + kRefreshPadPixels * scale;
  ImGui::SetCursorPosX(ImGui::GetWindowWidth() - refresh - kRightEdgePixels * scale);
  ImGui::BeginDisabled(!view.can_refresh);
  if (SkinButton(core::Tr("Actualiser###equipement_vu_actualiser"), art, ImVec2(refresh, 0.0F))) {
    answer.verb = ViewEquipVerb::kRefresh;
  }
  ImGui::EndDisabled();
  if (!view.can_refresh && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    SkinTooltipFormat("%s", core::Tr("Cette fiche n'a pas été demandée depuis le menu "
                                     "contextuel : le paquet ne porte pas l'identifiant du "
                                     "joueur."));
  }
}

void DrawFooter(ViewEquipWindowState& state, const ViewEquipView& view, const SkinArt& art) {
  static_cast<void>(
      SkinCheckBox(core::Tr("Emplacements vides###equipement_vu_vides"), &state.show_empty, art));
  ImGui::SameLine();
  static_cast<void>(
      SkinCheckBox(core::Tr("Comparer###equipement_vu_comparer"), &state.compare, art));
  SkinTooltip(
      core::Tr("Affiche en regard ce que JE porte au même emplacement. La fenêtre "
               "s'élargit."));
  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
  ImGui::Text(core::Tr("· %d pièce(s)"), view.piece_count);
  ImGui::PopStyleColor();
  // UN VRAI BOUTON « Fermer », en plus de la croix : celle du skin est petite,
  // et cette fenêtre se ferme souvent. Le même état que la croix.
  const float scale = Scale();
  const float close = ImGui::CalcTextSize(core::Tr("Fermer")).x + kClosePadPixels * scale;
  ImGui::SameLine();
  ImGui::SetCursorPosX(ImGui::GetWindowWidth() - close - kRightEdgePixels * scale);
  if (SkinButton(core::Tr("Fermer"), art, ImVec2(close, 0.0F))) {
    state.open = false;
  }
}

}  // namespace

ViewEquipAnswer DrawViewEquipWindow(ViewEquipWindowState& state, const ViewEquipView& view,
                                    const SkinArt& art) {
  ViewEquipAnswer answer;
  if (!state.open) {
    return answer;
  }
  if (state.raise) {
    ImGui::SetNextWindowFocus();
    state.raise = false;
  }
  // TAILLE FIXE, À CHAQUE IMAGE : sans `Always`, une taille rangée dans le
  // fichier d'une version précédente survivrait à l'interdiction de
  // redimensionner, et la fenêtre rouvrirait à l'ancienne taille.
  const float scale = Scale();
  ImGui::SetNextWindowSize(
      ImVec2((state.compare ? kComparedWidthPixels : kWindowWidthPixels) * scale,
             kWindowHeightPixels * scale),
      ImGuiCond_Always);
  if (BeginSkinnedWindow(view.title.c_str(), &state.open, art, SkinnedTitle::kBar,
                         ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
    DrawHeader(view, art, answer);
    ImGui::Separator();

    const float doll_pane = kDollPanePixels * scale;
    const float room = ImGui::GetContentRegionAvail().y - kBottomRowPixels * scale;
    ImGui::BeginChild("###equipement_vu_pantin", ImVec2(doll_pane, room), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar);
    DrawDollPanel(state, view, art, doll_pane - kDollInsetPixels * scale, room);
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("###equipement_vu_liste", ImVec2(0.0F, room));
    DrawPieceList(state, view, answer);
    ImGui::EndChild();

    DrawFooter(state, view, art);
  }
  EndSkinnedWindow();
  return answer;
}

}  // namespace ro::ui
