// LES GESTES DE SOURIS DE LA TABLE DES RACCOURCIS : leur nom dans le fichier et
// à l'écran, l'action qu'un geste peut porter, et le libellé d'une liaison.
// Voir `input/hotkeys.h`.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

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

// Le nom affiché d'une touche qui n'est pas un caractère, dans la langue de
// l'interface. Nul quand SDL nomme déjà la touche comme on l'écrirait (F1, Tab,
// Pause…). Le fichier garde le nom anglais de SDL, que seul l'affichage traduit.
[[nodiscard]] const char* TranslatedKeyName(SDL_Keycode key) {
  switch (key) {
    case SDLK_RETURN:
      return ro::core::Tr("Entrée");
    case SDLK_ESCAPE:
      return ro::core::Tr("Échap");
    case SDLK_BACKSPACE:
      return ro::core::Tr("Retour arrière");
    case SDLK_SPACE:
      return ro::core::Tr("Espace");
    case SDLK_DELETE:
      return ro::core::Tr("Suppr");
    case SDLK_INSERT:
      return ro::core::Tr("Inser");
    case SDLK_HOME:
      return ro::core::Tr("Début");
    case SDLK_END:
      return ro::core::Tr("Fin");
    case SDLK_PAGEUP:
      return ro::core::Tr("Page préc.");
    case SDLK_PAGEDOWN:
      return ro::core::Tr("Page suiv.");
    case SDLK_LEFT:
      return ro::core::Tr("Flèche gauche");
    case SDLK_RIGHT:
      return ro::core::Tr("Flèche droite");
    case SDLK_UP:
      return ro::core::Tr("Flèche haut");
    case SDLK_DOWN:
      return ro::core::Tr("Flèche bas");
    case SDLK_PRINTSCREEN:
      return ro::core::Tr("Impr Écran");
    case SDLK_SCROLLLOCK:
      return ro::core::Tr("Arrêt défil.");
    case SDLK_CAPSLOCK:
      return ro::core::Tr("Verr. maj.");
    case SDLK_NUMLOCKCLEAR:
      return ro::core::Tr("Verr. num.");
    case SDLK_APPLICATION:
      return ro::core::Tr("Menu");
    case SDLK_LCTRL:
      return ro::core::Tr("Ctrl gauche");
    case SDLK_RCTRL:
      return ro::core::Tr("Ctrl droit");
    case SDLK_LSHIFT:
      return ro::core::Tr("Maj gauche");
    case SDLK_RSHIFT:
      return ro::core::Tr("Maj droite");
    case SDLK_LALT:
      return ro::core::Tr("Alt gauche");
    case SDLK_RALT:
      return ro::core::Tr("Alt Gr");
    case SDLK_LGUI:
      return ro::core::Tr("Windows gauche");
    case SDLK_RGUI:
      return ro::core::Tr("Windows droite");
    case SDLK_KP_ENTER:
      return ro::core::Tr("Pavé num. Entrée");
    default:
      return nullptr;
  }
}

// Le préfixe que SDL donne aux touches du pavé numérique : « Keypad 1 ».
constexpr std::string_view kKeypadPrefix = "Keypad ";

[[nodiscard]] std::string DisplayedKeyName(SDL_Keycode key) {
  if (const char* translated = TranslatedKeyName(key); translated != nullptr) {
    return translated;
  }
  const char* name = SDL_GetKeyName(key);
  if (name == nullptr || *name == '\0') {
    return "?";
  }
  const std::string_view sdl_name(name);
  if (sdl_name.starts_with(kKeypadPrefix)) {
    std::string label = ro::core::Tr("Pavé num.");
    label += ' ';
    label += sdl_name.substr(kKeypadPrefix.size());
    return label;
  }
  return std::string(sdl_name);
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
    case GameAction::kStepForward:
    case GameAction::kStepBack:
    case GameAction::kStepLeft:
    case GameAction::kStepRight:
    case GameAction::kFlyUp:
    case GameAction::kFlyDown:
    case GameAction::kVoiceTalk:
      return ActionInput::kKeyHold;
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
    case ActionInput::kKeyHold:
      return !gesture.has_value();
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
  label += DisplayedKeyName(static_cast<SDL_Keycode>(binding.key));
  return label;
}

}  // namespace moonclient
