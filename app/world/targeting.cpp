#include "world/targeting.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "ro/sim/world.h"

#include "world/versus_gate.h"

namespace moonclient {
namespace {

// Les options qui cachent une entité : Hiding, Cloaking, et le `@hide` du staff.
// Garder la cible d'une entité invisible, ce serait offrir un détecteur.
constexpr std::uint32_t kConcealingOptions =
    ro::sim::kOptionHide | ro::sim::kOptionCloak | ro::sim::kOptionInvisible;

// Les cadences de redemande. Une jauge redemandée deux fois et demie par seconde
// se lit comme continue ; le nom se redemande plus souvent que le client ne
// s'autorise à le faire, faute de quoi il resterait inconnu jusqu'à dix
// secondes.
constexpr std::uint32_t kInfoPollMs = 400;
constexpr std::uint32_t kNamePollMs = 700;
constexpr std::uint32_t kStatusesPollMs = 700;
// Après un refus de la liste des états, on se tait ce temps-là pour ce GID.
constexpr std::uint32_t kRefusedQuietMs = 15000;

[[nodiscard]] bool Elapsed(std::uint32_t since_ms, std::uint32_t now_ms, std::uint32_t period_ms) {
  return since_ms == 0 || now_ms - since_ms >= period_ms;
}

}  // namespace

bool TargetIsConcealed(const ro::sim::Actor& actor) {
  return (actor.option & kConcealingOptions) != 0;
}

bool TargetTakesMarker(const ro::sim::Actor& actor) {
  if (actor.kind == ro::sim::ActorKind::kNonPlayer && actor.look.job == ro::sim::kPortalJob) {
    return false;
  }
  return !TargetIsConcealed(actor);
}

SkillAimKind SkillAimKindOf(const ro::sim::KnownSkill& skill) {
  return (skill.targeting & ro::sim::kSkillOnEnemy) != 0 ? SkillAimKind::kOffensive
                                                         : SkillAimKind::kSupport;
}

void SortByDistance(std::vector<TargetCandidate>& candidates) {
  std::stable_sort(candidates.begin(), candidates.end(),
                   [](const TargetCandidate& left, const TargetCandidate& right) {
                     return left.distance_squared < right.distance_squared;
                   });
}

bool Targeting::Designate(ro::sim::World& world, std::uint32_t id) {
  if (!enabled() || !world.Select(id)) {
    return false;
  }
  hidden_id_ = 0;
  return true;
}

bool Targeting::DesignateByProxy(ro::sim::World& world, std::uint32_t id) {
  if (!Designate(world, id)) {
    return false;
  }
  selection_write_ = id;
  return true;
}

bool Targeting::ApplyKeyboardTarget(ro::sim::World& world, std::uint32_t id, std::uint32_t now_ms) {
  if (id == 0 || !world.Select(id)) {
    return false;
  }
  hidden_id_ = 0;
  last_cycle_ms_ = now_ms;
  selection_write_ = id;
  return true;
}

void Targeting::NoteOnScreen(std::vector<TargetCandidate> candidates) {
  on_screen_ = std::move(candidates);
}

bool Targeting::TargetNearest(ro::sim::World& world, std::uint32_t now_ms) {
  if (!enabled() || on_screen_.empty()) {
    return false;
  }
  const auto nearest =
      std::min_element(on_screen_.begin(), on_screen_.end(),
                       [](const TargetCandidate& left, const TargetCandidate& right) {
                         return left.distance_squared < right.distance_squared;
                       });
  return ApplyKeyboardTarget(world, nearest->id, now_ms);
}

bool Targeting::CycleTarget(ro::sim::World& world, bool forward, std::uint32_t now_ms) {
  if (!enabled() || on_screen_.empty()) {
    return false;
  }
  std::vector<TargetCandidate> found = on_screen_;
  SortByDistance(found);
  const auto count = static_cast<int>(found.size());

  // Une pause remet le cycle à zéro : c'est ce qui sépare « parcourir ce qu'il
  // y a autour » (appuis rapprochés) de « prendre une cible » (appui isolé).
  const int reset_ms = settings_->cycle_reset_ms;
  const bool restart =
      reset_ms > 0 &&
      (last_cycle_ms_ == 0 || now_ms - last_cycle_ms_ > static_cast<std::uint32_t>(reset_ms));

  int current = -1;
  if (!restart) {
    for (int rank = 0; rank < count; ++rank) {
      if (found[static_cast<std::size_t>(rank)].id == world.selected_id()) {
        current = rank;
        break;
      }
    }
  }

  int next = 0;
  if (restart) {
    // Le plus proche, quel que soit le sens : « précédente » après une pause
    // veut dire « reprends la main », pas « le plus lointain ».
    next = 0;
  } else if (current < 0) {
    next = forward ? 0 : count - 1;
  } else {
    next = current + (forward ? 1 : -1);
    if (next >= count) {
      if (!settings_->cycle_wrap) {
        return false;
      }
      next = 0;
    } else if (next < 0) {
      if (!settings_->cycle_wrap) {
        return false;
      }
      next = count - 1;
    }
  }
  return ApplyKeyboardTarget(world, found[static_cast<std::size_t>(next)].id, now_ms);
}

bool Targeting::ClearTarget(ro::sim::World& world) {
  if (!enabled() || world.selected_id() == 0) {
    return false;
  }
  world.Deselect();
  // La dispense d'attaque tombe avec la cible : elle avait été accordée pour
  // engager CETTE entité.
  explicit_attack_id_ = 0;
  explicit_engage_pending_ = false;
  explicit_attack_once_ = false;
  selection_write_ = 0;
  return true;
}

void Targeting::NoteSkillTarget(ro::sim::World& world, std::uint32_t id) {
  if (!enabled() || id == 0 || id == world.player_id()) {
    return;
  }
  if (world.Select(id)) {
    hidden_id_ = 0;
  }
}

void Targeting::Revalidate(ro::sim::World& world) const {
  const std::uint32_t selected = world.selected_id();
  if (selected == 0) {
    return;
  }
  // Éteint, le mode ne laisse rien derrière lui.
  if (!enabled()) {
    world.Deselect();
    return;
  }
  const ro::sim::Actor* actor = world.selected();
  const ro::sim::TargetDetail& detail = world.target_detail();
  const bool refused = detail.id == selected && detail.told && !detail.reachable;
  if (actor == nullptr || TargetIsConcealed(*actor) || refused) {
    world.Deselect();
  }
}

bool Targeting::SuppressClickEngage(bool skill_armed) {
  if (!enabled()) {
    return false;
  }
  // L'ordre explicite passe, et ne consomme qu'un seul passage : le clic
  // ordinaire qui suit retombe sous la règle commune.
  if (explicit_engage_pending_) {
    explicit_engage_pending_ = false;
    return false;
  }
  // Une compétence armée emprunte le même chemin : la bloquer empêcherait tout
  // lancement au clic.
  if (!settings_->click_no_attack || skill_armed) {
    return false;
  }
  // Un clic ordinaire cible, et referme la dispense en cours.
  explicit_attack_id_ = 0;
  explicit_attack_once_ = false;
  return true;
}

int Targeting::FilterBasicAttack(std::uint32_t target_id, int action) {
  if (target_id == 0 || target_id != explicit_attack_id_) {
    return enabled() && settings_->click_no_attack ? kAttackDropped : action;
  }
  const bool single = explicit_attack_once_;
  if (single) {
    explicit_attack_id_ = 0;
    explicit_attack_once_ = false;
  }
  return single ? kAttackActionOnce : action;
}

void Targeting::NoteExplicitAttack(std::uint32_t id, bool once) {
  explicit_attack_id_ = id;
  explicit_engage_pending_ = id != 0;
  explicit_attack_once_ = once;
}

bool Targeting::WantsContinuousAttack(bool siege_map, bool ctrl_held, bool no_ctrl) {
  return !siege_map && AttackRepeats(ctrl_held, no_ctrl);
}

void Targeting::NoteWorldPress(bool second_of_double) {
  // Un appui neuf ne sait encore rien de ce qu'il touche : voir `NotePressedOn`.
  pressed_on_id_ = 0;
  if (second_of_double) {
    double_click_pending_ = true;
  }
}

std::uint32_t Targeting::TakeDoubleClickTarget(const ro::sim::World& world, bool skill_armed) {
  if (!double_click_pending_) {
    return 0;
  }
  double_click_pending_ = false;
  // Sans « le clic cible sans attaquer », le premier appui a déjà engagé.
  if (!enabled() || !settings_->click_no_attack || skill_armed) {
    return 0;
  }
  const std::uint32_t selected = world.selected_id();
  // 🔴 LE SECOND APPUI DOIT ÊTRE TOMBÉ SUR ELLE. Un double-clic au sol, pour
  // marcher, attaquait la cible restée désignée.
  //
  // Une dispense déjà ouverte pour cette cible : rien de plus à rejouer.
  if (selected == 0 || pressed_on_id_ != selected || explicit_attack_id_ == selected) {
    return 0;
  }
  return selected;
}

void Targeting::NoteFrameClicks(const ro::sim::World& world, bool left, bool right) {
  if (left) {
    frame_click_id_ = world.selected_id();
  }
  if (right) {
    frame_menu_id_ = world.selected_id();
  }
}

std::uint32_t Targeting::TakeFrameClick() { return std::exchange(frame_click_id_, 0); }

std::uint32_t Targeting::TakeFrameMenu() { return std::exchange(frame_menu_id_, 0); }

bool Targeting::FramesAct(const ro::sim::World& world, bool locked, bool ground_skill_armed) const {
  // Déverrouillés, les cadres se posent : le clic leur sert à ça. Un sort au
  // sol vise une case, et un cadre n'en est pas une.
  return enabled() && settings_->proxy_click && locked && world.selected_id() != 0 &&
         !ground_skill_armed;
}

void Targeting::HideFor(const ro::sim::World& world, std::uint32_t id) {
  if (id != 0 && id == world.selected_id()) {
    hidden_id_ = id;
  }
}

bool Targeting::Hidden(const ro::sim::World& world) const {
  return hidden_id_ != 0 && hidden_id_ == world.selected_id();
}

bool Targeting::ShownFor(const ro::sim::World& world, std::uint32_t id) const {
  return enabled() && id != 0 && id == world.selected_id() && !Hidden(world);
}

const ro::sim::Actor* Targeting::MarkedActor(const ro::sim::World& world) const {
  if (!enabled() || !settings_->native_marker || Hidden(world)) {
    return nullptr;
  }
  // Le personnage lui-même ne reçoit jamais la flèche : la recherche qui la
  // pose ne le voit pas.
  const ro::sim::Actor* actor = world.selected();
  return actor != nullptr && actor->id != world.player_id() && TargetTakesMarker(*actor) ? actor
                                                                                         : nullptr;
}

std::uint32_t Targeting::SkillTargetId(const ro::sim::World& world, SkillAimKind aim) const {
  if (!CastsOnTarget() || Hidden(world)) {
    return 0;
  }
  const ro::sim::Actor* actor = world.selected();
  if (actor == nullptr || TargetIsConcealed(*actor) || actor->id == world.player_id()) {
    return 0;
  }
  // Viser un mort avec un sort d'attaque n'est jamais ce qu'on veut ; un soutien,
  // lui, peut vouloir le relever.
  if (aim == SkillAimKind::kOffensive && actor->posture == ro::sim::Posture::kDead) {
    return 0;
  }
  return actor->id;
}

TargetRequests Targeting::Poll(const ro::sim::World& world, std::uint32_t now_ms,
                               bool statuses_shown) {
  TargetRequests asked;
  // Masquée, la cible ne se redemande plus : rien ne la montre.
  const ro::sim::Actor* actor = enabled() && !Hidden(world) ? world.selected() : nullptr;
  if (actor == nullptr) {
    polled_id_ = 0;
    return asked;
  }
  if (actor->id != polled_id_) {
    polled_id_ = actor->id;
    info_polled_ms_ = 0;
    statuses_polled_ms_ = 0;
    name_polled_ms_ = 0;
    refusals_seen_ = 0;
  }

  // Le nom : tant que la carte du serveur manque, on la redemande.
  if ((actor->name.empty() || !actor->card.has_value()) &&
      Elapsed(name_polled_ms_, now_ms, kNamePollMs)) {
    name_polled_ms_ = now_ms;
    asked.name_id = actor->id;
  }
  if (Elapsed(info_polled_ms_, now_ms, kInfoPollMs)) {
    info_polled_ms_ = now_ms;
    asked.info_id = actor->id;
  }

  // Jamais mes propres états : la barre d'états les montre déjà.
  if (statuses_shown && actor->id != world.player_id()) {
    const ro::sim::TargetDetail& detail = world.target_detail();
    if (detail.id == actor->id && detail.statuses_refusals != refusals_seen_) {
      refusals_seen_ = detail.statuses_refusals;
      statuses_refused_ms_[actor->id] = now_ms;
    }
    const auto refused = statuses_refused_ms_.find(actor->id);
    const bool quiet =
        refused != statuses_refused_ms_.end() && now_ms - refused->second < kRefusedQuietMs;
    if (!quiet && Elapsed(statuses_polled_ms_, now_ms, kStatusesPollMs)) {
      statuses_polled_ms_ = now_ms;
      asked.statuses_id = actor->id;
    }
  }
  return asked;
}

void Targeting::Forget() {
  explicit_attack_id_ = 0;
  explicit_engage_pending_ = false;
  explicit_attack_once_ = false;
  double_click_pending_ = false;
  frame_click_id_ = 0;
  frame_menu_id_ = 0;
  hidden_id_ = 0;
  polled_id_ = 0;
  statuses_refused_ms_.clear();
  on_screen_.clear();
}

}  // namespace moonclient
