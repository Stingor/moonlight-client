// LA VUE À LA PREMIÈRE PERSONNE ET LE VOL LIBRE, dans la partie : l'œil posé à
// chaque image, et les touches tenues qui demandent une case devant le regard
// ou déplacent l'œil. Les règles vivent dans `input/first_person.h` et
// `input/free_flight.h` ; ce fichier les relie au monde, à la carte et au fil.

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>

#include <SDL3/SDL.h>

#include <glm/glm.hpp>

#include "ro/net/free_flight.h"
#include "ro/net/movement.h"
#include "ro/render/terrain_mesh.h"
#include "ro/sim/actor.h"
#include "ro/sim/jump.h"
#include "ro/sim/movement.h"
#include "ro/ui/character_staff.h"
#include "ro/ui/chat_bar.h"

#include "character/staff_view.h"
#include "game/game.h"
#include "game/game_settings.h"
#include "input/first_person.h"
#include "input/free_flight.h"
#include "input/hotkeys.h"
#include "input/key_gate.h"
#include "world/click_action.h"

namespace moonclient {
namespace {

// L'ACTION DE LA TABLE QUI PORTE CHAQUE DIRECTION, dans l'ordre de `FlightKey`.
//
// ⚠ LES QUATRE PREMIÈRES SONT AUSSI CELLES DE `StrideKey`, dans le même ordre :
// la marche et le vol partagent leurs touches.
constexpr std::array<GameAction, kFlightKeyCount> kMoveActions = {
    GameAction::kStepForward, GameAction::kStepBack, GameAction::kStepLeft,
    GameAction::kStepRight,   GameAction::kFlyUp,    GameAction::kFlyDown};
static_assert(static_cast<std::size_t>(StrideKey::kRight) ==
              static_cast<std::size_t>(FlightKey::kRight));

// LA DIRECTION QUE PORTE CETTE TOUCHE, au rang de `FlightKey`.
//
// ⚠ MAJ NE COMPTE PAS : on marche en la tenant, et elle accélère le vol. Ctrl
// et Alt comptent, eux : Alt+Z ouvre le groupe, même quand Z avance.
[[nodiscard]] std::optional<std::size_t> MoveKeyOf(const Hotkeys& bindings,
                                                   const SDL_KeyboardEvent& key) {
  const bool ctrl = (key.mod & SDL_KMOD_CTRL) != 0;
  const bool alt = (key.mod & SDL_KMOD_ALT) != 0;
  for (std::size_t rank = 0; rank < kMoveActions.size(); ++rank) {
    const Binding binding = bindings.Of(kMoveActions[rank]);
    if (binding.bound() && binding.key == key.key && binding.ctrl == ctrl && binding.alt == alt) {
      return rank;
    }
  }
  return std::nullopt;
}

// La direction que porte cette touche, quels que soient les modificateurs :
// au relâcher, un Alt pressé en marchant ne doit pas laisser la marche tenue.
[[nodiscard]] std::optional<std::size_t> MoveKeyReleasedBy(const Hotkeys& bindings,
                                                           SDL_Keycode key) {
  for (std::size_t rank = 0; rank < kMoveActions.size(); ++rank) {
    const Binding binding = bindings.Of(kMoveActions[rank]);
    if (binding.bound() && binding.key == key) {
      return rank;
    }
  }
  return std::nullopt;
}

[[nodiscard]] bool IsBinding(const Binding& binding, const SDL_KeyboardEvent& key) {
  return binding.bound() && binding.key == key.key &&
         binding.ctrl == ((key.mod & SDL_KMOD_CTRL) != 0) &&
         binding.shift == ((key.mod & SDL_KMOD_SHIFT) != 0) &&
         binding.alt == ((key.mod & SDL_KMOD_ALT) != 0);
}

}  // namespace

bool Game::FirstPersonTakes(const SDL_Event& event) {
  // ⚠ UNE LETTRE QUI S'ÉCRIT N'EST PAS UN PAS : la conversation et les champs
  // de saisie gardent la leur, exactement comme pour les barres.
  const auto goes_to_game = [this, &event] {
    const ro::ui::InputGate gate = interface_->gate();
    const ro::ui::ChatWindowState& chat = windows_.chat.main;
    return KeyGoesToGame({.interface_has_keyboard = gate.keyboard_to_ui,
                          .typing = gate.typing_to_ui,
                          .chat_bar_takes_typing = ro::ui::ChatBarTakesTyping(
                              chat.input.battle_mode, chat.input.unfolded, gate.text_to_ui)},
                         event.key.key, event.key.mod);
  };
  // LE VOL LIBRE SE BASCULE ICI, et non dans la route des raccourcis : seul
  // l'administrateur décolle, et la caméra qu'il quitte est celle-ci.
  if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
      IsBinding(settings_->bindings().Of(GameAction::kFreeFlight), event.key) &&
      windows_.staff_tools.admin && goes_to_game()) {
    if (free_flight_.active()) {
      free_flight_.Leave(camera_);
    } else {
      free_flight_.Enter(camera_);
      // 🔴 L'ADMINISTRATEUR VOLE INVISIBLE : son corps suit l'œil à travers la
      // carte, et les joueurs le verraient glisser d'un bout à l'autre. `@hide`
      // est une bascule : elle ne part que vers qui ne l'est pas déjà.
      const ro::sim::Actor* self = world_.player();
      if (self != nullptr && (self->option & ro::sim::kOptionInvisible) == 0) {
        SendStaffOrder({.order = ro::ui::StaffOrder::kToggleHide}, link_, world_);
      }
    }
    return true;
  }
  if (!first_person_.active() && !free_flight_.active()) {
    return false;
  }
  switch (event.type) {
    case SDL_EVENT_MOUSE_MOTION:
      // LE REGARD SUIT LE GLISSER DU BOUTON DROIT, hors des fenêtres.
      //
      // ⚠ L'ÉVÉNEMENT N'EST PAS AVALÉ : la route ordinaire doit voir le glisser,
      // sans quoi elle prendrait le relâchement pour un clic droit et ouvrirait
      // le menu de ce qui se trouve sous la souris.
      if ((event.motion.state & SDL_BUTTON_RMASK) != 0 && !interface_->gate().mouse_to_ui) {
        if (free_flight_.active()) {
          free_flight_.Look(event.motion.xrel, event.motion.yrel);
        } else {
          first_person_.Look(event.motion.xrel, event.motion.yrel);
        }
      }
      return false;
    case SDL_EVENT_KEY_DOWN: {
      // ⚠ EN VOL, LES MODIFICATEURS NE COMPTENT PAS : Ctrl y descend, et la
      // touche d'avance pressée pendant la descente doit avancer quand même.
      const std::optional<std::size_t> rank =
          free_flight_.active() ? MoveKeyReleasedBy(settings_->bindings(), event.key.key)
                                : MoveKeyOf(settings_->bindings(), event.key);
      // ⚠ HORS DU VOL, SES TOUCHES PASSENT : Espace, qui y monte, saute à pied.
      if (!rank.has_value() || (!free_flight_.active() && *rank >= kStrideKeyCount) ||
          !goes_to_game()) {
        return false;
      }
      // 🔴 AVALÉE : la touche de marche ne déclenche rien d'autre pendant la vue.
      if (free_flight_.active()) {
        free_flight_.Press(static_cast<FlightKey>(*rank));
      } else if (*rank < kStrideKeyCount) {
        first_person_.Press(static_cast<StrideKey>(*rank));
      }
      return true;
    }
    case SDL_EVENT_KEY_UP:
      if (const std::optional<std::size_t> rank =
              MoveKeyReleasedBy(settings_->bindings(), event.key.key)) {
        free_flight_.Release(static_cast<FlightKey>(*rank));
        if (*rank < kStrideKeyCount) {
          first_person_.Release(static_cast<StrideKey>(*rank));
        }
      }
      return false;
    default:
      return false;
  }
}

