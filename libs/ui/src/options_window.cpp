#include "ro/ui/options_window.h"

#include <algorithm>
#include <array>
#include <cstddef>
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
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/window_name.h"

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
// Et ce qu'on lui passe pour enrouler au bord de la fenêtre : zéro.
constexpr float kWrapAtWindowEdge = 0.0F;

// Le marqueur d'aide, tel qu'il se mesure. ⚠ LE MÊME TEXTE QUE CELUI QUE
// `SkinHelpMarker` écrit : mesurer autre chose que ce qu'on dessine donne une
// rangée qui déborde d'un cheveu, ou un marqueur qui disparaît pour rien.
constexpr const char* kHelpMark = "(?)";

// La largeur d'un contrôle : de quoi loger un curseur avec ses deux flèches et
// la valeur qu'il affiche.
constexpr float kControlWidthPixels = 190.0F;

// L'identité de la boîte qui garde le geste destructeur. ⚠ ELLE NE SE MONTRE
// PAS : c'est une clé d'ImGui, pas un titre.
// ⚠ JAMAIS TRADUIT : c'est une clé ImGui, pas un texte.
constexpr const char* kDisplayBoxId = "##garder_affichage";
constexpr const char* kRestoreBoxId = "##rendre_les_defauts";
constexpr const char* kRestorePageBoxId = "##rendre_la_page";
// Celle d'une rangée qui agit. Constante parce qu'elle vit sous le `PushID` de
// la rangée, qui la rend unique.
constexpr const char* kActionBoxId = "##confirmer";
// Celle du bouton invisible d'un cadre qu'on manipule, sous la même pile.
constexpr const char* kCanvasId = "##cadre";
// Celle du volet de droite, et sa largeur.
constexpr const char* kSidePanelId = "##volet_options";
constexpr float kSidePanelWidthPixels = 620.0F;

// LE BANDEAU D'UN BLOC REPLIABLE, aux couleurs de la peau : un gris léger au
// repos, le gris des champs sous la souris. ⚠ LES COULEURS D'IMGUI SONT CELLES
// D'UN OUTIL, et un bandeau bleu vif se verrait plus que le bloc qu'il annonce.
constexpr ImU32 kFoldColor = IM_COL32(165, 170, 180, 90);
// L'air laissé au-dessus d'un bloc qui en suit un autre.
constexpr float kFoldGapPixels = 6.0F;
// Le seul bouton de la barre de titre, au rang de sa pose.
constexpr int kBugReportRank = 0;

// LE BANDEAU D'UN BLOC, et s'il est ouvert.
//
// ⚠ SON ÉTAT SE RANGE SOUS SON TITRE, HORS DE LA PILE D'IDENTITÉS : celle-ci
// porte l'onglet ouvert, et un bloc fermé sur un onglet se rouvrirait sur le
// suivant. Voir `OptionRowView::fold`.
[[nodiscard]] bool DrawFold(const char* title, bool first, const SkinLook& look) {
  ImGuiStorage* const storage = ImGui::GetStateStorage();
  const ImGuiID key = ImHashStr(title);
  const bool was_open = storage->GetBool(key, true);
  if (!first) {
    ImGui::Dummy(ImVec2(0.0F, kFoldGapPixels));
  }
  ImGui::PushStyleColor(ImGuiCol_Header, kFoldColor);
  ImGui::PushStyleColor(ImGuiCol_HeaderHovered, look.input);
  ImGui::PushStyleColor(ImGuiCol_HeaderActive, look.input);
  ImGui::PushStyleColor(ImGuiCol_Text, look.title_text);
  ImGui::SetNextItemOpen(was_open, ImGuiCond_Always);
  const bool open = ImGui::CollapsingHeader(title);
  ImGui::PopStyleColor(4);
  storage->SetBool(key, open);
  return open;
}

void FaintText(const char* text) {
  ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
  ImGui::TextUnformatted(text);
  ImGui::PopStyleColor();
}

[[nodiscard]] bool HasText(const char* text) { return text != nullptr && text[0] != '\0'; }

