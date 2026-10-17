#include "world/attack_intent.h"

#include <algorithm>
#include <cstdint>
#include <limits>

#include "ro/core/i18n.h"
#include "ro/net/action.h"
#include "ro/net/movement.h"
#include "ro/net/npc_dialog.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/player_status.h"
#include "ro/sim/world.h"

#include "world/click_action.h"
#include "world/click_rules.h"

namespace moonclient {
namespace {

// Le refus d'attaquer sur une monture s'écrit en rouge, en `0xRRGGBB`.
constexpr std::uint32_t kRidingRefusalColor = 0xFF0000;

}  // namespace

std::uint16_t AttackRangeCells(const ro::sim::World& world) {
  const std::int64_t told = world.player_status().attack_range;
  if (told <= 0) {
    // ⚠ MOINS UN VEUT DIRE « PAS ENCORE DIT », et zéro n'est pas une portée :
    // s'approcher à zéro case viserait la case même du monstre, où l'on ne
    // peut pas se tenir.
    return kDefaultAttackRangeCells;
  }
  return static_cast<std::uint16_t>(
      std::min<std::int64_t>(told, std::numeric_limits<std::uint16_t>::max()));
}

ro::sim::PendingAction ArmAttack(const ro::sim::World& world, const ro::sim::Actor& target,
                                 bool repeat) {
  ro::sim::PendingAction action;
  action.target_id = target.id;
  // CE QUE LE GESTE A DÉCIDÉ, retenu jusqu'au coup : voir `PendingAction::repeat`
  // pour la raison de le figer ici plutôt que de relire la touche plus tard.
  action.repeat = repeat;
  // LA COMPÉTENCE RESTE À ZÉRO : c'est ce qui dit « un coup ordinaire », et
  // c'est là-dessus que la cadence d'approche se décide.
  action.range_cells = AttackRangeCells(world);
  const ro::sim::Actor* self = world.player();
  action.chase_repeat_ms = ro::sim::ChaseRepeatMs(self != nullptr ? self->speed_ms_per_cell : 0,
                                                  target.speed_ms_per_cell);
  return action;
}

ro::sim::PendingAction ArmNpcContact(const ro::sim::World& world, const ro::sim::Actor& npc) {
  ro::sim::PendingAction action;
  action.target_id = npc.id;
  action.deed = ro::sim::PendingDeed::kTalk;
  // Une case de moins que la portée du jeu : la tolérance du test de portée
  // ne doit pas faire parler de la case où le serveur refuse encore.
  action.range_cells = static_cast<std::uint16_t>(std::max(1, world.session().GameAreaCells() - 1));
  return action;
}

ro::sim::PendingAction ArmFarPickup(const ro::sim::Actor& item) {
  ro::sim::PendingAction action;
  action.target_id = item.id;
  action.deed = ro::sim::PendingDeed::kPickUp;
  // Une case : avec la tolérance du test de portée, c'est encore à portée de
  // ramassage, qui en compte deux.
  action.range_cells = 1;
  return action;
}

ro::sim::PendingAction ArmAttackOnGroundUnit(const ro::sim::World& world,
                                             const ro::sim::SkillUnit& unit, bool repeat) {
  ro::sim::PendingAction action;
  action.target_id = unit.id;
  action.repeat = repeat;
  action.range_cells = std::max(kGroundUnitAttackReachCells, AttackRangeCells(world));
  return action;
}

bool AdvanceAttack(ro::net::Connection& link, ro::sim::World& world, ro::sim::WalkPlanner& planner,
                   const ro::sim::WalkMap* walk, std::optional<ro::sim::PendingAction>& attack,
                   ro::sim::ApproachWalkClock& approach,
                   std::optional<std::uint32_t> refused_target, bool repeat_on_rearm,
                   std::uint32_t now_ms, Targeting* targeting, MouseEngagement* engagement) {
  // LE REFUS DU SERVEUR D'ABORD. ⚠ IL NE VAUT QUE POUR L'IMAGE OÙ IL ARRIVE :
  // le gardé plus longtemps relancerait la poursuite bien après que le joueur
  // a visé autre chose.
  bool rearmed = false;
  if (refused_target.has_value()) {
    if (const ro::sim::Actor* target = world.Find(*refused_target); target != nullptr) {
      attack = ArmAttack(world, *target, repeat_on_rearm);
      rearmed = true;
      if (engagement != nullptr) {
        NoteAttackRearmed(*engagement, target->id, /*stops_on_release=*/!repeat_on_rearm);
      }
    }
  }
  if (!attack.has_value() || walk == nullptr) {
    return rearmed;
  }
  const ro::sim::PendingMove move =
      ro::sim::AdvancePendingAction(world, planner, *walk, *attack, approach, now_ms);
  switch (move.step) {
    case ro::sim::PendingStep::kWait:
      break;
    case ro::sim::PendingStep::kApproach:
      ReportRefusal(ro::net::RequestMove(link, move.toward));
      break;
    case ro::sim::PendingStep::kRelease: {
      // À PORTÉE : le coup part, et l'inscription s'efface. ⚠ SANS RELÂCHE —
      // le serveur enchaîne alors les coups de lui-même, et c'est son refus
      // qui nous rendra la main si la cible s'éloigne. Un clic qui ne se
      // poursuit pas frappe aussi sans relâche tant que le bouton est tenu :
      // voir `BasicAttackAction`.
      const std::uint32_t target_id = attack->target_id;
      if (attack->deed == ro::sim::PendingDeed::kTalk) {
        attack.reset();
        ReportRefusal(ro::net::ContactNpc(link, target_id));
        break;
      }
      if (attack->deed == ro::sim::PendingDeed::kPickUp) {
        attack.reset();
        ReportRefusal(ro::net::RequestPickup(link, target_id));
        break;
      }
      const int asked = engagement != nullptr
                            ? BasicAttackAction(attack->repeat, engagement->action, target_id)
                            : (attack->repeat ? kAttackActionRepeat : kAttackActionOnce);
      attack.reset();
      const int action =
          targeting != nullptr ? targeting->FilterBasicAttack(target_id, asked) : asked;
      if (action == kAttackDropped) {
        break;
      }
      if (const ro::sim::Actor* self = world.player();
          self != nullptr && RidingWithholdsAttack(*self)) {
        world.RecordChat(ro::sim::ChatKind::kServer, 0,
                         ro::core::Tr("Impossible d'attaquer sur une monture."), now_ms,
                         kRidingRefusalColor);
        break;
      }
      ReportRefusal(ro::net::RequestAttack(link, target_id, action != kAttackActionOnce));
      if (engagement != nullptr) {
        NoteStrike(*engagement, target_id);
      }
      break;
    }
    case ro::sim::PendingStep::kGiveUp:
      attack.reset();
      break;
  }
  return rearmed;
}

}  // namespace moonclient
