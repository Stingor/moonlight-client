// Voir l'en-tête.

#include "ro/sim/companion_ai.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "ro/sim/actor.h"
#include "ro/sim/companion_server_rules.h"
#include "ro/sim/map_mode.h"
#include "ro/sim/movement.h"
#include "ro/sim/world.h"

#include "companion_brain_detail.h"

namespace ro::sim {
namespace {

// LES DEUX PREMIÈRES ESPÈCES de chaque lignée (Lif, Amistr) ne prennent pas
// l'initiative.
constexpr int kHomunculusPassiveSpeciesPerLine = 2;

// LE MAÎTRE S'EST ÉLOIGNÉ de la case vers laquelle on l'a rejoint : au-delà de
// cette distance, le rejoindre se redemande même en pleine marche.
constexpr int kFollowRetargetCells = 3;
// LE PAS D'UNE FUITE, en cases, à l'opposé de ce qui attaque.
constexpr int kFleeStepCells = 5;
// UNE CIBLE AUTOMATIQUE QUI NE REÇOIT AUCUN COUP pendant ce délai d'attaques
// envoyées est lâchée : le serveur refuse sans un mot l'attaque d'une cible
// qu'il interdit (l'invocation d'un autre joueur hors PvP). Dix secondes
// couvrent au moins cinq délais d'attaque même au plus lent (deux secondes), et
// un coup raté s'annonce quand même : une attaque acceptée laisse toujours une
// trace bien avant.
constexpr std::uint32_t kUnansweredAttacksDropMs = 10000;
// Un tick enchaîne au plus ce nombre d'étapes sans ordre : changer d'état, puis
// agir dans le nouvel état.
constexpr int kMaxStepsPerTick = 4;

}  // namespace

using companion_detail::ApproachCell;
using companion_detail::AwayFrom;
using companion_detail::kMoveSettleMs;
using companion_detail::Midpoint;
using companion_detail::PercentOf;
using companion_detail::Plain;
using companion_detail::ShortenTo;

bool BasicAggression(AiCompanion kind, std::uint16_t job) noexcept {
  switch (kind) {
    case AiCompanion::kHomunculus: {
      const int type = static_cast<int>(job) - kHomunculusJobBase;
      if (type < 1 || type > kHomunculusBasicTypeCount) {
        return false;
      }
      return (type - 1) % kHomunculusSpeciesPerLine >= kHomunculusPassiveSpeciesPerLine;
    }
    case AiCompanion::kMercenary: {
      const int type = static_cast<int>(job) - kMercenaryJobBase;
      return type < 1 || type > kGuildMercenaryCount;
    }
  }
  return false;
}

AiCompanion AiCompanionOfJob(std::uint16_t job) noexcept {
  const int type = static_cast<int>(job) - kHomunculusJobBase;
  const int guild_type = static_cast<int>(job) - kMercenaryJobBase;
  const bool homunculus = type >= 1 && type <= kHomunculusLastType &&
                          !(guild_type >= 1 && guild_type <= kGuildMercenaryCount);
  return homunculus ? AiCompanion::kHomunculus : AiCompanion::kMercenary;
}

CompanionAiSettings BasicCompanionAiSettings(AiCompanion kind, std::uint16_t job) noexcept {
  CompanionAiSettings settings;
  settings.attack_on_own = BasicAggression(kind, job);
  return settings;
}

AttackPress AttackDesignation::Press(std::uint32_t actor_id, bool reserve,
                                     bool second_of_double) noexcept {
  if (second_of_double) {
    return AttackPress::kIgnored;
  }
  if (reserve) {
    designated_id_ = 0;
    return AttackPress::kReserveAttack;
  }
  if (designated_id_ != actor_id) {
    designated_id_ = actor_id;
    return AttackPress::kDesignated;
  }
  designated_id_ = 0;
  return AttackPress::kAttack;
}

std::optional<CompanionOrder> CompanionBrain::Tick(const World& world, const CompanionFacts& facts,
                                                   const CompanionAiSettings& asked,
                                                   std::uint32_t now_ms) {
  if (last_tick_ms_.has_value() && now_ms - *last_tick_ms_ < kCompanionAiTickMs) {
    return std::nullopt;
  }
  last_tick_ms_ = now_ms;
  refusals_.clear();
  // 🔴 L'IA APPLIQUE LE PROFIL SOUS LES RÈGLES DU SERVEUR : ses distances
  // s'arrêtent au rayon de vue. Voir `SettingsUnderServerRules`. ⚠ APRÈS LA
  // CADENCE : la copie des réglages ne se paie qu'aux ticks qui décident.
  const std::optional<CompanionAiSettings> ruled = SettingsUnderServerRules(asked, world.session());
  const CompanionAiSettings& settings = ruled.has_value() ? *ruled : asked;
  if (!facts.active) {
    return std::nullopt;
  }
  const Actor* self = world.Find(facts.actor_id);
  const Actor* owner = world.player();
  if (self == nullptr || owner == nullptr) {
    return std::nullopt;
  }
  skills_.Observe(*self, now_ms);
  SyncRules(settings, now_ms);
  CountTracedSkills(now_ms);
  std::vector<const Actor*> friends;
  if (!settings.friends.empty()) {
    for (const auto& [actor_id, actor] : world.actors()) {
      if (actor.kind == ActorKind::kPlayer && actor_id != owner->id &&
          std::ranges::find(settings.friends, actor.name) != settings.friends.end()) {
        friends.push_back(&actor);
      }
    }
  }
  const Situation s{
      .world = world,
      .self = *self,
      .owner = *owner,
      .facts = facts,
      .settings = settings,
      .now_ms = now_ms,
      .self_cell = Plain(StandingCell(*self)),
      .owner_cell = Plain(StandingCell(*owner)),
      .owner_walking = IsWalking(*owner, now_ms),
      .side = OwnSide{.owner_id = owner->id,
                      .companion_id = facts.actor_id,
                      .partner_id = facts.partner_id,
                      .party_ids = facts.party_member_ids},
      .friends = std::move(friends),
      .players = PlayerTargetingOf(world.map_mode()),
      .guild_spared = OwnerGuildSpared(world.map_mode()),
  };
  NoteMonsters(s);
  NoteRuleFacts(s);
  NoteAttackAnswered(s);
  wanted_monster_sheet_ = 0;
  offense_held_ = false;
  if (pending_.has_value()) {
    const DirectOrder order = *pending_;
    pending_.reset();
    StartDirect(s, order);
  }
  // LES RÈGLES, APRÈS L'ORDRE DIRECT ET AVANT LE COMPORTEMENT DE BASE. Un ordre
  // direct les fait taire, qu'il coure, qu'il attende dans la file réservée ou
  // qu'il tienne sa case — combat parti de la case compris —, comme l'attente.
  const bool direct =
      mode_ == Mode::kMoveOrder || mode_ == Mode::kSkillOrder || mode_ == Mode::kHoldCell ||
      (mode_ == Mode::kFight && (target_direct_ || fight_from_hold_)) || !reserved_.empty();
  if (!direct && !standby_ && !settings.rules.empty()) {
    if (std::optional<RuleDecision> decision = TryRules(s); decision.has_value()) {
      if (decision->order.has_value()) {
        return decision->order;
      }
      // « NE RIEN FAIRE » : la base ne garde que ses déplacements de sûreté.
      offense_held_ = true;
    }
  }
  for (int step = 0; step < kMaxStepsPerTick; ++step) {
    const Mode mode_before = mode_;
    const std::uint32_t target_before = target_id_;
    const std::size_t reserved_before = reserved_.size();
    std::optional<CompanionOrder> order = Step(s);
    if (order.has_value()) {
      return order;
    }
    if (mode_ == mode_before && target_id_ == target_before &&
        reserved_.size() == reserved_before) {
      break;
    }
  }
  return std::nullopt;
}

void CompanionBrain::Command(const DirectOrder& order) {
  pending_ = order;
  reserved_.clear();
  standby_ = false;
}

bool CompanionBrain::Reserve(const DirectOrder& order) {
  if (reserved_.size() >= kCompanionReserveCapacity) {
    return false;
  }
  reserved_.push_back(order);
  return true;
}

void CompanionBrain::ToggleStandby() {
  if (standby_) {
    standby_ = false;
    return;
  }
  standby_ = true;
  reserved_.clear();
  pending_.reset();
  ClearFight();
  mode_ = Mode::kIdle;
}

void CompanionBrain::ForgetMap(bool keep_standby, std::uint32_t now_ms) {
  CompanionMemory kept = Remember(now_ms);
  kept.standby = keep_standby && standby_;
  *this = CompanionBrain();
  Recall(kept, now_ms);
  arrived_ms_ = now_ms;
}

std::optional<CompanionOrder> CompanionBrain::Step(const Situation& s) {
  switch (mode_) {
    case Mode::kMoveOrder:
      return StepMoveOrder(s);
    case Mode::kSkillOrder:
      return StepSkillOrder(s);
    case Mode::kHoldCell:
      return StepHold(s);
    case Mode::kFight:
      // Un ordre direct, ou la case qu'il tient : le comportement de base ne
      // l'interrompt pas.
      if (target_direct_ || fight_from_hold_) {
        return StepFight(s);
      }
      break;
    case Mode::kIdle:
      break;
  }
  return StepBase(s);
}

std::optional<CompanionOrder> CompanionBrain::StepBase(const Situation& s) {
  const CompanionAiSettings& settings = s.settings;
  const int hp_percent = PercentOf(s.facts.hp, s.facts.max_hp);
  if (settings.flee_below_hp_percent > 0 && hp_percent < settings.flee_below_hp_percent) {
    EndFight(s, false);
    return Flee(s);
  }
  if (settings.return_below_hp_percent > 0 && hp_percent < settings.return_below_hp_percent) {
    EndFight(s, false);
    return FollowOwner(s, settings.follow_distance_cells, CompanionAiReason::kRetreat);
  }
  if (settings.owner_seated == OwnerSeated::kStayBesidePassive &&
      s.owner.posture == Posture::kSitting) {
    EndFight(s, false);
    return FollowOwner(s, companion_detail::kOwnerSideCells, CompanionAiReason::kOwnerSeated);
  }
  const int leash_cells =
      s.owner_walking ? settings.leash_walking_cells : settings.leash_still_cells;
  if (leash_cells > 0 && WholeCellDistance(s.self_cell, s.owner_cell) > leash_cells) {
    EndFight(s, false);
    // 🔴 LA LAISSE RAMÈNE DANS LA LAISSE : un suivi plus large qu'elle
    // laisserait le compagnon planté où elle l'a arrêté, hors de la laisse et
    // hors de tout combat.
    return FollowOwner(s, std::min(settings.follow_distance_cells, leash_cells),
                       CompanionAiReason::kLeash);
  }
  bool avoiding = false;
  std::optional<CompanionOrder> avoid = TryAvoid(s, avoiding);
  if (avoiding) {
    return avoid;
  }
  // « NE RIEN FAIRE » : ni attaque ni skill — combat, initiative, harcèlement,
  // ouverture —, mais le suivi du maître, comme la laisse, le repli, la fuite et
  // l'évitement plus haut.
  if (offense_held_) {
    return FollowOwner(s, settings.follow_distance_cells, CompanionAiReason::kFollowOwner);
  }
  if (mode_ == Mode::kFight) {
    return StepFight(s);
  }
  return StepIdle(s);
}

std::optional<CompanionOrder> CompanionBrain::StepIdle(const Situation& s) {
  mode_ = Mode::kIdle;
  if (!standby_ && !reserved_.empty()) {
    const DirectOrder next = reserved_.front();
    reserved_.pop_front();
    StartDirect(s, next);
    return std::nullopt;
  }
  if (!standby_ || s.settings.standby_defends) {
    const std::optional<TargetPick> pick = SelectTarget(s, standby_, /*holding_cell=*/false);
    if (pick.has_value()) {
      StartFight(*pick, /*direct=*/false, /*from_hold=*/false, s.now_ms);
      return std::nullopt;
    }
  }
  return FollowOwner(s, s.settings.follow_distance_cells,
                     standby_ ? CompanionAiReason::kStandby : CompanionAiReason::kFollowOwner);
}

std::optional<CompanionOrder> CompanionBrain::StepFight(const Situation& s) {
  const Actor* target = s.world.Find(target_id_);
  if (target == nullptr || !CanBeTargeted(*target) ||
      !WithinCells(s.self_cell, StandingCell(*target), kCompanionSightCells)) {
    EndFight(s, true);
    return std::nullopt;
  }
  if (!target_direct_) {
    if (!RereadTargetLine(s, *target)) {
      EndFight(s, false);
      return std::nullopt;
    }
    const CompanionTactic* tactic = target_tactic_.has_value() ? &*target_tactic_ : nullptr;
    if (target_reason_ == CompanionAiReason::kOwnInitiative &&
        SparedFor(s, *target, tactic, /*engaged=*/true)) {
      EndFight(s, true);
      return std::nullopt;
    }
    // « TENIR » : une frappe qui touche, puis la cible est laissée à d'autres.
    if (target_rank_ == TacticRank::kHold && s.self.attack.has_value() &&
        s.self.attack->target == target_id_ && s.self.attack->damage > 0 &&
        static_cast<std::int64_t>(s.self.attack->started_ms) >=
            static_cast<std::int64_t>(engaged_since_ms_)) {
      held_released_ms_[target_id_] = s.now_ms;
      EndFight(s, false);
      return std::nullopt;
    }
    if (TargetLeftUntouched(s) || AttacksUnanswered(s)) {
      abandoned_target_id_ = target_id_;
      abandoned_at_ms_ = s.now_ms;
      EndFight(s, true);
      return std::nullopt;
    }
    if (s.settings.retarget_while_chasing && !fight_from_hold_) {
      const std::optional<TargetPick> pick = SelectTarget(s, standby_, /*holding_cell=*/false);
      // UN RANG SUPÉRIEUR prend la place ; à rang égal, une raison plus pressante.
      const bool higher = pick.has_value() && pick->actor_id != target_id_ &&
                          (pick->rank < target_rank_ ||
                           (pick->rank == target_rank_ && pick->reason < target_reason_));
      if (higher) {
        StartFight(*pick, /*direct=*/false, /*from_hold=*/false, s.now_ms);
        return std::nullopt;
      }
    }
  }
  const CompanionAiReason reason =
      target_direct_ ? CompanionAiReason::kDirectAttack : target_reason_;
  const CellPosition target_cell = Plain(StandingCell(*target));
  const int attack_range_cells = std::max(s.facts.attack_range_cells, 1);
  const bool in_attack_range = WithinCells(s.self_cell, target_cell, attack_range_cells);
  // UNE CIBLE À ÉVITER se combat à distance : jamais plus près que son seuil.
  // L'attaque simple ne vaut que si sa portée dépasse le seuil.
  const std::optional<int> avoid_cells =
      target_direct_ ? std::nullopt : AvoidThresholdOf(s, *target);
  const bool melee_barred = avoid_cells.has_value() && attack_range_cells <= *avoid_cells;
  if (std::optional<CompanionOrder> skill = TrySkill(s, *target, target_cell, in_attack_range);
      skill.has_value()) {
    return skill;
  }
  if (std::optional<CompanionOrder> harass = TryHarass(s); harass.has_value()) {
    return harass;
  }
  // LES SKILLS RÉGLÉS POUR CETTE CIBLE : une tactique qui les interdit, ou qui
  // en a épuisé l'usage, ramène au combat ordinaire, « skills seuls » compris.
  const std::optional<TunedSkills> tuned = TunedForTarget(s);
  const OffensiveSkills mode =
      tuned.has_value() ? s.settings.skills.offensive : OffensiveSkills::kNever;
  const bool skills_only = mode == OffensiveSkills::kSkillsOnly;
  if (in_attack_range && !skills_only && !melee_barred) {
    if (!AttackReady(s)) {
      return std::nullopt;
    }
    return Emit(s, CompanionOrder{.kind = CompanionOrder::Kind::kAttack, .target_id = target_id_},
                reason);
  }
  // LA PORTÉE OÙ S'ARRÊTER. En « skills seuls », celle des skills : jamais
  // d'attaque simple, et à portée sans skill lançable, il attend. En « poursuite
  // et combat », celle du plus long skill qui partirait maintenant : il s'arrête
  // là, lance, puis reprend vers la portée d'attaque.
  int range_cells = melee_barred ? 0 : attack_range_cells;
  if (skills_only) {
    range_cells = CompanionSkillCaster::LongestReach(tuned->settings, s.facts.skills)
                      .value_or(attack_range_cells);
  } else if ((mode == OffensiveSkills::kChaseAndCombat ||
              (avoid_cells.has_value() && mode != OffensiveSkills::kNever)) &&
             SkillsAllowedOn(s, *target)) {
    range_cells = std::max(
        range_cells,
        skills_.ReachNow(tuned->settings, s.facts.skills, MomentOf(s, target_cell)).value_or(0));
  }
  // Rien qui porte au-delà du seuil d'une cible à éviter : il ne s'en approche
  // pas, et attend.
  if (avoid_cells.has_value() && range_cells <= *avoid_cells) {
    return std::nullopt;
  }
  if (WithinCells(s.self_cell, target_cell, range_cells)) {
    return std::nullopt;
  }
  if (fight_from_hold_ || !ChaseAllowed(s)) {
    EndFight(s, true);
    return std::nullopt;
  }
  return MoveToward(s, target_cell, range_cells, reason);
}

std::optional<CompanionOrder> CompanionBrain::StepMoveOrder(const Situation& s) {
  if (!move_sent_) {
    move_sent_ = true;
    moved_to_cell_.reset();
    return Emit(s, CompanionOrder{.kind = CompanionOrder::Kind::kMove, .cell = move_destination_},
                CompanionAiReason::kDirectMove);
  }
  if (IsWalking(s.self, s.now_ms) || !MoveSettled(s.now_ms)) {
    return std::nullopt;
  }
  // Arrivé, ou arrêté en chemin. Le reste d'un long trajet attend dans la file.
  mode_ = (s.settings.move_order_holds_cell && reserved_.empty()) ? Mode::kHoldCell : Mode::kIdle;
  return std::nullopt;
}

std::optional<CompanionOrder> CompanionBrain::StepHold(const Situation& s) {
  if (!reserved_.empty()) {
    mode_ = Mode::kIdle;
    return std::nullopt;
  }
  if (!s.settings.move_order_hold_fights) {
    return std::nullopt;
  }
  const std::optional<TargetPick> pick = SelectTarget(s, false, /*holding_cell=*/true);
  if (pick.has_value()) {
    StartFight(*pick, /*direct=*/false, /*from_hold=*/true, s.now_ms);
  }
  return std::nullopt;
}

std::optional<CompanionOrder> CompanionBrain::StepSkillOrder(const Situation& s) {
  const bool on_actor = skill_order_.kind == DirectOrder::Kind::kSkillOnActor;
  CellPosition target_cell = Plain(skill_order_.cell);
  if (on_actor && skill_order_.target_id != s.facts.actor_id) {
    const Actor* target = s.world.Find(skill_order_.target_id);
    const bool lost = target == nullptr || target->posture == Posture::kDead ||
                      (!s.side.Includes(target->id) && !CanBeTargeted(*target)) ||
                      !WithinCells(s.self_cell, StandingCell(*target), kCompanionSightCells);
    if (lost) {
      mode_ = Mode::kIdle;
      return std::nullopt;
    }
    target_cell = Plain(StandingCell(*target));
  } else if (on_actor) {
    target_cell = s.self_cell;
  }
  const bool on_self = on_actor && skill_order_.target_id == s.facts.actor_id;
  // UNE CASE HORS DE VUE ne s'atteint pas : l'ordre s'abandonne, comme une
  // cible perdue.
  if (!on_self && !WithinCells(s.self_cell, target_cell, kCompanionSightCells)) {
    mode_ = Mode::kIdle;
    return std::nullopt;
  }
  // ⚠ LE SERVEUR MESURE LA PORTÉE SUR LA POSITION COURANTE, et refuse sans un
  // mot un skill lancé en chemin : tant que le compagnon marche, `self_cell` est
  // la case d'ARRIVÉE, et rien ne part.
  const bool stopped = !IsWalking(s.self, s.now_ms) && MoveSettled(s.now_ms);
  const int range_cells = std::max(skill_order_.skill_range_cells, 1);
  if (!on_self && !WithinCells(s.self_cell, target_cell, range_cells)) {
    // ARRÊTÉ HORS DE PORTÉE APRÈS AVOIR VISÉ CETTE CASE D'APPROCHE : elle ne
    // s'atteint pas, l'ordre s'abandonne au lieu de se redemander sans fin.
    if (stopped && moved_to_cell_ == ApproachDestination(s, target_cell, range_cells)) {
      mode_ = Mode::kIdle;
      return std::nullopt;
    }
    return MoveToward(s, target_cell, range_cells, CompanionAiReason::kDirectSkill);
  }
  // Le délai d'arrivée le retient lui aussi, voir `SkillsAllowedOn`.
  if ((!on_self && !stopped) || ArrivalHolding(s)) {
    return std::nullopt;
  }
  mode_ = Mode::kIdle;
  skills_.NoteDirectSent(skill_order_.skill_id, s.now_ms);
  CompanionOrder order{.kind = CompanionOrder::Kind::kSkill,
                       .skill_id = skill_order_.skill_id,
                       .skill_level = skill_order_.skill_level};
  if (on_actor) {
    order.target_id = skill_order_.target_id;
  } else {
    order.cell = target_cell;
  }
  return Emit(s, order, CompanionAiReason::kDirectSkill);
}

std::optional<CompanionOrder> CompanionBrain::FollowOwner(const Situation& s, int threshold_cells,
                                                          CompanionAiReason reason) {
  if (WholeCellDistance(s.self_cell, s.owner_cell) <= threshold_cells) {
    followed_owner_cell_.reset();
    return std::nullopt;
  }
  // UN MONSTRE ÉVITÉ PRÈS DU MAÎTRE : le suivi vise une case hors de son seuil,
  // celle où l'évitement le mènerait, au lieu de le ramener contre le maître.
  if (const std::optional<CellPosition> safe = SafeCellNearOwner(s); safe.has_value()) {
    return *safe == s.self_cell ? std::nullopt : MoveTo(s, *safe, reason);
  }
  if (followed_owner_cell_.has_value()) {
    const bool owner_moved_on =
        WholeCellDistance(*followed_owner_cell_, s.owner_cell) > kFollowRetargetCells;
    const bool stopped = !IsWalking(s.self, s.now_ms) && MoveSettled(s.now_ms);
    if (!owner_moved_on && !stopped) {
      return std::nullopt;
    }
  }
  return Emit(s, CompanionOrder{.kind = CompanionOrder::Kind::kReturnToOwner}, reason);
}

std::optional<CompanionOrder> CompanionBrain::Flee(const Situation& s) {
  // La menace la plus proche parmi ce qui l'attaque.
  const Actor* threat = nullptr;
  int threat_cells = 0;
  for (const auto& [actor_id, actor] : s.world.actors()) {
    if (s.side.Includes(actor_id) || !IsAttacking(actor, s.self.id, s.now_ms)) {
      continue;
    }
    const int cells = WholeCellDistance(s.self_cell, StandingCell(actor));
    if (threat == nullptr || cells < threat_cells ||
        (cells == threat_cells && actor_id < threat->id)) {
      threat = &actor;
      threat_cells = cells;
    }
  }
  if (threat == nullptr) {
    return FollowOwner(s, s.settings.follow_distance_cells, CompanionAiReason::kFlee);
  }
  return MoveTo(s, AwayFrom(s.self_cell, StandingCell(*threat), kFleeStepCells),
                CompanionAiReason::kFlee);
}

std::optional<CompanionOrder> CompanionBrain::MoveTo(const Situation& s, CellPosition destination,
                                                     CompanionAiReason reason) {
  if (moved_to_cell_ == destination && (IsWalking(s.self, s.now_ms) || !MoveSettled(s.now_ms))) {
    return std::nullopt;
  }
  return Emit(s, CompanionOrder{.kind = CompanionOrder::Kind::kMove, .cell = destination}, reason);
}

CellPosition CompanionBrain::ApproachDestination(const Situation& s, CellPosition target_cell,
                                                 int range_cells) {
  return ShortenTo(s.self_cell, ApproachCell(s.self_cell, target_cell, range_cells),
                   kCompanionMoveMostCells);
}

std::optional<CompanionOrder> CompanionBrain::MoveToward(const Situation& s,
                                                         CellPosition target_cell, int range_cells,
                                                         CompanionAiReason reason) {
  const CellPosition destination = ApproachDestination(s, target_cell, range_cells);
  if (moved_to_cell_ == destination && (IsWalking(s.self, s.now_ms) || !MoveSettled(s.now_ms))) {
    return std::nullopt;
  }
  return Emit(s, CompanionOrder{.kind = CompanionOrder::Kind::kMove, .cell = destination}, reason);
}

std::optional<CompanionOrder> CompanionBrain::Emit(const Situation& s, CompanionOrder order,
                                                   CompanionAiReason reason) {
  last_reason_ = reason;
  switch (order.kind) {
    case CompanionOrder::Kind::kMove:
      moved_to_cell_ = order.cell;
      followed_owner_cell_.reset();
      last_move_sent_ms_ = s.now_ms;
      // En poursuite, les attaques sans réponse ne comptent plus : la cible
      // s'est éloignée.
      unanswered_since_ms_.reset();
      break;
    case CompanionOrder::Kind::kReturnToOwner:
      followed_owner_cell_ = s.owner_cell;
      moved_to_cell_.reset();
      last_move_sent_ms_ = s.now_ms;
      break;
    case CompanionOrder::Kind::kAttack:
      last_attack_sent_ms_ = s.now_ms;
      if (!unanswered_since_ms_.has_value()) {
        unanswered_since_ms_ = s.now_ms;
      }
      break;
    case CompanionOrder::Kind::kSkill:
      skill_sent_ms_[order.skill_id] = s.now_ms;
      break;
    case CompanionOrder::Kind::kNone:
      break;
  }
  return order;
}

void CompanionBrain::NoteAttackAnswered(const Situation& s) {
  if (unanswered_since_ms_.has_value() && s.self.attack.has_value() &&
      s.self.attack->target == target_id_ &&
      static_cast<std::int64_t>(s.self.attack->started_ms) >=
          static_cast<std::int64_t>(*unanswered_since_ms_)) {
    unanswered_since_ms_.reset();
  }
}

bool CompanionBrain::ArrivalHolding(const Situation& s) const {
  // ⚠ L'ARRIVÉE PEUT ÊTRE DATÉE APRÈS L'INSTANT DU TICK (relue après le
  // chargement de la carte) : l'écart est signé.
  return arrived_ms_.has_value() &&
         static_cast<std::int64_t>(s.now_ms) - static_cast<std::int64_t>(*arrived_ms_) <
             static_cast<std::int64_t>(s.settings.arrival_attack_hold_ms);
}

bool CompanionBrain::AttackReady(const Situation& s) const {
  if (ArrivalHolding(s)) {
    return false;
  }
  return !last_attack_sent_ms_.has_value() ||
         s.now_ms - *last_attack_sent_ms_ >= s.facts.amotion_ms;
}

bool CompanionBrain::MoveSettled(std::uint32_t now_ms) const {
  return !last_move_sent_ms_.has_value() || now_ms - *last_move_sent_ms_ >= kMoveSettleMs;
}

bool CompanionBrain::TargetLeftUntouched(const Situation& s) const {
  if (s.settings.drop_untouched_after_ms == 0) {
    return false;
  }
  std::uint32_t untouched_ms = s.now_ms - engaged_since_ms_;
  if (s.self.attack.has_value() && s.self.attack->target == target_id_) {
    untouched_ms = std::min(untouched_ms, s.now_ms - s.self.attack->started_ms);
  }
  return untouched_ms >= s.settings.drop_untouched_after_ms;
}

bool CompanionBrain::AttacksUnanswered(const Situation& s) const {
  return unanswered_since_ms_.has_value() &&
         s.now_ms - *unanswered_since_ms_ >= kUnansweredAttacksDropMs;
}

void CompanionBrain::StartDirect(const Situation& s, const DirectOrder& order) {
  ClearFight();
  moved_to_cell_.reset();
  followed_owner_cell_.reset();
  switch (order.kind) {
    case DirectOrder::Kind::kMove: {
      CellPosition destination = Plain(order.cell);
      // Un long trajet se fait par moitiés : la suite attend en tête de file.
      // ⚠ LA FILE GARDE SA CAPACITÉ : pleine, le trajet s'arrête au point milieu.
      while (ManhattanCells(s.self_cell, destination) > kCompanionMoveLegCells &&
             reserved_.size() < kCompanionReserveCapacity) {
        DirectOrder rest = order;
        rest.cell = destination;
        reserved_.push_front(rest);
        destination = Midpoint(s.self_cell, destination);
      }
      move_destination_ = destination;
      move_sent_ = false;
      mode_ = Mode::kMoveOrder;
      return;
    }
    case DirectOrder::Kind::kAttack:
      if (s.side.Includes(order.target_id) || s.world.Find(order.target_id) == nullptr) {
        mode_ = Mode::kIdle;
        return;
      }
      StartFight(
          TargetPick{.actor_id = order.target_id, .reason = CompanionAiReason::kDirectAttack},
          /*direct=*/true, /*from_hold=*/false, s.now_ms);
      return;
    case DirectOrder::Kind::kSkillOnActor:
    case DirectOrder::Kind::kSkillOnCell:
      skill_order_ = order;
      mode_ = Mode::kSkillOrder;
      return;
  }
}

void CompanionBrain::StartFight(const TargetPick& pick, bool direct, bool from_hold,
                                std::uint32_t now_ms) {
  target_id_ = pick.actor_id;
  target_reason_ = pick.reason;
  target_rank_ = pick.rank;
  target_tactic_ = pick.tactic;
  target_direct_ = direct;
  fight_from_hold_ = from_hold;
  engaged_since_ms_ = now_ms;
  mode_ = Mode::kFight;
  moved_to_cell_.reset();
  followed_owner_cell_.reset();
}

void CompanionBrain::EndFight(const Situation& s, bool over) {
  if (mode_ != Mode::kFight) {
    return;
  }
  const bool from_hold = fight_from_hold_;
  ClearFight();
  mode_ = from_hold ? Mode::kHoldCell : Mode::kIdle;
  if (over && !from_hold && s.settings.standby_after_fight) {
    standby_ = true;
  }
}

void CompanionBrain::ClearFight() {
  target_id_ = 0;
  target_reason_ = CompanionAiReason::kNone;
  target_direct_ = false;
  fight_from_hold_ = false;
  moved_to_cell_.reset();
  unanswered_since_ms_.reset();
  target_rank_ = TacticRank::kMedium;
  target_tactic_.reset();
}

}  // namespace ro::sim
