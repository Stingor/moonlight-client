#pragma once

#include <cstdint>
#include <optional>

#include "input/hotkeys.h"
#include "options/game_option_ids.h"

namespace moonclient {

// CE QU'ON DEMANDE À L'ENREGISTREUR DE ZONE OU À LA CAPTURE D'ÉCRAN, d'où que
// vienne la demande : une touche liée, ou un bouton de la section Capture des
// options. Le jeu et l'arène de survivors les jouent par le même aiguillage,
// pour qu'aucun des deux n'en oublie un.
enum class RecorderVerb : std::uint8_t {
  kRecord,      // filmer la zone, ou arrêter le film en cours
  kPicture,     // une image fixe de la zone, au presse-papier
  kZone,        // retracer la zone à la souris
  kScreenshot,  // l'écran entier, sous `ScreenShot/`
};

// Le geste qu'une action liée à une touche demande ; rien pour une autre action.
[[nodiscard]] constexpr std::optional<RecorderVerb> RecorderVerbOfAction(
    std::optional<ActionRef> action) noexcept {
  if (action == ActionRef(GameAction::kCaptureRecord)) return RecorderVerb::kRecord;
  if (action == ActionRef(GameAction::kCapturePicture)) return RecorderVerb::kPicture;
  if (action == ActionRef(GameAction::kCaptureZone)) return RecorderVerb::kZone;
  if (action == ActionRef(GameAction::kScreenshot)) return RecorderVerb::kScreenshot;
  return std::nullopt;
}

// Le geste qu'un bouton des options demande, par l'identifiant de sa ligne.
// ⚠ PAR `NamedOption`, jamais par un transtypage : un identifiant composé
// tronqué retomberait sur un bouton de la capture.
[[nodiscard]] constexpr std::optional<RecorderVerb> RecorderVerbOfOption(int option_id) noexcept {
  const std::optional<GameOption> named = NamedOption(option_id);
  if (named == GameOption::kCaptureRecord) return RecorderVerb::kRecord;
  if (named == GameOption::kCapturePicture) return RecorderVerb::kPicture;
  if (named == GameOption::kCaptureZone) return RecorderVerb::kZone;
  return std::nullopt;
}

}  // namespace moonclient
