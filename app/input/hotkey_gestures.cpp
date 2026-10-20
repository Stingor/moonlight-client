// LES GESTES DE SOURIS DE LA TABLE DES RACCOURCIS : leur nom dans le fichier et
// à l'écran, l'action qu'un geste peut porter, et le libellé d'une liaison.
// Voir `input/hotkeys.h`.

#include <cstdint>
#include <optional>
#include <string>

#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_mouse.h>

#include "ro/core/i18n.h"
#include "ro/core/text.h"

#include "input/hotkeys.h"

namespace moonclient {
namespace {

constexpr auto kLastGesture = MouseGesture::kMiddleHold;

// Le dernier point de code Unicode : au-delà, un code de touche n'est plus un caractère.
constexpr std::uint32_t kLastCodePoint = 0x10FFFF;
// L'écart entre une minuscule ASCII et sa capitale.
constexpr std::uint32_t kAsciiCaseGap = 'a' - 'A';

// UNE TOUCHE DONT LE CODE EST UN CARACTÈRE IMPRIMABLE s'écrit par ce caractère,
// et se relit par lui, sans passer par SDL_GetKeyName ni SDL_GetKeyFromName.
//
// 🔴 CES DEUX-LÀ CONSULTENT LA DISPOSITION DU CLAVIER, et le second ne rend pas
// toujours ce que le premier a écrit. Sur AZERTY, « 4 » peut se relire comme
// l'apostrophe de la même touche, puisque le 4 s'y tape avec Maj — alors que la
// touche, pressée en jeu, rend bien « 4 ». La case perdait sa touche à chaque
// démarrage.
//
// ⚠ Espace, Entrée, Tab, Échap, Retour et Suppr gardent leur nom : leur code
// est un caractère, mais un caractère qui ne se lit pas dans un fichier.
[[nodiscard]] bool IsCharacterKey(std::uint32_t key) {
  return key > SDLK_SPACE && key != SDLK_DELETE && key <= kLastCodePoint;
}

[[nodiscard]] const char* FileNameOfGesture(MouseGesture gesture) {
  switch (gesture) {
    case MouseGesture::kRightDrag:
      return "MouseRightDrag";
    case MouseGesture::kRightDoubleClick:
      return "MouseRightDoubleClick";
    case MouseGesture::kMiddleDrag:
      return "MouseMiddleDrag";
    case MouseGesture::kMiddleDoubleClick:
      return "MouseMiddleDoubleClick";
    case MouseGesture::kWheel:
      return "MouseWheel";
    case MouseGesture::kLeftClick:
      return "MouseLeftClick";
    case MouseGesture::kRightClick:
      return "MouseRightClick";
    case MouseGesture::kMiddleHold:
      return "MouseMiddleHold";
  }
  return "";
}

[[nodiscard]] const char* LabelOfGesture(MouseGesture gesture) {
  switch (gesture) {
    case MouseGesture::kRightDrag:
      return ro::core::Tr("Clic droit glissé");
    case MouseGesture::kRightDoubleClick:
      return ro::core::Tr("Double-clic droit");
    case MouseGesture::kMiddleDrag:
      return ro::core::Tr("Clic milieu glissé");
    case MouseGesture::kMiddleDoubleClick:
      return ro::core::Tr("Double-clic milieu");
    case MouseGesture::kWheel:
      return ro::core::Tr("Molette");
    case MouseGesture::kLeftClick:
      return ro::core::Tr("Clic gauche");
    case MouseGesture::kRightClick:
      return ro::core::Tr("Clic droit");
    case MouseGesture::kMiddleHold:
      return ro::core::Tr("Clic milieu maintenu");
  }
  return "?";
}

}  // namespace

std::optional<MouseGesture> PressGestureOf(std::uint8_t button) noexcept {
  switch (button) {
    case SDL_BUTTON_LEFT:
      return MouseGesture::kLeftClick;
    case SDL_BUTTON_RIGHT:
      return MouseGesture::kRightClick;
    case SDL_BUTTON_MIDDLE:
      return MouseGesture::kMiddleHold;
    default:
      return std::nullopt;
  }
}

std::optional<MouseGesture> DoubleClickGestureOf(std::uint8_t button) noexcept {
  switch (button) {
    case SDL_BUTTON_RIGHT:
      return MouseGesture::kRightDoubleClick;
    case SDL_BUTTON_MIDDLE:
      return MouseGesture::kMiddleDoubleClick;
    default:
      return std::nullopt;
  }
}

std::optional<MouseGesture> DragGestureOf(std::uint32_t held) noexcept {
  if ((held & SDL_BUTTON_RMASK) != 0) {
    return MouseGesture::kRightDrag;
  }
  if ((held & SDL_BUTTON_MMASK) != 0) {
    return MouseGesture::kMiddleDrag;
  }
  return std::nullopt;
}

std::optional<MouseGesture> GestureOfKey(std::uint32_t key) noexcept {
  if ((key & kMouseGestureMark) == 0) {
    return std::nullopt;
  }
  const std::uint32_t rank = key & ~kMouseGestureMark;
  if (rank < static_cast<std::uint32_t>(MouseGesture::kRightDrag) ||
      rank > static_cast<std::uint32_t>(kLastGesture)) {
    return std::nullopt;
  }
  return static_cast<MouseGesture>(rank);
}

ActionInput InputOfAction(ActionRef action) {
  const std::optional<GameAction> named = action.named();
  if (!named.has_value()) {
    return ActionInput::kPress;
  }
  switch (*named) {
    case GameAction::kCameraOrbit:
    case GameAction::kCameraTilt:
    case GameAction::kCameraOrbitZoom:
    case GameAction::kCameraTiltZoom:
      return ActionInput::kDrag;
    case GameAction::kCameraZoomWheel:
    case GameAction::kCameraTiltWheel:
      return ActionInput::kWheel;
    case GameAction::kHomunculusOrder:
    case GameAction::kMercenaryOrder:
      return ActionInput::kWorldClick;
    case GameAction::kShortcutWheel:
      return ActionInput::kHold;
    default:
      return ActionInput::kPress;
  }
}

bool BindingFits(ActionRef action, Binding binding) {
  if (!binding.bound()) {
    return true;
  }
  const std::optional<MouseGesture> gesture = GestureOfKey(binding.key);
  switch (InputOfAction(action)) {
    case ActionInput::kDrag:
      return gesture == MouseGesture::kRightDrag || gesture == MouseGesture::kMiddleDrag;
    case ActionInput::kWheel:
      return gesture == MouseGesture::kWheel;
    case ActionInput::kWorldClick:
      return (gesture == MouseGesture::kLeftClick || gesture == MouseGesture::kRightClick) &&
             !binding.shift && (binding.alt || binding.ctrl) &&
             !(gesture == MouseGesture::kLeftClick && binding.ctrl && !binding.alt);
    case ActionInput::kPress:
      return !gesture.has_value() || gesture == MouseGesture::kRightDoubleClick ||
             gesture == MouseGesture::kMiddleDoubleClick;
    case ActionInput::kHold:
      // ⚠ UN BOUTON QU'ON TIENT, et seulement celui du milieu : le gauche marche
      // et le droit tourne la caméra, l'un comme l'autre dès l'appui.
      return !gesture.has_value() || gesture == MouseGesture::kMiddleHold;
  }
  return false;
}

const char* CaptureHintOfAction(ActionRef action) {
  switch (InputOfAction(action)) {
    case ActionInput::kWorldClick:
      return ro::core::Tr(
          "Cliquez dans le jeu en tenant Alt ou Ctrl — ni Maj, ni Ctrl + clic gauche seul. "
          "Échap annule, Suppr retire le raccourci.");
    case ActionInput::kHold:
      return ro::core::Tr(
          "Appuyez sur une touche, ou sur le bouton du milieu de la souris. Échap annule, "
          "Suppr retire le raccourci.");
    default:
      return nullptr;
  }
}

std::string FileNameOfKey(std::uint32_t key) {
  if (const std::optional<MouseGesture> gesture = GestureOfKey(key); gesture.has_value()) {
    return FileNameOfGesture(*gesture);
  }
  if (IsCharacterKey(key)) {
    // Une lettre s'écrit en capitale, comme elle est gravée sur la touche.
    const bool lower_letter = key >= 'a' && key <= 'z';
    std::string name;
    ro::core::AppendUtf8(name, static_cast<char32_t>(lower_letter ? key - kAsciiCaseGap : key));
    return name;
  }
  const char* name = SDL_GetKeyName(static_cast<SDL_Keycode>(key));
  return name != nullptr ? name : "";
}

std::uint32_t KeyOfFileName(const std::string& name) {
  for (auto rank = static_cast<std::uint32_t>(MouseGesture::kRightDrag);
       rank <= static_cast<std::uint32_t>(kLastGesture); ++rank) {
    const auto gesture = static_cast<MouseGesture>(rank);
    if (name == FileNameOfGesture(gesture)) {
      return KeyOfGesture(gesture);
    }
  }
  if (!name.empty()) {
    std::size_t after_first = 0;
    const char32_t first = ro::core::NextCodePoint(name, after_first);
    if (after_first == name.size() && first != ro::core::kReplacementCharacter &&
        IsCharacterKey(first)) {
      const bool capital_letter = first >= 'A' && first <= 'Z';
      return capital_letter ? first + kAsciiCaseGap : first;
    }
  }
  return static_cast<std::uint32_t>(SDL_GetKeyFromName(name.c_str()));
}

std::string LabelOfBinding(Binding binding) {
  if (!binding.bound()) {
    return std::string();
  }
  std::string label;
  if (binding.ctrl) {
    label += ro::core::Tr("Ctrl");
    label += '+';
  }
  if (binding.alt) {
    label += ro::core::Tr("Alt");
    label += '+';
  }
  if (binding.shift) {
    label += ro::core::Tr("Maj");
    label += '+';
  }
  if (const std::optional<MouseGesture> gesture = GestureOfKey(binding.key); gesture.has_value()) {
    label += LabelOfGesture(*gesture);
    return label;
  }
  const char* name = SDL_GetKeyName(static_cast<SDL_Keycode>(binding.key));
  label += name != nullptr ? name : "?";
  return label;
}

}  // namespace moonclient
