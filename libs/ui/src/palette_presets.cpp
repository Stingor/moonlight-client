#include "palette_presets.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <string_view>

#include <imgui_internal.h>  // GetInputTextState : resynchroniser le champ de nom

#include "ro/core/i18n.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_widgets.h"

namespace ro::ui {
namespace {

// La largeur d'un nom dans la liste, et celle du champ de saisie, en largeurs
// de police. ⚠ EXPLICITES : un `Selectable` auto-dimensionné prend TOUTE la
// largeur restante et avalerait le clic de la ligne entière.
constexpr float kPresetRowInFonts = 9.0F;
constexpr float kPresetFieldInFonts = 10.0F;
constexpr float kPresetHintWrapInFonts = 22.0F;

// LE NOM SANS SES ESPACES DE BORD : « soirée » et « soirée  » sont le même
// préréglage pour qui les lit.
[[nodiscard]] std::string_view Trimmed(std::string_view name) {
  constexpr std::string_view kBlank = " \t";
  const std::size_t first = name.find_first_not_of(kBlank);
  if (first == std::string_view::npos) {
    return {};
  }
  const std::size_t last = name.find_last_not_of(kBlank);
  return name.substr(first, last - first + 1);
}

// LE MENU D'UN PRÉRÉGLAGE, au clic droit : charger une partie, renommer.
void DrawPresetMenu(const PaletteEditorState& state, const PaletteEditorView& view,
                    const std::string& name, PaletteEditorAction& action) {
  if (!BeginSkinPopupContextItem("menu")) {
    return;
  }
  const auto load = [&action, &name](const char* label, PresetPart part) {
    if (ImGui::MenuItem(label)) {
      action.verb = PaletteEditorVerb::kLoadPreset;
      action.preset = name;
      action.preset_part = part;
    }
  };
  load(core::Tr("Tout charger"), PresetPart::kAll);
  load(core::Tr("Les couleurs seules"), PresetPart::kColors);
  load(core::Tr("Les cheveux seuls"), PresetPart::kHair);
  ImGui::Separator();
  // ⚠ LE NOUVEAU NOM EST CELUI DU CHAMP : un second champ, dans un menu, se
  // refermerait au premier clic dedans.
  const std::string_view typed = Trimmed(state.preset_name.data());
  const bool free = !typed.empty() && typed != name &&
                    std::ranges::find(view.presets, typed) == view.presets.end();
  if (ImGui::MenuItem(core::Tr("Renommer avec le nom saisi"), nullptr, false, free)) {
    action.verb = PaletteEditorVerb::kRenamePreset;
    action.preset = name;
    action.renamed = std::string(typed);
  }
  EndSkinPopup();
}

// UNE LIGNE DE LA LISTE : la croix qui l'efface, puis son nom. Rend vrai quand
// la liste va bouger sous nos pieds — une ligne effacée.
[[nodiscard]] bool DrawPresetRow(PaletteEditorState& state, const PaletteEditorView& view, int rank,
                                 const SkinArt& art, PaletteEditorAction& action) {
  const std::string& name = view.presets[static_cast<std::size_t>(rank)];
  // 🔴 LA CROIX EST SOUMISE AVANT LE NOM, et ce n'est pas de la mise en page :
  // un `Selectable` occupe toute la largeur restante et avalait le clic de la
  // ligne entière, croix comprise. Soumettre le bouton d'abord lui réserve son
  // rectangle ; le `Selectable` prend ce qui reste.
  //
  // Centrage vertical : un bouton a la hauteur d'un cadre, un `Selectable`
  // celle d'une ligne de texte. On descend le plus court de la moitié de
  // l'écart, sinon la croix flotte au-dessus du nom.
  const float line = ImGui::GetTextLineHeight();
  const float frame = ImGui::GetFrameHeight();
  const float top = ImGui::GetCursorPosY();
  if (frame < line) {
    ImGui::SetCursorPosY(top + (line - frame) * 0.5F);
  }
  const bool erased = SkinSmallButton("x", art);
  ImGui::SetCursorPosY(top);
  if (erased) {
    action.verb = PaletteEditorVerb::kDeletePreset;
    action.preset = name;
    return true;
  }
  ImGui::SameLine();
  if (frame > line) {
    ImGui::SetCursorPosY(top + (frame - line) * 0.5F);
  }
  if (ImGui::Selectable(name.c_str(), false, 0,
                        ImVec2(ImGui::GetFontSize() * kPresetRowInFonts, 0.0F))) {
    action.preset = name;
    // 🔴 MAJ + CLIC = POSER LE CODE DANS LE CHAT. C'est la convention de gestes
    // du projet, la même que sur un objet ou une compétence, et c'est ce qui la
    // rend utile : le joueur n'a rien de neuf à apprendre.
    if (ImGui::GetIO().KeyShift) {
      action.verb = PaletteEditorVerb::kSharePreset;
    } else {
      action.verb = PaletteEditorVerb::kLoadPreset;
      // Le nom revient dans le champ : charger, retoucher, puis réenregistrer
      // sous le même nom est le geste le plus courant, et le retaper à chaque
      // fois n'apporte rien.
      const std::size_t room = std::min(name.size(), state.preset_name.size() - 1);
      std::memcpy(state.preset_name.data(), name.data(), room);
      state.preset_name[room] = '\0';
      // 🔴 ET ON RESYNCHRONISE LE WIDGET : écrire dans le tampon ne suffit pas
      // si le champ a le focus — voir `PaletteEditorState::preset_field`.
      if (state.preset_field != 0) {
        if (ImGuiInputTextState* typing = ImGui::GetInputTextState(state.preset_field)) {
          typing->ReloadUserBufAndMoveToEnd();
        }
      }
    }
  }
  if (ImGui::IsItemHovered()) {
    // 🔴 LE SURVOL MONTRE LE PRÉRÉGLAGE SUR LE PANTIN : on choisit en voyant,
    // sans rien charger.
    state.hovered_preset = rank;
    SkinTooltip(
        core::Tr("Clic : charger. Maj+clic : le lien dans le chat. Clic droit : charger une "
                 "partie, renommer."));
  }
  DrawPresetMenu(state, view, name, action);
  return false;
}

// LE CODE PARTAGEABLE.
//
// La même chaîne que celle rangée en base : un joueur la copie et la donne à un
// autre. Le PRESSE-PAPIERS plutôt qu'un champ de texte, parce que quatre-vingt-
// dix caractères d'hexadécimal ne se lisent ni ne se retapent.
void DrawShareCode(const PaletteEditorView& view, const SkinArt& art, PaletteEditorAction& action) {
  if (SkinButton(core::Tr("Copier le code"), art)) {
    ImGui::SetClipboardText(view.share_code);
    action.verb = PaletteEditorVerb::kCopyCode;
  }
  ImGui::SameLine();
  // Poser le code dans la barre de chat. ⚠ LE GESTE S'ARRÊTE LÀ : c'est le
  // joueur qui envoie, comme pour tout autre lien.
  if (SkinButton(core::Tr("Partager dans le chat"), art)) {
    action.verb = PaletteEditorVerb::kShareCode;
  }
  if (ImGui::IsItemHovered()) {
    // ⚠ ON DIT CE QUE LE GESTE FAIT, ET OÙ IL S'ARRÊTE : le lien se pose dans
    // la saisie, c'est le joueur qui envoie. ⛔ Et l'on ne promet rien de plus
    // que ce que le fil tient — un client sans greffon verra la balise brute,
    // ce qui reste un code à coller.
    SkinTooltip(
        core::Tr("Pose le lien de ton style dans ta saisie — à toi d'envoyer. Qui le "
                 "reçoit le voit sur lui d'un clic."));
  }
  ImGui::SameLine();
  if (SkinButton(core::Tr("Coller un code"), art)) {
    const char* clipboard = ImGui::GetClipboardText();
    action.verb = PaletteEditorVerb::kPasteCode;
    action.pasted = clipboard != nullptr ? clipboard : "";
  }
  if (view.code_refused) {
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetFontSize() * kPresetHintWrapInFonts);
    ImGui::TextUnformatted(core::Tr("Code illisible ou d'une version trop ancienne."));
    ImGui::PopTextWrapPos();
  }
}

}  // namespace

