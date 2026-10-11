#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "ro/net/connection.h"
#include "ro/sim/actor.h"
#include "ro/sim/pending_action.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/entity_menu.h"

#include "world/deferred_walk.h"
#include "world/follow_rules.h"

namespace ro::sim {
class WalkMap;
}  // namespace ro::sim

namespace moonclient {

// ── LE SUIVI, JOUÉ ──────────────────────────────────────────────────────────
//
// Ce que `world/follow_rules.h` décide, envoyé et dit. Le client d'origine
// suit sans un mot ; celui-ci écrit le départ et l'arrêt dans la conversation,
// offre « Suivre » dans le menu d'un joueur et la commande `/follow`.

// Commence à suivre `target`, un autre joueur : la garde de poids d'abord
// (voir `PassesWeightGuard`), puis la ligne « Vous suivez … ». Rend vrai si le
// suivi a commencé.
bool StartFollowing(FollowState& state, ro::sim::World& world, const ro::sim::Actor& target,
                    std::uint32_t now_ms);

// Arrête le suivi en cours, et dit pourquoi. Sans suivi : rien.
void StopFollowing(FollowState& state, ro::sim::World& world, FollowEnd end, std::uint32_t now_ms);

// `/follow <nom>` et `/follow` : voir `ReadFollowCommand`. Rend vrai si la
// ligne était la commande — elle ne part alors jamais au serveur.
bool RunFollowCommand(FollowState& state, ro::sim::World& world, std::string_view line,
                      std::uint32_t now_ms);

// Ce que le tic du suivi touche.
struct FollowStage {
  ro::net::Connection* link = nullptr;
  ro::sim::World* world = nullptr;
  ro::sim::WalkPlanner* planner = nullptr;
  // Nulle le temps qu'une carte se charge.
  const ro::sim::WalkMap* walk = nullptr;
  // La marche part par la voie du clic au sol, avec son plancher et ce que le
  // geste retient : voir `RequestGroundWalk`.
  ro::sim::ApproachWalkClock* walk_clock = nullptr;
  DeferredWalk* deferred_walk = nullptr;
  std::optional<PlayerGesture> player_gesture;
  // Le menu d'entité, refermé par un pas qui part.
  ro::ui::EntityMenuState* entity_menu = nullptr;
  bool in_chat_room = false;
  bool left_held = false;
  std::uint32_t now_ms = 0;
};

// L'IMAGE DU SUIVI : la décision de `AdvanceFollow`, jouée.
void TickFollow(FollowState& state, const FollowStage& stage);

}  // namespace moonclient
