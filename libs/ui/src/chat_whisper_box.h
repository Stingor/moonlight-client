#pragma once

// LA BOX « PSEUDO » DE LA RANGÉE DE SAISIE.
//
// En-tête PRIVÉ de `libs/ui/src` : seule la rangée de saisie la dessine.

#include "ro/ui/chat_input.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// LA BOX « PSEUDO », et la liste qui déroule les derniers correspondants.
//
// ⚠ ELLE VIENT AVANT LE TEXTE, là où le client la met après. Notre champ de
// saisie GRANDIT en hauteur quand la phrase est longue ; une box posée après
// lui se retrouverait alignée sur le bas d'un champ à quatre lignes, loin de
// l'œil. Le client n'a pas ce problème : sa barre ne grandit pas.
// CE QUE LA BOX A VU PASSER. Relevé sur elle et rendu à la rangée : la rangée
// seule sait quoi en faire, parce qu'elle voit AUSSI ce qu'a fait l'autre champ.
struct WhisperBoxGesture {
  // Entrée, tapée depuis la box.
  bool submitted = false;
  // Elle a le clavier cette image.
  bool active = false;
  // Échap vient de l'en faire sortir.
  bool escaped = false;
};

[[nodiscard]] WhisperBoxGesture DrawWhisperBox(ChatInputState& state,
                                               const ChatLinePreview& preview, ChatInputAsk* asked,
                                               const SkinArt& art);

}  // namespace ro::ui
