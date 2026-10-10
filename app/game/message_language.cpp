// Les mots de l'interface tenus par `ItemWords`. Voir l'en-tête.

#include "game/message_language.h"

#include "ro/core/i18n.h"

namespace moonclient {

void FollowMessageLanguage(ro::vfs::ItemWords& words, std::size_t& followed_serial) {
  const std::size_t serial = ro::core::LanguageSerial();
  if (serial == followed_serial) {
    return;
  }
  followed_serial = serial;
  ro::vfs::FillMessageWords(words);
}

}  // namespace moonclient
