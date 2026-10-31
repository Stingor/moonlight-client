// LA VUE À LA PREMIÈRE PERSONNE, dans la partie : l'œil posé à chaque image, et
// les touches de marche qui demandent une case devant le regard. La règle vit
// dans `input/first_person.h` ; ce fichier la relie au monde, à la carte et au
// fil.

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>

#include <SDL3/SDL.h>

#include <glm/glm.hpp>

#include "ro/net/movement.h"
#include "ro/sim/jump.h"
#include "ro/sim/movement.h"
#include "ro/ui/chat_bar.h"

#include "game/game.h"
#include "game/game_settings.h"
#include "input/first_person.h"
#include "input/hotkeys.h"
#include "input/key_gate.h"
#include "world/click_action.h"

namespace moonclient {
namespace {

// L'ACTION DE LA TABLE QUI PORTE CHAQUE DIRECTION, dans l'ordre de `StrideKey`.
constexpr std::array<GameAction, kStrideKeyCount> kStrideActions = {
    GameAction::kStepForward, GameAction::kStepBack, GameAction::kStepLeft, GameAction::kStepRight};

// LA DIRECTION QUE PORTE CETTE TOUCHE.
//
// ⚠ MAJ NE COMPTE PAS : on marche en la tenant. Ctrl et Alt comptent, eux :
// Alt+Z ouvre le groupe, même quand Z avance.
[[nodiscard]] std::optional<StrideKey> StrideKeyOf(const Hotkeys& bindings,
                                                   const SDL_KeyboardEvent& key) {
  const bool ctrl = (key.mod & SDL_KMOD_CTRL) != 0;
  const bool alt = (key.mod & SDL_KMOD_ALT) != 0;
  for (std::size_t rank = 0; rank < kStrideActions.size(); ++rank) {
    const Binding binding = bindings.Of(kStrideActions[rank]);
    if (binding.bound() && binding.key == key.key && binding.ctrl == ctrl && binding.alt == alt) {
      return static_cast<StrideKey>(rank);
    }
  }
  return std::nullopt;
}

// La direction que relâche cette touche, quels que soient les modificateurs :
// un Alt pressé en marchant ne doit pas laisser la marche tenue.
[[nodiscard]] std::optional<StrideKey> StrideKeyReleasedBy(const Hotkeys& bindings,
                                                           SDL_Keycode key) {
  for (std::size_t rank = 0; rank < kStrideActions.size(); ++rank) {
    const Binding binding = bindings.Of(kStrideActions[rank]);
    if (binding.bound() && binding.key == key) {
      return static_cast<StrideKey>(rank);
    }
  }
  return std::nullopt;
}

}  // namespace

bool Game::FirstPersonTakes(const SDL_Event& event) {
  switch (event.type) {
    case SDL_EVENT_MOUSE_MOTION:
      // LE REGARD SUIT LE GLISSER DU BOUTON DROIT, hors des fenêtres.
      //
      // ⚠ L'ÉVÉNEMENT N'EST PAS AVALÉ : la route ordinaire doit voir le glisser,
      // sans quoi elle prendrait le relâchement pour un clic droit et ouvrirait
      // le menu de ce qui se trouve sous la souris.
      if ((event.motion.state & SDL_BUTTON_RMASK) != 0 && !interface_->gate().mouse_to_ui) {
        first_person_.Look(event.motion.xrel, event.motion.yrel);
      }
      return false;
    case SDL_EVENT_KEY_DOWN: {
      const std::optional<StrideKey> key = StrideKeyOf(settings_->bindings(), event.key);
      if (!key.has_value()) {
        return false;
      }
      // ⚠ UNE LETTRE QUI S'ÉCRIT N'EST PAS UN PAS : la conversation et les champs
      // de saisie gardent la leur, exactement comme pour les barres.
      const ro::ui::InputGate gate = interface_->gate();
      const ro::ui::ChatWindowState& chat = windows_.chat.main;
      if (!KeyGoesToGame({.interface_has_keyboard = gate.keyboard_to_ui,
                          .typing = gate.typing_to_ui,
                          .chat_bar_takes_typing = ro::ui::ChatBarTakesTyping(
                              chat.input.battle_mode, chat.input.unfolded, gate.text_to_ui)},
                         event.key.key, event.key.mod)) {
        return false;
      }
      // 🔴 AVALÉE : la touche de marche ne déclenche rien d'autre pendant la vue.
      first_person_.Press(*key);
      return true;
    }
    case SDL_EVENT_KEY_UP:
      if (const std::optional<StrideKey> key =
              StrideKeyReleasedBy(settings_->bindings(), event.key.key)) {
        first_person_.Release(*key);
      }
      return false;
    default:
      return false;
  }
}

void Game::FrameFirstPerson(std::uint32_t now_ms) {
  scene_actors_.player_unseen = first_person_.active();
  const ro::sim::Actor* self = world_.player();
  if (!first_person_.active() || self == nullptr || !stage_.has_value()) {
    return;
  }
  // UNE TOUCHE RELÂCHÉE HORS DE LA FENÊTRE n'envoie aucun événement : l'état
  // réel du clavier la relâche ici.
  const bool* keyboard = SDL_GetKeyboardState(nullptr);
  for (std::size_t rank = 0; rank < kStrideActions.size(); ++rank) {
    const Binding binding = settings_->bindings().Of(kStrideActions[rank]);
    if (!binding.bound() || !keyboard[SDL_GetScancodeFromKey(binding.key, nullptr)]) {
      first_person_.Release(static_cast<StrideKey>(rank));
    }
  }
  const ro::sim::WalkPath& path = planner_.PathFor(*self);
  const ro::sim::FractionalCell cell = ro::sim::InterpolatedCell(*self, path, now_ms);
  const std::array<float, 3> ground = GroundPositionAt(*stage_, cell.x, cell.y);
  // L'ŒIL SAUTE AVEC LE CORPS. La veille, elle, garde sa propre pose.
  if (!afk_screen_.active()) {
    first_person_.Aim(
        camera_, glm::vec3(ground[0], ground[1] + ro::sim::JumpLiftAt(*self, now_ms), ground[2]));
  }
  if (!link_.is_open()) {
    return;
  }
  const std::optional<ro::sim::CellPosition> stride =
      FirstPersonStride(stage_->walk_map, static_cast<std::int32_t>(std::lround(cell.x)),
                        static_cast<std::int32_t>(std::lround(cell.y)), first_person_.yaw_degrees(),
                        first_person_.held());
  if (stride.has_value()) {
    if (first_person_.StrideDue(now_ms, kWalkRepeatMs)) {
      RequestGroundWalk(link_, world_, grimoire_.approach_clock(), &world_mouse_.deferred_walk,
                        scene_actors_.player_gesture, *stride, now_ms);
    }
  } else if (first_person_.TakeStrideEnd()) {
    // LES TOUCHES RELÂCHÉES ARRÊTENT LE PERSONNAGE sur la case où il passe, au
    // lieu de le laisser finir sa foulée.
    ReportRefusal(ro::net::RequestMove(link_, ro::sim::StoppedCell(*self, path, now_ms)));
  }
}

}  // namespace moonclient
