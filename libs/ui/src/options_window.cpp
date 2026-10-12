#include "ro/ui/options_window.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <string_view>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include "ro/core/i18n.h"
#include "ro/core/text.h"
#include "ro/ui/bug_report_window.h"
#include "ro/ui/chat_link_offer.h"
#include "ro/ui/modal_box.h"
#include "ro/ui/nav_panel.h"
#include "ro/ui/options_search.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/window_name.h"

#include "options_rows.h"

namespace ro::ui {
namespace {

// ⚠ ASSEZ LARGE POUR UNE RANGÉE ENTIÈRE : le contrôle, son libellé, la
// commande et le marqueur. Plus étroite, la fenêtre reste utilisable — la
// commande cède la place d'elle-même — mais elle s'ouvre sur un panneau
// amputé, ce qui se lit comme un défaut.
constexpr float kFirstWidthPixels = 640.0F;
constexpr float kFirstHeightPixels = 360.0F;
// Ce qu'on passe à `PushTextWrapPos` pour qu'il n'enroule RIEN. Un zéro y
// signifie « à la largeur de la fenêtre », ce qui est l'inverse de ce qu'on
// veut : il faut une valeur négative.
constexpr float kNoTextWrap = -1.0F;

// L'identité de la boîte qui garde le geste destructeur. ⚠ ELLE NE SE MONTRE
// PAS : c'est une clé d'ImGui, pas un titre.
// ⚠ JAMAIS TRADUIT : c'est une clé ImGui, pas un texte.
constexpr const char* kDisplayBoxId = "##garder_affichage";
constexpr const char* kRestoreBoxId = "##rendre_les_defauts";
constexpr const char* kRestorePageBoxId = "##rendre_la_page";
// Celle du volet de droite, et sa largeur.
constexpr const char* kSidePanelId = "##volet_options";
constexpr float kSidePanelWidthPixels = 620.0F;

// Le seul bouton de la barre de titre, au rang de sa pose.
constexpr int kBugReportRank = 0;
// De quoi écrire « Résultats : » et un compte, dans toutes les langues.
constexpr std::size_t kFoundTextBytes = 64;

// UNE SECTION : ses réglages, le contrôle d'abord et le libellé ensuite.
//
// ⚠ CE SENS-LÀ, ET C'EST CELUI DU CLIENT. Un panneau qui met le libellé à
// gauche et le contrôle à droite fait parcourir toute la largeur pour savoir ce
// qu'une case vaut ; les cases alignées à gauche se lisent en une colonne, et le
// mot est juste à côté de ce qu'il nomme.
//
// ⚠ ET IL N'Y A PAS DE TABLEAU. Les contrôles ont tous la même largeur, donc
// les libellés s'alignent d'eux-mêmes — un tableau ne ferait que reprendre aux
// widgets le libellé qu'ils savent poser.
//
// SOUS UN FILTRE, les réglages communs et les lignes ne gardent que ce qu'il
// trouve — sauf quand le titre de la page l'est (`whole`). Ce qui ne défile pas
// et le volet restent entiers : ce sont des aperçus, pas des réglages.
void DrawSection(const OptionSectionView& section, OptionsWindowState& state, const SkinArt& art,
                 bool offers_links, const OptionsSearch& search, bool whole,
                 OptionsWindowAction* asked) {
  const bool filtered = search.active() && !whole;
  const std::vector<bool> shared_kept =
      filtered ? KeptRows(section.shared_rows, search, false) : std::vector<bool>{};
  const std::vector<bool> rows_kept =
      filtered ? KeptRows(section.rows, search, false) : std::vector<bool>{};
  // 🔴 CE QU'UN SÉLECTEUR DÉROULÉ RECOUVRE EST NEUTRALISÉ. Il s'ouvre
  // sous sa pastille et mange les rangées suivantes ; les laisser vivantes
  // ferait attraper un curseur invisible en visant une couleur.
  // ⚠ RESSERRÉ : un panneau de réglages est dense par nature, et les écarts
  // par défaut d'ImGui y font défiler pour trouver ce qui tiendrait à l'écran.
  PushCompactStyle();
  // 🔴 ET LE TEXTE NE S'ENROULE PAS ICI. Le volet pousse un
  // enroulement à sa largeur, et il a raison : une aide d'une phrase tient
  // rarement sur une ligne. Mais une RANGÉE de réglage EST une ligne —
  // enroulée, son libellé descend une lettre par ligne le long du bord, et
  // la rangée suivante se retrouve dix lignes plus bas.
  ImGui::PushTextWrapPos(kNoTextWrap);
  bool covered = false;
  // CE QUI NE DÉFILE PAS, puis un volet pour tout le reste, qui défile seul.
  // Voir `OptionSectionView::pinned_rows`.
  const bool pinned = !section.pinned_rows.empty();
  if (pinned) {
    ImGui::PushID(state.section);
    DrawOptionRows("epingle", section.pinned_rows, art, asked, &covered, nullptr,
                   &state.row_wanted);
    ImGui::PopID();
    ImGui::BeginChild("defile", ImVec2(0.0F, 0.0F), ImGuiChildFlags_None);
  }
  // CE QUI VAUT POUR TOUS LES ONGLETS, AVANT EUX. Voir
  // `OptionSectionView::shared_rows` : c'est la PLACE qui dit qu'un réglage est
  // commun, rien d'autre ne le peut.
  ImGui::PushID(state.section);
  DrawOptionRows("communs", section.shared_rows, art, asked, &covered,
                 filtered ? &shared_kept : nullptr, &state.row_wanted);
  ImGui::PopID();
  // LA SECONDE NAVIGATION, quand la section en porte une : une rangée d'onglets
  // au-dessus de ses lignes. Voir `OptionSectionView::tabs`.
  //
  // ⚠ LA FENÊTRE NE SAIT PAS CE QU'ILS DÉSIGNENT. Elle montre les libellés reçus
  // et retient le rang choisi ; c'est l'appelant qui, à l'image suivante, ne
  // compose que les lignes de cet onglet-là.
  if (!section.tabs.empty()) {
    const auto last = static_cast<int>(section.tabs.size()) - 1;
    state.section_tab = std::clamp(state.section_tab, 0, last);
    // ⚠ UNE IDENTITÉ PAR SECTION : le nom du jeu d'onglets est le même pour
    // toutes, donc deux sections qui en porteraient partageraient l'onglet
    // ouvert — et changer de section montrerait la page de l'autre.
    ImGui::PushID(state.section);
    if (ImGui::BeginTabBar("pages", ImGuiTabBarFlags_FittingPolicyScroll)) {
      for (int rank = 0; rank <= last; ++rank) {
        // 🔴 L'ONGLET OUVERT EST CELUI QU'IMGUI TIENT, et on se contente de le
        // relever. Imposer le rang de l'état À CHAQUE IMAGE — ce que faisait la
        // première version — annulait le clic aussitôt reçu : `SetSelected` est
        // un ORDRE, pas une description, et il rouvrait la première page avant
        // même que le rang n'ait changé. Les onglets restaient bloqués sur elle.
        //
        // ⚠ ON N'IMPOSE QUE CE QUI EST DEMANDÉ, et une seule fois : `wanted`
        // porte un rang à ouvrir, puis s'efface. C'est par là que passe le
        // retour au premier onglet quand on change de section, et c'est par là
        // que passera l'ouverture du panneau SUR une barre donnée.
        const ImGuiTabItemFlags flags =
            state.section_tab_wanted == rank ? ImGuiTabItemFlags_SetSelected : 0;
        const bool opened =
            ImGui::BeginTabItem(section.tabs[static_cast<std::size_t>(rank)], nullptr, flags);
        // ⚠ SUR LA LANGUETTE, avant son contenu : voir `OfferChatLink`. Le Maj +
        // clic la sélectionne aussi au passage, et montre ce qu'on vient de poser.
        if (OfferChatLink(offers_links)) {
          asked->link_section = state.section;
          asked->link_tab = rank;
        }
        if (opened) {
          state.section_tab = rank;
          ImGui::EndTabItem();
        }
      }
      ImGui::EndTabBar();
    }
    ImGui::PopID();
  }
  // ⚠ SOUS L'IDENTITÉ DE LA PAGE : les colonnes mesurées se rangent par page,
  // et une autre page ne prête pas la sienne.
  ImGui::PushID(state.section);
  ImGui::PushID(state.section_tab);
  DrawOptionRows("lignes", section.rows, art, asked, &covered, filtered ? &rows_kept : nullptr,
                 &state.row_wanted);
  ImGui::PopID();
  ImGui::PopID();
  if (section.body) {
    // LES ONGLETS QUE LA PAGE DESSINE ELLE-MÊME reçoivent la demande, quand la
    // section n'en porte pas qui l'auraient déjà prise, et rendent leur lien
    // comme ceux de la section.
    OptionBodyTabs tabs{.wanted = section.tabs.empty() ? state.section_tab_wanted : -1,
                        .offers_links = offers_links};
    ImGui::PushID(state.section);
    ImGui::BeginDisabled(covered);
    section.body(art, tabs);
    ImGui::EndDisabled();
    ImGui::PopID();
    if (tabs.link >= 0) {
      asked->link_section = state.section;
      asked->link_tab = tabs.link;
    }
  }
  // ⚠ CONSOMMÉ QUOI QU'IL ARRIVE, y compris si le rang demandé n'existe pas :
  // une demande qui survivrait rouvrirait sa page à chaque image, et l'on
  // serait bloqué dessus.
  state.section_tab_wanted = -1;
  state.row_wanted = -1;
  if (pinned) {
    PaintWindowScrollbar(art);
    ImGui::EndChild();
  }
  ImGui::PopTextWrapPos();
  PopCompactStyle();
}

// LE VOLET DE DROITE d'une section qui en porte un : collé au bord de la
// fenêtre, de toute sa hauteur. Voir `OptionSectionView::side_rows`.
//
// ⚠ IL SUIT LA FENÊTRE, il ne se déplace ni ne se retaille seul : c'est une
// rallonge de la page, pas une fenêtre de plus. Sans place à droite de l'écran,
// il passe à gauche.
void DrawSidePanel(const OptionSectionView& section, const OptionsWindowState& state,
                   ImVec2 window_at, ImVec2 window_size, const SkinArt& art,
                   OptionsWindowAction* asked) {
  if (section.side_rows.empty()) {
    return;
  }
  const float width = kSidePanelWidthPixels * ImGui::GetStyle().FontScaleMain;
  float left = window_at.x + window_size.x;
  if (left + width > ImGui::GetIO().DisplaySize.x && window_at.x >= width) {
    left = window_at.x - width;
  }
  ImGui::SetNextWindowPos(ImVec2(left, window_at.y));
  ImGui::SetNextWindowSize(ImVec2(width, window_size.y));
  constexpr ImGuiWindowFlags kPanelFlags =
      ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoFocusOnAppearing;
  if (BeginSkinnedWindow(kSidePanelId, nullptr, art, SkinnedTitle::kNone, kPanelFlags)) {
    PushCompactStyle();
    ImGui::PushTextWrapPos(kNoTextWrap);
    bool covered = false;
    ImGui::PushID(state.section);
    DrawOptionRows("volet", section.side_rows, art, asked, &covered, nullptr, nullptr);
    ImGui::PopID();
    ImGui::PopTextWrapPos();
    PopCompactStyle();
  }
  EndSkinnedWindow();
}

// CE QUI DISTINGUE UN PANNEAU DE RÉGLAGES D'UN AUTRE, hors de ses pages.
struct SettingsWindowWords {
  // La question de « Rétablir les réglages d'origine » : elle dit ce qui sera
  // rendu, et un panneau qui ne montre qu'une partie ne rend que celle-là.
  const char* restore_all_question = nullptr;
  // Une ligne en texte secondaire au-dessus des boutons du bas, ou nul.
  const char* footnote = nullptr;
  // Le Maj + clic d'une entrée ou d'un onglet pose son lien dans le chat.
  bool offers_links = false;
  // La barre de titre porte « Signaler un bug », quand le réglage le veut.
  bool reports_bugs = false;
  // UN CHAMP DE FILTRE au-dessus de la navigation. ⚠ PAS À L'ÉCRAN DE CONNEXION :
  // ses trois pages se lisent d'un coup d'œil.
  bool filters = false;
};

// LE CORPS D'UN PANNEAU DE RÉGLAGES, quel qu'il soit : la nav, les lignes, et
// le geste qui rend les valeurs d'origine.
[[nodiscard]] OptionsWindowAction DrawSettingsWindow(WindowName& name, OptionsWindowState& state,
                                                     std::span<const OptionSectionView> sections,
                                                     const SkinArt& art,
                                                     const SettingsWindowWords& words) {
  OptionsWindowAction asked;
  if (!state.open) {
    // ⚠ LA QUESTION DU FILET SE REPOSERA À LA RÉOUVERTURE : fermée, la fenêtre
    // ne la montre plus, et un souvenir de « déjà demandée » laisserait le
    // temps répondre sans qu'elle soit revue.
    state.revert_asked = false;
    // ⚠ LE FILTRE AUSSI : rouverte, la fenêtre montre toutes ses pages. Un mot
    // tapé la veille cacherait celles qu'on vient chercher.
    state.filter.front() = '\0';
    return asked;
  }

  ImGui::SetNextWindowSize(ImVec2(kFirstWidthPixels, kFirstHeightPixels), ImGuiCond_FirstUseEver);
  const bool bug_button = words.reports_bugs && state.bug_report;
  PoseBugReportButton(bug_button);
  if (!BeginSkinnedWindow(name.Get(), &state.open, art, SkinnedTitle::kBar)) {
    EndSkinnedWindow();
    return asked;
  }
  asked.report_bug = bug_button && TitleActionClicked(kBugReportRank);

  // ⚠ L'INDICE SE REBORNE À CHAQUE IMAGE : une section retirée d'une version à
  // l'autre laisserait sinon un indice qui désigne le vide.
  const int last = static_cast<int>(sections.size()) - 1;
  state.section = std::clamp(state.section, 0, std::max(0, last));

  // CE QUE LE FILTRE TROUVE, section par section. Voir `options_search.h`.
  // Ctrl+F le rejoint d'où qu'on soit dans la fenêtre.
  const bool focus_filter =
      words.filters && ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_F, ImGuiInputFlags_RouteFocused);
  const OptionsSearch search(words.filters ? state.filter.data() : "");
  std::vector<SectionFinding> findings(sections.size());
  std::vector<bool> shown(sections.size(), true);
  int found_settings = 0;
  if (search.active()) {
    for (std::size_t rank = 0; rank < sections.size(); ++rank) {
      findings[rank] = FindInSection(sections[rank], search);
      shown[rank] = findings[rank].shown;
      found_settings += shown[rank] ? findings[rank].settings : 0;
    }
  }
  // ⚠ LES ENTRÉES TRAVERSENT EN TAMPON. La nav n'en garde rien ; les rassembler
  // ici les fait vivre le temps de l'appel, et lui évite d'apprendre ce qu'est
  // une section de réglages.
  const std::vector<NavEntry> titles = SortedNavEntries(sections, shown);
  // LA PAGE OUVERTE QUE LE FILTRE ÉCARTE CÈDE LA PLACE à la première trouvée :
  // garder sous les yeux une page sans résultat ferait croire qu'il n'y en a
  // nulle part.
  if (!titles.empty() && !shown[static_cast<std::size_t>(state.section)]) {
    state.section = titles.front().target;
  }
  std::array<char, kFoundTextBytes> found_text{};
  if (search.active()) {
    std::snprintf(found_text.data(), found_text.size(), core::Tr("Résultats : %d"), found_settings);
  }
  NavFilter filter{.typed = state.filter,
                   .hint = core::Tr("Filtrer..."),
                   .found = search.active() ? found_text.data() : nullptr,
                   .focus = focus_filter};

