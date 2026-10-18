// La box « Pseudo ». Voir l'en-tête.

#include "chat_whisper_box.h"

#include <algorithm>
#include <cstring>
#include <string>

#include "ro/core/i18n.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_tooltip.h"

#include "ui_input.h"
#include "ui_scale.h"

namespace ro::ui {
namespace {

// Le menu des derniers destinataires, ouvert au clic droit sur la box.
constexpr const char* kWhisperMenu = "##recents";

}  // namespace

[[nodiscard]] WhisperBoxGesture DrawWhisperBox(ChatInputState& state,
                                               const ChatLinePreview& preview,
                                               ChatInputAsk* asked) {
  WhisperBoxGesture gesture;
  ImGui::SetNextItemWidth(Px(kChatWhisperBoxWidth));
  // ⚠ LE CLAVIER PEUT VENIR ICI, et pas seulement à la phrase : c'est le joueur
  // qui décide où il laisse son curseur, et un envoi le lui rend là où il
  // l'avait mis.
  if (state.wants_focus && state.focus_on_whisper) {
    ImGui::SetKeyboardFocusHere();
    // Consommée seulement le geste fini — même raison que pour la phrase, plus
    // bas : `SetKeyboardFocusHere` refuse sec tant qu'un bouton est enfoncé.
    if (!ImGui::IsAnyMouseDown() && !ImGui::IsDragDropActive()) {
      state.wants_focus = false;
    }
  }
  // 🔴 ENTRÉE VAUT DEPUIS ICI AUSSI. Le client d'origine ne demande pas au
  // joueur de savoir dans laquelle des deux boîtes il a laissé son curseur :
  // Entrée envoie, ou referme si la phrase est vide, que le pseudo soit rempli
  // ou non. Sans `EnterReturnsTrue`, la touche se perdait — et en battle mode
  // la barre restait ouverte, sourde.
  gesture.submitted =
      ImGui::InputTextWithHint("##pseudo", core::Tr("Pseudo"), state.whisper_to.data(),
                               state.whisper_to.size(), ImGuiInputTextFlags_EnterReturnsTrue);
  // Relevé ICI : la liste écrit dans CE tampon, et il faudra le lui dire — le
  // widget tient sa propre copie pendant l'édition.
  const ImGuiID box = ImGui::GetItemID();
  gesture.active = ImGui::IsItemActive();
  // ⚠ RELEVÉ COLLÉ AU WIDGET : plus bas, « l'item » désignerait le dernier
  // dessiné — une infobulle, un menu — et le test d'Échap porterait sur autre
  // chose.
  gesture.escaped = ImGui::IsItemDeactivated() && ImGui::IsKeyPressed(ImGuiKey_Escape, false);
  // TABULATION → LA PHRASE. Elle est dessinée APRÈS, dans la même image : le
  // clavier y bascule tout de suite, sans le battement d'une image.
  if (gesture.active && ImGui::IsKeyPressed(ImGuiKey_Tab, false)) {
    state.focus_on_whisper = false;
    state.wants_focus = true;
  }

  // 🔴 CLIC DROIT = LES RÉCENTS, et c'est ce que fait le bouton « Select
  // Receiver » du client avec sa liste. Le geste est le même que sur une
  // languette, et il ne coûte pas un pixel d'une rangée déjà serrée — le bouton
  // qu'on avait mis à côté prenait la largeur d'un carré pour un geste qui n'en
  // demande aucune.
  if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
    ImGui::OpenPopup(kWhisperMenu);
  }
  // ⚠ PAS PENDANT QUE LA LISTE EST OUVERTE : elle se déroule juste sous la box,
  // et l'infobulle viendrait recouvrir les noms qu'on cherche à lire.
  if (ImGui::IsItemHovered() && !ImGui::IsPopupOpen(kWhisperMenu)) {
    SkinTooltipFormat("%s\n%s",
                      core::Tr("À qui chuchoter. Rempli, tout ce que tu écris lui part "
                               "— sauf ce qui porte déjà un sigil."),
                      core::Tr("Clic droit : les récents."));
  }

  if (!BeginSkinPopup(kWhisperMenu)) {
    return gesture;
  }
  if (preview.recent_whispers.empty()) {
    // ⚠ UN MOT PLUTÔT QU'UNE LISTE VIDE : un popup qui s'ouvre sur rien se lit
    // comme un défaut, alors que c'est simplement qu'on n'a chuchoté à personne.
    ImGui::TextUnformatted(core::Tr("Aucun destinataire récent"));
  } else {
    for (const std::string_view name : preview.recent_whispers) {
      // ⚠ UNE COPIE BORNÉE, jamais un `strcpy` : un nom plus long que la box —
      // d'un fichier, d'un serveur modifié — déborderait le tampon.
      if (ImGui::Selectable(std::string(name).c_str())) {
        const std::size_t kept = std::min(name.size(), state.whisper_to.size() - 1);
        std::memcpy(state.whisper_to.data(), name.data(), kept);
        state.whisper_to[kept] = '\0';
        // 🔴 ET ON PRÉVIENT LE CHAMP. Écrire dans le tampon d'un
        // `InputText` sans le lui dire laisse son curseur posé sur l'ancien texte ;
        // s'il était plus long, le curseur dépasse la fin et la première image qui
        // redessine le champ franchit une assertion d'ImGui — ce qui TUE le
        // programme.
        ReloadInputField(box);
        // Le nom est choisi : on veut ÉCRIRE la phrase, pas cliquer — et c'est
        // à elle que le clavier va, pas à la box qu'on vient de remplir.
        state.focus_on_whisper = false;
        state.wants_focus = true;
      }
    }
    ImGui::Separator();
    if (ImGui::Selectable(core::Tr("Vider la liste"))) {
      asked->forget_whispers = true;
    }
  }
  EndSkinPopup();
  return gesture;
}

}  // namespace ro::ui
