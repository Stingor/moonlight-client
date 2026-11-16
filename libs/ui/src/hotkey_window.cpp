#include "ro/ui/hotkey_window.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/core/ascii.h"
#include "ro/core/search_key.h"
#include "ro/ui/bug_report_window.h"
#include "ro/ui/chat_link_offer.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/window_name.h"

#include "ui_scale.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

constexpr float kFirstWidthPixels = 420.0F;
constexpr float kFirstHeightPixels = 380.0F;
// La colonne des touches : assez pour « Ctrl+Maj+Retour arrière ».
constexpr float kKeyColumnPixels = 150.0F;
// Le seul bouton de la barre de titre, au rang de sa pose.
constexpr int kBugReportRank = 0;

// L'intertitre d'une famille de lignes : une couleur choisie pour le corps clair
// du jeu, traduite pour celui de l'habillage.
constexpr ImU32 kGroupTextColor = IM_COL32(86, 96, 124, 255);

// Choisir un autre onglet annule la capture : sa ligne peut n'être plus à
// l'écran, et la prochaine touche lui irait sans que le joueur le voie.
//
// MAJ + CLIC SUR LA LANGUETTE : son lien dans le chat, lu avant le contenu de
// l'onglet — voir `chat_link_offer.h`.
void TabItem(HotkeyWindowState& state, int tab, std::string_view label, HotkeyWindowAction& asked) {
  std::array<char, 96> id{};
  std::snprintf(id.data(), id.size(), "%.*s###onglet%d", static_cast<int>(label.size()),
                label.data(), tab);
  const ImGuiTabItemFlags flags =
      state.tab_request == tab ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
  const bool opened = ImGui::BeginTabItem(id.data(), nullptr, flags);
  if (OfferChatLink(true)) {
    asked.link_tab = tab;
  }
  if (!opened) {
    return;
  }
  if (state.tab != tab) {
    state.tab = tab;
    state.capturing = -1;
  }
  ImGui::EndTabItem();
}

enum class HotkeyColumn : std::uint8_t { kAction, kKey };

// Une ligne à ranger, avec ses clés de tri déjà calculées.
struct SortedRow {
  int rank = 0;
  // Le rang de sa famille — onglet et intertitre — dans l'ordre de la table.
  int family = 0;
  std::string action_key;
  std::string key_key;
};

// L'ORDRE DES LIGNES SELON L'EN-TÊTE CHOISI, par défaut l'action de A à Z.
//
// ⚠ LE TRI SE FAIT DANS CHAQUE FAMILLE. Les intertitres — « Barre 3 » — ne se
// posent que là où la famille change : mêlées, les cases de deux barres en
// feraient surgir un à chaque ligne.
//
// ⚠ L'ORDRE DE LA LANGUE MONTRÉE : les clés sont celles des libellés traduits,
// sans casse ni accents, et leurs nombres se lisent comme des nombres —
// « case 2 » avant « case 10 ». Une action sans touche se range toujours en
// fin de famille quand on trie par touche : « (aucune) » n'est pas un nom.
//
// Recalculé à chaque image : un peu plus de cent lignes, et un ordre qui ne
// dépend que de la vue — la touche d'une ligne change sous le tri.
[[nodiscard]] std::vector<int> SortedRanks(const HotkeyWindowView& view) {
  HotkeyColumn column = HotkeyColumn::kAction;
  bool ascending = true;
  if (ImGuiTableSortSpecs* const specs = ImGui::TableGetSortSpecs(); specs != nullptr) {
    if (specs->SpecsCount > 0) {
      column = specs->Specs[0].ColumnIndex == static_cast<int>(HotkeyColumn::kKey)
                   ? HotkeyColumn::kKey
                   : HotkeyColumn::kAction;
      ascending = specs->Specs[0].SortDirection != ImGuiSortDirection_Descending;
    }
    specs->SpecsDirty = false;
  }

  std::vector<std::pair<int, std::string_view>> families;
  std::vector<SortedRow> rows;
  rows.reserve(view.rows.size());
  for (int rank = 0; rank < static_cast<int>(view.rows.size()); ++rank) {
    const HotkeyRowView& row = view.rows[static_cast<std::size_t>(rank)];
    const std::pair<int, std::string_view> family(row.tab, row.group);
    auto known = std::ranges::find(families, family);
    if (known == families.end()) {
      known = families.insert(families.end(), family);
    }
    rows.push_back(SortedRow{.rank = rank,
                             .family = static_cast<int>(known - families.begin()),
                             .action_key = core::SearchKey(row.action),
                             .key_key = core::SearchKey(row.key)});
  }

  std::ranges::sort(rows, [&](const SortedRow& left, const SortedRow& right) {
    if (left.family != right.family) {
      return left.family < right.family;
    }
    int order = 0;
    if (column == HotkeyColumn::kKey) {
      if (left.key_key.empty() != right.key_key.empty()) {
        return right.key_key.empty();
      }
      order = core::CompareSearchKeys(left.key_key, right.key_key);
    } else {
      order = core::CompareSearchKeys(left.action_key, right.action_key);
    }
    if (order != 0) {
      return ascending ? order < 0 : order > 0;
    }
    if (column == HotkeyColumn::kKey) {
      if (const int by_action = core::CompareSearchKeys(left.action_key, right.action_key);
          by_action != 0) {
        return by_action < 0;
      }
    }
    return left.rank < right.rank;
  });

  std::vector<int> ranks;
  ranks.reserve(rows.size());
  for (const SortedRow& row : rows) {
    ranks.push_back(row.rank);
  }
  return ranks;
}

}  // namespace

