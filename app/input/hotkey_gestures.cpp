// LES GESTES DE SOURIS DE LA TABLE DES RACCOURCIS : leur nom dans le fichier et
// à l'écran, l'action qu'un geste peut porter, et le libellé d'une liaison.
// Voir `input/hotkeys.h`.

#include <optional>
#include <string>

#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_keycode.h>

#include "ro/core/i18n.h"

#include "input/hotkeys.h"

namespace moonclient {
namespace {

constexpr auto kLastGesture = MouseGesture::kRightClick;

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
  }
  return "?";
}

}  // namespace

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
  }
  return false;
}

const char* CaptureHintOfAction(ActionRef action) {
  if (InputOfAction(action) != ActionInput::kWorldClick) {
    return nullptr;
  }
  return ro::core::Tr(
      "Cliquez dans le jeu en tenant Alt ou Ctrl — ni Maj, ni Ctrl + clic gauche seul. "
      "Échap annule, Suppr retire le raccourci.");
}

std::string FileNameOfKey(std::uint32_t key) {
  if (const std::optional<MouseGesture> gesture = GestureOfKey(key); gesture.has_value()) {
    return FileNameOfGesture(*gesture);
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
