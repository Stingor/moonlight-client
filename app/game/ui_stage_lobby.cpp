// L'IMAGE D'INTERFACE DE L'ÉCRAN DE CONNEXION.
//
// ⚠ MÊME CLASSE, AUTRE UNITÉ DE TRADUCTION, et ce n'est pas un rangement :
// `UiStage::Draw` rend des `Answers`, qui sont la réponse de l'étage de JEU —
// quarante champs dont pas un ne concerne un formulaire. Allonger cette
// fonction reviendrait à faire traverser tout l'état du jeu à un écran qui
// n'en a pas.
//
// Ce qui est partagé, et qui compte, c'est le CONTEXTE : le même atlas de
// polices, le même habillage, la même pile d'échappement. Deux étages
// d'interface, ce serait deux atlas — et ImGui grave ses textures au fil des
// images.

#include "game/ui_stage.h"

namespace moonclient {

ro::ui::LoginWindowAction UiStage::DrawLobby(ro::ui::LoginWindowState& login,
                                             const ro::ui::LoginWindowView& view,
                                             const ro::ui::MapSignView* map_sign,
                                             ro::ui::OptionsWindowState& settings,
                                             std::span<const ro::ui::OptionSectionView> pages,
                                             ro::ui::OptionsWindowAction* settings_asked) {
  backend_.BeginFrame(*ui_);
  ro::ui::LoginWindowAction action = ro::ui::DrawLoginWindow(login, view, skin_);
  // ⚠ APRÈS LE FORMULAIRE, donc PAR-DESSUS : c'est de lui qu'on l'ouvre.
  *settings_asked = ro::ui::DrawLoginSettingsWindow(settings, pages, skin_);
  // Le nom de la ville du décor, par-dessus le formulaire comme en jeu.
  if (map_sign != nullptr) {
    ro::ui::DrawMapSign(*map_sign, ui_->MapSignFont());
  }
  // ⚠ LA PILE D'ÉCHAPPEMENT TOURNE AUSSI ICI : elle est ce qui décide à qui va
  // la touche, et un écran qui la sauterait laisserait la partie suivante
  // hériter d'un état vieux d'une session. Échap ferme les réglages, comme les
  // Options en jeu ; sans rien à fermer, il demande s'il faut quitter.
  if (ro::ui::FollowLoginEscape(escape_, &settings.open, skin_)) {
    action.verb = ro::ui::LoginVerb::kQuit;
  }
  ui_->EndFrame();
  backend_.Draw();
  return action;
}

UiStage::LobbySeatsAnswers UiStage::DrawCharacters(ro::ui::CharacterSelectState& seats,
                                                   ro::ui::CharacterCreateState& create,
                                                   ro::ui::CharacterDeleteState& remove,
                                                   const LobbySeatsFrame& frame) {
  backend_.BeginFrame(*ui_);
  LobbySeatsAnswers answers;
  answers.seats = ro::ui::DrawCharacterSelect(seats, *frame.seats, skin_);
  // ⚠ APRÈS LA TABLÉE, donc PAR-DESSUS : la scène est déclarée
  // `NoBringToFrontOnFocus`, si bien qu'un clic sur un siège ne la remonte pas
  // sur ces fenêtres-ci. Sans cet ordre, la création se retrouverait derrière
  // la salle — visible nulle part, et le clavier confisqué.
  answers.create = ro::ui::DrawCharacterCreate(create, *frame.create, skin_);
  answers.remove = ro::ui::DrawCharacterDelete(remove, *frame.remove, skin_);
  escape_.Process();
  ui_->EndFrame();
  backend_.Draw();
  return answers;
}

}  // namespace moonclient
