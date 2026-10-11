// LES JOUEURS DANS LE CHOIX DE LA CIBLE du cerveau d'un compagnon : là où la
// carte les rend adversaires, avec leurs conduites et les règles fixes. Voir
// `ro/sim/companion_players.h` et `ro/sim/companion_ai.h`.

#include <optional>
#include <utility>

#include "ro/sim/battlefield.h"
#include "ro/sim/companion_ai.h"
#include "ro/sim/companion_players.h"
#include "ro/sim/world.h"

#include "companion_brain_detail.h"

namespace ro::sim {

namespace {

[[nodiscard]] bool StanceReadsConducts(PlayerStance stance) noexcept {
  return stance == PlayerStance::kConducts || stance == PlayerStance::kAsMonster;
}

}  // namespace

PlayerStance CompanionBrain::StanceOf(const Situation& s, const Actor& player) {
  if (s.players != PlayerTargeting::kByCamp) {
    return PlayerStanceOf(s.players, BattleRelation::kUnknown);
  }
  const Battlefield& field = s.world.battlefield();
  const BattleRelation relation = field.IsEnemy(player.id)  ? BattleRelation::kEnemy
                                  : field.IsAlly(player.id) ? BattleRelation::kAlly
                                                            : BattleRelation::kUnknown;
  return PlayerStanceOf(s.players, relation);
}

bool CompanionBrain::IsPlayerFoe(const Situation& s, const Actor& actor) const {
  // ⚠ LA VUE N'EST PAS JUGÉE ICI : elle borne un joueur comme un monstre, au
  // même endroit (le choix de la cible, le combat qui la perd au-delà de
  // `kCompanionSightCells`).
  return actor.kind == ActorKind::kPlayer && StanceOf(s, actor) != PlayerStance::kNone &&
         actor.id != s.owner.id && CanBeTargeted(actor) &&
         !ShieldedPlayer(actor, s.side, s.settings.friends, s.facts.owner_guild_id, s.guild_spared);
}

std::optional<CompanionTactic> CompanionBrain::PlayerTacticOf(const Situation& s,
                                                              const Actor& player) const {
  // Une conduite ne se lit que là où la visée des joueurs est pleine : en duel,
  // et contre un joueur de camp inconnu au champ de bataille, seule la défense
  // vaut.
  if (!StanceReadsConducts(StanceOf(s, player))) {
    return std::nullopt;
  }
  const CompanionPlayerConduct* conduct = ConductFor(
      s.settings.player_conducts, player, OwnerGuildmate(player, s.facts.owner_guild_id));
  if (conduct == nullptr) {
    return std::nullopt;
  }
  return TacticOfConduct(*conduct);
}

bool CompanionBrain::IsRuleFoe(const Situation& s, const Actor& actor) const {
  if (actor.kind != ActorKind::kPlayer) {
    return IsRuleTarget(s, actor);
  }
  if (!StanceReadsConducts(StanceOf(s, actor)) || !IsPlayerFoe(s, actor) ||
      !WithinCells(s.self_cell, StandingCell(actor), kCompanionSightCells)) {
    return false;
  }
  const std::optional<CompanionTactic> tactic = PlayerTacticOf(s, actor);
  return !tactic.has_value() || tactic->rank != TacticRank::kIgnore;
}

bool CompanionBrain::RereadTargetLine(const Situation& s, const Actor& target) {
  std::optional<CompanionTactic> current;
  if (target.kind == ActorKind::kMonster) {
    // Le maître d'une invocation s'annonce après elle : celle qu'on découvre du
    // camp se lâche.
    if (SummonSideOf(s.world, target, s.side) == SummonSide::kOwnSide) {
      return false;
    }
    if (const CompanionTactic* line = TacticOf(s, target); line != nullptr) {
      current = *line;
    }
  } else if (target.kind == ActorKind::kPlayer) {
    if (!IsPlayerFoe(s, target)) {
      return false;
    }
    current = PlayerTacticOf(s, target);
  }
  if (!current.has_value()) {
    return true;
  }
  if (current->rank == TacticRank::kIgnore) {
    return false;
  }
  target_rank_ = current->rank;
  target_tactic_ = std::move(current);
  return true;
}

}  // namespace ro::sim
