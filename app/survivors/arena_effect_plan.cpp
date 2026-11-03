#include "survivors/arena_effect_plan.h"

#include <algorithm>
#include <cmath>
#include <functional>

#include "ro/render/effect_visibility.h"
#include "ro/sim/level_up_effect.h"

#include "survivors/arena_camera.h"

namespace moonclient {
namespace {

// L'apparence que le héros montre : celle de l'évolution dès la première.
[[nodiscard]] std::uint16_t HeroJobOf(const ro::sim::SurvivorsField& field) {
  if (field.hero_class == nullptr) return 0;
  const ro::sim::ClassRule& hero_class = *field.hero_class;
  return field.hero.evolutions > 0 && hero_class.evolved_job_id != 0 ? hero_class.evolved_job_id
                                                                     : hero_class.job_id;
}

// LE CÔNE se montre par autant de projectiles de son effet, tirés du héros en
// éventail sur toute son ouverture, chacun visant la longueur du cône. L'effet
// s'oriente sur sa cible : posé au sol sans elle, il resterait droit.
inline constexpr std::int64_t kConeBursts = 3;

// Les lancers d'un cône : du bord gauche au bord droit, l'axe au milieu.
void PlanCone(const ro::sim::SurvivorsCast& cast, const SkillLook& look,
              std::vector<ArenaLaunch>& out) {
  const ro::sim::FieldPoint axis = cast.to - cast.from;
  const std::uint8_t facing = ro::sim::HeadingOf(axis, 0);
  const auto length = static_cast<std::int32_t>(ro::sim::IntegerSqrt(
      static_cast<std::uint64_t>(ro::sim::DistanceSquared(cast.from, cast.to))));
  for (std::int64_t burst = 0; burst < kConeBursts; ++burst) {
    const std::int64_t turn =
        std::int64_t{cast.spread_steps} * (2 * burst - (kConeBursts - 1)) / (kConeBursts - 1);
    const auto heading = static_cast<std::uint8_t>(
        (std::int64_t{facing} + turn + ro::sim::kHeadingSteps) % ro::sim::kHeadingSteps);
    out.push_back(ArenaLaunch{.effect_id = look.effect_id,
                              .kind = ArenaLaunchKind::kOnHero,
                              .at = cast.from,
                              .target = cast.from + ro::sim::TurnVector(heading, length)});
  }
}

// L'effet qu'un lancer de skill fait naître, d'après sa sorte.
void PlanCast(const ro::sim::SurvivorsCast& cast, const SkillLook& look,
              std::vector<ArenaLaunch>& out) {
  switch (cast.kind) {
    case ro::sim::CastKind::kStrike:
    case ro::sim::CastKind::kBlast:
      out.push_back(ArenaLaunch{
          .effect_id = look.effect_id, .kind = ArenaLaunchKind::kOnGround, .at = cast.to});
      return;
    case ro::sim::CastKind::kShot:
    case ro::sim::CastKind::kNova:
      out.push_back(ArenaLaunch{.effect_id = look.effect_id,
                                .kind = look.bearer == SkillBearer::kHero
                                            ? ArenaLaunchKind::kOnHero
                                            : ArenaLaunchKind::kOnGround,
                                .at = look.bearer == SkillBearer::kHero ? cast.from : cast.to,
                                .target = cast.to});
      return;
    case ro::sim::CastKind::kConeOn:
      PlanCone(cast, look, out);
      return;
    case ro::sim::CastKind::kStormOn:
      if (look.storm_sown) {
        out.push_back(ArenaLaunch{
            .effect_id = look.effect_id, .kind = ArenaLaunchKind::kOnGround, .at = cast.to});
      }
      return;
    // Orbites et zones se lisent sur l'état : voir `PlanArenaOrbs`.
    case ro::sim::CastKind::kOrbitOn:
    case ro::sim::CastKind::kOrbitOff:
    case ro::sim::CastKind::kWallOn:
    case ro::sim::CastKind::kRingOn:
    case ro::sim::CastKind::kZoneOff:
    case ro::sim::CastKind::kBlockOn:
      return;
  }
}

[[nodiscard]] std::int64_t Squared(std::int64_t value) { return value * value; }

// Le rang d'un impact : répété sur un monstre déjà touché dans l'image, proche
// du héros, ou plus loin. Un impact sans monstre nommé n'est jamais répété.
[[nodiscard]] ArenaEffectRank HitRank(const ro::sim::SurvivorsHit& hit, ro::sim::FieldPoint hero,
                                      std::vector<std::uint32_t>& struck_serials) {
  if (hit.foe_serial != 0) {
    if (std::ranges::find(struck_serials, hit.foe_serial) != struck_serials.end()) {
      return ArenaEffectRank::kLoose;
    }
    struck_serials.push_back(hit.foe_serial);
  }
  const std::int64_t reach = std::int64_t{kArenaNearHitCells} * ro::sim::kSubunitsPerCell;
  const bool near =
      Squared(std::int64_t{hit.at.x} - hero.x) + Squared(std::int64_t{hit.at.y} - hero.y) <=
      Squared(reach);
  return near ? ArenaEffectRank::kNearHit : ArenaEffectRank::kHit;
}

// Une case de la simulation en sous-unités, comptée depuis une emprise en cases
// flottantes, arrondie au-dessus : le cadre ne rétrécit jamais.
[[nodiscard]] std::int32_t UnitsOfCells(float cells) {
  return static_cast<std::int32_t>(
             std::ceil(cells * static_cast<float>(ro::sim::kSubunitsPerCell))) +
         kArenaSightMarginCells * ro::sim::kSubunitsPerCell;
}

}  // namespace

ArenaSight ArenaSightOf(const ro::render::Camera& camera, float aspect_ratio,
                        ro::sim::FieldPoint hero) {
  const std::optional<GroundFootprint> footprint = GroundFootprintOf(camera, aspect_ratio);
  if (!footprint.has_value()) return ArenaSight{};
  return ArenaSight{.bounded = true,
                    .west_x = hero.x - UnitsOfCells(footprint->west_cells),
                    .east_x = hero.x + UnitsOfCells(footprint->east_cells),
                    .south_y = hero.y - UnitsOfCells(footprint->south_cells),
                    .north_y = hero.y + UnitsOfCells(footprint->north_cells)};
}

void ArenaEffectBudget::BeginFrame(std::size_t living_seen, std::size_t living_anywhere) noexcept {
  living_seen_ = living_seen;
  living_anywhere_ = living_anywhere;
  launched_ = 0;
  skipped_ = 0;
}

std::size_t ArenaEffectBudget::LivingCeiling(ArenaEffectRank rank) const noexcept {
  if (!reserves_held_) return caps_.living;
  std::size_t reserved = 0;
  switch (rank) {
    case ArenaEffectRank::kHero:
      break;
    case ArenaEffectRank::kNearHit:
    case ArenaEffectRank::kHit:
      reserved = kArenaHeroLivingReserve;
      break;
    case ArenaEffectRank::kLoose:
      reserved = kArenaHeroLivingReserve + kArenaHitLivingReserve;
      break;
  }
  return caps_.living > reserved ? caps_.living - reserved : 0;
}

bool ArenaEffectBudget::Admit(ArenaEffectRank rank) noexcept {
  const bool over_caps = launched_ >= caps_.launches_per_frame || living_seen_ >= caps_.living ||
                         living_anywhere_ >= caps_.living * kArenaLivingAnywhereFactor;
  const bool over_ceiling = living_seen_ >= LivingCeiling(rank);
  if (over_caps || over_ceiling) {
    ++skipped_;
    ++skipped_total_;
    if (!over_caps) ++ranked_out_total_;
    return false;
  }
  ++launched_;
  ++living_seen_;
  ++living_anywhere_;
  return true;
}

void ArenaEffectLives::Add(std::uint32_t ends_ms, std::optional<ro::sim::FieldPoint> at) {
  lives_.push_back(Life{.ends_ms = ends_ms, .at = at});
}

void ArenaEffectLives::Expire(std::uint32_t now_ms) {
  if (now_ms < last_now_ms_) lives_.clear();
  last_now_ms_ = now_ms;
  std::erase_if(lives_, [now_ms](const Life& life) { return life.ends_ms <= now_ms; });
}

void ArenaEffectLives::Clear() noexcept {
  lives_.clear();
  last_now_ms_ = 0;
}

std::size_t ArenaEffectLives::CountSeen(const ArenaSight& sight) const noexcept {
  return static_cast<std::size_t>(std::ranges::count_if(
      lives_, [&sight](const Life& life) { return !life.at.has_value() || sight.Sees(*life.at); }));
}

bool ArenaLaunchSeen(const ArenaLaunch& launch, const ArenaSight& sight) noexcept {
  return launch.kind == ArenaLaunchKind::kOnHero || sight.Sees(launch.at) ||
         (launch.target.has_value() && sight.Sees(*launch.target));
}

ArenaSkillLooks::ArenaSkillLooks(const ro::sim::SurvivorsRules& rules) {
  looks_.reserve(rules.skills.size());
  for (const ro::sim::SurvivorsSkillRule& skill : rules.skills) {
    looks_.push_back(SkillLookOf(skill));
  }
}

const SkillLook* ArenaSkillLooks::Find(std::uint16_t skill_id) const noexcept {
  const auto found = std::ranges::find(looks_, skill_id, &SkillLook::skill_id);
  return found != looks_.end() ? &*found : nullptr;
}

void CollectArenaLaunches(const ro::sim::SurvivorsNews& news, const ro::sim::SurvivorsField& field,
                          const ArenaSkillLooks& looks, ArenaEffectSwitches switches,
                          std::vector<ArenaLaunch>& out) {
  out.clear();
  if (!switches.shown) return;
  if (!switches.reduced) {
    if (news.level_ups > 0) {
      out.push_back(ArenaLaunch{.effect_id = ro::sim::BaseLevelUpEffectId(HeroJobOf(field)),
                                .kind = ArenaLaunchKind::kOnHero,
                                .at = field.hero.position});
    }
    if (!news.evolutions.empty()) {
      out.push_back(ArenaLaunch{.effect_id = kJobChangeEffect,
                                .kind = ArenaLaunchKind::kOnHero,
                                .at = field.hero.position});
    }
    // UNE CARTE PRISE BRILLE SUR LE HÉROS, une fois par image.
    if (!news.cards_found.empty()) {
      out.push_back(ArenaLaunch{.effect_id = kCardPickupEffect,
                                .kind = ArenaLaunchKind::kOnHero,
                                .at = field.hero.position});
    }
    for (const ro::sim::SurvivorsCast& cast : news.casts) {
      if (const SkillLook* look = looks.Find(cast.skill_id)) PlanCast(cast, *look, out);
    }
  }
  std::vector<std::uint32_t> struck_serials;
  for (const ro::sim::SurvivorsHit& hit : news.hits) {
    const SkillLook* look = looks.Find(hit.skill_id);
    if (look == nullptr) continue;
    out.push_back(ArenaLaunch{.effect_id = switches.reduced ? kPlainHitEffect : look->hit_effect_id,
                              .kind = ArenaLaunchKind::kOnGround,
                              .at = hit.at,
                              .rank = HitRank(hit, field.hero.position, struck_serials)});
  }
}

void AdmitArenaLaunches(const ArenaSight& sight, ArenaEffectBudget& budget,
                        std::vector<ArenaLaunch>& launches) {
  std::ranges::stable_sort(launches, std::less<>(), &ArenaLaunch::rank);
  std::erase_if(launches, [&sight, &budget](const ArenaLaunch& launch) {
    if (!ArenaLaunchSeen(launch, sight)) {
      budget.NoteUnseen();
      return true;
    }
    return !budget.Admit(launch.rank);
  });
}

void PlanArenaLaunches(const ro::sim::SurvivorsNews& news, const ro::sim::SurvivorsField& field,
                       const ArenaSkillLooks& looks, ArenaEffectSwitches switches,
                       const ArenaSight& sight, ArenaEffectBudget& budget,
                       std::vector<ArenaLaunch>& out) {
  CollectArenaLaunches(news, field, looks, switches, out);
  AdmitArenaLaunches(sight, budget, out);
}

void PlanArenaOrbs(std::span<const ro::sim::SurvivorsOrb> orbs, const ArenaSkillLooks& looks,
                   ArenaEffectSwitches switches, std::vector<ArenaOrbLook>& out) {
  out.clear();
  if (!switches.shown || switches.reduced) return;
  for (const ro::sim::SurvivorsOrb& orb : orbs) {
    if (const SkillLook* look = looks.Find(orb.skill_id)) {
      out.push_back(ArenaOrbLook{.effect_id = look->effect_id, .at = orb.position});
    }
  }
}

bool ArenaZoneGroundShown(std::uint32_t effect_id, ArenaEffectSwitches switches) {
  return (switches.shown && !switches.reduced) || ro::render::IsForcedVisibleEffect(effect_id);
}

void PlanArenaZoneGrounds(std::span<const ro::sim::SurvivorsZone> zones,
                          const ArenaSkillLooks& looks, ArenaEffectSwitches switches,
                          std::vector<ArenaZoneGround>& out) {
  out.clear();
  for (const ro::sim::SurvivorsZone& zone : zones) {
    const SkillLook* look = looks.Find(zone.skill_id);
    if (look == nullptr || look->ground_effect_id == 0) continue;
    if (!ArenaZoneGroundShown(look->ground_effect_id, switches)) continue;
    out.push_back(ArenaZoneGround{.actor_id = kFirstZoneGroundActorId + zone.serial,
                                  .effect_id = look->ground_effect_id,
                                  .at = zone.from});
  }
}

}  // namespace moonclient
