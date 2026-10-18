// LA FICHE D'UN OBJET : les quatre onglets du serveur — « Monstres »,
// « Boss / MVP », « Script », « Combos ». La base de données les remplit tant
// que le serveur n'a pas répondu, ou hors ligne ; seuls les combos l'attendent.
//
// 🔴 UN ONGLET SANS CONTENU SE GRISE, IL NE DISPARAÎT PAS : la barre garde la
// même forme d'un objet à l'autre, et surtout elle ne se réorganise pas sous le
// curseur quand les réponses arrivent — un onglet qui disparaît au moment où on
// le vise fait cliquer sur le suivant. ⚠ Et il ne se grise que sur une réponse
// ARRIVÉE et vide : en attendant, on ne sait rien.

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>

#include <imgui.h>

#include "ro/core/ascii.h"
#include "ro/core/i18n.h"
#include "ro/ui/skin_widgets.h"

#include "description_odds.h"
#include "table_cells.h"
#include "ui_scale.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

constexpr float kFilterWidthPixels = 180.0F;
constexpr ImU32 kHeaderColor = IM_COL32(206, 198, 172, 255);
constexpr ImVec4 kMvpColor(0.85F, 0.15F, 0.15F, 1.0F);
constexpr ImVec4 kMiniBossColor(0.80F, 0.55F, 0.10F, 1.0F);
constexpr ImU32 kMvpRewardColor = IM_COL32(204, 140, 25, 255);
constexpr ImVec4 kNoteColor(0.70F, 0.55F, 0.20F, 1.0F);
constexpr ImVec4 kSectionColor(0.85F, 0.72F, 0.35F, 1.0F);
constexpr ImVec4 kComboColor(0.55F, 0.80F, 0.55F, 1.0F);
// L'objet qu'on décrit, dans la liste d'un combo : en or, parce qu'il n'est pas
// une destination.
constexpr ImU32 kSelfColor = IM_COL32(230, 200, 90, 255);
constexpr float kHundredthsPerPercent = 100.0F;
constexpr std::uint8_t kMvpRank = 2;
constexpr std::uint8_t kMiniBossRank = 1;
// Le plus large taux qu'une ligne écrit.
constexpr const char* kWidestRate = "100.00%";
constexpr std::size_t kRateBytes = 16;

constexpr int kRateColumn = 1;
constexpr int kKindColumn = 2;

[[nodiscard]] int Compare(std::uint32_t left, std::uint32_t right) {
  return left < right ? -1 : (left > right ? 1 : 0);
}

// « Chargement... » en attendant, l'appel au serveur quand personne n'a pu
// demander.
[[nodiscard]] bool Ready(DescriptionFetch fetch) {
  if (fetch == DescriptionFetch::kWaiting) {
    ImGui::TextDisabled("%s", core::Tr("Chargement..."));
  } else if (fetch == DescriptionFetch::kUnasked) {
    ImGui::TextDisabled("%s", core::Tr("Le serveur seul le sait : connectez-vous."));
  }
  return fetch == DescriptionFetch::kReady;
}

