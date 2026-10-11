#include "world/actor_footprints.h"

#include <algorithm>
#include <array>
#include <optional>
#include <vector>

#include "ro/render/effect_frame.h"
#include "ro/sim/actor.h"
#include "ro/sim/movement.h"
#include "ro/sim/world.h"

#include "world/actor_view.h"

namespace moonclient {
namespace {

// Au plus combien de tics un appel rattrape : un client resté figé ne sème pas
// d'un coup une traînée entière.
constexpr std::uint32_t kMostTicksCaughtUp = 64;

// Une empreinte due : l'effet à semer, et son angle en degrés du client.
struct Sowing {
  std::uint32_t effect_id = 0;
  std::int32_t azimuth_degrees = 0;
};

}  // namespace

void ActorFootprints::Notice(const ro::sim::World& world, const MapStage& stage,
                             ro::sim::WalkPlanner& planner, EffectNodesInPlay& nodes,
                             std::uint32_t now_ms) {
  // UN ACTEUR PARTI OUBLIE SA TRACE, sans quoi le compte d'un ancien retiendrait
  // la première empreinte de celui qui reprendra son identifiant.
  std::erase_if(traces_, [&world](const auto& kept) { return world.Find(kept.first) == nullptr; });

  const auto tick_ms = static_cast<std::uint32_t>(ro::render::kEffectTickMs);
  const std::uint32_t now_tick = now_ms / tick_ms;
  // Les effets de l'acteur en cours, relus pour chacun dans le même tampon.
  std::vector<ro::render::ActorEffectPlay> played;
  for (const auto& [id, actor] : world.actors()) {
    PlayedEffectsOf(actor, now_ms, played);
    const std::optional<ro::render::ActorTrail> running = ro::render::TrailAfterEffects(played);
    const bool chase_walking = (actor.option & ro::sim::kOptionChaseWalk) != 0;
    if (!running.has_value() && !chase_walking) {
      traces_.erase(id);
      continue;
    }
    const auto ground_at = [&](std::uint32_t at_ms) {
      const ro::sim::FractionalCell cell =
          ro::sim::InterpolatedCell(actor, planner.PathFor(actor), at_ms);
      const std::array<float, 3> ground = GroundPositionAt(stage, cell.x, cell.y);
      return glm::vec3(ground[0], ground[1], ground[2]);
    };
    // ⚠ LE PREMIER RELEVÉ EST LA PLACE OÙ ON LE DÉCOUVRE. Le client part d'un
    // champ que rien n'a encore écrit ; partir d'ici fait seulement que la
    // première empreinte s'oriente sur la première marche vue.
    const auto [found, fresh] = traces_.try_emplace(id);
    Trace& trace = found->second;
    if (fresh) {
      trace.last_ground = ground_at(now_ms);
      trace.beaten_tick = now_tick;
      continue;
    }
    const std::uint32_t first_tick = std::max(
        trace.beaten_tick + 1, now_tick > kMostTicksCaughtUp ? now_tick - kMostTicksCaughtUp : 0U);
    for (std::uint32_t tick = first_tick; tick <= now_tick; ++tick) {
      const glm::vec3 here = ground_at(tick * tick_ms);
      // UN TIC DU SEMEUR pour un mode : Chase Walk d'abord, la course ensuite,
      // et sur le MÊME compte quand les deux tiennent.
      const auto beat = [&](std::uint32_t period, bool chase) -> std::optional<Sowing> {
        const auto period_count = static_cast<std::int32_t>(period);
        ++trace.count;
        if (trace.count > 2 * period_count) {
          trace.count = 1;
        } else if (trace.count < 0) {
          trace.count = 0;
          trace.last_ground = here;
        }
        if (trace.count != period_count && trace.count != 2 * period_count) {
          return std::nullopt;
        }
        // LE PAS SE MESURE DANS LE REPÈRE DU CLIENT, dont la profondeur est
        // l'opposée de la nôtre.
        const glm::vec3 step = here - trace.last_ground;
        trace.last_ground = here;
        const std::optional<std::int32_t> azimuth =
            ro::render::FootprintAzimuthDegrees(step.x, -step.z);
        if (!azimuth.has_value()) {
          return std::nullopt;
        }
        const std::size_t foot = trace.count == period_count ? 0 : 1;
        if (chase) {
          return Sowing{ro::render::kChaseWalkFootprintEffects[foot], *azimuth};
        }
        if (!running->clock_past_period) {
          return std::nullopt;
        }
        return Sowing{ro::render::kFootprintEffects[foot], *azimuth};
      };
      std::array<std::optional<Sowing>, 2> due{};
      if (chase_walking) {
        due[0] = beat(ro::render::kChaseWalkFootprintPeriodTicks, true);
      }
      if (running.has_value()) {
        due[1] = beat(ro::render::kFootprintPeriodTicks, false);
      }
      for (const std::optional<Sowing>& sowing : due) {
        if (sowing.has_value()) {
          static_cast<void>(nodes.PlayProcedural(sowing->effect_id, here, std::nullopt,
                                                 static_cast<float>(sowing->azimuth_degrees)));
        }
      }
    }
    trace.beaten_tick = now_tick;
  }
}

}  // namespace moonclient
