// LE NOM D'UNE CAPTURE D'ÉCRAN, et la touche qui la prend.
//
// ⚠ CE QUE CES ESSAIS ATTRAPENT EST MUET EN JEU : une seconde capture prise dans
// la même seconde écraserait la première, et le joueur ne retrouverait qu'une
// image sur deux.

#include "capture/screenshot.h"

#include <filesystem>
#include <fstream>

#include <SDL3/SDL_keycode.h>

#include <catch2/catch_test_macros.hpp>

#include "input/hotkeys.h"
#include "support/temp_directory.h"

namespace {

using moonclient::FreeScreenshotPath;
using moonclient::GameAction;
using moonclient::Hotkeys;

void Touch(const std::filesystem::path& path) { std::ofstream(path) << "x"; }

TEST_CASE("Capture d'écran : le nom porte l'heure, sous le dossier donné", "[app][capture]") {
  const ro::test_support::TempDirectory folder;
  CHECK(FreeScreenshotPath(folder.path(), "2026-11-01_14-03-22") ==
        folder.path() / "screen_2026-11-01_14-03-22.png");
}

TEST_CASE("Capture d'écran : la même seconde prend un suffixe, sans écraser", "[app][capture]") {
  const ro::test_support::TempDirectory folder;
  Touch(folder.path() / "screen_2026-11-01_14-03-22.png");
  CHECK(FreeScreenshotPath(folder.path(), "2026-11-01_14-03-22") ==
        folder.path() / "screen_2026-11-01_14-03-22_2.png");
  Touch(folder.path() / "screen_2026-11-01_14-03-22_2.png");
  CHECK(FreeScreenshotPath(folder.path(), "2026-11-01_14-03-22") ==
        folder.path() / "screen_2026-11-01_14-03-22_3.png");
}

TEST_CASE("Capture d'écran : Impr. écran par défaut, et le joueur la change", "[app][capture]") {
  Hotkeys bindings;
  CHECK(bindings.Of(GameAction::kScreenshot).key == SDLK_PRINTSCREEN);
  const auto taken = bindings.ActionFor(SDLK_PRINTSCREEN, false, false, false);
  REQUIRE(taken.has_value());
  CHECK(*taken == moonclient::ActionRef(GameAction::kScreenshot));
  static_cast<void>(bindings.Bind(GameAction::kScreenshot, moonclient::Binding{.key = SDLK_F11}));
  CHECK(bindings.Of(GameAction::kScreenshot).key == SDLK_F11);
}

}  // namespace
