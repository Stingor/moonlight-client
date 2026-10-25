#include "survivors/horde_poses.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "ro/render/ailment_sprites.h"
#include "ro/sim/movement.h"
#include "ro/sim/survivors_content.h"
#include "ro/sim/survivors_rules.h"

#include "survivors/arena_camera.h"
#include "survivors/survivors_looks.h"

namespace moonclient {
namespace {

using ro::render::Motion;

// Le centre d'une case est à sa moitié.
constexpr float kHalfCell = 0.5F;
constexpr std::uint32_t kPerMille = 1000;
constexpr std::uint8_t kOpaque = 255;
constexpr std::size_t kAlpha = 3;
// Le nord du client, et un tour.
constexpr float kClientNorthDegrees = 180.0F;
constexpr float kFullTurnDegrees = 360.0F;

// Le temps écoulé depuis `start_ms`, nul avant.
[[nodiscard]] float ElapsedSince(std::uint64_t presentation_ms, std::uint64_t start_ms) noexcept {
  return presentation_ms > start_ms ? static_cast<float>(presentation_ms - start_ms) : 0.0F;
}

[[nodiscard]] std::uint64_t MsOfStep(std::uint32_t step) noexcept {
  return std::uint64_t{step} * ro::sim::kSurvivorsStepMs;
}

// La position interpolée, en cases continues.
[[nodiscard]] glm::vec2 Interpolated(ro::sim::FieldPoint previous, ro::sim::FieldPoint current,
                                     std::uint32_t blend_per_mille) noexcept {
  const glm::vec2 from(static_cast<float>(previous.x), static_cast<float>(previous.y));
  const glm::vec2 to(static_cast<float>(current.x), static_cast<float>(current.y));
  const float share =
      static_cast<float>(std::min(blend_per_mille, kPerMille)) / static_cast<float>(kPerMille);
  return CellOfField(from + (to - from) * share);
}

[[nodiscard]] glm::vec2 CellOfPoint(ro::sim::FieldPoint point) noexcept {
  return CellOfField(glm::vec2(static_cast<float>(point.x), static_cast<float>(point.y)));
}

// La face d'un sprite au cap `heading`, sous la caméra figée au nord. Le client
// arrondit l'angle pour un joueur au repos ou en marche, et le tronque pour
// tout le reste.
[[nodiscard]] std::size_t DirectionOf(std::uint8_t heading, bool rounded) noexcept {
  return ro::render::SpriteDirectionOfDegrees(ClientDegreesOfHeading(heading), kArenaYawDegrees,
                                              rounded);
}

// Un monstre caché sous terre ne se montre pas.
[[nodiscard]] bool Hidden(const ro::sim::SurvivorsField& field, const ro::sim::SurvivorsFoe& foe,
                          const ro::sim::FoeRule* rule) noexcept {
  return rule != nullptr && foe.act_until_step > field.step &&
         (rule->behaviour.traits & ro::sim::kFoeHides) != 0;
}

}  // namespace

float WalkingMs(std::uint64_t presentation_ms, float walk_ms, std::uint32_t sim_id) noexcept {
  const float phase_ms = walk_ms * static_cast<float>(sim_id % kHordeWalkPhaseCount) /
                         static_cast<float>(kHordeWalkPhaseCount);
  return static_cast<float>(presentation_ms) + phase_ms;
}

glm::vec2 CellOfField(glm::vec2 subunits) noexcept {
  return subunits / static_cast<float>(ro::sim::kSubunitsPerCell) - glm::vec2(kHalfCell);
}

float ClientDegreesOfHeading(std::uint8_t heading) noexcept {
  return kClientNorthDegrees + kFullTurnDegrees * static_cast<float>(heading) /
                                   static_cast<float>(ro::sim::kHeadingSteps);
}

void HordePoses::NoticeFalls(std::span<const ro::sim::SurvivorsFall> falls,
                             std::uint64_t presentation_ms) {
  for (const ro::sim::SurvivorsFall& fall : falls) {
    FallingBody body{.serial = fall.foe_serial,
                     .foe_id = fall.foe_id,
                     .cell = CellOfPoint(fall.at),
                     .direction = 0,
                     .elite = false,
                     .fell_ms = presentation_ms};
    const auto known = std::ranges::lower_bound(memory_, fall.foe_serial, {}, &FoeMemory::serial);
    if (known != memory_.end() && known->serial == fall.foe_serial) {
      body.direction = DirectionOf(known->facing, /*rounded=*/false);
      body.elite = known->elite;
    }
    falling_.push_back(body);
  }
  if (falling_.size() > kMostFallingBodies) {
    falling_.erase(falling_.begin(), falling_.begin() + static_cast<std::ptrdiff_t>(
                                                            falling_.size() - kMostFallingBodies));
  }
}

void HordePoses::Pose(const ro::sim::SurvivorsField& field, std::uint64_t presentation_ms,
                      std::uint32_t blend_per_mille, const HordeTimesOf& times_of) {
  poses_.clear();
  PoseHero(field, presentation_ms, blend_per_mille, times_of);
  PoseFoes(field, presentation_ms, blend_per_mille, times_of);
  PoseFalling(presentation_ms, times_of);
  PoseGround(field, presentation_ms, blend_per_mille);
}

void HordePoses::Reset() {
  memory_.clear();
  next_memory_.clear();
  falling_.clear();
  poses_.clear();
}

void HordePoses::PoseHero(const ro::sim::SurvivorsField& field, std::uint64_t presentation_ms,
                          std::uint32_t blend_per_mille, const HordeTimesOf& /*times_of*/) {
  const ro::sim::SurvivorsHero& hero = field.hero;
  HordePose pose{.kind = HordeBodyKind::kHero,
                 .cell = Interpolated(hero.previous_position, hero.position, blend_per_mille)};
  if (field.tally.outcome == ro::sim::SurvivorsOutcome::kFell) {
    pose.motion = Motion::kDie;
    pose.motion_ms = ElapsedSince(presentation_ms, MsOfStep(field.step));
  } else {
    const bool moving = hero.position != hero.previous_position;
    pose.motion = moving ? Motion::kWalk : Motion::kIdle;
    pose.motion_ms = static_cast<float>(presentation_ms);
    if (!moving) pose.held_frame = ro::sim::kGazeAhead;
  }
  pose.direction = DirectionOf(hero.facing, pose.motion != Motion::kDie);
  poses_.push_back(pose);
}

void HordePoses::PoseFoes(const ro::sim::SurvivorsField& field, std::uint64_t presentation_ms,
                          std::uint32_t blend_per_mille, const HordeTimesOf& times_of) {
  next_memory_.clear();
  std::size_t remembered = 0;
  for (const ro::sim::SurvivorsFoe& foe : field.foes) {
    // Les monstres et le souvenir vont tous deux par serial croissant : l'ordre
    // de naissance.
    while (remembered < memory_.size() && memory_[remembered].serial < foe.serial) ++remembered;
    FoeMemory memory{.serial = foe.serial};
    if (remembered < memory_.size() && memory_[remembered].serial == foe.serial) {
      memory = memory_[remembered];
    }
    memory.facing = foe.facing;
    memory.elite = foe.elite;
    // 🔴 LA BLESSURE EST BRIDÉE : un coup qui tombe pendant les 300 ms d'une
    // blessure commencée ne la relance pas.
    if (foe.hurt_step != memory.seen_hurt_step) {
      const std::uint64_t hit_ms = MsOfStep(foe.hurt_step);
      if (!memory.hurting || hit_ms >= memory.hurt_started_ms + kHurtReplayMs) {
        memory.hurt_started_ms = hit_ms;
        memory.hurting = true;
      }
      memory.seen_hurt_step = foe.hurt_step;
    }
    next_memory_.push_back(memory);

    const ro::sim::FoeRule* rule = ro::sim::FindFoe(*field.rules, foe.foe_id);
    if (Hidden(field, foe, rule)) continue;
    const HordeMotionTimes times = times_of(HordeBodyKind::kFoe, foe.foe_id);
    HordePose pose{.kind = HordeBodyKind::kFoe,
                   .sim_id = foe.serial,
                   .look_id = foe.foe_id,
                   .direction = DirectionOf(foe.facing, /*rounded=*/false),
                   .cell = Interpolated(foe.previous_position, foe.position, blend_per_mille)};
    if (foe.elite) {
      pose.scale = kEliteScale;
      pose.tint = kEliteTint;
    }
    const bool frozen = field.step < foe.freeze_until_step;
    if (frozen) {
      pose.tint = ro::render::kFrozenTint;
    } else if (field.step < foe.chill_until_step) {
      pose.tint = kChilledTint;
    }
    // L'attaque au contact commence au pas du dernier contact, que le pas de la
    // prochaine touche permise désigne.
    const bool touched =
        rule != nullptr && rule->touch_interval_ms > 0 && foe.touch_ready_step > foe.born_step;
    const float attack_ms =
        touched ? ElapsedSince(presentation_ms, MsOfStep(foe.touch_ready_step -
                                                         ro::sim::StepsOf(rule->touch_interval_ms)))
                : 0.0F;
    const float hurt_ms =
        memory.hurting ? ElapsedSince(presentation_ms, memory.hurt_started_ms) : 0.0F;
    if (frozen) {
      // Gelé, il se fige sur sa première image.
      pose.motion = Motion::kIdle;
      pose.motion_ms = 0.0F;
    } else if (touched && attack_ms < times.attack_ms) {
      pose.motion = Motion::kAttack;
      pose.motion_ms = attack_ms;
    } else if (memory.hurting && hurt_ms < times.hurt_ms) {
      pose.motion = Motion::kHurt;
      pose.motion_ms = hurt_ms;
    } else if (foe.units_per_step > 0) {
      pose.motion = Motion::kWalk;
      pose.motion_ms = WalkingMs(presentation_ms, times.walk_ms, foe.serial);
    } else {
      pose.motion = Motion::kIdle;
      pose.motion_ms = static_cast<float>(presentation_ms);
    }
    poses_.push_back(pose);
  }
  std::swap(memory_, next_memory_);
}

void HordePoses::PoseFalling(std::uint64_t presentation_ms, const HordeTimesOf& times_of) {
  // Un mort s'efface après sa chute ; effacé, il sort de la liste.
  const auto fade_ms = [&](const FallingBody& body) {
    const float die_ms = times_of(HordeBodyKind::kFalling, body.foe_id).die_ms;
    return ElapsedSince(presentation_ms, body.fell_ms) - die_ms;
  };
  std::erase_if(falling_, [&](const FallingBody& body) {
    return fade_ms(body) >= static_cast<float>(kFallFadeMs);
  });
  for (const FallingBody& body : falling_) {
    HordePose pose{.kind = HordeBodyKind::kFalling,
                   .sim_id = body.serial,
                   .look_id = body.foe_id,
                   .motion = Motion::kDie,
                   .motion_ms = ElapsedSince(presentation_ms, body.fell_ms),
                   .direction = body.direction,
                   .cell = body.cell};
    if (body.elite) {
      pose.scale = kEliteScale;
      pose.tint = kEliteTint;
    }
    const float faded_ms = std::max(fade_ms(body), 0.0F);
    const float kept = 1.0F - faded_ms / static_cast<float>(kFallFadeMs);
    pose.tint[kAlpha] = static_cast<std::uint8_t>(static_cast<float>(kOpaque) * kept);
    poses_.push_back(pose);
  }
}

void HordePoses::PoseGround(const ro::sim::SurvivorsField& field, std::uint64_t presentation_ms,
                            std::uint32_t blend_per_mille) {
  const auto at_rest = static_cast<float>(presentation_ms);
  for (const ro::sim::SurvivorsPlant& plant : field.plants) {
    poses_.push_back(HordePose{.kind = HordeBodyKind::kPlant,
                               .sim_id = plant.serial,
                               .look_id = plant.foe_id,
                               .motion_ms = at_rest,
                               .cell = CellOfPoint(plant.position)});
  }
  for (const ro::sim::SurvivorsPickup& pickup : field.pickups) {
    poses_.push_back(HordePose{.kind = HordeBodyKind::kPickup,
                               .sim_id = pickup.serial,
                               .look_id = PickupLookOf(pickup),
                               .motion_ms = at_rest,
                               .cell = CellOfPoint(pickup.position)});
  }
  for (std::size_t rank = 0; rank < field.gems.size(); ++rank) {
    const ro::sim::SurvivorsGem& gem = field.gems[rank];
    poses_.push_back(
        HordePose{.kind = HordeBodyKind::kGem,
                  .sim_id = static_cast<std::uint32_t>(rank),
                  .look_id = GemItemOf(gem.exp),
                  .motion_ms = at_rest,
                  .cell = Interpolated(gem.previous_position, gem.position, blend_per_mille)});
  }
}

}  // namespace moonclient
