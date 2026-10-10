#include "skill_list.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <span>
#include <vector>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_widgets.h"

#include "skill_cell.h"

namespace ro::ui {
namespace {

// ⚠ L'INDENTATION EST PLAFONNÉE, et la mesure vient des tables du client :
// onze des treize classes d'avant le renouveau tiennent en CINQ niveaux, mais le
// Moine monte à neuf et le Rôdeur à huit — avec une seule compétence à chaque
// palier profond. Indenter linéairement mangerait la colonne du nom pour une
// poignée de lignes ; au-delà du plafond le décalage se FIGE, et c'est le coude
// qui dit le parent.
constexpr int kIndentCap = 5;
constexpr float kIndentStepPixels = 14.0F;
// La gouttière : la place des traits, à gauche de l'icône.
constexpr float kGutterPixels = 14.0F;

// Les largeurs des colonnes chiffrées.
constexpr float kLevelColumnPixels = 56.0F;
constexpr float kSpColumnPixels = 40.0F;
constexpr float kRangeColumnPixels = 46.0F;
constexpr float kRaiseColumnPixels = 28.0F;

// Le fond d'une ligne : celle qu'on survole, et celles que sa branche relie.
constexpr ImU32 kFocusRowColor = IM_COL32(120, 95, 35, 90);
constexpr ImU32 kLinkedRowColor = IM_COL32(70, 75, 120, 80);
// Le coude de liaison : vif quand il touche la ligne survolée, discret sinon.
constexpr ImU32 kElbowHotColor = IM_COL32(255, 205, 105, 255);
constexpr ImU32 kElbowIdleColor = IM_COL32(150, 155, 190, 110);
constexpr float kElbowHotThickness = 2.5F;
constexpr float kElbowIdleThickness = 1.0F;
// De combien le trait s'arrête avant l'icône, et le rayon de la pastille.
constexpr float kElbowInsetPixels = 2.0F;
constexpr float kElbowDotHot = 3.0F;
constexpr float kElbowDotIdle = 2.0F;
// L'icône d'une compétence non apprise est assombrie, comme en grille.
constexpr ImU32 kUnlearnedTint = IM_COL32(110, 110, 110, 160);
constexpr ImVec4 kReservedColor(0.85F, 0.65F, 0.20F, 1.0F);

[[nodiscard]] bool Contains(std::string_view haystack, std::string_view needle) {
  if (needle.empty()) {
    return true;
  }
  const auto same = [](char left, char right) {
    return std::tolower(static_cast<unsigned char>(left)) ==
           std::tolower(static_cast<unsigned char>(right));
  };
  return std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end(), same) !=
         haystack.end();
}

// Le rang d'une compétence dans l'onglet, ou moins un quand il ne la porte pas.
[[nodiscard]] int RankOf(std::span<const SkillCellView> cells, std::uint16_t skill_id) {
  for (std::size_t rank = 0; rank < cells.size(); ++rank) {
    if (cells[rank].skill_id == skill_id) {
      return static_cast<int>(rank);
    }
  }
  return -1;
}

// LA PROFONDEUR DE CHAQUE COMPÉTENCE : un de plus que son prérequis le plus
// profond PRÉSENT DANS L'ONGLET.
//
// 🔴 PAR PASSES SUCCESSIVES, ET BORNÉES PAR LE NOMBRE DE NŒUDS. Ça converge en
// trois ou quatre tours — et la borne protège d'un cycle si les tables en
// portaient un. Ces données sont écrites à la main, sans invariant : ne rien
// supposer de leur forme.
//
// ⚠ « PRÉSENT DANS L'ONGLET » N'EST PAS UN DÉTAIL : une troisième classe réclame
// souvent une compétence de deuxième, qui vit dans un autre onglet. La compter
// donnerait une profondeur que rien à l'écran ne justifie.
[[nodiscard]] std::vector<int> DepthOf(std::span<const SkillCellView> cells,
                                       std::span<const std::vector<int>> needs_rank) {
  std::vector<int> depth(cells.size(), 0);
  for (std::size_t pass = 0; pass < cells.size(); ++pass) {
    bool changed = false;
    for (std::size_t rank = 0; rank < cells.size(); ++rank) {
      int deepest = 0;
      for (const int source : needs_rank[rank]) {
        if (source >= 0) {
          deepest = std::max(deepest, depth[static_cast<std::size_t>(source)] + 1);
        }
      }
      if (deepest != depth[rank]) {
        depth[rank] = deepest;
        changed = true;
      }
    }
    if (!changed) {
      break;
    }
  }
  return depth;
}

// La compétence survolée est-elle celle-ci, ou l'un de ses voisins directs ?
[[nodiscard]] bool LinkedToFocus(const SkillCellView& cell, std::span<const SkillCellView> cells,
                                 std::uint16_t focus) {
  if (focus == 0 || cell.skill_id == focus) {
    return false;
  }
  for (const SkillNeedView& need : cell.needs) {
    if (need.skill_id == focus) {
      return true;
    }
  }
  const int rank = RankOf(cells, focus);
  if (rank < 0) {
    return false;
  }
  for (const SkillNeedView& need : cells[static_cast<std::size_t>(rank)].needs) {
    if (need.skill_id == cell.skill_id) {
      return true;
    }
  }
  return false;
}

}  // namespace

