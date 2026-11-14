// LES PHRASES DE LA SAISIE DE TALKIE BOX : des textes de l'interface.

#include "ro/vfs/talkbox_words.h"

#include <catch2/catch_test_macros.hpp>

#include "ro/core/i18n.h"
#include "ro/vfs/item_words.h"

TEST_CASE("Talkie Box : le titre et la ligne de la saisie sont des textes de l'interface",
          "[vfs][talkbox]") {
  ro::vfs::ItemWords words;
  ro::vfs::FillMessageWords(words);
  CHECK(ro::vfs::TalkboxTitle(words) == ro::core::Tr("Enregistrer un message dans la Talkie Box"));
  CHECK(ro::vfs::TalkboxPrompt(words) == ro::core::Tr("Saisissez un message pour la Talkie Box"));
}

TEST_CASE("Talkie Box : des mots qu'aucun remplissage n'a posés n'écrivent rien",
          "[vfs][talkbox]") {
  const ro::vfs::ItemWords words;
  CHECK(ro::vfs::TalkboxTitle(words).empty());
  CHECK(ro::vfs::TalkboxPrompt(words).empty());
}
