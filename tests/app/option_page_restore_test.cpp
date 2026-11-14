// RENDRE UNE SEULE PAGE DU PANNEAU À L'ORIGINE.
//
// 🔴 CE QUE CE BANC TIENT : une page ne rend à l'origine que ses propres
// réglages, une portée imbriquée l'emporte sur celle qui l'entoure, et ce qui
// est déclaré hors de toute portée n'appartient à aucune page.

#include <catch2/catch_test_macros.hpp>

#include "game/settings_file.h"
#include "support/temp_directory.h"

namespace {

using moonclient::kNoSettingsPage;
using moonclient::SettingsFile;

constexpr int kChatPage = 4;
constexpr int kBubblePage = 5;
constexpr int kEmptyPage = 6;

}  // namespace

TEST_CASE("Réglages : une page ne rend à l'origine que les siens", "[app][reglages]") {
  const ro::test_support::TempDirectory folder;
  bool chat_shown = true;
  int bubble_ms = 3000;
  float scale = 1.0F;

  SettingsFile file(folder.path() / "reglages.yaml");
  {
    const SettingsFile::PageScope page(file, kChatPage);
    file.Declare("chat_shown", &chat_shown);
    {
      const SettingsFile::PageScope inner(file, kBubblePage);
      file.Declare("bubble_ms", &bubble_ms);
    }
  }
  file.Declare("scale", &scale);
  CHECK(file.PageOf("chat_shown") == kChatPage);
  CHECK(file.PageOf("bubble_ms") == kBubblePage);
  CHECK(file.PageOf("scale") == kNoSettingsPage);
  CHECK(file.HasPage(kBubblePage));
  CHECK_FALSE(file.HasPage(kEmptyPage));
  CHECK_FALSE(file.PageHoldsMachine(kChatPage));

  chat_shown = false;
  bubble_ms = 1;
  scale = 2.0F;
  file.RestoreDefaults(kChatPage);
  CHECK(chat_shown);
  CHECK(bubble_ms == 1);
  CHECK(scale == 2.0F);
}

TEST_CASE("Réglages : une page qui porte un réglage de la machine le dit", "[app][reglages]") {
  const ro::test_support::TempDirectory folder;
  int volume = 80;
  SettingsFile file(folder.path() / "reglages.yaml");
  {
    const SettingsFile::PageScope page(file, kChatPage);
    const SettingsFile::MachineScope machine(file);
    file.Declare("volume", &volume);
  }
  CHECK(file.PageHoldsMachine(kChatPage));
}