void Game::FrameFirstPerson(std::uint32_t now_ms) {
  // LE PERSONNAGE SE REVOIT EN VOL : l'œil l'a quitté.
  scene_actors_.player_unseen = first_person_.active() && !free_flight_.active();
  // Un rang perdu en plein vol ramène la caméra.
  if (free_flight_.active() && !windows_.staff_tools.admin) {
    free_flight_.Leave(camera_);
  }
  // UNE TOUCHE RELÂCHÉE HORS DE LA FENÊTRE n'envoie aucun événement : l'état
  // réel du clavier la relâche ici.
  const bool* keyboard = SDL_GetKeyboardState(nullptr);
  for (std::size_t rank = 0; rank < kMoveActions.size(); ++rank) {
    const Binding binding = settings_->bindings().Of(kMoveActions[rank]);
    if (!binding.bound() || !keyboard[SDL_GetScancodeFromKey(binding.key, nullptr)]) {
      free_flight_.Release(static_cast<FlightKey>(rank));
      if (rank < kStrideKeyCount) {
        first_person_.Release(static_cast<StrideKey>(rank));
      }
    }
  }
  if (free_flight_.active()) {
    free_flight_.Advance(now_ms, (SDL_GetModState() & SDL_KMOD_SHIFT) != 0);
    if (!afk_screen_.active()) {
      free_flight_.Aim(camera_);
    }
  }
  const ro::sim::Actor* self = world_.player();
  if (free_flight_.active() && self != nullptr && stage_.has_value() && link_.is_open()) {
    // LE CORPS SUIT L'ŒIL, pour que le serveur envoie ce que l'œil survole.
    const std::array<std::int32_t, 2> eye_cell =
        ro::render::CellAtPosition(stage_->scene.gnd, free_flight_.eye().x, free_flight_.eye().z);
    const ro::sim::FractionalCell body =
        ro::sim::InterpolatedCell(*self, planner_.PathFor(*self), now_ms);
    const std::optional<ro::sim::CellPosition> leash = FreeFlightLeashCell(
        eye_cell[0], eye_cell[1], static_cast<std::int32_t>(std::lround(body.x)),
        static_cast<std::int32_t>(std::lround(body.y)), stage_->walk_map.width(),
        stage_->walk_map.height());
    if (leash.has_value() && free_flight_.LeashDue(now_ms)) {
      ReportRefusal(ro::net::RequestFreeFlightCell(link_, static_cast<std::int16_t>(leash->x),
                                                   static_cast<std::int16_t>(leash->y)));
    }
  }
  if (!first_person_.active() || self == nullptr || !stage_.has_value()) {
    return;
  }
  const ro::sim::WalkPath& path = planner_.PathFor(*self);
  const ro::sim::FractionalCell cell = ro::sim::InterpolatedCell(*self, path, now_ms);
  const std::array<float, 3> ground = GroundPositionAt(*stage_, cell.x, cell.y);
  // L'ŒIL SAUTE AVEC LE CORPS. La veille et le vol, eux, gardent leur pose.
  if (!afk_screen_.active() && !free_flight_.active()) {
    first_person_.Aim(
        camera_, glm::vec3(ground[0], ground[1] + ro::sim::JumpLiftAt(*self, now_ms), ground[2]));
  }
  if (!link_.is_open()) {
    return;
  }
  // ⚠ EN VOL, LES TOUCHES DÉPLACENT L'ŒIL ET NON LE PERSONNAGE.
  const std::optional<ro::sim::CellPosition> stride =
      free_flight_.active()
          ? std::nullopt
          : FirstPersonStride(stage_->walk_map, static_cast<std::int32_t>(std::lround(cell.x)),
                              static_cast<std::int32_t>(std::lround(cell.y)),
                              first_person_.yaw_degrees(), first_person_.held());
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
