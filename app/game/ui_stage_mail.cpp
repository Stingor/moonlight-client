// LES FENÊTRES DU COURRIER, dans la trame de l'interface : leur dessin, et
// leur place dans la pile d'Échap. À part de `ui_stage.cpp`, qui n'a plus la
// place de les porter.

#include "game/ui_stage.h"

namespace moonclient {

void UiStage::DrawMailWindows(MailWindows& mail, ro::ui::ItemLinkFacts link_facts) {
  // LA BOÎTE, puis l'écriture, qu'on ouvre depuis elle : la seconde se pose
  // par-dessus la première.
  if (mail.box_view != nullptr) {
    mail.box_answer = ro::ui::DrawMailboxWindow(mail.box, *mail.box_view, skin_, link_facts);
  }
  if (mail.compose_view != nullptr) {
    mail.compose_answer =
        ro::ui::DrawMailComposeWindow(mail.compose, *mail.compose_view, skin_, link_facts);
  }
}

void UiStage::RegisterMailEscapes(MailWindows& mail) {
  // Échap baisse l'interrupteur, et `MailStage` lit une boîte baissée comme une
  // fermeture — sans un octet —, une écriture baissée comme une annulation,
  // qu'il dit au serveur.
  if (mail.box_view != nullptr) {
    escape_.Register(ro::ui::kMailboxWindowId, &mail.box.open);
  }
  if (mail.compose_view != nullptr) {
    escape_.Register(ro::ui::kMailComposeWindowId, &mail.compose.open);
  }
}

}  // namespace moonclient
