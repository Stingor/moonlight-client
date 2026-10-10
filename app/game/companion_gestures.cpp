#include "game/companion_gestures.h"

#include <optional>

#include "ro/render/actor_pick.h"
#include "ro/sim/companion_sight.h"
#include "ro/sim/world.h"

#include "game/companion_ai_stage.h"
#include "world/click_action.h"
#include "world/versus_gate.h"

namespace moonclient {
namespace {

using ro::sim::AiCompanion;

[[nodiscard]] const ro::sim::Actor* ActorUnder(const CompanionPointer& at) {
  const std::optional<std::size_t> picked =
      ro::render::PickActor(*at.camera, at.aspect_ratio, at.drawn, at.pointer.x, at.pointer.y,
                            at.viewport_width, at.viewport_height);
  if (!picked.has_value() || *picked >= at.drawn_ids.size()) {
    return nullptr;
  }
  return at.world->Find(at.drawn_ids[*picked]);
}

[[nodiscard]] std::optional<ro::sim::CellPosition> CellUnder(const CompanionPointer& at) {
  return GroundCellAt(*at.stage, *at.camera, at.aspect_ratio, at.pointer.x, at.pointer.y,
                      at.viewport_width, at.viewport_height);
}

// LES JOUEURS NE SONT ATTAQUABLES que là où la carte les rend adversaires : Maj,
// ici, range l'ordre et n'ouvre pas le ciblage d'un joueur.
[[nodiscard]] VersusGate CompanionVersus(const ro::sim::World& world) {
  return VersusGateOf(world, /*shift_held=*/false, /*no_shift=*/false);
}

[[nodiscard]] bool Attackable(const ro::sim::World& world, const ro::sim::Actor& actor) {
  return actor.id != world.player_id() && ClickEngagesAttack(actor) &&
         ActorTakesClick(actor, CompanionVersus(world)) && ro::sim::CanBeTargeted(actor);
}

// L'ORDRE DIRECT d'un geste : attaquer l'acteur attaquable sous la souris, ou
// aller à la case. Un acteur qui prend le clic sans être attaquable le garde ;
// sous `any_actor_keeps`, TOUT acteur non attaquable le garde, même celui que le
// clic ordinaire traverse (un joueur hors PvP, un pet, un mort).
[[nodiscard]] bool OrderGesture(CompanionAiStage& ai, AiCompanion companion,
                                const CompanionPointer& at, bool shift_held, bool second_of_double,
                                bool any_actor_keeps) {
  const ro::sim::Actor* actor = ActorUnder(at);
  if (actor != nullptr && Attackable(*at.world, *actor)) {
    static_cast<void>(ai.PressOnActor(companion, actor->id, shift_held, second_of_double));
    return true;
  }
  if (actor != nullptr &&
      (any_actor_keeps || ActorTakesClick(*actor, CompanionVersus(*at.world)))) {
    return false;
  }
  const std::optional<ro::sim::CellPosition> cell = CellUnder(at);
  if (!cell.has_value()) {
    return false;
  }
  static_cast<void>(ai.PressOnGround(companion, *cell, shift_held, second_of_double));
  return true;
}

}  // namespace

bool CompanionLeftPress(CompanionAiStage& ai, const CompanionPointer& at, bool grimoire_aiming,
                        std::optional<AiCompanion> ordered, bool shift_held,
                        bool second_of_double) {
  // Le grimoire armé garde le clic : c'est lui que le curseur montre.
  if (grimoire_aiming) {
    return false;
  }
  if (ai.aim().has_value()) {
    // ⚠ LE CLIC EST PRIS MÊME À CÔTÉ : tant que le skill attend, on ne marche
    // pas. Clic droit ou Échap le désarment.
    if (ai.aim()->on_ground) {
      if (const std::optional<ro::sim::CellPosition> cell = CellUnder(at); cell.has_value()) {
        static_cast<void>(ai.ReleaseOnGround(*cell));
      }
    } else if (const ro::sim::Actor* actor = ActorUnder(at); actor != nullptr) {
      static_cast<void>(ai.ReleaseOnActor(actor->id));
    }
    return true;
  }
  if (!ordered.has_value() || !ai.present(*ordered)) {
    return false;
  }
  // ⚠ UN ACTEUR QUE LE CLIC GAUCHE ORDINAIRE TRAVERSE est traversé ici aussi :
  // l'ordre part vers sa case, comme la marche.
  return OrderGesture(ai, *ordered, at, shift_held, second_of_double,
                      /*any_actor_keeps=*/false);
}

bool CompanionRightRelease(CompanionAiStage& ai, const CompanionPointer& at,
                           std::optional<AiCompanion> ordered, bool shift_held,
                           bool second_of_double) {
  if (!ordered.has_value() || !ai.present(*ordered)) {
    return false;
  }
  // Tout acteur non attaquable reste au menu d'entité.
  return OrderGesture(ai, *ordered, at, shift_held, second_of_double,
                      /*any_actor_keeps=*/true);
}

}  // namespace moonclient