HotkeyWindowAction DrawHotkeyWindow(HotkeyWindowState& state, const HotkeyWindowView& view,
                                    const SkinArt& art) {
  HotkeyWindowAction asked;
  if (!state.open) {
    // ⚠ UNE CAPTURE NE SURVIT PAS À LA FERMETURE. Laissée armée, elle avalerait
    // la prochaine touche du jeu sans que rien à l'écran ne l'explique.
    state.capturing = -1;
    return asked;
  }

  static WindowName name(kHotkeyWindowTitle, kHotkeyWindowId);
  ImGui::SetNextWindowSize(ImVec2(kFirstWidthPixels, kFirstHeightPixels), ImGuiCond_FirstUseEver);
  PoseBugReportButton(state.bug_report);
  if (!BeginSkinnedWindow(name.Get(), &state.open, art, SkinnedTitle::kBar)) {
    EndSkinnedWindow();
    return asked;
  }
  asked.report_bug = state.bug_report && TitleActionClicked(kBugReportRank);

  ImGui::SetNextItemWidth(-1.0F);
  ImGui::InputTextWithHint("##filtre", core::Tr("Rechercher une action ou une touche..."),
                           state.filter, sizeof(state.filter));

  if (state.tab >= static_cast<int>(view.tabs.size())) {
    state.tab = kAllHotkeyTabs;
  }
  if (BeginSkinTabBar("onglets", art)) {
    TabItem(state, kAllHotkeyTabs, core::Tr("Tout"), asked);
    for (int tab = 0; tab < static_cast<int>(view.tabs.size()); ++tab) {
      TabItem(state, tab, view.tabs[static_cast<std::size_t>(tab)], asked);
    }
    EndSkinTabBar();
  }
  state.tab_request.reset();

  if (state.capturing >= 0) {
    const auto waited = static_cast<std::size_t>(state.capturing);
    const char* hint = waited < view.rows.size() ? view.rows[waited].capture_hint : nullptr;
    FaintText(hint != nullptr
                  ? hint
                  : core::Tr("Appuyez sur une touche - Échap annule, Suppr retire le raccourci."));
  } else if (state.robbed[0] != '\0') {
    // 🔴 CE QU'ON VIENT DE PRENDRE, ET À QUI. La touche est VOLÉE à son
    // détenteur, comme dans le jeu ; à plus de cent lignes, il est presque
    // toujours hors de l'écran. Sans cette phrase, le joueur perd un raccourci
    // sans jamais l'apprendre. ⚠ PLUS SOUTENUE QUE LE TEXTE SECONDAIRE : c'est un
    // avertissement, pas une indication de routine.
    ColoredWrapped(PaintingLook().warn_text, state.robbed);
  } else {
    FaintText(core::Tr("Cliquez sur une touche pour la changer."));
  }
  ImGui::Separator();

  const float footer = SkinFooterHeight(art);
  if (ImGui::BeginChild("lignes", ImVec2(0.0F, -footer), ImGuiChildFlags_None)) {
    constexpr ImGuiTableFlags kFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                                       ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Sortable;
    const std::string_view tab_label = state.tab == kAllHotkeyTabs
                                           ? std::string_view()
                                           : view.tabs[static_cast<std::size_t>(state.tab)];
    bool any_shown = false;
    if (ImGui::BeginTable("raccourcis", 2, kFlags)) {
      ImGui::TableSetupColumn(core::Tr("Action"), ImGuiTableColumnFlags_WidthStretch |
                                                      ImGuiTableColumnFlags_DefaultSort);
      ImGui::TableSetupColumn(core::Tr("Touche"), ImGuiTableColumnFlags_WidthFixed,
                              Px(kKeyColumnPixels));
      ImGui::TableHeadersRow();
      const std::vector<int> ranks = SortedRanks(view);

      // La famille de la dernière ligne MONTRÉE. ⚠ CELLE QUI EST MONTRÉE, et
      // non celle de la ligne précédente : avec un filtre en cours, la première
      // case retenue d'une barre doit porter son intertitre même si les onze
      // autres sont écartées.
      std::string_view shown_group;
      const ImU32 group_ink = InkOn(kGroupTextColor, PaintingLook().body);
      for (const int rank : ranks) {
        const HotkeyRowView& row = view.rows[static_cast<std::size_t>(rank)];
        if (row.hidden || (state.tab != kAllHotkeyTabs && row.tab != state.tab)) {
          continue;
        }
        if (!core::AsciiContainsFolded(row.action, state.filter) &&
            !core::AsciiContainsFolded(row.key, state.filter)) {
          continue;
        }
        // Un intertitre qui répète le nom de l'onglet ne s'affiche pas.
        if (!row.group.empty() && row.group != tab_label &&
            (!any_shown || row.group != shown_group)) {
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ColoredText(group_ink, row.group);
          ImGui::TableNextColumn();
        }
        shown_group = row.group;
        any_shown = true;
        ImGui::PushID(rank);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        Unformatted(row.action);

        ImGui::TableNextColumn();
        // ⚠ LE BOUTON PORTE LA TOUCHE, et c'est lui qu'on clique pour la changer :
        // un champ de saisie inviterait à taper le NOM d'une touche, ce que
        // personne ne sait faire. Le client procède ainsi.
        std::array<char, 96> label{};
        const bool waiting = state.capturing == rank;
        std::snprintf(label.data(), label.size(), "%s###touche",
                      waiting           ? core::Tr("...")
                      : row.key.empty() ? core::Tr("(aucune)")
                                        : std::string(row.key).c_str());
        if (SkinSmallButton(label.data(), art, ImVec2(-1.0F, 0.0F))) {
          state.capturing = waiting ? -1 : rank;
        }
        // Le clic droit retire, sans passer par la capture : c'est le geste le
        // plus court pour délier une action qu'on ne veut plus.
        if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
          asked.cleared = rank;
          state.capturing = -1;
        }
        ImGui::PopID();
      }
      ImGui::EndTable();
    }
    if (!any_shown) {
      FaintText(core::Tr("Aucun résultat pour cette recherche."));
    }
    // L'ASCENSEUR DE L'ENFANT, PEINT ICI — tant qu'il est la fenêtre courante.
    // C'est le seul qui puisse apparaître dans une fenêtre habillée : celui de la
    // fenêtre elle-même est interdit, sa géométrie étant faussée par la barre de
    // titre qu'on peint. Sans cette ligne, la liste défile SANS RIEN MONTRER — la
    // palette rend les pièces d'ImGui transparentes, puisqu'on les peint.
    PaintWindowScrollbar(art);
  }
  ImGui::EndChild();

  if (SkinButton(core::Tr("Rétablir les touches d'origine"), art)) {
    asked.restore_defaults = true;
    state.capturing = -1;
  }

  EndSkinnedWindow();
  return asked;
}

}  // namespace ro::ui
