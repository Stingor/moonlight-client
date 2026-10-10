// Les phrases de la fenêtre de saisie de Talkie Box. Voir l'en-tête.

#include "ro/vfs/talkbox_words.h"

#include "ro/core/i18n.h"

namespace ro::vfs {

std::string_view TalkboxTitle(const ItemWords& words) { return words.talkbox.title; }

std::string_view TalkboxPrompt(const ItemWords& words) { return words.talkbox.prompt; }

void FillTalkboxWords(TalkboxWords& words) {
  words.title = core::Tr("Enregistrer un message dans la Talkie Box");
  words.prompt = core::Tr("Saisissez un message pour la Talkie Box");
}

}  // namespace ro::vfs
