#include "skill_cell.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>

#include <imgui.h>

#include "ro/core/decimal_text.h"
#include "ro/core/i18n.h"
#include "ro/ui/shortcut_bar.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/staff_skill_id.h"
#include "ro/ui/wheel_gate.h"

namespace ro::ui {
namespace {

// La teinte d'un rang RÉSERVÉ, celle de l'en-tête du grimoire.
constexpr ImVec4 kPointsReservedColor(0.85F, 0.65F, 0.20F, 1.0F);

// Le temps de recharge s'écrit en secondes ; le serveur le compte en millièmes.
constexpr std::uint32_t kMillisecondPerSecond = 1000;

// ── LA PASTILLE DU RANG DE LANCEMENT ────────────────────────────────────────
constexpr float kBadgePadX = 4.0F;
constexpr float kBadgePadY = 2.0F;
constexpr float kBadgeRounding = 4.0F;
constexpr ImU32 kBadgeBody = IM_COL32(15, 15, 20, 205);
constexpr ImU32 kBadgeEdge = IM_COL32(255, 205, 105, 220);
constexpr ImU32 kBadgeText = IM_COL32(255, 215, 130, 255);
constexpr float kBadgeEdgeThickness = 1.0F;

// LE RANG AUQUEL LA COMPÉTENCE PART. ⚠ ZÉRO VEUT DIRE « CELUI QU'ON A APPRIS » :
// c'est le défaut, et il n'a pas à s'écrire pour chaque case du grimoire.
[[nodiscard]] int EffectiveUseLevel(const SkillCellView& cell) {
  return cell.use_level > 0 ? cell.use_level : cell.learned;
}

// La compétence se DOSE-t-elle vraiment ? ⚠ IL FAUT LES DEUX : un effet qui
// dépend du rang, ET plus d'un rang appris. Ailleurs, la molette garde son rôle
// de défilement.
[[nodiscard]] bool LevelTunable(const SkillCellView& cell) {
  return cell.level_matters && cell.learned > 1;
}

// LA MOLETTE SUR CETTE CASE, ou zéro.
//
// 🔴 PARCOURIR LE GRIMOIRE NE DOIT RIEN RÉGLER, et c'est le portail partagé qui
// s'en charge : ses trois gardes valent pour toute zone qu'on règle à la
// molette, et les avoir en double les ferait diverger. Voir `ro/ui/wheel_gate.h`.
//
// La case s'identifie par son item ImGui, qui est aussi ce à quoi la possession
// de la molette se revendique.
[[nodiscard]] float CellWheel(SkillWindowState& state, bool hovered, bool engaged) {
  return WheelOver(state.wheel, ImGui::GetItemID(), hovered, engaged);
}

// LE MENU D'UNE CASE : ce qu'on peut faire d'une compétence, écrit noir sur
// blanc.
//
// 🔴 LE CLIENT N'EN A PAS, et c'est justement ce qui lui manque : ses gestes
// sont tous invisibles — clic droit pour monter, clic gauche pour décrire, et
// rien pour lancer depuis l'arbre. Un menu ne remplace aucun raccourci, il les
// ÉNONCE.
//
// ⚠ LES ENTRÉES IMPOSSIBLES SE GRISENT, elles ne disparaissent pas : une liste
// dont les lignes vont et viennent se relit à chaque ouverture.
void DrawCellMenu(const SkillCellView& cell, SkillWindowState& state, const SkinArt& art,
                  SkillWindowAction& action) {
  if (state.menu_skill != cell.skill_id) {
    return;
  }
  if (!BeginSkinPopup("case")) {
    return;
  }
  ImGui::TextDisabled("%.*s", static_cast<int>(cell.name.size()), cell.name.data());
  ImGui::Separator();

  ImGui::BeginDisabled(!cell.raisable);
  if (ImGui::MenuItem(core::Tr("Monter d'un rang"))) {
    action.reserve = cell.skill_id;
  }
  ImGui::EndDisabled();

  ImGui::BeginDisabled(!cell.castable);
  if (ImGui::MenuItem(core::Tr("Lancer"))) {
    action.cast = cell.skill_id;
  }
  ImGui::EndDisabled();

  // ── LE RANG AUQUEL ON LA LANCE ────────────────────────────────────────────
  //
  // 🔴 SEULEMENT POUR CELLES DONT L'EFFET EN DÉPEND : le proposer ailleurs
  // offrirait un réglage sans effet.
  //
  // ⚠ MAIS DÈS LE PREMIER RANG APPRIS, et non à partir de deux comme la
  // molette : la section MONTRE le rang autant qu'elle le règle, et sa
  // disparition sur une compétence de rang un se lirait « ce réglage n'existe
  // pas pour elle ». Les deux pas se grisent d'eux-mêmes quand il n'y a rien à
  // choisir.
  //
  // ⚠ CE N'EST PAS UNE ENTRÉE DE MENU : une entrée se CLIQUE et referme. Les
  // deux pas se pressent plusieurs fois de suite, et le menu doit rester
  // ouvert.
  if (cell.level_matters && cell.learned > 0) {
    ImGui::Separator();
    ImGui::Text(core::Tr("Lancer au niveau %d / %d"), EffectiveUseLevel(cell), cell.learned);
    const float step_side = SkinSmallButtonHeight(art);
    const ImVec2 step_size(step_side, step_side);
    const int shown = cell.use_level > 0 ? cell.use_level : cell.learned;
    ImGui::BeginDisabled(shown <= 1);
    if (SkinSmallButton("-", art, step_size)) {
      action.use_level_for = cell.skill_id;
      action.use_level = shown - 1;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(shown >= cell.learned);
    if (SkinSmallButton("+", art, step_size)) {
      action.use_level_for = cell.skill_id;
      action.use_level = shown + 1;
    }
    ImGui::EndDisabled();
  }

  ImGui::Separator();
  if (ImGui::MenuItem(core::Tr("Description"))) {
    action.describe = cell.skill_id;
  }
  EndSkinPopup();
}

}  // namespace

void DrawUseLevelBadge(ImDrawList* into, const SkillCellView& cell, ImVec2 icon_at, float side,
                       bool always) {
  if (cell.learned <= 0 || !cell.level_matters) {
    return;
  }
  const int use = EffectiveUseLevel(cell);
  if (use >= cell.learned && !always) {
    return;
  }
  std::array<char, ro::core::kDecimalTextBytes<int>> text{};
  std::snprintf(text.data(), text.size(), "%d", use);
  const ImVec2 size = ImGui::CalcTextSize(text.data());
  const ImVec2 at(icon_at.x + (side - size.x) * 0.5F, icon_at.y + (side - size.y) * 0.5F);
  const ImVec2 corner_min(at.x - kBadgePadX, at.y - kBadgePadY);
  const ImVec2 corner_max(at.x + size.x + kBadgePadX, at.y + size.y + kBadgePadY);
  into->AddRectFilled(corner_min, corner_max, kBadgeBody, kBadgeRounding);
  into->AddRect(corner_min, corner_max, kBadgeEdge, kBadgeRounding, 0, kBadgeEdgeThickness);
  into->AddText(at, kBadgeText, text.data());
}

void ShowSkillTooltip(const SkillCellView& cell, std::span<const SkillCellView> siblings,
                      bool staff_allowed) {
  if (BeginSkinTooltip()) {
    ImGui::TextUnformatted(cell.name.data(), cell.name.data() + cell.name.size());
    if (cell.max_level > 0) {
      ImGui::Text(core::Tr("Niveau %d / %d"), cell.effective_level(), cell.max_level);
    }
    if (cell.reserved > cell.learned) {
      ImGui::TextColored(kPointsReservedColor, core::Tr("%d rang(s) réservé(s)"),
                         cell.reserved - cell.learned);
    }
    // 🔴 PASSIVE OU ACTIVE, ET TOUJOURS DIT. C'est la première question qu'on se
    // pose devant une case inconnue — « est-ce que ça se lance ? » —, et le dessin
    // ne la distingue en rien.
    ImGui::TextDisabled("%s",
                        cell.passive ? core::Tr("Passive (toujours active)") : core::Tr("Active"));
    // ⚠ LE COÛT ET LA PORTÉE NE VALENT QUE POUR CE QU'ON A APPRIS : ils viennent du
    // SERVEUR, au rang appris, et ils sont nuls tant qu'il n'a rien dit.
    if (cell.learned > 0 && cell.sp_cost > 0) {
      ImGui::Text(core::Tr("SP : %d"), cell.sp_cost);
    }
    if (cell.learned > 0 && cell.range_cells > 0) {
      ImGui::Text(core::Tr("Portée : %d"), cell.range_cells);
    }
    if (cell.cooldown_ms > 0) {
      // ⚠ ARRONDI AU-DESSUS : « 0 s » pendant une seconde entière se lit « c'est
      // prêt », et le clic part pour rien.
      ImGui::Text(core::Tr("Encore %u s de temps de recharge"),
                  (cell.cooldown_ms + kMillisecondPerSecond - 1) / kMillisecondPerSecond);
    }
    for (const SkillNeedView& need : cell.needs) {
      const auto found = std::find_if(
          siblings.begin(), siblings.end(),
          [&need](const SkillCellView& other) { return other.skill_id == need.skill_id; });
      // Un prérequis peut vivre dans un AUTRE onglet — une troisième classe en
      // réclame souvent une de deuxième. Aucune flèche ne peut alors le désigner,
      // autant le dire.
      if (found == siblings.end()) {
        ImGui::Text(core::Tr("Requiert %u niveau %u (autre onglet)"), need.skill_id, need.level);
        continue;
      }
      ImGui::Text(core::Tr("Requiert %.*s niveau %u"), static_cast<int>(found->name.size()),
                  found->name.data(), need.level);
    }

    // ── CE QU'ON PEUT FAIRE DE CETTE CASE-LÀ ──────────────────────────────────
    //
    // 🔴 LES GESTES SONT LISTÉS SUR LA CASE, pas seulement dans le rappel de
    // l'en-tête, et ce n'est pas un doublon : celui de l'en-tête dit ce qui EXISTE,
    // celui-ci dit ce qui marche ICI. Un « Double-clic : lancer » sous une passive
    // serait un mensonge, et son absence est justement la réponse.
    ImGui::Separator();
    if (cell.raisable) {
      ImGui::TextDisabled(core::Tr("Clic : réserver un rang - Ctrl + clic : jusqu'au maximum"));
    }
    if (cell.castable) {
      ImGui::TextDisabled(core::Tr("Double-clic : lancer"));
    }
    if (LevelTunable(cell)) {
      ImGui::TextDisabled(core::Tr("Molette : niveau de lancement (%d / %d)"),
                          EffectiveUseLevel(cell), cell.learned);
    }
    ImGui::TextDisabled(core::Tr("Clic droit : menu - Ctrl + clic droit : description"));
    if (cell.castable) {
      ImGui::TextDisabled(core::Tr("Glisser : poser sur une barre d'action"));
    }
    DrawStaffSkillId(staff_allowed, StaffSkillId{.skill_id = cell.skill_id, .idname = cell.idname});
    EndSkinTooltip();
  }
}

void SkillCellGestures(const SkillCellView& cell, std::span<const SkillCellView> siblings,
                       SkillWindowState& state, const SkinArt& art, float icon_side, bool hot,
                       bool staff_allowed, SkillWindowAction& action) {
  // ── LA MOLETTE RÈGLE LE RANG DE LANCEMENT ───────────────────────────────
  //
  // 🔴 AU SURVOL ET PENDANT LE GLISSER, et il faut les deux : dans les deux cas
  // c'est cette case qui doit posséder la molette, sinon le grimoire défile sous
  // le curseur au lieu de régler le rang. Et il faut l'appeler ICI, tant que le
  // dernier élément soumis est bien la case — après `BeginDragDropSource` ce
  // serait l'infobulle du glisser.
  //
  // ⚠ UN CRAN VAUT UN RANG, quel que soit le pas de la molette : les souris à
  // défilement libre envoient des fractions, et on ne veut pas zéro rang de plus.
  const bool dragging = ImGui::IsItemActive();
  if (LevelTunable(cell)) {
    const float wheel = CellWheel(state, hot && ImGui::GetDragDropPayload() == nullptr, dragging);
    if (wheel != 0.0F) {
      const int use = EffectiveUseLevel(cell);
      const int wanted = std::clamp(use + (wheel > 0.0F ? 1 : -1), 1, cell.learned);
      if (wanted != use) {
        action.use_level_for = cell.skill_id;
        action.use_level = wanted;
      }
    }
  }

  // ⚠ LE GLISSER VERS LA BARRE, et il se pose APRÈS la molette et AVANT
  // l'infobulle : ImGui remplace celle-ci par celle du glisser dès qu'il
  // commence, et une infobulle déjà ouverte le masquerait.
  //
  // ⚠ UNE PASSIVE NE SE POSE PAS : la barre ne saurait rien en faire, et une
  // case morte s'y installerait pour toujours.
  // LE MODE CLAVIER LIT LA MÊME CHOSE QUE LE GLISSER : la compétence et son
  // rang effectif, et seulement si elle se pose.
  if (hot && cell.castable) {
    state.keyboard_skill = cell.skill_id;
    state.keyboard_level = static_cast<std::uint16_t>(EffectiveUseLevel(cell));
  }
  if (cell.castable && ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
    // 🔴 LE RANG EFFECTIF, RELU À CHAQUE IMAGE : la molette peut encore le
    // changer en plein vol, et c'est celui-là qui sera posé sur la barre.
    const SkillDrag carried{.skill_id = cell.skill_id,
                            .level = static_cast<std::uint16_t>(EffectiveUseLevel(cell))};
    ImGui::SetDragDropPayload(kSkillDragPayload, &carried, sizeof(carried));
    if (cell.icon.usable()) {
      const ImVec2 icon_at = ImGui::GetCursorScreenPos();
      ImGui::Image(cell.icon.id, ImVec2(icon_side, icon_side));
      // ⚠ MONTRÉE MÊME AU MAXIMUM ICI, contrairement à la grille : on voit le
      // rang qui sera posé AVANT de lâcher.
      DrawUseLevelBadge(ImGui::GetWindowDrawList(), cell, icon_at, icon_side, /*always=*/true);
      ImGui::SameLine();
    }
    ImGui::TextUnformatted(cell.name.data(), cell.name.data() + cell.name.size());
    if (LevelTunable(cell)) {
      ImGui::SameLine();
      ImGui::TextDisabled("%s", core::Tr("(molette : rang)"));
    }
    ImGui::EndDragDropSource();
  }

  // ── LES GESTES ──────────────────────────────────────────────────────────
  //
  // 🔴 CLIC SIMPLE ET DOUBLE-CLIC COMMENCENT PAR LE MÊME APPUI, et il faut donc
  // ATTENDRE de savoir lequel c'était. Sans cela, un double-clic pour LANCER
  // réserverait d'abord un point au passage.
  //
  // 🔴 LES BOUTONS SONT INVERSÉS PAR RAPPORT AU CLIENT, exprès : chez lui le
  // clic DROIT monte et le clic GAUCHE décrit. Ici le clic gauche est le geste
  // COURANT — dépenser un point —, et Ctrl + clic droit reprend le « décrire »
  // déjà en place dans le sac, le coffre et la charrette.
  if (hot) {
    ShowSkillTooltip(cell, siblings, staff_allowed);
    // ⚠ NE PAS REMETTRE À ZÉRO AU SECOND CLIC D'UN DOUBLE : `IsItemClicked` y
    // est vrai aussi, et repartir de zéro ici effacerait le verdict que le
    // double-clic attend.
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) &&
        ImGui::GetIO().MouseClickedLastCount[ImGuiMouseButton_Left] <= 1) {
      state.pressed_skill = cell.skill_id;
      state.pressed_moved = false;
    }
    if (ImGui::GetIO().KeyCtrl && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
      action.describe = cell.skill_id;
    }
    // ⚠ AU RELÂCHÉ, comme les autres surgissants de cette interface : né de
    // l'enfoncement, le menu paraîtrait derrière la fenêtre qui reprend le
    // premier plan.
    if (!ImGui::GetIO().KeyCtrl && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
      state.menu_skill = cell.skill_id;
      ImGui::OpenPopup("case");
    }
  }
  // 🔴 HORS DU TEST DE SURVOL, et il le faut : l'action du clic simple est
  // différée de trois dixièmes de seconde, et la souris a le temps de quitter
  // la case. Seule l'origine de l'appui compte.
  if (state.pressed_skill == cell.skill_id) {
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
      // 🔴 LE VERDICT SE PREND ICI ET SE RETIENT : `GetMouseDragDelta` ne rend
      // plus rien passé l'image du relâché. Le relire plus tard donnerait
      // « n'a pas glissé » pour tout le monde, et un glisser vers la barre
      // réserverait un point au passage.
      const ImVec2 moved = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
      // ⚠ ET UN GLISSER EN COURS COMPTE AUSSI : une charge posée sans que le
      // curseur ait bougé d'un pixel reste un glisser, pas un clic.
      state.pressed_moved =
          moved.x != 0.0F || moved.y != 0.0F || ImGui::GetDragDropPayload() != nullptr;
      // 🔴 CTRL AU MOMENT DU RELÂCHÉ, PAS TROIS DIXIÈMES APRÈS : l'action est
      // différée, et relire la touche au moment où elle part donnerait « jusqu'au
      // maximum » à qui a relâché Ctrl entre-temps.
      state.pressed_to_max = ImGui::GetIO().KeyCtrl;
    }
    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !state.pressed_moved) {
      // 🔴 ET UN DOUBLE-CLIC SUR CE QU'ON NE PEUT PAS LANCER RÉSERVE : une
      // passive, ou une compétence qu'on n'a pas encore, n'a rien à lancer — et
      // un geste qui ne fait rien se réessaie, puis fait douter de la souris.
      if (cell.castable) {
        action.cast = cell.skill_id;
      } else {
        action.reserve = cell.skill_id;
        action.to_max = state.pressed_to_max;
      }
      state.pressed_skill = 0;
    } else if (!state.pressed_moved &&
               ImGui::IsMouseReleasedWithDelay(ImGuiMouseButton_Left,
                                               ImGui::GetIO().MouseDoubleClickTime) &&
               ImGui::GetIO().MouseClickedLastCount[ImGuiMouseButton_Left] == 1) {
      action.reserve = cell.skill_id;
      action.to_max = state.pressed_to_max;
      state.pressed_skill = 0;
    }
  }
  DrawCellMenu(cell, state, art, action);
}

}  // namespace ro::ui