  const float footer = SkinFooterHeight(art) +
                       (words.footnote != nullptr ? ImGui::GetTextLineHeightWithSpacing() : 0.0F);
  if (ImGui::BeginChild("volets", ImVec2(0.0F, -footer), ImGuiChildFlags_None)) {
    const int was = state.section;
    BeginNavPanel("sections", titles, &state.section, art,
                  words.offers_links ? &asked.link_section : nullptr,
                  words.filters ? &filter : nullptr);
    // ⚠ APRÈS LA NAV, PAS AVANT : c'est elle qui change la section, et le test
    // posé plus haut n'aurait vu que le rebornage. Changer de section repart de
    // son PREMIER onglet — l'onglet 6 d'une section n'a rien à voir avec
    // l'onglet 6 d'une autre.
    if (state.section != was) {
      state.section_tab = 0;
      // ⚠ ON DEMANDE, on n'écrit pas : c'est ImGui qui tient l'onglet ouvert, et
      // il garde celui de la section qu'on vient de quitter tant qu'on ne lui a
      // pas dit d'en ouvrir un autre.
      state.section_tab_wanted = 0;
    }
    if (titles.empty() && search.active()) {
      ImGui::TextDisabled("%s", core::Tr("Aucun réglage ne correspond à ce filtre."));
    } else if (!sections.empty()) {
      const auto rank = static_cast<std::size_t>(state.section);
      DrawSection(sections[rank], state, art, words.offers_links, search, findings[rank].whole,
                  &asked);
    }
    EndNavPanel(art);
  }
  ImGui::EndChild();

