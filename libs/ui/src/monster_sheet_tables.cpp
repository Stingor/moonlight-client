// Les quatre onglets à tables de la fiche de monstre : résistances, butin,
// apparitions, compétences.

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <numeric>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>

#include "ro/core/ascii.h"
#include "ro/core/i18n.h"
#include "ro/ui/skin_widgets.h"

#include "monster_sheet_parts.h"
#include "table_cells.h"

namespace ro::ui::monster_sheet {
namespace {

// Les résistances, cinq par ligne.
constexpr int kResistColumns = 5;
// Les seuils qui colorent une résistance : immunisé ou soigné, faible, normal.
constexpr int kResistHalfPercent = 50;
constexpr int kResistNormalPercent = 100;

// Les colonnes des tables, en pixels d'interface.
constexpr float kFilterWidthPixels = 180.0F;
// Les plus larges valeurs qu'écrivent les colonnes fixes : un taux, un
// nombre d'apparitions, un niveau, un identifiant de compétence.
constexpr const char* kWidestRate = "100.00%";
constexpr const char* kWidestCount = "9999";
constexpr const char* kWidestLevel = "99";
constexpr const char* kWidestSkillId = "99999";
// L'icône d'un butin, en lignes de texte.
constexpr float kDropIconLines = 1.4F;

// Les centièmes d'un taux, et les pour-cent qu'ils font.
constexpr float kHundredthsPerPercent = 100.0F;

constexpr const char* kDropMenuId = "##mi_link_menu";

// Les colonnes que les tris désignent.
constexpr int kDropRateColumn = 1;
constexpr int kDropKindColumn = 2;
constexpr int kSpawnCountColumn = 1;
constexpr int kSkillLevelColumn = 1;
constexpr int kSkillIdColumn = 2;

// Vert = le monstre encaisse peu, rouge = il encaisse plein ou plus. ⚠ LE CAS
// NORMAL PREND LA COULEUR DE TEXTE COURANTE : sur un corps clair, toute teinte
// pâle disparaîtrait.
[[nodiscard]] ImU32 ResistColor(int percent) {
  if (percent <= 0) {
    return kGreenColor;
  }
  if (percent < kResistHalfPercent) {
    return kLowResistColor;
  }
  if (percent <= kResistNormalPercent) {
    return ImGui::GetColorU32(ImGuiCol_Text);
  }
  return kRedColor;
}

template <typename T>
[[nodiscard]] int Compare(const T& left, const T& right) {
  return left < right ? -1 : (right < left ? 1 : 0);
}

// Un lien se suit au RELÂCHEMENT, comme tout lien du client : la fenêtre qu'il
// ouvre naît à l'image suivante, et partie de l'enfoncement elle s'ouvrirait
// derrière celle d'où le clic vient.
[[nodiscard]] bool LinkReleased(bool hovered, bool right_too) {
  return hovered && (ImGui::IsMouseReleased(ImGuiMouseButton_Left) ||
                     (right_too && ImGui::IsMouseReleased(ImGuiMouseButton_Right)));
}

// L'ORDRE D'UNE TABLE, refait à chaque image depuis son tri. ⚠ LE MODÈLE NE
// BOUGE PAS : l'ordre du serveur reste la référence, et une table de deux cent
// cinquante-cinq lignes se trie sans qu'on la retienne.
template <typename Row, typename Compared>
void SortRows(std::vector<int>& order, std::span<const Row> rows, Compared compared) {
  const ImGuiTableSortSpecs* const sort = ImGui::TableGetSortSpecs();
  if (sort == nullptr || sort->SpecsCount == 0) {
    return;
  }
  const ImGuiTableColumnSortSpecs& spec = sort->Specs[0];
  const bool ascending = spec.SortDirection == ImGuiSortDirection_Ascending;
  std::ranges::stable_sort(order, [&](int left, int right) {
    const int said = compared(rows[static_cast<std::size_t>(left)],
                              rows[static_cast<std::size_t>(right)], spec.ColumnIndex);
    return ascending ? said < 0 : said > 0;
  });
}

// Un nom de butin, icône comprise, avec les gestes de tout objet
// (`ro::ui::ItemLinkCell`).
void DropCell(MonsterSheetWindowState& state, const MonsterSheetDropView& drop, int row,
              MonsterSheetAnswer& answer) {
  ImGui::PushID(row);
  const char* const shown = drop.name.empty() ? core::Tr("(item inconnu)") : drop.name.c_str();
  DrawItemLinkCell(ItemLinkCell{.item_id = drop.item_id,
                                .text = shown,
                                .icon = drop.icon,
                                .icon_side = ImGui::GetTextLineHeight() * kDropIconLines,
                                .keep_icon_place = true},
                   state.menu, answer.item);
  ImGui::PopID();
}

// LA LARGEUR INITIALE D'UNE COLONNE FIXE ET TRIABLE : son en-tête traduit,
// flèche comprise, ou la plus large de ses valeurs.
[[nodiscard]] float FixedColumnWidth(const char* header,
                                     std::initializer_list<std::string_view> values) {
  return std::max(SortedHeaderWidth(header), WidestText(values));
}

// Le menu d'un butin, ouvert HORS de la table : une cellule change la pile
// d'ids, et le surgissant ne s'y retrouverait pas.
void DrawDropMenu(MonsterSheetWindowState& state, const MonsterSheetView& view,
                  MonsterSheetAnswer& answer) {
  DrawItemLinkMenu(kDropMenuId, state.menu, answer.item, view.link_facts);
}

}  // namespace

void DrawResistTab(const MonsterSheetView& view) {
  Label(
      core::Tr("Pourcentage de dégâts ENCAISSÉS par élément d'attaque. 100 % = normal, 0 % "
               "= immunisé, au-dessus de 100 % = faiblesse."));
  const std::string element(view.words.element_value);
  ImGui::Text(core::Tr("Élément du monstre : %s niveau %u"), element.c_str(),
              static_cast<unsigned>(view.element_level));
  ImGui::Separator();
  if (!ImGui::BeginTable("##mi_resist", kResistColumns,
                         ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_Resizable |
                             ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
    return;
  }
  for (std::size_t element_rank = 0; element_rank < kMonsterSheetResistCount; ++element_rank) {
    ImGui::TableNextColumn();
    const std::string label(view.words.resist[element_rank]);
    Label(label.c_str());
    const int percent = view.resist_percent[element_rank];
    Colored(ResistColor(percent), Printed("%d %%", percent).c_str());
  }
  ImGui::EndTable();
}

void DrawDropsTab(MonsterSheetWindowState& state, const MonsterSheetView& view, const SkinArt& art,
                  MonsterSheetAnswer& answer) {
  if (view.drops.empty()) {
    Label(core::Tr("Ce monstre ne laisse rien tomber."));
    return;
  }
  ImGui::SetNextItemWidth(Px(kFilterWidthPixels));
  state.drop_filter.Draw(core::Tr("Filtrer (item)"));

  // 🔴 LES COLONNES SE RÈGLENT À LA SOURIS, et leur largeur initiale tient le
  // plus long libellé TRADUIT : « Récompense MVP » ne se coupe pas à
  // l'ouverture. Seul le nom s'étire. Réglée plus étroite, une cellule coupée
  // se lit entière au survol.
  constexpr ImGuiTableFlags kFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                     ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable |
                                     ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit;
  const char* const mvp_reward = core::Tr("Récompense MVP");
  const char* const ordinary_drop = core::Tr("Drop normal");
  if (ImGui::BeginTable("##mi_drops", 3, kFlags)) {
    ImGui::TableSetupColumn(core::Tr("Item"), ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn(core::Tr("Taux"),
                            ImGuiTableColumnFlags_WidthFixed |
                                ImGuiTableColumnFlags_PreferSortDescending |
                                ImGuiTableColumnFlags_DefaultSort,
                            FixedColumnWidth(core::Tr("Taux"), {kWidestRate}));
    ImGui::TableSetupColumn(core::Tr("Type"), ImGuiTableColumnFlags_WidthFixed,
                            FixedColumnWidth(core::Tr("Type"), {mvp_reward, ordinary_drop}));
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();

    state.drop_order.clear();
    for (std::size_t rank = 0; rank < view.drops.size(); ++rank) {
      if (state.drop_filter.PassFilter(view.drops[rank].name.c_str())) {
        state.drop_order.push_back(static_cast<int>(rank));
      }
    }
    SortRows(state.drop_order, view.drops,
             [](const MonsterSheetDropView& left, const MonsterSheetDropView& right, int column) {
               if (column == kDropRateColumn) {
                 return Compare(left.rate_hundredths, right.rate_hundredths);
               }
               if (column == kDropKindColumn) {
                 return Compare(left.mvp_reward, right.mvp_reward);
               }
               return core::AsciiCompareFolded(left.name, right.name);
             });

    for (const int rank : state.drop_order) {
      const MonsterSheetDropView& drop = view.drops[static_cast<std::size_t>(rank)];
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      DropCell(state, drop, rank, answer);
      ImGui::TableNextColumn();
      // Deux décimales, en centièmes de pour cent, comme la fiche d'objet.
      CellText(Printed("%.2f%%", static_cast<double>(static_cast<float>(drop.rate_hundredths) /
                                                     kHundredthsPerPercent)));
      ImGui::TableNextColumn();
      if (drop.mvp_reward) {
        Colored(kAmberColor, mvp_reward);
      } else {
        Label(ordinary_drop);
      }
    }
    PaintWindowScrollbar(art);
    ImGui::EndTable();
  }
  DrawDropMenu(state, view, answer);
}

void DrawSpawnsTab(MonsterSheetWindowState& state, const MonsterSheetView& view, const SkinArt& art,
                   MonsterSheetAnswer& answer) {
  if (view.spawns.empty()) {
    LabelWrapped(
        core::Tr("Aucun spawn permanent connu : ce monstre n'apparaît que par script "
                 "(invocation, quête, instance) ou n'est plus placé sur aucune carte."));
    return;
  }
  if (ImGui::BeginTable("##mi_spawns", 2,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                            ImGuiTableFlags_Sortable | ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn(core::Tr("Carte"),
                            ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_DefaultSort);
    ImGui::TableSetupColumn(
        core::Tr("Nombre"),
        ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending,
        FixedColumnWidth(core::Tr("Nombre"), {kWidestCount}));
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();

    state.spawn_order.resize(view.spawns.size());
    std::iota(state.spawn_order.begin(), state.spawn_order.end(), 0);
    SortRows(state.spawn_order, view.spawns,
             [](const MonsterSheetSpawnView& left, const MonsterSheetSpawnView& right, int column) {
               return column == kSpawnCountColumn
                          ? Compare(left.count, right.count)
                          : core::AsciiCompareFolded(left.label, right.label);
             });
    for (const int rank : state.spawn_order) {
      const MonsterSheetSpawnView& spawn = view.spawns[static_cast<std::size_t>(rank)];
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      // LE NOM QU'ON LIT, PUIS LE NOM INTERNE entre parenthèses : le premier
      // dit où c'est, le second sert dans une commande et entre joueurs. La
      // colonne trie sur ce qu'elle montre.
      //
      // LES TROIS GESTES D'UN LIEU, ceux de tout lien de lieu du client : le
      // clic guide vers la carte entière, le Maj+clic pose son lien dans la
      // saisie, le survol demande son infobulle — nom lisible et plan —, que
      // `app` compose.
      ImGui::PushID(rank);
      Colored(LinkColor(), spawn.label.c_str());
      const bool hovered = ImGui::IsItemHovered();
      DecorateLink(hovered);
      if (hovered) {
        answer.hovered_map = spawn.map_name;
      }
      if (LinkReleased(hovered, false)) {
        (ImGui::GetIO().KeyShift ? answer.link_map : answer.guide_map) = spawn.map_name;
      }
      ImGui::PopID();
      ImGui::TableNextColumn();
      CellText(Printed("%u", static_cast<unsigned>(spawn.count)));
    }
    PaintWindowScrollbar(art);
    ImGui::EndTable();
  }
  Label(core::Tr("Clic sur une carte : le guidage · Maj+clic : son lien dans le chat"));
  Label(core::Tr("Les spawns d'instance et les invocations de script ne sont pas comptés."));
}

void DrawSkillsTab(MonsterSheetWindowState& state, const MonsterSheetView& view, const SkinArt& art,
                   MonsterSheetAnswer& answer) {
  Label(core::Tr("Clic sur un skill : sa description (fenêtre du client)."));
  if (!ImGui::BeginTable("##mi_skills", 3,
                         ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                             ImGuiTableFlags_Sortable | ImGuiTableFlags_Resizable |
                             ImGuiTableFlags_SizingFixedFit)) {
    return;
  }
  ImGui::TableSetupColumn(core::Tr("Skill"),
                          ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_DefaultSort);
  ImGui::TableSetupColumn(
      core::Tr("Niveau"),
      ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending,
      FixedColumnWidth(core::Tr("Niveau"), {kWidestLevel}));
  ImGui::TableSetupColumn(core::Tr("ID"), ImGuiTableColumnFlags_WidthFixed,
                          FixedColumnWidth(core::Tr("ID"), {kWidestSkillId}));
  ImGui::TableSetupScrollFreeze(0, 1);
  ImGui::TableHeadersRow();

  state.skill_order.resize(view.skills.size());
  std::iota(state.skill_order.begin(), state.skill_order.end(), 0);
  SortRows(state.skill_order, view.skills,
           [](const MonsterSheetSkillView& left, const MonsterSheetSkillView& right, int column) {
             if (column == kSkillLevelColumn) {
               return Compare(left.level, right.level);
             }
             if (column == kSkillIdColumn) {
               return Compare(left.skill_id, right.skill_id);
             }
             return core::AsciiCompareFolded(left.name, right.name);
           });
  for (const int rank : state.skill_order) {
    const MonsterSheetSkillView& skill = view.skills[static_cast<std::size_t>(rank)];
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::PushID(rank);
    // ⚠ CLIQUABLE SEULEMENT QUAND L'INSTALLATION A UNE DESCRIPTION : les
    // autres sont listées, elles ne mentent juste pas sur ce qu'un clic
    // donnerait.
    if (skill.describable) {
      Colored(LinkColor(), skill.name.c_str());
      const bool hovered = ImGui::IsItemHovered();
      DecorateLink(hovered);
      if (LinkReleased(hovered, true)) {
        answer.describe_skill = skill.skill_id;
      }
    } else {
      CellText(skill.name);
    }
    ImGui::PopID();
    ImGui::TableNextColumn();
    CellText(Printed("%u", static_cast<unsigned>(skill.level)));
    ImGui::TableNextColumn();
    Label(Printed("%u", static_cast<unsigned>(skill.skill_id)).c_str());
  }
  PaintWindowScrollbar(art);
  ImGui::EndTable();
}

}  // namespace ro::ui::monster_sheet
