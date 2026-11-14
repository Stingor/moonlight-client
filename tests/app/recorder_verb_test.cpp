// L'AIGUILLAGE DES GESTES DE CAPTURE, sans fenêtre ni carte graphique : la
// touche liée ou le bouton des options donne le geste, que le jeu et l'arène
// de survivors jouent de la même façon. La touche d'usine de la capture
// d'écran passe la route de l'arène, qui ne la lit pas.

#include "capture/recorder_verb.h"

#include <cstdint>
#include <optional>

#include <SDL3/SDL.h>

#include <catch2/catch_test_macros.hpp>

#include "input/hotkeys.h"
#include "options/game_option_ids.h"
#include "survivors/survivors_route.h"

using moonclient::ActionRef;
using moonclient::GameAction;
using moonclient::GameOption;
using moonclient::Hotkeys;
using moonclient::RecorderVerb;
using moonclient::RecorderVerbOfAction;
using moonclient::RecorderVerbOfOption;

namespace {

// Un identifiant composé qui, tronqué sur huit bits, vaudrait `kCaptureRecord`.
constexpr int kComposedRowId = 4000;

}  // namespace

TEST_CASE("geste de capture : chaque action liée donne son geste, les autres aucun",
          "[app][capture]") {
  CHECK(RecorderVerbOfAction(ActionRef(GameAction::kCaptureRecord)) == RecorderVerb::kRecord);
  CHECK(RecorderVerbOfAction(ActionRef(GameAction::kCapturePicture)) == RecorderVerb::kPicture);
  CHECK(RecorderVerbOfAction(ActionRef(GameAction::kCaptureZone)) == RecorderVerb::kZone);
  CHECK(RecorderVerbOfAction(ActionRef(GameAction::kScreenshot)) == RecorderVerb::kScreenshot);
  CHECK_FALSE(RecorderVerbOfAction(ActionRef(GameAction::kAfkScreen)).has_value());
  CHECK_FALSE(RecorderVerbOfAction(std::nullopt).has_value());
}

TEST_CASE("geste de capture : Impr. écran d'usine capture l'écran, et l'arène la laisse passer",
          "[app][capture][survivors]") {
  const Hotkeys bindings;
  CHECK(RecorderVerbOfAction(bindings.ActionFor(SDLK_PRINTSCREEN, false, false, false)) ==
        RecorderVerb::kScreenshot);
  CHECK_FALSE(moonclient::SurvivorsReadsKey(static_cast<std::uint32_t>(SDL_SCANCODE_PRINTSCREEN)));
}

TEST_CASE("geste de capture : les trois boutons des options, et rien d'autre", "[app][capture]") {
  CHECK(RecorderVerbOfOption(static_cast<int>(GameOption::kCaptureRecord)) ==
        RecorderVerb::kRecord);
  CHECK(RecorderVerbOfOption(static_cast<int>(GameOption::kCapturePicture)) ==
        RecorderVerb::kPicture);
  CHECK(RecorderVerbOfOption(static_cast<int>(GameOption::kCaptureZone)) == RecorderVerb::kZone);
  // Un curseur de la même section règle, il ne déclenche rien.
  CHECK_FALSE(
      RecorderVerbOfOption(static_cast<int>(GameOption::kCaptureFramesPerSecond)).has_value());
  // ⚠ Un identifiant composé ne retombe pas sur un bouton de la capture.
  CHECK_FALSE(RecorderVerbOfOption(kComposedRowId).has_value());
  CHECK_FALSE(RecorderVerbOfOption(-1).has_value());
}
