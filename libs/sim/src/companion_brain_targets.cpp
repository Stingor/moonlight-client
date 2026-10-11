// LE CHOIX DE LA CIBLE PAR LE CERVEAU D'UN COMPAGNON, avec les tactiques par
// monstre : rangs, déclencheurs, partage, poursuite, et l'évitement. Voir
// `ro/sim/companion_ai.h`.

#include <algorithm>

#include "ro/sim/companion_ai.h"
#include "ro/sim/companion_tactics.h"
#include "ro/sim/movement.h"
#include "ro/sim/world.h"

#include "companion_brain_detail.h"

namespace ro::sim {
namespace {

// UNE CIBLE « TENIR » LÂCHÉE après sa frappe se reprend après ce délai, si elle
// ne vise pas le compagnon.
constexpr std::uint32_t kHoldRetakeMs = 1500;

[[nodiscard]] bool ChaseOf(const CompanionTactic* tactic, bool base_chase) noexcept {
  if (tactic == nullptr) {
    return base_chase;
  }
  switch (tactic->chase) {
    case TacticChase::kAlways:
      return true;
    case TacticChase::kNever:
      return false;
    case TacticChase::kAsBase:
      break;
  }
  return base_chase;
}

}  // namespace

using companion_detail::AwayFrom;
using companion_detail::CellAt;
using companion_detail::PercentOf;

bool CompanionBrain::Outranks(const TargetPick& pick, const TargetPick& than) noexcept {
  if (pick.rank != than.rank) {
    return pick.rank < than.rank;
  }
  if (pick.reason != than.reason) {
    return pick.reason < than.reason;
  }
  if (pick.distance_cells != than.distance_cells) {
    return pick.distance_cells < than.distance_cells;
  }
  return pick.actor_id < than.actor_id;
}

std::optional<CompanionBrain::TargetPick> CompanionBrain::SelectTarget(const Situation& s,
                                                                       bool defence_only,
                                                                       bool holding_cell) const {
  const CompanionAiSettings& settings = s.settings;
  const int engage_cells =
      s.owner_walking ? settings.engage_walking_cells : settings.engage_still_cells;
  const int leash_cells =
      s.owner_walking ? settings.leash_walking_cells : settings.leash_still_cells;
  // Les seuils de PV et de SP « pour attaquer de lui-même » bornent toute
  // initiative, celle du niveau 1 comme celle d'une ligne « toujours ».
  const bool initiative_allowed =
      !defence_only &&
      PercentOf(s.facts.hp, s.facts.max_hp) >= settings.own_attack_min_hp_percent &&
      PercentOf(s.facts.sp, s.facts.max_sp) >= settings.own_attack_min_sp_percent;
  const int range_cells = std::max(s.facts.attack_range_cells, 1);

  refusals_.clear();
  std::optional<TargetPick> best;
  for (const auto& [actor_id, actor] : s.world.actors()) {
    const bool monster = actor.kind == ActorKind::kMonster;
    const auto refuse = [this, actor_id](TargetRefusal why) {
      refusals_.emplace_back(actor_id, why);
    };
    // UN JOUEUR N'ENTRE QUE SUR UNE CARTE QUI LE REND ADVERSAIRE, hors des règles
    // fixes : jamais le maître, ses amis, son groupe, ni sa guilde là où elle
    // est épargnée.
    const bool rival = !monster && IsPlayerFoe(s, actor);
    // 🔴 UNE INVOCATION DU CAMP N'EST JAMAIS UNE CIBLE AUTOMATIQUE — ni défense,
    // ni assistance, ni tactique — même si elle frappe : plantes du maître,
    // Légion de notre homoncule. Un ordre direct reste permis, le serveur juge.
    if (!monster && !rival) {
      continue;
    }
    if (IsAbandoned(actor_id, s.now_ms)) {
      refuse(TargetRefusal::kAbandoned);
      continue;
    }
    if (!CanBeTargeted(actor)) {
      refuse(TargetRefusal::kConcealed);
      continue;
    }
    if (monster && SummonSideOf(s.world, actor, s.side) == SummonSide::kOwnSide) {
      refuse(TargetRefusal::kOwnSummon);
      continue;
    }
    // L'ADVERSAIRE D'UN CHAMP DE BATAILLE se prend comme un monstre, y compris
    // par « attaquer de lui-même ». Voir `PlayerStanceOf`.
    const PlayerStance stance = rival ? StanceOf(s, actor) : PlayerStance::kNone;
    const bool like_monster = monster || stance == PlayerStance::kAsMonster;
    // LA CONDUITE D'UN JOUEUR se lit comme la tactique d'un monstre.
    const std::optional<CompanionTactic> conduct =
        rival ? PlayerTacticOf(s, actor) : std::optional<CompanionTactic>();
    const CompanionTactic* tactic =
        monster ? TacticOf(s, actor) : (conduct.has_value() ? &*conduct : nullptr);
    const CellPosition cell = StandingCell(actor);
    const bool in_range_only = holding_cell || !ChaseOf(tactic, settings.chase);
    // 🔴 L'ENGAGEMENT SE MESURE DANS LE CARRÉ DU SERVEUR, pas en cercle : un
    // rayon égal au rayon de vue couvre ainsi tout ce que le maître voit,
    // coins compris.
    //
    // 🔴 LA LAISSE BORNE AUSSI LA CIBLE : un monstre qu'on ne frappe qu'en
    // s'éloignant du maître au-delà de la laisse n'est pas pris, sans quoi le
    // combat s'ouvre et se coupe en boucle.
    if (!WithinCells(s.self_cell, cell, kCompanionSightCells)) {
      refuse(TargetRefusal::kBeyondSight);
      continue;
    }
    if (in_range_only && !WithinCells(s.self_cell, cell, range_cells)) {
      refuse(TargetRefusal::kOutOfReach);
      continue;
    }
    if (engage_cells > 0 && !WithinSquareCells(s.owner_cell, cell, engage_cells)) {
      refuse(TargetRefusal::kBeyondEngagement);
      continue;
    }
    if (leash_cells > 0 && !WithinCells(s.owner_cell, cell, leash_cells + range_cells)) {
      refuse(TargetRefusal::kBeyondLeash);
      continue;
    }
    const bool hits_owner = IsAttacking(actor, s.owner.id, s.now_ms);
    const bool hits_self = IsAttacking(actor, s.self.id, s.now_ms);
    const bool hits_partner = IsAttacking(actor, s.facts.partner_id, s.now_ms);
    const bool hits_friend =
        std::ranges::any_of(s.friends, [&actor, &s](const Actor* friend_actor) {
          return IsAttacking(actor, friend_actor->id, s.now_ms);
        });
    // ⚠ AIDER LE MAÎTRE CONTRE UN JOUEUR EST VOULU sous la pleine visée (PvP,
    // WoE, adversaire d'un champ de bataille) : le maître a choisi son
    // adversaire. En duel, seule la défense vaut.
    const bool assisted =
        !defence_only && settings.assist_owner &&
        (monster || stance == PlayerStance::kConducts || stance == PlayerStance::kAsMonster) &&
        IsAttacking(s.owner, actor_id, s.now_ms);

    // UNE LIGNE NE REMPLACE QUE L'INITIATIVE. La défense et l'assistance du
    // niveau 1 restent ; les déclencheurs « s'il attaque » d'une ligne défendent
    // en plus, même quand les cases « défendre » sont décochées : c'est une
    // demande explicite du joueur pour ce monstre.
    TacticRank rank = TacticRank::kMedium;
    bool line_defends_owner = false;
    bool line_defends_self = false;
    bool line_initiative = false;
    if (tactic != nullptr) {
      rank = tactic->rank;
      if (rank == TacticRank::kIgnore) {
        refuse(TargetRefusal::kIgnored);
        continue;
      }
      if (rank == TacticRank::kHold) {
        const auto released = held_released_ms_.find(actor_id);
        if (released != held_released_ms_.end() &&
            (s.now_ms - released->second < kHoldRetakeMs || hits_self)) {
          refuse(TargetRefusal::kHeld);
          continue;
        }
      }
      line_defends_owner = tactic->trigger != TacticTrigger::kIfAttacksCompanion;
      line_defends_self = true;
      line_initiative = tactic->trigger == TacticTrigger::kAlways;
    }
    CompanionAiReason reason = CompanionAiReason::kNone;
    if (hits_owner && (settings.defend_owner || line_defends_owner)) {
      reason = CompanionAiReason::kDefendOwner;
    } else if (hits_self && (settings.defend_self || line_defends_self)) {
      reason = CompanionAiReason::kDefendSelf;
    } else if (hits_partner && (settings.defend_partner || line_defends_owner)) {
      reason = CompanionAiReason::kDefendPartner;
    } else if (hits_friend && (settings.defend_owner || line_defends_owner)) {
      // UN AMI SE DÉFEND COMME LE MAÎTRE, sous la même case.
      reason = CompanionAiReason::kDefendFriend;
    } else if (assisted) {
      reason = CompanionAiReason::kAssistOwner;
    } else if (initiative_allowed &&
               // ⚠ « ATTAQUER DE LUI-MÊME » NE VISE QUE CE QUI SE PREND COMME UN
               // MONSTRE : ailleurs qu'au champ de bataille, un joueur n'est pris
               // d'initiative que par une ligne « toujours ».
               (tactic != nullptr ? line_initiative : like_monster && settings.attack_on_own) &&
               !SparedFor(s, actor, tactic)) {
      reason = CompanionAiReason::kOwnInitiative;
    }
    if (reason == CompanionAiReason::kNone) {
      const bool wanted =
          tactic != nullptr ? line_initiative : like_monster && settings.attack_on_own;
      refuse(!wanted               ? TargetRefusal::kNoMotive
             : !initiative_allowed ? TargetRefusal::kInitiativeBarred
                                   : TargetRefusal::kSpared);
      continue;
    }
    TargetPick pick{.actor_id = actor_id,
                    .reason = reason,
                    .distance_cells = WholeCellDistance(s.self_cell, cell),
                    .rank = rank};
    if (tactic != nullptr) {
      pick.tactic = *tactic;
    }
    if (!best.has_value() || Outranks(pick, *best)) {
      best = std::move(pick);
    }
  }
  return best;
}

bool CompanionBrain::IsAbandoned(std::uint32_t actor_id, std::uint32_t now_ms) const noexcept {
  return actor_id != 0 && actor_id == abandoned_target_id_ &&
         now_ms - abandoned_at_ms_ < kAbandonedRetakeMs;
}

std::optional<CompanionBrain::TargetRefusal> CompanionBrain::RefusalOf(
    std::uint32_t actor_id) const {
  for (const auto& [refused_id, why] : refusals_) {
    if (refused_id == actor_id) {
      return why;
    }
  }
  return std::nullopt;
}

void CompanionBrain::NoteMonsters(const Situation& s) {
  for (const auto& [actor_id, actor] : s.world.actors()) {
    if (actor.kind != ActorKind::kMonster) {
      continue;
    }
    first_seen_ms_.try_emplace(actor_id, s.now_ms);
    if (actor.movement.has_value() || actor.attack.has_value() || actor.cast.has_value()) {
      active_monsters_.insert(actor_id);
    }
    // Un ami compte comme le camp : « s'il attaque » l'inclut, évitement compris.
    const bool hits_friend =
        std::ranges::any_of(s.friends, [&actor, &s](const Actor* friend_actor) {
          return IsAttacking(actor, friend_actor->id, s.now_ms);
        });
    if (IsAttacking(actor, s.owner.id, s.now_ms) || IsAttacking(actor, s.self.id, s.now_ms) ||
        IsAttacking(actor, s.facts.partner_id, s.now_ms) || hits_friend) {
      camp_attackers_.insert(actor_id);
    }
  }
}

bool CompanionBrain::IsStill(std::uint32_t actor_id, std::uint32_t now_ms) const {
  if (active_monsters_.contains(actor_id)) {
    return false;
  }
  const auto seen = first_seen_ms_.find(actor_id);
  return seen != first_seen_ms_.end() && now_ms - seen->second >= kStillObservationMs;
}

const CompanionTactic* CompanionBrain::TacticOf(const Situation& s, const Actor& monster) const {
  return TacticFor(s.settings.tactics, monster, IsStill(monster.id, s.now_ms),
                   SummonSideOf(s.world, monster, s.side) == SummonSide::kMinion);
}

bool CompanionBrain::ChaseAllowed(const Situation& s) const {
  if (target_direct_) {
    return true;
  }
  return ChaseOf(target_tactic_.has_value() ? &*target_tactic_ : nullptr, s.settings.chase);
}

bool CompanionBrain::SparedFor(const Situation& s, const Actor& monster,
                               const CompanionTactic* tactic, bool engaged) const {
  // Le partage ne parle que de monstres.
  if (monster.kind != ActorKind::kMonster) {
    return false;
  }
  // L'INVOCATION D'UN AUTRE JOUEUR est son monstre, quoi qu'elle fasse.
  if (SummonSideOf(s.world, monster, s.side) != SummonSide::kOthers &&
      !IsOthersMonster(s.world, monster, s.side, s.now_ms)) {
    return false;
  }
  if (tactic == nullptr) {
    return s.settings.spare_others_monsters;
  }
  switch (tactic->share) {
    case TacticShare::kNever:
      return true;
    case TacticShare::kIfStill:
      return !engaged && IsWalking(monster, s.now_ms);
    case TacticShare::kAlways:
      return false;
  }
  return true;
}

std::optional<int> CompanionBrain::AvoidThresholdOf(const Situation& s,
                                                    const Actor& monster) const {
  // Une invocation du camp ne s'évite pas : elle ne nous vise pas.
  if (monster.kind != ActorKind::kMonster || !CanBeTargeted(monster) ||
      SummonSideOf(s.world, monster, s.side) == SummonSide::kOwnSide) {
    return std::nullopt;
  }
  const CompanionTactic* tactic = TacticOf(s, monster);
  if (tactic == nullptr || tactic->avoid == TacticAvoid::kNever ||
      (tactic->avoid == TacticAvoid::kIfAttacks && !camp_attackers_.contains(monster.id))) {
    return std::nullopt;
  }
  return tactic->avoid_threshold_cells;
}

std::optional<CellPosition> CompanionBrain::SafeCellNearOwner(const Situation& s) const {
  const int follow_cells = std::max(s.settings.follow_distance_cells, 0);
  struct Avoided {
    CellPosition cell;
    int threshold_cells = 0;
  };
  std::vector<Avoided> avoided;
  for (const auto& [actor_id, actor] : s.world.actors()) {
    const std::optional<int> threshold = AvoidThresholdOf(s, actor);
    const CellPosition cell = StandingCell(actor);
    // Seuls comptent ceux qui gênent autour du maître.
    if (threshold.has_value() && WithinCells(s.owner_cell, cell, *threshold + follow_cells + 1)) {
      avoided.push_back(Avoided{.cell = cell, .threshold_cells = *threshold});
    }
  }
  if (avoided.empty()) {
    return std::nullopt;
  }
  std::optional<CellPosition> best;
  int best_squared = 0;
  for (int dy = -follow_cells; dy <= follow_cells; ++dy) {
    for (int dx = -follow_cells; dx <= follow_cells; ++dx) {
      const CellPosition cell =
          CellAt(static_cast<int>(s.owner_cell.x) + dx, static_cast<int>(s.owner_cell.y) + dy);
      if (!WithinCells(s.owner_cell, cell, follow_cells) ||
          std::ranges::any_of(avoided, [&cell](const Avoided& one) {
            return WholeCellDistance(cell, one.cell) <= one.threshold_cells;
          })) {
        continue;
      }
      // La plus proche au sens strict (carré de la distance) : deux cases à la
      // même distance entière ne se départagent pas par l'ordre du parcours.
      const int sx = static_cast<int>(cell.x) - static_cast<int>(s.self_cell.x);
      const int sy = static_cast<int>(cell.y) - static_cast<int>(s.self_cell.y);
      const int squared = (sx * sx) + (sy * sy);
      if (!best.has_value() || squared < best_squared) {
        best = cell;
        best_squared = squared;
      }
    }
  }
  return best;
}

std::optional<CompanionOrder> CompanionBrain::TryAvoid(const Situation& s, bool& avoiding) {
  avoiding = false;
  const Actor* threat = nullptr;
  int threat_cells = 0;
  for (const auto& [actor_id, actor] : s.world.actors()) {
    const std::optional<int> threshold = AvoidThresholdOf(s, actor);
    if (!threshold.has_value()) {
      continue;
    }
    const int cells = WholeCellDistance(s.self_cell, StandingCell(actor));
    if (cells > *threshold) {
      continue;
    }
    if (threat == nullptr || cells < threat_cells ||
        (cells == threat_cells && actor_id < threat->id)) {
      threat = &actor;
      threat_cells = cells;
    }
  }
  if (threat == nullptr) {
    return std::nullopt;
  }
  // D'ABORD UNE CASE QUE LE SUIVI ACCEPTE : à portée de suivi du maître et hors
  // du seuil de tout monstre évité. Le suivi n'a alors rien à reprendre.
  if (const std::optional<CellPosition> safe = SafeCellNearOwner(s);
      safe.has_value() && *safe != s.self_cell) {
    avoiding = true;
    return MoveTo(s, *safe, CompanionAiReason::kAvoid);
  }
  // SINON S'ÉCARTER DE « PAS » CASES, sans dépasser l'éloignement maximum du
  // maître quand il est réglé : le pas se raccourcit jusqu'à tenir dedans.
  const CompanionTactic* tactic = TacticOf(s, *threat);
  const CellPosition threat_cell = StandingCell(*threat);
  const int leash_cells =
      s.owner_walking ? s.settings.leash_walking_cells : s.settings.leash_still_cells;
  for (int step = tactic->avoid_step_cells; step > 0; --step) {
    const CellPosition away = AwayFrom(s.self_cell, threat_cell, step);
    if (away == s.self_cell ||
        (leash_cells > 0 && WholeCellDistance(away, s.owner_cell) > leash_cells)) {
      continue;
    }
    avoiding = true;
    return MoveTo(s, away, CompanionAiReason::kAvoid);
  }
  return std::nullopt;
}

}  // namespace ro::sim
