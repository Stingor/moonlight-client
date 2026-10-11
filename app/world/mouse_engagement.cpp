#include "world/mouse_engagement.h"

#include "ro/sim/world.h"

#include "world/native_marker.h"
#include "world/targeting.h"

namespace moonclient {
namespace {

[[nodiscard]] bool Moved(const MouseEngagement& engagement, PointerPx mouse) {
  return mouse.x != engagement.last_mouse.x || mouse.y != engagement.last_mouse.y;
}

// Une passe de la souris. `fresh_press` et `moved` ne valent que pour la
// première passe de l'image : la seconde ne voit ni appui ni mouvement.
void SettleOnce(MouseEngagement& engagement, const EngagementFrame& frame, bool fresh_press,
                bool moved, const ro::sim::World& world) {
  // Engagé et continu, la souris désigne même à travers une fenêtre.
  const bool through_interface = engagement.engaged && engagement.continuous;
  if (engagement.engaged && !engagement.continuous) {
    // Bouton tenu : rien ne bouge tant qu'on ne l'a pas relâché. L'appui, lui,
    // ne retient rien.
    if (engagement.left_held && !fresh_press) {
      return;
    }
    if (!frame.anything_under && engagement.hovered_player_id == 0) {
      engagement.engaged = false;
    }
  } else if (!engagement.engaged && !frame.anything_under) {
    if (!engagement.continuous && !fresh_press && moved) {
      engagement.hovered_player_id = 0;
    }
    return;
  }

  // Le joueur survolé passe avant le survol : il ne se lâche qu'au mouvement,
  // ou quand il n'est plus là.
  if (!engagement.continuous && engagement.hovered_player_id != 0) {
    if (moved || EngagedActor(world, engagement.hovered_player_id) == nullptr) {
      engagement.hovered_player_id = 0;
    }
    return;
  }
  if (!frame.anything_under || (frame.over_interface && !through_interface)) {
    return;
  }
  if (frame.player_under != 0 &&
      (frame.player_attackable || (!engagement.engaged && !engagement.left_held))) {
    engagement.hovered_player_id = frame.player_under;
  }
}

}  // namespace

void Disengage(MouseEngagement& engagement) {
  engagement.engaged = false;
  engagement.hovered_player_id = 0;
}

void NoteLeftPress(MouseEngagement& engagement, bool second_of_double, MouseRoute route) {
  engagement.left_held = true;
  engagement.fresh_press = true;
  if (route.runs() && engagement.engaged && engagement.continuous && !second_of_double) {
    Disengage(engagement);
  }
}

void NoteFocusLost(MouseEngagement& engagement) { engagement.left_held = false; }

void NoteEngagingClick(MouseEngagement& engagement, std::uint32_t id, bool continuous) {
  engagement.engaged = true;
  engagement.continuous = continuous;
  engagement.clicked_id = id;
}

void NoteRelease(MouseEngagement& engagement) {
  engagement.left_held = false;
  PlayerAction& action = engagement.action;
  switch (action.deed) {
    case PlayerDeed::kSkillOnTarget:
      // Une compétence visée retombe au relâchement, qu'elle soit partie ou
      // non : c'est le lancer, pas le bouton, qui l'achève.
      Disengage(engagement);
      return;
    case PlayerDeed::kAttack:
      if (!action.stops_on_release) {
        return;
      }
      action.released = true;
      if (action.struck) {
        EndDeed(engagement);
      }
      return;
    case PlayerDeed::kNothing:
    case PlayerDeed::kPickup:
      return;
  }
}

bool ReleaseStopsAttack(const MouseEngagement& engagement) {
  const PlayerAction& action = engagement.action;
  return action.deed == PlayerDeed::kAttack && action.stops_on_release && action.struck;
}

bool EngagementShowsSword(const MouseEngagement& engagement) {
  return engagement.engaged && !engagement.continuous && engagement.left_held;
}

bool RearmedAttackRepeats(const MouseEngagement& engagement) { return engagement.continuous; }

void NoteDeed(MouseEngagement& engagement, PlayerDeed deed, std::uint32_t target_id,
              bool stops_on_release) {
  engagement.action =
      PlayerAction{.deed = deed, .target_id = target_id, .stops_on_release = stops_on_release};
}

void EndDeed(MouseEngagement& engagement) {
  engagement.action = PlayerAction();
  Disengage(engagement);
}

bool CompanionSkillDisengages(std::uint32_t target_id, const ro::sim::World& world) {
  if (target_id == 0 || target_id == world.player_id()) {
    return true;
  }
  const ro::sim::Actor* target = world.Find(target_id);
  return target == nullptr || !TargetIsConcealed(*target);
}

void NoteStrike(MouseEngagement& engagement, std::uint32_t target_id) {
  PlayerAction& action = engagement.action;
  if (action.deed != PlayerDeed::kAttack || action.target_id != target_id) {
    return;
  }
  action.struck = true;
  // Relâchée avant d'atteindre sa cible, l'attaque qui ne se poursuit pas
  // porte un dernier coup et s'achève avec lui.
  if (action.stops_on_release && action.released) {
    EndDeed(engagement);
  }
}

void NoteAttackRearmed(MouseEngagement& engagement, std::uint32_t target_id,
                       bool stops_on_release) {
  PlayerAction& action = engagement.action;
  if (action.deed == PlayerDeed::kAttack && action.target_id == target_id) {
    action.struck = false;
    return;
  }
  NoteDeed(engagement, PlayerDeed::kAttack, target_id, stops_on_release);
}

void AdvanceDeed(MouseEngagement& engagement, const ro::sim::World& world) {
  PlayerAction& action = engagement.action;
  if (action.deed == PlayerDeed::kNothing ||
      (action.deed == PlayerDeed::kSkillOnTarget && action.target_id == world.player_id())) {
    return;
  }
  const ro::sim::Actor* target = EngagedActor(world, action.target_id);
  if (target == nullptr) {
    EndDeed(engagement);
    return;
  }
  if (target->posture != ro::sim::Posture::kDead) {
    action.target_seen_alive = true;
  } else if (action.target_seen_alive) {
    EndDeed(engagement);
  }
}

void Settle(MouseEngagement& engagement, const EngagementFrame& frame,
            const ro::sim::World& world) {
  SettleOnce(engagement, frame, engagement.fresh_press, Moved(engagement, frame.mouse), world);
  SettleOnce(engagement, frame, /*fresh_press=*/false, /*moved=*/false, world);
  SkipSettle(engagement, frame.mouse);
}

void SkipSettle(MouseEngagement& engagement, PointerPx mouse) {
  engagement.fresh_press = false;
  engagement.last_mouse = mouse;
}

void EnterMap(MouseEngagement& engagement, bool same_map) {
  engagement.clicked_id = 0;
  engagement.hovered_player_id = 0;
  engagement.left_held = false;
  engagement.fresh_press = false;
  engagement.death_freeze = false;
  // Le personnage arrive sur la carte sans action en cours ; cet oubli-là ne
  // fait rien retomber.
  engagement.action = PlayerAction();
  if (!same_map) {
    engagement.engaged = false;
    engagement.continuous = false;
    engagement.siege_map = false;
  }
}

bool WorldMouseFrozen(const MouseEngagement& engagement, const ro::sim::World& world,
                      bool own_bar_shown) {
  const bool dialog_holds = world.npc_dialog().has_value() && world.npc_dialog()->holds_world;
  return engagement.death_freeze || engagement.trade_freeze || (dialog_holds && !own_bar_shown) ||
         world.storage_open();
}

bool SiegeMap(const MouseEngagement& engagement, const ro::sim::MapMode& mode) {
  return engagement.siege_map || mode.guild_emblems_shown;
}

void NoteMapMode(MouseEngagement& engagement, const ro::sim::MapMode& mode) {
  engagement.siege_map = SiegeMap(engagement, mode);
}

}  // namespace moonclient
