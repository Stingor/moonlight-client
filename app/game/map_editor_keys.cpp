#include "game/map_editor_keys.h"

#include <optional>
#include <utility>

#include <SDL3/SDL_clipboard.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_stdinc.h>

#include "ro/ui/model_clipboard.h"

namespace moonclient {
namespace {

// La direction d'une flèche ; rien pour une autre touche.
std::optional<NudgeDirection> ArrowOf(SDL_Keycode key) {
  switch (key) {
    case SDLK_LEFT:
      return NudgeDirection::kLeft;
    case SDLK_RIGHT:
      return NudgeDirection::kRight;
    case SDLK_UP:
      return NudgeDirection::kUp;
    case SDLK_DOWN:
      return NudgeDirection::kDown;
    default:
      return std::nullopt;
  }
}

}  // namespace

bool TakeMapEditorKey(MapEditorSession& editor, const SDL_KeyboardEvent& key, bool text_input) {
  if (text_input) {
    return false;
  }
  const bool ctrl_held = (key.mod & SDL_KMOD_CTRL) != 0;
  const bool shift_held = (key.mod & SDL_KMOD_SHIFT) != 0;
  // CTRL+FLÈCHES poussent ce qui est désigné — la répétition compte : tenir la
  // touche pousse en continu.
  if (ctrl_held) {
    if (const std::optional<NudgeDirection> direction = ArrowOf(key.key)) {
      editor.Queue(ro::ui::map_editor::Nudge{MapEditorNudge(*direction, shift_held)});
      return true;
    }
  }
  if (key.repeat) {
    return false;
  }
  if (key.key == SDLK_ESCAPE && editor.Deselect()) {
    return true;
  }
  // SUPPR, sans Ctrl : retirer ou masquer ce qui est désigné.
  if (key.key == SDLK_DELETE && !ctrl_held) {
    editor.Queue(ro::ui::map_editor::Delete{});
    return true;
  }
  // CTRL+V COLLE LE MODÈLE du presse-papiers au point de pose, s'il en porte un.
  if (ctrl_held && key.key == SDLK_V) {
    char* clipboard = SDL_GetClipboardText();
    const std::optional<ro::ui::ClipboardModel> model =
        clipboard != nullptr ? ro::ui::ParseModelClipboard(clipboard) : std::nullopt;
    SDL_free(clipboard);
    if (model.has_value()) {
      editor.Queue(ro::ui::map_editor::AddModel{.file = model->model_file,
                                                .rotation = model->rotation,
                                                .scale = model->scale,
                                                .opacity = model->opacity});
      return true;
    }
  }
  // LES RACCOURCIS DE CTRL, lettres et chiffres : voir `MapEditorShortcut`.
  constexpr SDL_Keycode kLastAscii = 0x7F;
  if (ctrl_held && key.key <= kLastAscii) {
    if (std::optional<ro::ui::MapEditorIntent> intent =
            MapEditorShortcut(static_cast<char>(key.key), shift_held)) {
      editor.Queue(std::move(*intent));
      return true;
    }
  }
  return false;
}

}  // namespace moonclient