void DrawPalettePresets(PaletteEditorState& state, const PaletteEditorView& view,
                        const SkinArt& art, PaletteEditorAction& action) {
  ImGui::Separator();
  // 🔴 LA SECTION S'OUVRE D'ELLE-MÊME QUAND IL Y A UN BROUILLON, et une seule
  // fois. C'est le seul moment où quelqu'un a besoin de la trouver sans savoir
  // qu'elle existe : il vient de perdre une session, et un bouton de sauvetage
  // replié dans un menu ne sauve personne. Le reste du temps, la section reste
  // comme le joueur l'a laissée.
  if (view.has_draft) {
    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
  }
  if (!ImGui::TreeNode(core::Tr("Préréglages"))) {
    return;
  }

  if (view.has_draft) {
    if (SkinButton(core::Tr("Dernier style non validé"), art)) {
      action.verb = PaletteEditorVerb::kLoadDraft;
    }
    if (ImGui::IsItemHovered()) {
      // ⛔ « POUR CE CORPS » ET NON « POUR CE PERSONNAGE » : c'est la clé que
      // le brouillon emploie vraiment — voir `character/style_store.h`. Écrire
      // l'autre serait une phrase fausse.
      SkinTooltip(
          core::Tr("Ce que tu composais la dernière fois sans le valider. Gardé sur cet "
                   "ordinateur, pour ce corps."));
    }
    ImGui::Separator();
  }

  for (std::size_t rank = 0; rank < view.presets.size(); ++rank) {
    ImGui::PushID(static_cast<int>(rank));
    const bool erased = DrawPresetRow(state, view, static_cast<int>(rank), art, action);
    ImGui::PopID();
    if (erased) {
      break;  // la liste va bouger sous nos pieds
    }
  }

  ImGui::SetNextItemWidth(ImGui::GetFontSize() * kPresetFieldInFonts);
  ImGui::InputText("##nom", state.preset_name.data(), state.preset_name.size());
  // Relevé APRÈS soumission : c'est le seul moment où l'identifiant du widget
  // est connu. Il servira à l'image suivante.
  state.preset_field = ImGui::GetItemID();
  ImGui::SameLine();
  const std::string_view typed = Trimmed(state.preset_name.data());
  const bool named = !typed.empty();
  const bool known = std::ranges::find(view.presets, typed) != view.presets.end();
  // ⚠ GRISÉ PLUTÔT QUE SANS EFFET : sans nom, ou une fois le plafond atteint et
  // le nom inconnu, « Enregistrer » ne peut rien faire.
  ImGui::BeginDisabled(!named || (view.presets_full && !known));
  if (SkinButton(core::Tr("Enregistrer"), art)) {
    action.verb = PaletteEditorVerb::kSavePreset;
    action.preset = std::string(typed);
  }
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered() && view.presets_full && !known) {
    SkinTooltip(
        core::Tr("Tu as rangé autant de styles que la liste en tient. Efface-en un, ou "
                 "réenregistre sous un nom déjà là."));
  }

  ImGui::Separator();
  DrawShareCode(view, art, action);
  ImGui::TreePop();
}

}  // namespace ro::ui
