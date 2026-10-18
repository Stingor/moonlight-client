#include "ro/ui/character_delete_window.h"

#include <array>
#include <cstdio>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/window_name.h"

#include "ui_scale.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

// La largeur du champ, en points d'interface : une adresse
// de courriel est longue, et la voir en entier fait partie de la vérification.
constexpr float kSecretFieldPoints = 300.0F;

constexpr ImU32 kRefusalColor = IM_COL32(255, 96, 96, 255);
constexpr ImU32 kPendingColor = IM_COL32(216, 209, 115, 255);
// L'avertissement : le rouge du client, pour ce qui ne se rattrape pas.
constexpr ImU32 kWarningColor = IM_COL32(200, 60, 60, 255);

}  // namespace

CharacterDeleteAction DrawCharacterDelete(CharacterDeleteState& state,
                                          const CharacterDeleteView& view, const SkinArt& art) {
  CharacterDeleteAction action;
  if (!state.open()) {
    return action;
  }

  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5F, 0.5F));

  static WindowName name(kDeleteWindowTitle, kDeleteWindowId);
  if (!BeginSkinnedWindow(name.Get(), nullptr, art, SkinnedTitle::kBar,
                          ImGuiWindowFlags_AlwaysAutoResize)) {
    EndSkinnedWindow();
    return action;
  }

  const float field = kSecretFieldPoints * Scale();

  // ⚠ LE NOM EN TOUTES LETTRES, et non « ce personnage » : c'est la dernière
  // occasion de s'apercevoir qu'on en vise un autre.
  std::array<char, 128> question = {};
  std::snprintf(question.data(), question.size(), core::Tr("Supprimer DÉFINITIVEMENT « %s » ?"),
                state.name.c_str());
  ImGui::TextUnformatted(question.data());

  ColoredText(kWarningColor, core::Tr("Cette action est IRRÉVERSIBLE."));

  // ⚠ « LE SECRET DU COMPTE » ET NON « TON EMAIL » DANS LE TEXTE DE TÊTE :
  // c'est `char_del_option` qui décide lequel des deux le serveur exige, et il
  // se règle. L'invite du champ, elle, nomme celui de CE serveur.
  ImGui::TextUnformatted(core::Tr("Saisis le secret du compte pour confirmer :"));
  ImGui::SetNextItemWidth(field);
  if (ImGui::IsWindowAppearing()) {
    ImGui::SetKeyboardFocusHere();
  }
  const bool submitted_here =
      ImGui::InputTextWithHint("##secret", core::Tr("email du compte"), state.secret.data(),
                               state.secret.size(), ImGuiInputTextFlags_EnterReturnsTrue);

  ImGui::Spacing();
  if (view.pending) {
    ColoredText(kPendingColor, core::Tr("Suppression en cours…"));
  } else if (!view.refusal.empty()) {
    // ⚠ IL S'ENROULE : le motif du serveur énumère ses causes, et une ligne
    // qui traverse l'écran ferait grandir la fenêtre à sa mesure.
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + field);
    ColoredText(kRefusalColor, view.refusal.c_str());
    ImGui::PopTextWrapPos();
  }

  // ⚠ LE CHAMP VIDE NE PART PAS, et ce n'est pas une politesse : sur ce
  // serveur, une clé vide est ACCEPTÉE quand le compte porte l'adresse par
  // défaut « a@a.com » (`chclif_delchar_check`). Un envoi accidentel à vide
  // supprimerait donc pour de bon sur ces comptes-là.
  const bool can_delete = state.secret[0] != '\0' && !view.pending;
  ImGui::BeginDisabled(!can_delete);
  if (SkinButton(core::Tr("Supprimer"), art) || (submitted_here && can_delete)) {
    action.verb = CharacterDeleteVerb::kConfirm;
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  if (SkinButton(core::Tr("Annuler"), art)) {
    action.verb = CharacterDeleteVerb::kCancel;
  }
  if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
    action.verb = CharacterDeleteVerb::kCancel;
  }

  EndSkinnedWindow();
  return action;
}

}  // namespace ro::ui
