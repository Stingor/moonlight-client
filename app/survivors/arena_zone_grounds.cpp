#include "survivors/arena_zone_grounds.h"

#include <algorithm>
#include <cstdint>
#include <utility>

namespace moonclient {

void ArenaZoneGroundKeeper::Sow(Lit& lit, std::uint32_t now_ms, const ArenaZoneGroundHands& hands) {
  if (now_ms < lit.retry_ms) return;
  if (!hands.sow(lit.ground, !lit.sown_once)) {
    lit.retry_ms = now_ms + kZoneGroundRetryMs;
    return;
  }
  lit.sown_once = true;
  lit.retry_ms = 0;
  ++sown_total_;
}

void ArenaZoneGroundKeeper::Keep(std::span<const ArenaZoneGround> planned, std::uint32_t now_ms,
                                 const ArenaZoneGroundHands& hands) {
  // UNE ZONE SORTIE DU PLAN EMPORTE SON EFFET AU SOL, et l'acteur qui le portait.
  std::erase_if(lit_, [planned, &hands](const Lit& lit) {
    if (std::ranges::any_of(planned, [&lit](const ArenaZoneGround& ground) {
          return ground.actor_id == lit.ground.actor_id;
        })) {
      return false;
    }
    hands.put_out(lit.ground);
    return true;
  });
  for (const ArenaZoneGround& ground : planned) {
    auto lit = std::ranges::find_if(
        lit_, [&ground](const Lit& known) { return known.ground.actor_id == ground.actor_id; });
    if (lit == lit_.end()) {
      lit_.push_back(Lit{.ground = ground});
      Sow(lit_.back(), now_ms, hands);
      continue;
    }
    // UN VISUEL PERMANENT NE SE RELANCE PAS : il ne se resème que quand tout
    // ce qu'il avait semé est mort de sa propre vie, comme sous une unité.
    if (lit->sown_once && hands.carries(lit->ground)) continue;
    Sow(*lit, now_ms, hands);
  }
}

void ArenaZoneGroundKeeper::PutOutHidden(ArenaEffectSwitches switches,
                                         const ArenaZoneGroundHands& hands) {
  std::erase_if(lit_, [switches, &hands](const Lit& lit) {
    if (ArenaZoneGroundShown(lit.ground.effect_id, switches)) return false;
    hands.put_out(lit.ground);
    return true;
  });
}

void ArenaZoneGroundKeeper::Forget(const ArenaZoneGroundHands& hands) {
  for (const Lit& lit : lit_) hands.put_out(lit.ground);
  lit_.clear();
  sown_total_ = 0;
}

ArenaZoneGroundHands ArenaZoneGroundHandsOn(EffectsInPlay& effects, ro::sim::World& world,
                                            std::function<void(const ArenaZoneGround&)> place,
                                            std::function<glm::vec3(ro::sim::FieldPoint)> ground_of,
                                            std::uint32_t now_ms) {
  return ArenaZoneGroundHands{
      .carries =
          [&effects](const ArenaZoneGround& ground) {
            return effects.Carries(ground.effect_id, ground.actor_id);
          },
      .sow =
          [&effects, place = std::move(place), ground_of = std::move(ground_of), now_ms](
              const ArenaZoneGround& ground, bool first) {
            if (first) place(ground);
            constexpr bool kNotEndless = false;
            constexpr std::int32_t kPatternLife = 0;
            return effects.PlayById(ground.effect_id, ground_of(ground.at), now_ms, ground.actor_id,
                                    kNotEndless, kPatternLife, first);
          },
      .put_out =
          [&effects, &world](const ArenaZoneGround& ground) {
            effects.StopById(ground.effect_id, ground.actor_id);
            static_cast<void>(world.Remove(ground.actor_id));
          },
  };
}

}  // namespace moonclient