// La table des monstres qui laissent tomber l'objet. `with_kind` : la colonne
// qui sépare un butin d'une récompense de MVP.
//
// 🔴 LES COLONNES SE RÈGLENT À LA SOURIS, et leur largeur initiale tient le
// plus long libellé TRADUIT : « Récompense MVP » ne se coupe pas à l'ouverture.
// Réglée plus étroite, une cellule coupée se lit entière au survol.
void DrawDropTable(DescriptionWindowState& state, const DescriptionDropsView& drops,
                   ImGuiTextFilter& filter, const char* table_id, bool with_kind,
                   DescriptionWindowAction& action) {
  if (drops.rows.empty()) {
    ImGui::TextDisabled("%s", core::Tr("Aucune source."));
    return;
  }
  // ⚠ UNE IDENTITÉ PAR TABLE : les deux onglets portent le même texte visible,
  // et deux champs de même libellé se confondraient.
  const std::string label = std::string(core::Tr("Filtrer (monstre)")) + "##" + table_id;
  ImGui::SetNextItemWidth(Px(kFilterWidthPixels));
  filter.Draw(label.c_str());

  state.drops_order.clear();
  for (std::size_t rank = 0; rank < drops.rows.size(); ++rank) {
    if (PassesTextFilter(filter, drops.rows[rank].name)) {
      state.drops_order.push_back(static_cast<int>(rank));
    }
  }
  constexpr ImGuiTableFlags kFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                     ImGuiTableFlags_Sortable | ImGuiTableFlags_Resizable |
                                     ImGuiTableFlags_SizingFixedFit;
  const char* const mvp_reward = core::Tr("Récompense MVP");
  const char* const ordinary_drop = core::Tr("Drop normal");
  ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, kHeaderColor);
  if (ImGui::BeginTable(table_id, with_kind ? 3 : 2, kFlags)) {
    ImGui::TableSetupColumn(core::Tr("Monstre"), ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn(
        core::Tr("Taux"),
        ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending |
            ImGuiTableColumnFlags_DefaultSort,
        std::max(SortedHeaderWidth(core::Tr("Taux")), WidestText({kWidestRate})));
    if (with_kind) {
      ImGui::TableSetupColumn(
          core::Tr("Type"), ImGuiTableColumnFlags_WidthFixed,
          std::max(SortedHeaderWidth(core::Tr("Type")), WidestText({mvp_reward, ordinary_drop})));
    }
    ImGui::TableHeadersRow();
    if (const ImGuiTableSortSpecs* sort = ImGui::TableGetSortSpecs();
        sort != nullptr && sort->SpecsCount > 0) {
      const ImGuiTableColumnSortSpecs& spec = sort->Specs[0];
      const bool ascending = spec.SortDirection == ImGuiSortDirection_Ascending;
      std::ranges::stable_sort(state.drops_order, [&](int left, int right) {
        const DescriptionDropRow& a = drops.rows[static_cast<std::size_t>(left)];
        const DescriptionDropRow& b = drops.rows[static_cast<std::size_t>(right)];
        int compared = 0;
        if (spec.ColumnIndex == kRateColumn) {
          compared = Compare(a.rate_hundredths, b.rate_hundredths);
        } else if (spec.ColumnIndex == kKindColumn) {
          // Le mécanisme, puis le taux décroissant à mécanisme égal.
          compared = a.mvp_reward != b.mvp_reward ? (a.mvp_reward ? 1 : -1)
                                                  : -Compare(a.rate_hundredths, b.rate_hundredths);
        } else {
          compared = core::AsciiCompareFolded(a.name, b.name);
        }
        return ascending ? compared < 0 : compared > 0;
      });
    }
    for (const int rank : state.drops_order) {
      const DescriptionDropRow& row = drops.rows[static_cast<std::size_t>(rank)];
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      if (row.boss_rank == kMvpRank) {
        ImGui::TextColored(kMvpColor, "%s", core::Tr("[MVP]"));
        ImGui::SameLine();
      } else if (row.boss_rank == kMiniBossRank) {
        ImGui::TextColored(kMiniBossColor, "%s", core::Tr("[Mini]"));
        ImGui::SameLine();
      }
      ImGui::PushID("source");
      ImGui::PushID(rank);
      const std::string shown = std::string(row.name) + " (" + std::to_string(row.monster_id) + ")";
      DrawDescriptionLink(
          state,
          DescriptionLinkCell{
              .id = row.monster_id, .monster = true, .name = row.name, .shown = shown},
          action);
      ImGui::PopID();
      ImGui::PopID();
      ImGui::TableNextColumn();
      std::array<char, kRateBytes> rate{};
      std::snprintf(
          rate.data(), rate.size(), "%.2f%%",
          static_cast<double>(static_cast<float>(row.rate_hundredths) / kHundredthsPerPercent));
      CellText(rate.data());
      if (with_kind) {
        ImGui::TableNextColumn();
        if (row.mvp_reward) {
          CellText(mvp_reward, kMvpRewardColor);
        } else {
          CellText(ordinary_drop, ImGui::GetColorU32(ImGuiCol_TextDisabled));
        }
      }
    }
    ImGui::EndTable();
  }
  ImGui::PopStyleColor();
  if (drops.treasure_excluded > 0) {
    ImGui::TextDisabled(core::Tr("%u coffre(s) au trésor exclu(s)."),
                        static_cast<unsigned>(drops.treasure_excluded));
  }
  if (drops.truncated) {
    ImGui::TextDisabled("%s", core::Tr("... autres sources : voir la base de données."));
  }
  if (drops.from_database) {
    ImGui::TextColored(kNoteColor, "%s",
                       core::Tr("Taux réels de la base de données : le serveur n'a pas encore "
                                "répondu."));
    return;
  }
  // Les donjons instanciés ne sont pas encore parcourus par le serveur : leurs
  // sources peuvent manquer.
  ImGui::TextColored(kNoteColor, "%s",
                     core::Tr("Note : les spawns d'instance ne sont pas encore comptés/scannés."));
}

[[nodiscard]] bool Readable(const DescriptionScriptsView& scripts) {
  if (!Ready(scripts.fetch)) {
    return false;
  }
  if (scripts.unknown) {
    ImGui::TextDisabled("%s", core::Tr("Script indisponible (item introuvable serveur)."));
    return false;
  }
  return true;
}

}  // namespace

bool SourcesKnownEmpty(const DescriptionDropsView& drops) {
  return drops.fetch == DescriptionFetch::kReady && drops.rows.empty();
}

