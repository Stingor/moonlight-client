#include "ro/sim/survivors_bot.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <span>
#include <vector>

#include "ro/sim/survivors_content.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_play.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"
#include "ro/sim/walk_map.h"

#include "survivors_space.h"
#include "survivors_volley.h"

namespace ro::sim {

namespace {

using survivors::FoeRadius;
using survivors::Frozen;
using survivors::Hidden;
using survivors::InsideSpawnFrame;
using survivors::kPercent;
using survivors::LargestFoeRadius;
using survivors::SegmentDistanceSquared;

// Seize caps candidats : un sur quatre.
constexpr std::uint8_t kCandidateStride = 4;
// La fuite d'un monstre : (portée de menace − distance) × contact / 16.
constexpr std::int64_t kFleeDivisor = 16;
// Un monstre caché ou gelé ne compte que pour la moitié de sa fuite.
constexpr std::int64_t kHarmlessDivisor = 2;
// L'orbite : la moitié de la fuite, tournée d'un quart de tour.
constexpr std::int64_t kOrbitDivisor = 2;
// Le désir d'un but : un vecteur de cette longueur vers lui (robot).
constexpr std::int32_t kDesireUnits = Cells(8);
// Les points sondés devant le héros, à une et deux cases.
constexpr std::int32_t kNearProbeUnits = Cells(1);
constexpr std::int32_t kFarProbeUnits = Cells(2);
// Un tir de monstre en vol se suit sur au plus une seconde de sa course.
constexpr std::uint32_t kShotLookaheadSteps = kStepsPerSecond;
// La pénalité d'un cap menacé : plus que toute note qu'une somme de 400
// monstres, de contact et de portée validés, peut donner.
constexpr std::int64_t kThreatPenalty = std::int64_t{1} << 52U;
// Les tampons du robot, réservés à la construction : de quoi tenir la horde
// la plus nombreuse du catalogue sans réallouer.
constexpr std::size_t kReservedFoes = 512;
// Les gemmes ou plantes dont le robot éprouve le segment, à chaque pas.
constexpr std::uint32_t kMostGemTries = 8;

// Rangs de la table de priorité des offres, du meilleur au pire.
enum class OfferTier : std::uint8_t {
  kEvolutionCard,  // la carte qui complète l'évolution d'un skill possédé
  kEvolvingSkill,  // la montée d'un skill qui a une évolution
  kFreshSkill,     // un skill neuf, tant qu'un emplacement reste
  kCard,           // une carte neuve ou montée
  kOtherSkill,     // la montée d'un autre skill
  kWanted,         // le soin sous le seuil, sinon les zénys
  kUnwanted,       // l'autre des deux, et tout ce que le robot ne prend pas
};

struct Push {
  std::int64_t x = 0;
  std::int64_t y = 0;
  void Add(FieldPoint vector) noexcept {
    x += vector.x;
    y += vector.y;
  }
};

[[nodiscard]] std::int32_t ToUnits(std::int64_t units) noexcept {
  return static_cast<std::int32_t>(std::clamp<std::int64_t>(
      units, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()));
}

[[nodiscard]] std::int64_t Distance(FieldPoint a, FieldPoint b) noexcept {
  return IntegerSqrt(static_cast<std::uint64_t>(DistanceSquared(a, b)));
}

// `point` à moins de `radius` (inclus) de `center`.
[[nodiscard]] bool Within(FieldPoint point, FieldPoint center, std::int64_t radius) noexcept {
  return DistanceSquared(point, center) <= radius * radius;
}

// Le contact d'un monstre, au moins 1 : un monstre sans contact gêne encore
// le passage. Celui d'une élite est majoré comme dans la partie.
[[nodiscard]] std::int64_t ContactOf(const SurvivorsRules& rules, const SurvivorsFoe& foe) {
  const FoeRule* rule = FindFoe(rules, foe.foe_id);
  std::int64_t contact = rule != nullptr ? rule->touch_damage : 0;
  if (foe.elite)
    contact = contact * std::clamp(rules.elites.touch_pct, 0, kMostStatBonus) / kPercent;
  return std::max<std::int64_t>(contact, 1);
}

// Le point `units` devant le héros au cap `heading`, saturé dans l'int32.
[[nodiscard]] FieldPoint Ahead(FieldPoint hero, std::uint8_t heading, std::int32_t units) noexcept {
  const FieldPoint step = TurnVector(heading, units);
  return {ToUnits(std::int64_t{hero.x} + step.x), ToUnits(std::int64_t{hero.y} + step.y)};
}

[[nodiscard]] bool PassableAt(const WalkMap& arena, FieldPoint point) noexcept {
  return arena.Passable(CellOf(point.x), CellOf(point.y));
}

// UNE MENACE ANNONCÉE sous `point` : un séisme ou une flaque qui n'est pas
// éteinte, l'acte annoncé d'un monstre (charge, bond, tir : le segment de son
// corps à son but), ou un tir en vol sur sa course de la seconde qui vient.
// Chaque aire est élargie du rayon du héros.
[[nodiscard]] bool Threatened(const SurvivorsField& field, FieldPoint point) {
  const std::int64_t hero_radius = field.rules->tuning.hero_radius_units;
  for (const SurvivorsHazard& hazard : field.hazards) {
    if (hazard.until_step <= field.step) continue;
    if (Within(point, hazard.center, hazard.radius_units + hero_radius)) return true;
  }
  for (const SurvivorsFoe& foe : field.foes) {
    if (foe.hp <= 0 || foe.telegraph_until_step <= field.step) continue;
    const std::int64_t reach = FoeRadius(*field.rules, foe) + hero_radius;
    if (SegmentDistanceSquared(point, foe.position, foe.act_target) <= reach * reach) return true;
  }
  for (const SurvivorsFoeShot& shot : field.foe_shots) {
    if (shot.expires_step <= field.step) continue;
    const std::int64_t steps = std::min(shot.expires_step - field.step, kShotLookaheadSteps);
    const FieldPoint end{ToUnits(shot.position.x + std::int64_t{shot.velocity.x} * steps),
                         ToUnits(shot.position.y + std::int64_t{shot.velocity.y} * steps)};
    const std::int64_t reach = std::int64_t{shot.radius_units} + hero_radius;
    if (SegmentDistanceSquared(point, shot.position, end) <= reach * reach) return true;
  }
  return false;
}

// Le retour au centre : sur chaque axe, ce qui manque à la marge d'un bord.
[[nodiscard]] Push EdgePull(const SurvivorsField& field, std::int32_t margin) {
  Push pull;
  const FieldPoint hero = field.hero.position;
  const std::int64_t width = Cells(field.arena.width());
  const std::int64_t height = Cells(field.arena.height());
  pull.x += std::max<std::int64_t>(margin - std::int64_t{hero.x}, 0);
  pull.x -= std::max<std::int64_t>(margin - (width - hero.x), 0);
  pull.y += std::max<std::int64_t>(margin - std::int64_t{hero.y}, 0);
  pull.y -= std::max<std::int64_t>(margin - (height - hero.y), 0);
  return pull;
}

// Vrai si aucun monstre de `near_foes` ne se tient sur le segment de `from`
// à `to`, élargi de son rayon et de celui du héros.
[[nodiscard]] bool ClearPath(const SurvivorsField& field, std::span<const std::uint32_t> near_foes,
                             FieldPoint from, FieldPoint to) {
  const std::int64_t hero_radius = field.rules->tuning.hero_radius_units;
  for (const std::uint32_t index : near_foes) {
    const SurvivorsFoe& foe = field.foes[index];
    const std::int64_t reach = FoeRadius(*field.rules, foe) + hero_radius;
    if (SegmentDistanceSquared(foe.position, from, to) <= reach * reach) return false;
  }
  return true;
}

// LA GEMME OU LA PLANTE À ALLER CHERCHER : la plus proche à moins de `reach`
// dont le segment est libre, les gemmes avant les plantes et chacune dans son
// ordre à égalité. Les candidates s'éprouvent de la plus proche à la plus
// lointaine, au plus kMostGemTries : au-delà, le robot n'en cherche pas.
[[nodiscard]] std::optional<FieldPoint> NearestClearGem(const SurvivorsField& field,
                                                        std::span<const std::uint32_t> near_foes,
                                                        std::int64_t reach) {
  const FieldPoint hero = field.hero.position;
  const std::int64_t reach_squared = reach * reach;
  const std::size_t candidates = field.gems.size() + field.plants.size();
  const auto position_of = [&field](std::size_t sequence) {
    return sequence < field.gems.size() ? field.gems[sequence].position
                                        : field.plants[sequence - field.gems.size()].position;
  };
  // La dernière candidate écartée : (distance au carré, rang dans la suite).
  std::int64_t last_distance = -1;
  std::size_t last_sequence = 0;
  for (std::uint32_t attempt = 0; attempt < kMostGemTries; ++attempt) {
    std::optional<std::size_t> chosen;
    std::int64_t chosen_distance = 0;
    for (std::size_t sequence = 0; sequence < candidates; ++sequence) {
      const std::int64_t distance = DistanceSquared(position_of(sequence), hero);
      if (distance > reach_squared) continue;
      const bool after_last =
          distance > last_distance || (distance == last_distance && sequence > last_sequence);
      if (!after_last) continue;
      if (!chosen || distance < chosen_distance) {
        chosen = sequence;
        chosen_distance = distance;
      }
    }
    if (!chosen) return std::nullopt;
    const FieldPoint target = position_of(*chosen);
    if (ClearPath(field, near_foes, hero, target)) return target;
    last_distance = chosen_distance;
    last_sequence = *chosen;
  }
  return std::nullopt;
}

// Le plus proche objet au sol visible (dans le cadre d'apparition) de sorte
// `kind`, ou de toute sorte sans `kind` ; le premier tombé à égalité. Une
// carte au sol n'est jamais visée : elle ne sert pas la partie, et le robot
// joue la même partie qu'elle tombe ou non.
[[nodiscard]] const SurvivorsPickup* NearestPickup(const SurvivorsField& field,
                                                   const PickupKind* kind) {
  const SurvivorsPickup* chosen = nullptr;
  std::int64_t nearest = std::numeric_limits<std::int64_t>::max();
  for (const SurvivorsPickup& pickup : field.pickups) {
    if (pickup.kind == PickupKind::kCard) continue;
    if (kind != nullptr && pickup.kind != *kind) continue;
    if (!InsideSpawnFrame(field.rules->spawn_frame, field.hero.position, pickup.position)) continue;
    const std::int64_t distance = DistanceSquared(pickup.position, field.hero.position);
    if (distance < nearest) {
      nearest = distance;
      chosen = &pickup;
    }
  }
  return chosen;
}

[[nodiscard]] bool BelowPct(const SurvivorsHero& hero, std::int32_t pct) noexcept {
  return std::int64_t{hero.hp} * kPercent < std::int64_t{pct} * hero.max_hp;
}

// La place de `id` dans `order` ; la taille de `order` pour un Id absent.
[[nodiscard]] std::size_t RankIn(std::span<const std::uint16_t> order, std::uint16_t id) noexcept {
  const auto found = std::find(order.begin(), order.end(), id);
  return static_cast<std::size_t>(found - order.begin());
}

[[nodiscard]] const OwnedSkill* OwnedSkillOf(const SurvivorsHero& hero, std::uint16_t skill_id) {
  for (const OwnedSkill& owned : hero.skills) {
    if (owned.skill_id == skill_id) return &owned;
  }
  return nullptr;
}

// Vrai si `card_item_id` complète l'évolution d'un skill que le héros possède.
[[nodiscard]] bool CompletesEvolution(const SurvivorsField& field, std::uint16_t card_item_id) {
  for (const OwnedSkill& owned : field.hero.skills) {
    const EvolutionRule* evolution = FindEvolutionOf(*field.rules, owned.skill_id);
    if (evolution != nullptr && evolution->card_item_id == card_item_id) return true;
  }
  return false;
}

// Le classement d'une offre : son rang, puis une clé dans le rang (plus
// petite, meilleure).
struct OfferRank {
  OfferTier tier = OfferTier::kUnwanted;
  std::size_t key = 0;
  [[nodiscard]] bool operator<(const OfferRank& other) const noexcept {
    if (tier != other.tier) return tier < other.tier;
    return key < other.key;
  }
};

}  // namespace

SurvivorsBot::SurvivorsBot(const SurvivorsBotProfile& profile) : profile_(profile) {
  near_foes_.reserve(kReservedFoes);
}

SurvivorsInput SurvivorsBot::Steer(const SurvivorsField& field) {
  if (field.rules == nullptr) return SurvivorsInput{};
  const SurvivorsRules& rules = *field.rules;
  const FieldPoint hero = field.hero.position;
  const std::int64_t threat = std::max(profile_.threat_radius_units, 0);
  const std::int64_t panic = std::max(profile_.panic_radius_units, 0);
  const std::int64_t reach = std::max(profile_.gem_reach_units, 0);
  // Un monstre peut croiser le segment d'une gemme s'il est à moins de sa
  // portée, plus son rayon et celui du héros.
  const std::int64_t crossing =
      reach + LargestFoeRadius(rules) + std::int64_t{rules.tuning.hero_radius_units};

  // LA FUITE, et les monstres proches.
  Push flee;
  bool panicked = false;
  near_foes_.clear();
  for (std::size_t index = 0; index < field.foes.size(); ++index) {
    const SurvivorsFoe& foe = field.foes[index];
    if (foe.hp <= 0) continue;
    const std::int64_t distance_squared = DistanceSquared(foe.position, hero);
    if (distance_squared <= crossing * crossing) {
      near_foes_.push_back(static_cast<std::uint32_t>(index));
    }
    if (distance_squared < panic * panic) panicked = true;
    if (distance_squared >= threat * threat) continue;
    const std::int64_t distance = Distance(foe.position, hero);
    std::int64_t length = (threat - distance) * ContactOf(rules, foe) / kFleeDivisor;
    if (Hidden(field, foe) || Frozen(field, foe)) length /= kHarmlessDivisor;
    flee.Add(Toward(foe.position, hero, ToUnits(length)));
  }

  // LES BORDS.
  Push sum = flee;
  const Push edge = EdgePull(field, profile_.edge_margin_units);
  sum.x += edge.x;
  sum.y += edge.y;

  // LE DÉSIR, sans monstre à portée de panique : le coffre visible, la
  // Meat sous le seuil, l'objet visible le plus proche, puis la gemme ou la
  // plante la plus proche à portée dont le segment ne croise aucun monstre.
  std::optional<FieldPoint> goal;
  if (!panicked) {
    constexpr PickupKind kChest = PickupKind::kChest;
    constexpr PickupKind kMeat = PickupKind::kMeat;
    const SurvivorsPickup* wanted = NearestPickup(field, &kChest);
    if (wanted == nullptr && BelowPct(field.hero, profile_.heal_below_pct)) {
      wanted = NearestPickup(field, &kMeat);
    }
    if (wanted == nullptr) wanted = NearestPickup(field, nullptr);
    if (wanted != nullptr) {
      goal = wanted->position;
    } else {
      goal = NearestClearGem(field, near_foes_, reach);
    }
  }
  if (goal) {
    sum.Add(Toward(hero, *goal, kDesireUnits));
  } else {
    // L'ORBITE : un quart de tour de la fuite, dans le sens que donne la
    // parité de la minute.
    const bool clockwise = (field.step / kStepsPerMinute) % 2 != 0;
    const std::int64_t turned_x = (clockwise ? flee.y : -flee.y) / kOrbitDivisor;
    const std::int64_t turned_y = (clockwise ? -flee.x : flee.x) / kOrbitDivisor;
    sum.x += turned_x;
    sum.y += turned_y;
  }

  // LES CAPS : murés, menacés, notés.
  std::array<bool, kHeadingSteps> barred{};
  std::array<bool, kHeadingSteps> menaced{};
  std::uint8_t best = kNoHeading;
  std::int64_t best_score = 0;
  for (std::uint8_t heading = 0; heading < kHeadingSteps; heading += kCandidateStride) {
    const FieldPoint far = Ahead(hero, heading, kFarProbeUnits);
    barred[heading] = !PassableAt(field.arena, Ahead(hero, heading, kNearProbeUnits)) ||
                      !PassableAt(field.arena, far);
    if (barred[heading]) continue;
    menaced[heading] = Threatened(field, far);
    const FieldPoint unit = TurnVector(heading, kSubunitsPerCell);
    std::int64_t score = sum.x * unit.x + sum.y * unit.y;
    if (menaced[heading]) score -= kThreatPenalty;
    if (best == kNoHeading || score > best_score) {
      best = heading;
      best_score = score;
    }
  }
  // Rien ne pousse et le héros ne se tient dans aucune menace : il reste.
  if (best != kNoHeading && sum.x == 0 && sum.y == 0 && !Threatened(field, hero)) {
    best = kNoHeading;
  }

  // L'HYSTÉRÉSIS : le cap tenu, ni muré ni menacé, reste `steady_steps` pas.
  const bool held_sound = held_heading_ != kNoHeading && held_heading_ < kHeadingSteps &&
                          !barred[held_heading_] && !menaced[held_heading_];
  if (held_sound && field.step - held_since_step_ < profile_.steady_steps) {
    return SurvivorsInput{.heading = held_heading_};
  }
  if (best != held_heading_) {
    held_heading_ = best;
    held_since_step_ = field.step;
  }
  return SurvivorsInput{.heading = held_heading_};
}

SurvivorsAnswer SurvivorsBot::Choose(const SurvivorsField& field) const {
  SurvivorsAnswer answer{.kind = AnswerKind::kPick, .offer_index = 0};
  if (field.rules == nullptr || field.phase != SurvivorsPhase::kChoosing) return answer;
  const SurvivorsRules& rules = *field.rules;
  const bool hurt = BelowPct(field.hero, profile_.heal_below_pct);
  const bool slot_free = field.hero.skills.size() < rules.caps.skill_slots;
  OfferRank best;
  for (std::size_t index = 0; index < field.offers.size(); ++index) {
    const SurvivorsOffer& offer = field.offers[index];
    OfferRank rank;
    switch (offer.kind) {
      case OfferKind::kNewCard:
      case OfferKind::kCardLevel:
        rank = {OfferTier::kCard, RankIn(profile_.card_order, offer.id)};
        if (offer.kind == OfferKind::kNewCard && CompletesEvolution(field, offer.id)) {
          rank = {OfferTier::kEvolutionCard, 0};
        }
        break;
      case OfferKind::kSkillLevel: {
        if (FindEvolutionOf(rules, offer.id) == nullptr) {
          rank = {OfferTier::kOtherSkill, 0};
          break;
        }
        // Le plus avancé d'abord : le plus haut niveau atteint passe devant.
        const OwnedSkill* owned = OwnedSkillOf(field.hero, offer.id);
        const std::size_t level = owned != nullptr ? owned->level : 0;
        rank = {OfferTier::kEvolvingSkill, kMostSkillLevels - std::min(level, kMostSkillLevels)};
        break;
      }
      case OfferKind::kNewSkill:
        if (slot_free) rank = {OfferTier::kFreshSkill, RankIn(profile_.skill_order, offer.id)};
        break;
      case OfferKind::kHeal:
        rank = {hurt ? OfferTier::kWanted : OfferTier::kUnwanted, 0};
        break;
      case OfferKind::kZeny:
        rank = {hurt ? OfferTier::kUnwanted : OfferTier::kWanted, 0};
        break;
    }
    if (index == 0 || rank < best) {
      best = rank;
      answer.offer_index = static_cast<std::uint8_t>(index);
    }
  }
  return answer;
}

const SurvivorsBotProfile& MageBotProfile() {
  // Les skills et les cartes dans l'ordre de préférence du Magicien (robot).
  static constexpr std::array<std::uint16_t, 8> kSkillOrder{19, 13, 10, 11, 18, 15, 14, 20};
  static constexpr std::array<std::uint16_t, 8> kCardOrder{4019, 4072, 4003, 4197,
                                                           4009, 4022, 4016, 4001};
  static const SurvivorsBotProfile kProfile{.skill_order = kSkillOrder,
                                            .card_order = kCardOrder,
                                            .threat_radius_units = Cells(8),
                                            .panic_radius_units = Cells(3),
                                            .gem_reach_units = Cells(10),
                                            .edge_margin_units = Cells(20),
                                            .heal_below_pct = 50,
                                            .steady_steps = 5};
  return kProfile;
}

std::uint32_t PlayBot(SurvivorsPlay& play, SurvivorsBot& bot, std::uint32_t most_steps,
                      const std::function<void(const SurvivorsRun&)>& on_step) {
  std::uint32_t played = 0;
  while (played < most_steps) {
    const SurvivorsField& field = play.run().field();
    if (field.phase == SurvivorsPhase::kOver) break;
    if (field.phase == SurvivorsPhase::kChoosing) {
      if (!play.Answer(bot.Choose(field))) break;
      continue;
    }
    if (play.Advance(kSurvivorsStepMs, bot.Steer(field)) == 0) break;
    ++played;
    if (on_step) on_step(play.run());
  }
  return played;
}

}  // namespace ro::sim
