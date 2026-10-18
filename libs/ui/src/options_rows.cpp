// LES RANGÉES D'UNE PAGE DE RÉGLAGES. Voir `options_rows.h`.

#include "options_rows.h"

#include <algorithm>
#include <array>
#include <optional>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>

#include "ro/core/i18n.h"
#include "ro/core/text.h"
#include "ro/ui/modal_box.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_widgets.h"

#include "ui_scale.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

// Ce qu'on passe à `PushTextWrapPos` pour enrouler au bord de la fenêtre : zéro.
constexpr float kWrapAtWindowEdge = 0.0F;

// Le marqueur d'aide, tel qu'il se mesure. ⚠ LE MÊME TEXTE QUE CELUI QUE
// `SkinHelpMarker` écrit : mesurer autre chose que ce qu'on dessine donne une
// rangée qui déborde d'un cheveu, ou un marqueur qui disparaît pour rien.
constexpr const char* kHelpMark = "(?)";

// La largeur d'un contrôle : de quoi loger un curseur avec ses deux flèches et
// la valeur qu'il affiche.
constexpr float kControlWidthPixels = 190.0F;

// L'identité de la boîte d'une rangée qui agit. ⚠ JAMAIS TRADUITE : c'est une
// clé ImGui. Constante parce qu'elle vit sous le `PushID` de la rangée, qui la
// rend unique.
constexpr const char* kActionBoxId = "##confirmer";
// Celle du bouton invisible d'un cadre qu'on manipule, sous la même pile.
constexpr const char* kCanvasId = "##cadre";

// LE BANDEAU D'UN BLOC REPLIABLE, aux couleurs de la peau : un gris léger au
// repos, le gris des champs sous la souris. ⚠ LES COULEURS D'IMGUI SONT CELLES
// D'UN OUTIL, et un bandeau bleu vif se verrait plus que le bloc qu'il annonce.
constexpr ImU32 kFoldColor = IM_COL32(165, 170, 180, 90);
// L'air laissé au-dessus d'un bloc qui en suit un autre.
constexpr float kFoldGapPixels = 6.0F;

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
  ImGui::SetNextItemWidth(Px(kControlWidthPixels));
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

}  // namespace

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
//
// SOUS UN FILTRE — `kept` non nul —, seules les rangées gardées se dessinent.
// ⚠ LEUR TITRE LES PRÉCÈDE QUAND MÊME : le bloc et l'intertitre d'une rangée
// trouvée se posent devant la première qui se montre, même si la rangée qui
// les porte reste cachée. Les blocs sont alors tous ouverts, sans toucher à ce
// qu'ils retiennent : on cherche, on ne range pas.
//
// `row_wanted` : la rangée à amener en haut de la page, puis moins un.
void DrawOptionRows(const char* scope, std::span<const OptionRowView> rows, const SkinArt& art,
                    OptionsWindowAction* asked, bool* covered, const std::vector<bool>* kept,
                    int* row_wanted) {
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
  // Sous un filtre : le titre de bloc et l'intertitre qui attendent leur
  // première rangée montrée.
  const char* pending_fold = nullptr;
  const char* pending_heading = nullptr;
  for (std::size_t rank = 0; rank < rows.size(); ++rank) {
    const OptionRowView& row = rows[rank];
    if (kept != nullptr) {
      if (row.fold != nullptr) {
        pending_fold = row.fold;
        pending_heading = nullptr;
      }
      if (row.heading != nullptr) {
        pending_heading = row.heading;
      }
    } else if (row.fold != nullptr) {
      ImGui::PushID(row.id);
      folded = !DrawFold(row.fold, rank == 0, art.look());
      ImGui::PopID();
    }
    if (folded || (kept != nullptr && !(*kept)[rank])) {
      continue;
    }
    if (row.heading != nullptr && rank != 0) {
      flush();
      ++block;
      widest = 0.0F;
      column = column_of();
    }
    ImGui::PushID(row.id);
    // ⚠ LE TITRE AVANT LE GRISAGE : un intertitre éteint ne se lit plus, et
    // c'est justement lui qui dit POURQUOI ce qui suit est éteint.
    const char* const heading = kept != nullptr ? pending_heading : row.heading;
    if (pending_fold != nullptr) {
      ImGui::SeparatorText(pending_fold);
      pending_fold = nullptr;
    }
    if (heading != nullptr) {
      ImGui::SeparatorText(heading);
      pending_heading = nullptr;
    }
    // ⚠ APRÈS LE TITRE : c'est lui qu'on amène en haut, la rangée suit dessous.
    if (row_wanted != nullptr && *row_wanted == row.id) {
      ImGui::SetScrollHereY(0.0F);
      *row_wanted = -1;
    }
    if (row.indented) {
      ImGui::Indent();
    }
    // ⚠ APRÈS L'INDENTATION : `Indent` ramène le curseur en début de ligne, et
    // défairait la suite qu'on vient de demander. Sous un filtre, les sœurs se
    // montrent ensemble : celle d'avant est donc bien là.
    if (row.same_line && rank != 0) {
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

}  // namespace ro::ui