  if (words.footnote != nullptr) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextUnformatted(words.footnote);
    ImGui::PopStyleColor();
  }

  // ⚠ LE GESTE DESTRUCTEUR DEMANDE, IL N'AGIT PAS. Il rend au joueur les
  // réglages d'origine — tous, d'un coup, sans retour possible : un clic de
  // trop le long du bord d'une fenêtre ne doit pas les emporter.
  if (SkinButton(core::Tr("Rétablir les réglages d'origine"), art)) {
    AskModal(kRestoreBoxId);
  }
  if (DrawModalBox(kRestoreBoxId, words.restore_all_question, core::Tr("Oui"), core::Tr("Non"),
                   art) == ModalAnswer::kYes) {
    asked.restore_defaults = true;
  }
  // LA PAGE OUVERTE SEULE, à côté : même garde, parce que c'est le même geste
  // sans retour, en plus petit.
  if (!sections.empty() && sections[static_cast<std::size_t>(state.section)].restorable) {
    ImGui::SameLine();
    if (SkinButton(core::Tr("Rétablir cette page"), art)) {
      AskModal(kRestorePageBoxId);
    }
  }
  // FERMER, au bout de la même ligne, du côté de la poignée de redimension :
  // la place qu'il tient dans les autres fenêtres.
  const char* const close = core::Tr("Fermer");
  const float close_width = SkinButtonWidth(close, art);
  ImGui::SameLine();
  ImGui::SetCursorPosX(
      std::max(ImGui::GetCursorPosX(), ImGui::GetContentRegionMax().x - close_width));
  if (SkinButton(close, art, ImVec2(close_width, 0.0F))) {
    state.open = false;
  }
  if (DrawModalBox(kRestorePageBoxId,
                   core::Tr("Rendre aux réglages de cette page leur valeur d'origine ?"),
                   core::Tr("Oui"), core::Tr("Non"), art) == ModalAnswer::kYes) {
    asked.restore_section = state.section;
  }