bool ScriptsKnownEmpty(const DescriptionScriptsView& scripts) {
  return scripts.fetch == DescriptionFetch::kReady && scripts.main.empty() &&
         scripts.equip.empty() && scripts.unequip.empty();
}

bool CombosKnownEmpty(const DescriptionScriptsView& scripts) {
  return scripts.fetch == DescriptionFetch::kReady && !scripts.from_database &&
         scripts.combos.empty();
}

void DrawSourcesTab(DescriptionWindowState& state, const DescriptionView& view, bool boss,
                    const SkinArt& art, DescriptionWindowAction& action) {
  const DescriptionDropsView& drops = boss ? view.drops_boss : view.drops_ordinary;
  const char* const table_id = boss ? "sources_boss" : "sources_ordinaires";
  if (view.online) {
    const std::string button = std::string(core::Tr("Rafraichir")) + "##" + table_id;
    if (SkinSmallButton(button.c_str(), art)) {
      (boss ? action.refresh_boss : action.refresh_ordinary) = true;
    }
  }
  if (Ready(drops.fetch)) {
    ImGuiTextFilter& filter =
        boss ? state.boss_filters[view.item_id] : state.ordinary_filters[view.item_id];
    DrawDropTable(state, drops, filter, table_id, boss, action);
  }
}

void DrawScriptsTab(const DescriptionScriptsView& scripts, const SkinArt& art) {
  if (!Readable(scripts)) {
    return;
  }
  if (scripts.main.empty() && scripts.equip.empty() && scripts.unequip.empty()) {
    ImGui::TextDisabled("%s", core::Tr("Cet item n'a aucun script."));
    return;
  }
  // Les trois noms de champ de la base d'objets : des termes du serveur.
  if (!scripts.main.empty()) {
    ImGui::TextColored(kSectionColor, "%s", "Script");
    DrawScriptCode("script_principal", scripts.main, art);
  }
  if (!scripts.equip.empty()) {
    ImGui::TextColored(kSectionColor, "%s", "EquipScript");
    DrawScriptCode("script_equipe", scripts.equip, art);
  }
  if (!scripts.unequip.empty()) {
    ImGui::TextColored(kSectionColor, "%s", "UnEquipScript");
    DrawScriptCode("script_desequipe", scripts.unequip, art);
  }
}

void DrawCombosTab(DescriptionWindowState& state, const DescriptionView& view, const SkinArt& art,
                   DescriptionWindowAction& action) {
  const DescriptionScriptsView& scripts = view.scripts;
  if (scripts.from_database) {
    // La base ne connaît pas les combos : seul le serveur répond.
    ImGui::TextDisabled("%s", view.online ? core::Tr("Chargement...")
                                          : core::Tr("Le serveur seul le sait : connectez-vous."));
    return;
  }
  if (!Readable(scripts)) {
    return;
  }
  if (scripts.combos.empty()) {
    ImGui::TextDisabled("%s", core::Tr("Cet item ne fait partie d'aucun combo."));
    return;
  }
  for (std::size_t rank = 0; rank < scripts.combos.size(); ++rank) {
    const DescriptionComboView& combo = scripts.combos[rank];
    ImGui::PushID(static_cast<int>(rank));
    ImGui::TextColored(kComboColor, core::Tr("Combo %zu"), rank + 1);
    // Les membres, joints par « + », repliés à la main : ImGui ne replie pas une
    // suite de widgets, c'est la place restante qui décide d'aller à la ligne.
    const float right_edge = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
    for (std::size_t member = 0; member < combo.members.size(); ++member) {
      const DescriptionComboMember& part = combo.members[member];
      const std::string shown =
          part.name.empty() ? "#" + std::to_string(part.item_id) : std::string(part.name);
      if (member > 0) {
        ImGui::SameLine(0.0F, 0.0F);
        ImGui::TextUnformatted(" + ");
        if (ImGui::GetItemRectMax().x + ImGui::CalcTextSize(shown.c_str()).x < right_edge) {
          ImGui::SameLine(0.0F, 0.0F);
        }
      }
      ImGui::PushID(static_cast<int>(member));
      const bool self = part.item_id == view.item_id;
      DrawDescriptionLink(state,
                          DescriptionLinkCell{.id = part.item_id,
                                              .name = part.name,
                                              .shown = shown,
                                              .color = self ? kSelfColor : 0},
                          action);
      ImGui::PopID();
    }
    if (!combo.script.empty()) {
      const std::string box = "combo_" + std::to_string(rank);
      DrawScriptCode(box.c_str(), combo.script, art);
    } else {
      ImGui::TextDisabled("%s", core::Tr("(pas de script)"));
    }
    if (rank + 1 < scripts.combos.size()) {
      ImGui::Separator();
    }
    ImGui::PopID();
  }
}

}  // namespace ro::ui