// Ce qui SUIT le contrôle : la commande équivalente, puis le marqueur d'aide.
//
// ⚠ LE LIBELLÉ, LUI, N'EST PAS ICI : chaque widget du skin pose le sien, à SA
// droite, comme le fait ImGui. Le lui reprendre pour le dessiner nous-mêmes
// obligerait à réaligner à la main ce qu'il aligne déjà.
//
// LE BOUTON DE RETOUR d'une rangée, en colonne derrière ce qui le précède.
// Voir `OptionRowView::revert`.
//
// ⚠ SON ART SE CENTRE SUR LA RANGÉE : un petit bouton est moins haut qu'une
// pastille, et calé sur le haut de la ligne il paraîtrait remonté.
void DrawRevert(const OptionRowView& row, float column, float used, float* widest,
                const SkinArt& art, OptionsWindowAction* asked) {
  if (row.revert == nullptr) {
    return;
  }
  const float window_x = ImGui::GetWindowPos().x;
  *widest = std::max(*widest, used - window_x);
  if (row.at_reference) {
    return;
  }
  const float middle_y = (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5F;
  ImGui::SameLine();
  ImGui::SetCursorScreenPos(
      ImVec2(std::max(used + ImGui::GetStyle().ItemSpacing.x, window_x + column),
             SkinSmallButtonTopCenteredOn(art, middle_y)));
  if (SkinSmallButton(row.revert, art)) {
    asked->changed = row.id;
    asked->revert = true;
  }
  if (row.revert_help != nullptr) {
    SkinTooltip(row.revert_help);
  }
}

// 🔴 LES COMMANDES D'UN BLOC SE POSENT EN COLONNE, sur `command_column` — un
// abscisse relatif à la fenêtre, zéro tant qu'on ne l'a pas mesuré. Au fil de
// chaque libellé, elles dansaient d'une rangée à l'autre. `widest` reçoit où
// finit ce qui les précède, pour mesurer la colonne de l'image suivante. Le
// bouton de retour se range de même, derrière l'aide.
void DrawRowTail(const OptionRowView& row, float command_column, float* widest, const SkinArt& art,
                 OptionsWindowAction* asked) {
  // Un bouton porte déjà son libellé ; il ne lui reste que l'aide.
  const ImGuiStyle& style = ImGui::GetStyle();
  // Le bord droit de la zone de contenu, relevé AVANT tout `SameLine` : le
  // curseur est alors en début de ligne suivante, donc `avail` porte la largeur
  // entière.
  const float edge = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
  // ⚠ L'OBJET D'UN BOUTON PASSE EN PREMIER, et sans condition de place : sans
  // lui, le bouton ne dit pas sur quoi il agit.
  if (row.object != nullptr) {
    ImGui::SameLine();
    ImGui::TextUnformatted(row.object);
  }
  // LA PRÉCISION SUIT L'OBJET, ou le bouton seul quand il n'en a pas : une
  // rangée qui n'a qu'une précision à dire la montre en texte secondaire.
  if (row.object_note != nullptr) {
    ImGui::SameLine();
    FaintText(row.object_note);
  }
  float used = ImGui::GetItemRectMax().x;

  // ⚠ LE MARQUEUR PASSE AVANT LA COMMANDE quand les deux ne tiennent pas. Il
  // porte une phrase entière ; la commande n'est qu'un rappel, et se retrouve
  // dans l'aide de toute façon. Une mise en garde seule a son marqueur aussi.
  const bool has_help = HasText(row.help) || HasText(row.help_warning);
  const float help_room = has_help ? style.ItemSpacing.x + ImGui::CalcTextSize(kHelpMark).x : 0.0F;
  if (row.command != nullptr) {
    const float window_x = ImGui::GetWindowPos().x;
    *widest = std::max(*widest, used - window_x);
    const float start = std::max(used + style.ItemSpacing.x, window_x + command_column);
    const float room = start - used + ImGui::CalcTextSize(row.command).x;
    if (used + room + help_room <= edge) {
      ImGui::SameLine();
      ImGui::SetCursorScreenPos(ImVec2(start, ImGui::GetCursorScreenPos().y));
      // ⚠ PAS TRADUITE. Une commande de chat est tapée telle quelle : la traduire
      // donnerait à lire un mot qui ne déclenche rien.
      FaintText(row.command);
      used += room;
    }
  }
  if (has_help && used + help_room <= edge) {
    ImGui::SameLine();
    SkinHelpMarker(row.help, row.help_warning);
    used = ImGui::GetItemRectMax().x;
  }
  DrawRevert(row, command_column, used, widest, art, asked);
}

// Le contrôle d'une ligne. Rend vrai si le joueur l'a bougé, et remplit la
// demande. ⚠ IL TRAVAILLE SUR UNE COPIE : la fenêtre ne possède aucun réglage,
// elle ne fait que dire ce qui a été demandé.
[[nodiscard]] bool DrawControl(const OptionRowView& row, const SkinArt& art,
                               OptionsWindowAction* asked, bool* covering) {
  // ⚠ UNE LARGEUR FIXE, ET NON LA PLACE RESTANTE. Un contrôle étiré jusqu'au
  // bord repousserait son libellé hors de la fenêtre ; et une colonne de
  // contrôles de MÊME largeur aligne les libellés d'elle-même, sans tableau.
  ImGui::SetNextItemWidth(kControlWidthPixels * ImGui::GetStyle().FontScaleMain);
  switch (row.kind) {
    case OptionKind::kToggle: {
      bool on = row.on;
      if (SkinCheckBox(row.label, &on, art)) {
        asked->changed = row.id;
        asked->on = on;
        return true;
      }
      return false;
    }
    case OptionKind::kSlider: {
      // ⚠ CE QUE LE WIDGET REND FAIT FOI, ET RIEN D'AUTRE. Les flèches et la
      // molette éditent un curseur sans jamais le rendre ACTIF — voir
      // `skin_slider.cpp` —, donc tout ce qui attendrait `IsItemActive` ou un
      // relâchement laisserait tomber ces deux gestes en silence.
      float value = row.value;
      if (SkinSliderFloat(row.label, &value, row.lowest, row.highest, art, row.format)) {
        asked->changed = row.id;
        asked->value = value;
        return true;
      }
      return false;
    }
    case OptionKind::kChoice: {
      int chosen = row.chosen;
      if (SkinCombo(row.label, &chosen, row.choices, art)) {
        asked->changed = row.id;
        asked->chosen = chosen;
        return true;
      }
      return false;
    }
    case OptionKind::kNote:
      // ⚠ EN TEXTE SECONDAIRE : elle informe, elle ne demande rien. De la même
      // couleur qu'un réglage, elle se lirait comme un libellé dont le contrôle
      // aurait disparu.
      //
      // ⚠ ET ELLE S'ENROULE, elle seule : le volet coupe l'enroulement pour
      // qu'un LIBELLÉ reste sur sa ligne, mais une note est un paragraphe — sans
      // enroulement, elle file sous le bord droit et ne se lit qu'à moitié.
      ImGui::PushStyleColor(ImGuiCol_Text, row.warning
                                               ? ImGui::ColorConvertU32ToFloat4(art.look().bad_text)
                                               : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
      ImGui::PushTextWrapPos(kWrapAtWindowEdge);
      ImGui::TextUnformatted(row.label);
      ImGui::PopTextWrapPos();
      ImGui::PopStyleColor();
      return false;
    case OptionKind::kCanvas: {
      // ⚠ LA PLACE SE RÉSERVE AVANT DE PEINDRE, par un élément vide : c'est lui
      // qui fait descendre la rangée suivante. Et la peinture se borne au
      // cadre — une barre posée très haut ne déborde pas sur la rangée d'au-dessus.
      const ImVec2 min = ImGui::GetCursorScreenPos();
      const float height =
          row.canvas_height > 0.0F ? row.canvas_height : ImGui::GetContentRegionAvail().y;
      const ImVec2 max(min.x + ImGui::GetContentRegionAvail().x, min.y + height);
      // ⚠ UN CADRE QU'ON MANIPULE EST UN BOUTON INVISIBLE, et non un élément
      // vide : sans élément actif sous la souris, tirer dans le cadre traînerait
      // la fenêtre entière. Le cadre lit ensuite ce bouton — survolé, actif.
      if (row.edit) {
        ImGui::InvisibleButton(
            kCanvasId, ImVec2(max.x - min.x, height),
            ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
      } else {
        ImGui::Dummy(ImVec2(max.x - min.x, height));
      }
      ImDrawList* into = ImGui::GetWindowDrawList();
      into->PushClipRect(min, max, true);
      std::optional<OptionCanvasMove> moved;
      if (row.edit) {
        moved = row.edit(*into, min, max);
      } else if (row.paint) {
        row.paint(*into, min, max);
      }
      into->PopClipRect();
      if (moved.has_value()) {
        asked->changed = moved->anchor;
        asked->dragged = moved->copy_to < 0;
        asked->point = moved->point;
        asked->copy_to = moved->copy_to;
        return true;
      }
      return false;
    }
    case OptionKind::kAction: {
      const bool pressed =
          row.small_button ? SkinSmallButton(row.label, art) : SkinButton(row.label, art);
      if (row.confirm == nullptr) {
        if (pressed) {
          asked->changed = row.id;
          return true;
        }
        return false;
      }
      // ⚠ LE GESTE DESTRUCTEUR DEMANDE, IL N'AGIT PAS. L'identifiant peut être
      // constant : on est sous le `PushID` de la rangée, donc il est déjà
      // unique dans la fenêtre.
      if (pressed) {
        AskModal(kActionBoxId);
      }
      if (DrawModalBox(kActionBoxId, row.confirm, core::Tr("Oui"), core::Tr("Non"), art) ==
          ModalAnswer::kYes) {
        asked->changed = row.id;
        return true;
      }
      return false;
    }
    case OptionKind::kPanel:
      if (row.panel && row.panel(art)) {
        asked->changed = row.id;
        return true;
      }
      return false;
    case OptionKind::kText: {
      // ⚠ RECOPIÉ DE LA RANGÉE À CHAQUE IMAGE : la saisie en cours vit chez
      // ImGui, et ce qu'elle rend revient par l'appelant à l'image suivante.
      // Un texte trop long se coupe à une frontière de caractère : une frappe
      // enregistrerait sinon un caractère tronqué.
      std::array<char, kOptionTextBytes> typed{};
      const std::string_view whole = row.text != nullptr ? row.text : "";
      const std::string_view held =
          core::WithoutDanglingUtf8(whole.substr(0, std::min(whole.size(), typed.size() - 1)));
      std::ranges::copy(held, typed.begin());
      if (ImGui::InputText(row.label, typed.data(), typed.size())) {
        asked->changed = row.id;
        asked->text = typed.data();
        return true;
      }
      return false;
    }
    case OptionKind::kColor: {
      ImU32 color = row.color;
      const SkinColorPick picked = SkinColorSwatch(row.label, &color, art);
      // 🔴 LE SÉLECTEUR RECOUVRE LES RANGÉES DU DESSOUS, et c'est
      // inévitable : un popup s'ouvre sous l'élément qui l'appelle. On le dit
      // à la section, qui neutralise la suite — viser le nuancier ne doit pas
      // revenir à attraper un curseur resté dessous.
      *covering = picked.open;
      if (picked.changed) {
        asked->changed = row.id;
        asked->color = color;
        return true;
      }
      return false;
    }
  }
  return false;
}

// UNE SUITE DE RANGÉES.
//
// ⚠ `covered` TRAVERSE LES DEUX APPELS d'une section, et ce n'est pas un détail
// de forme : un nuancier ouvert dans les réglages communs recouvre les onglets
// ET les lignes qui suivent. Le remettre à faux entre les deux rendrait
// cliquable ce qu'on ne voit pas.
//
// ⚠ LA COLONNE DES COMMANDES SE MESURE À L'IMAGE PRÉCÉDENTE, bloc par bloc —
// un bloc court d'un intertitre au suivant. Elle n'est connue qu'une fois les
// libellés posés, et une image de retard ne se voit pas ; la garder dans l'état
// d'ImGui la laisse suivre la langue et l'échelle sans rien retenir ailleurs.
void DrawRows(const char* scope, std::span<const OptionRowView> rows, const SkinArt& art,
              OptionsWindowAction* asked, bool* covered) {
  ImGui::PushID(scope);
  ImGuiStorage* const columns = ImGui::GetStateStorage();
  int block = 0;
  float widest = 0.0F;
  const auto flush = [&] {
    ImGui::PushID(block);
    columns->SetFloat(ImGui::GetID("colonne"),
                      widest > 0.0F ? widest + ImGui::GetStyle().ItemSpacing.x : 0.0F);
    ImGui::PopID();
  };
  const auto column_of = [&] {
    ImGui::PushID(block);
    const float column = columns->GetFloat(ImGui::GetID("colonne"), 0.0F);
    ImGui::PopID();
    return column;
  };
  float column = column_of();
  // Dans un bloc fermé, les rangées ne se dessinent pas jusqu'au bloc suivant.
  bool folded = false;
  for (const OptionRowView& row : rows) {
    if (row.fold != nullptr) {
      ImGui::PushID(row.id);
      folded = !DrawFold(row.fold, &row == rows.data(), art.look());
      ImGui::PopID();
    }
    if (folded) {
      continue;
    }
    if (row.heading != nullptr && &row != rows.data()) {
      flush();
      ++block;
      widest = 0.0F;
      column = column_of();
    }
    ImGui::PushID(row.id);
    // ⚠ LE TITRE AVANT LE GRISAGE : un intertitre éteint ne se lit plus, et
    // c'est justement lui qui dit POURQUOI ce qui suit est éteint.
    if (row.heading != nullptr) {
      ImGui::SeparatorText(row.heading);
    }
    if (row.indented) {
      ImGui::Indent();
    }
    // ⚠ APRÈS L'INDENTATION : `Indent` ramène le curseur en début de ligne, et
    // défairait la suite qu'on vient de demander.
    if (row.same_line && &row != rows.data()) {
      ImGui::SameLine();
    }
    ImGui::BeginDisabled(*covered || row.disabled);
    bool covering = false;
    static_cast<void>(DrawControl(row, art, asked, &covering));
    DrawRowTail(row, column, &widest, art, asked);
    ImGui::EndDisabled();
    if (row.indented) {
      ImGui::Unindent();
    }
    // ⚠ UNE RANGÉE GRISÉE NE RECOUVRE RIEN : son nuancier ne s'ouvre pas.
    *covered = *covered || (covering && !row.disabled);
    ImGui::PopID();
  }
  flush();
  ImGui::PopID();
}

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
void DrawSection(const OptionSectionView& section, OptionsWindowState& state, const SkinArt& art,
                 bool offers_links, OptionsWindowAction* asked) {
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
    DrawRows("epingle", section.pinned_rows, art, asked, &covered);
    ImGui::PopID();
    ImGui::BeginChild("defile", ImVec2(0.0F, 0.0F), ImGuiChildFlags_None);
  }
  // CE QUI VAUT POUR TOUS LES ONGLETS, AVANT EUX. Voir
  // `OptionSectionView::shared_rows` : c'est la PLACE qui dit qu'un réglage est
  // commun, rien d'autre ne le peut.
  ImGui::PushID(state.section);
  DrawRows("communs", section.shared_rows, art, asked, &covered);
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
  DrawRows("lignes", section.rows, art, asked, &covered);
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
    DrawRows("volet", section.side_rows, art, asked, &covered);
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

  // ⚠ LES ENTRÉES TRAVERSENT EN TAMPON. La nav n'en garde rien ; les rassembler
  // ici les fait vivre le temps de l'appel, et lui évite d'apprendre ce qu'est
  // une section de réglages.
  std::vector<NavEntry> titles;
  titles.reserve(sections.size());
  for (const OptionSectionView& section : sections) {
    titles.push_back(NavEntry{.label = section.title, .group = section.group});
  }
  // ⚠ L'INDICE SE REBORNE À CHAQUE IMAGE : une section retirée d'une version à
  // l'autre laisserait sinon un indice qui désigne le vide, et le fichier de
  // réglages du joueur porte cet indice.
  const int last = static_cast<int>(titles.size()) - 1;
  state.section = std::clamp(state.section, 0, std::max(0, last));

  const float footer = SkinFooterHeight(art) +
                       (words.footnote != nullptr ? ImGui::GetTextLineHeightWithSpacing() : 0.0F);
  if (ImGui::BeginChild("volets", ImVec2(0.0F, -footer), ImGuiChildFlags_None)) {
    const int was = state.section;
    BeginNavPanel("sections", titles, &state.section, art,
                  words.offers_links ? &asked.link_section : nullptr);
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
    if (!sections.empty()) {
      DrawSection(sections[static_cast<std::size_t>(state.section)], state, art, words.offers_links,
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
          .reports_bugs = true});
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