  // ── LE FILET DE L'AFFICHAGE ───────────────────────────────────────────────
  //
  // ⚠ DESSINÉ DANS CETTE FENÊTRE, mais c'est un SURGISSANT : il se centre sur
  // l'écran et non sur elle. Le joueur qui vient de changer sa résolution a
  // forcément ce panneau ouvert — c'est de là qu'il l'a changée.
  if (state.revert_seconds >= 0) {
    // ⚠ DEMANDÉE UNE FOIS PAR ARMEMENT. Voir `OptionsWindowState::revert_asked`
    // : dessiner un surgissant ne l'ouvre pas, et une boîte jamais ouverte
    // laisse le temps répondre « non » à la place du joueur.
    if (!state.revert_asked) {
      state.revert_asked = true;
      AskModal(kDisplayBoxId);
    }
    switch (DrawRevertBox(kDisplayBoxId, core::Tr("Garder ce réglage d'affichage ?"),
                          core::Tr("Vos fenêtres suivent ce réglage. Revenir rétablit l'affichage "
                                   "précédent, et leur place avec."),
                          core::Tr("Garder"), core::Tr("Revenir"), state.revert_seconds, art)) {
      case ModalAnswer::kYes:
        asked.keep_display = true;
        break;
      case ModalAnswer::kNo:
        asked.revert_display = true;
        break;
      case ModalAnswer::kWaiting:
        break;
    }
  } else {
    state.revert_asked = false;
  }