void DrawSkillList(SkillWindowState& state, const SkillGroupView& group, std::string_view filter,
                   const SkinArt& art, bool staff_allowed, SkillWindowAction& action) {
  const std::span<const SkillCellView> cells = group.cells;
  if (cells.empty()) {
    return;
  }

  // ⚠ LA TABLE DE CORRESPONDANCE SE BÂTIT UNE FOIS : sans elle, chaque passe du
  // calcul de profondeur relancerait une recherche linéaire par prérequis.
  std::vector<std::vector<int>> needs_rank(cells.size());
  for (std::size_t rank = 0; rank < cells.size(); ++rank) {
    needs_rank[rank].reserve(cells[rank].needs.size());
    for (const SkillNeedView& need : cells[rank].needs) {
      needs_rank[rank].push_back(RankOf(cells, need.skill_id));
    }
  }
  const std::vector<int> depth = DepthOf(cells, needs_rank);

  // 🔴 TRIÉE PAR PROFONDEUR, ET STABLE : à profondeur égale, l'ordre de la
  // grille — celui des tables du client. C'est ce tri qui garantit qu'un
  // prérequis paraît AVANT ce qu'il débloque, donc que son ancre est connue
  // quand vient le moment de tracer le coude.
  std::vector<std::size_t> order(cells.size());
  std::iota(order.begin(), order.end(), std::size_t{0});
  std::ranges::stable_sort(order, [&](std::size_t left, std::size_t right) {
    if (depth[left] != depth[right]) {
      return depth[left] < depth[right];
    }
    return cells[left].position < cells[right].position;
  });

  constexpr auto kFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
                          ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollY;
  if (!ImGui::BeginTable("competences", 5, kFlags)) {
    return;
  }
  ImGui::TableSetupColumn(core::Tr("Skill"), ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn(core::Tr("Niveau"), ImGuiTableColumnFlags_WidthFixed, kLevelColumnPixels);
  ImGui::TableSetupColumn(core::Tr("SP"), ImGuiTableColumnFlags_WidthFixed, kSpColumnPixels);
  ImGui::TableSetupColumn(core::Tr("Portée"), ImGuiTableColumnFlags_WidthFixed, kRangeColumnPixels);
  ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, kRaiseColumnPixels);
  ImGui::TableSetupScrollFreeze(0, 1);
  ImGui::TableHeadersRow();

  const float icon = ImGui::GetTextLineHeight();
  const std::uint16_t focus = state.hovered_skill;
  std::uint16_t hovered_now = 0;
  // ⚠ L'ANCRE D'UNE LIGNE NON ENCORE DESSINÉE VAUT MOINS UN : un prérequis filtré
  // hors de la vue n'a pas de coude, et le tracer vers une ordonnée jamais posée
  // barrerait la table.
  std::vector<ImVec2> anchor(cells.size(), ImVec2(-1.0F, -1.0F));

  for (const std::size_t rank : order) {
    const SkillCellView& cell = cells[rank];
    if (!Contains(cell.name, filter)) {
      continue;
    }
    ImGui::PushID(static_cast<int>(cell.skill_id));
    ImGui::TableNextRow();
    if (cell.skill_id == focus) {
      ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, kFocusRowColor);
    } else if (LinkedToFocus(cell, cells, focus)) {
      ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, kLinkedRowColor);
    }

    // ── LA COMPÉTENCE, INDENTÉE À SA PROFONDEUR ─────────────────────────────
    ImGui::TableNextColumn();
    const float indent =
        kGutterPixels + static_cast<float>(std::min(depth[rank], kIndentCap)) * kIndentStepPixels;
    ImGui::Indent(indent);
    const ImVec2 at = ImGui::GetCursorScreenPos();
    anchor[rank] = ImVec2(at.x, at.y + icon * 0.5F);
    ImDrawList* const into = ImGui::GetWindowDrawList();

    // LE COUDE VERS CHAQUE PRÉREQUIS.
    //
    // ⚠ LA VERTICALE PASSE À UN DEMI-PAS À GAUCHE de l'icône du parent, dans la
    // gouttière : sous son centre, elle traverserait les icônes des lignes de
    // même profondeur.
    //
    // ⚠ ET LA RÉDUCTION TRANSITIVE S'APPLIQUE ICI AUSSI : la liste du client est
    // aplatie, tout tracer doublerait les chemins. C'est la même arête que la
    // grille écarte, et le même test la tranche.
    for (std::size_t need = 0; need < cell.needs.size(); ++need) {
      const int source = needs_rank[rank][need];
      if (source < 0 || anchor[static_cast<std::size_t>(source)].x < 0.0F) {
        continue;
      }
      if (SkillEdgeIsRedundant(cells, cell, cell.needs[need].skill_id)) {
        continue;
      }
      const bool hot = focus != 0 && (focus == cell.skill_id || focus == cell.needs[need].skill_id);
      const ImU32 color = hot ? kElbowHotColor : kElbowIdleColor;
      const float thickness = hot ? kElbowHotThickness : kElbowIdleThickness;
      const ImVec2 from = anchor[static_cast<std::size_t>(source)];
      const float turn = from.x - kIndentStepPixels * 0.5F;
      into->AddLine(ImVec2(from.x - kElbowInsetPixels, from.y), ImVec2(turn, from.y), color,
                    thickness);
      into->AddLine(ImVec2(turn, from.y), ImVec2(turn, anchor[rank].y), color, thickness);
      into->AddLine(ImVec2(turn, anchor[rank].y),
                    ImVec2(anchor[rank].x - kElbowInsetPixels, anchor[rank].y), color, thickness);
      into->AddCircleFilled(ImVec2(anchor[rank].x - kElbowDotHot, anchor[rank].y),
                            hot ? kElbowDotHot : kElbowDotIdle, color);
    }

    // 🔴 LA LIGNE ENTIÈRE EST LE WIDGET, ICÔNE COMPRISE, et elle est soumise
    // AVANT le visuel. Avec l'icône dessinée d'abord puis un élément réduit au
    // nom, l'icône devenait une zone MORTE — survolée, elle ne donnait ni
    // infobulle ni menu, alors que c'est justement là qu'on vise.
    ImGui::Selectable("##ligne", false, ImGuiSelectableFlags_None, ImVec2(0.0F, icon));
    const bool hot = ImGui::IsItemHovered();
    if (hot) {
      hovered_now = cell.skill_id;
    }
    if (cell.icon.usable()) {
      into->AddImage(cell.icon.id, at, ImVec2(at.x + icon, at.y + icon), ImVec2(0.0F, 0.0F),
                     ImVec2(1.0F, 1.0F),
                     cell.effective_level() > 0 ? IM_COL32_WHITE : kUnlearnedTint);
    }
    // Le même repère qu'en grille : la pastille du rang de lancement.
    DrawUseLevelBadge(into, cell, at, icon);
    into->AddText(
        ImVec2(at.x + icon + ImGui::GetStyle().ItemSpacing.x, at.y),
        ImGui::GetColorU32(cell.effective_level() == 0 ? ImGuiCol_TextDisabled : ImGuiCol_Text),
        cell.name.data(), cell.name.data() + cell.name.size());
    SkillCellGestures(cell, cells, state, art, icon, hot, staff_allowed, action);
    ImGui::Unindent(indent);

    // ── LE RANG, LE COÛT, LA PORTÉE ─────────────────────────────────────────
    ImGui::TableNextColumn();
    if (cell.reserved > cell.learned) {
      ImGui::TextColored(kReservedColor, "%d/%d", cell.effective_level(), cell.max_level);
    } else if (cell.learned > 0) {
      ImGui::Text("%d/%d", cell.learned, cell.max_level);
    } else {
      ImGui::TextDisabled("-/%d", cell.max_level);
    }

    ImGui::TableNextColumn();
    if (cell.passive) {
      ImGui::TextDisabled("%s", core::Tr("passif"));
    } else if (cell.learned > 0) {
      ImGui::Text("%d", cell.sp_cost);
    } else {
      ImGui::TextDisabled("-");
    }

    ImGui::TableNextColumn();
    if (cell.learned > 0 && cell.range_cells > 0) {
      ImGui::Text("%d", cell.range_cells);
    } else {
      ImGui::TextDisabled("-");
    }

    // ── LE « + », QUI N'EXISTE QUE DANS CETTE VUE ───────────────────────────
    //
    // 🔴 LA GRILLE N'EN A PAS : posé au survol par-dessus une case, il se
    // ferait voler le clic par la case elle-même, soumise avant lui.
    // Ici il a SA cellule de tableau — aucun recouvrement, aucun conflit.
    ImGui::TableNextColumn();
    if (cell.raisable) {
      if (SkinSmallButton("+", art, ImVec2(kRaiseColumnPixels, 0.0F))) {
        action.reserve = cell.skill_id;
        action.to_max = ImGui::GetIO().KeyCtrl;
      }
      if (ImGui::IsItemHovered()) {
        SkinTooltip(core::Tr("Réserve un rang ; Ctrl : jusqu'au maximum."));
      }
    }
    ImGui::PopID();
  }
  ImGui::EndTable();
  state.hovered_skill = hovered_now;
}

}  // namespace ro::ui
