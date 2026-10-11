#include "world/follow_action.h"

#include <array>
#include <cstdio>
#include <string>

#include "ro/core/i18n.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/world.h"

#include "world/blocked_npcs.h"
#include "world/click_action.h"

namespace moonclient {
namespace {

using ro::core::Tr;

// Assez pour un nom de vingt-quatre octets et la phrase qui l'entoure.
constexpr std::size_t kLineBytes = 160;

using Line = std::array<char, kLineBytes>;

// Une ligne du suivi, dans la couleur de ce que le client dit de lui-même.
void Say(ro::sim::World& world, const char* text, std::uint32_t now_ms) {
  world.RecordChat(ro::sim::ChatKind::kServer, 0, text, now_ms, kClientSaysColor);
}

}  // namespace

bool StartFollowing(FollowState& state, ro::sim::World& world, const ro::sim::Actor& target,
                    std::uint32_t now_ms) {
  if (!CanFollow(target, world.player_id()) || !PassesWeightGuard(world, 0, now_ms)) {
    return false;
  }
  BeginFollow(state, target);
  Line line{};
  std::snprintf(line.data(), line.size(), Tr("Vous suivez %s."), state.followed_name.c_str());
  Say(world, line.data(), now_ms);
  return true;
}

void StopFollowing(FollowState& state, ro::sim::World& world, FollowEnd end, std::uint32_t now_ms) {
  if (!state.active()) {
    return;
  }
  const std::string name = state.followed_name;
  EndFollow(state);
  Line line{};
  if (end == FollowEnd::kOutOfSight) {
    std::snprintf(line.data(), line.size(), Tr("%s est hors de vue : suivi arrêté."), name.c_str());
  } else {
    std::snprintf(line.data(), line.size(), Tr("Vous ne suivez plus %s."), name.c_str());
  }
  Say(world, line.data(), now_ms);
}

bool RunFollowCommand(FollowState& state, ro::sim::World& world, std::string_view line,
                      std::uint32_t now_ms) {
  const std::optional<std::string> name = ReadFollowCommand(line);
  if (!name.has_value()) {
    return false;
  }
  if (name->empty()) {
    if (state.active()) {
      StopFollowing(state, world, FollowEnd::kLetGo, now_ms);
    } else {
      Say(world, Tr("Vous ne suivez personne."), now_ms);
    }
    return true;
  }
  const ro::sim::Actor* target = PlayerInViewNamed(world, *name);
  if (target == nullptr) {
    Line said{};
    std::snprintf(said.data(), said.size(), Tr("%s n'est pas en vue."), name->c_str());
    Say(world, said.data(), now_ms);
    return true;
  }
  static_cast<void>(StartFollowing(state, world, *target, now_ms));
  return true;
}

void TickFollow(FollowState& state, const FollowStage& stage) {
  const FollowDecision decision = AdvanceFollow(state, *stage.world, stage.walk,
                                                FollowTick{.left_held = stage.left_held,
                                                           .in_chat_room = stage.in_chat_room,
                                                           .now_ms = stage.now_ms});
  switch (decision.step) {
    case FollowStep::kNothing:
      return;
    case FollowStep::kStop:
      StopFollowing(state, *stage.world, decision.end, stage.now_ms);
      return;
    case FollowStep::kTurn: {
      // Le demi-tour du clic au sol, assis : voir `TurnTowardCell`.
      WorldClick scene;
      scene.link = stage.link;
      scene.world = stage.world;
      scene.planner = stage.planner;
      scene.now_ms = stage.now_ms;
      static_cast<void>(TurnTowardCell(scene, decision.cell));
      return;
    }
    case FollowStep::kWalk:
      if (stage.entity_menu != nullptr) {
        stage.entity_menu->open = false;
      }
      RequestGroundWalk(*stage.link, *stage.world, *stage.walk_clock, stage.deferred_walk,
                        stage.player_gesture, decision.cell, stage.now_ms);
      return;
  }
}

}  // namespace moonclient