  const ImVec2 window_at = ImGui::GetWindowPos();
  const ImVec2 window_size = ImGui::GetWindowSize();
  EndSkinnedWindow();
  if (!sections.empty()) {
    DrawSidePanel(sections[static_cast<std::size_t>(state.section)], state, window_at, window_size,
                  art, &asked);
  }
  return asked;
}

}  // namespace

OptionsWindowAction DrawOptionsWindow(OptionsWindowState& state,
                                      std::span<const OptionSectionView> sections,
                                      const SkinArt& art) {
  // ⚠ UN NOM QUI DURE : c'est lui qui porte l'identité sous laquelle ImGui
  // retient la place et la taille de la fenêtre.
  static WindowName name(kOptionsWindowTitle, kOptionsWindowId);
  return DrawSettingsWindow(
      name, state, sections, art,
      SettingsWindowWords{
          .restore_all_question = core::Tr("Rendre à tous les réglages leur valeur d'origine ?"),
          .offers_links = true,
          .reports_bugs = true,
          .filters = true});
}

OptionsWindowAction DrawLoginSettingsWindow(OptionsWindowState& state,
                                            std::span<const OptionSectionView> sections,
                                            const SkinArt& art) {
  static WindowName name(kLoginSettingsWindowTitle, kLoginSettingsWindowId);
  return DrawSettingsWindow(
      name, state, sections, art,
      SettingsWindowWords{.restore_all_question =
                              core::Tr("Rendre aux réglages de ces pages leur valeur d'origine ?"),
                          .footnote = core::Tr("Le reste se règle en jeu : Échap, puis Options.")});
}

}  // namespace ro::ui
