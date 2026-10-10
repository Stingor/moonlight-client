#include "world/actor_footprints.h"

#include <algorithm>
#include <array>
#include <vector>

#include "ro/render/effect_frame.h"
#include "ro/sim/movement.h"
#include "ro/sim/world.h"

#include "world/actor_view.h"

namespace moonclient {
namespace {

// De combien l'acteur doit avoir bougé pour qu'une trace se justifie, en unités
// du monde.
//
// Le client compare les trois coordonnées à l'identique et ne sème que si l'une
// a changé ; en flottants, comparer à l'identique ne veut rien dire. Un
// dixième d'unité est bien en deçà d'un pas — une cellule en vaut cinq — et
// bien au-delà du bruit d'une interpolation.
constexpr float kMovedEnough = 0.1F;

}  // namespace

void ActorFootprints::Notice(const ro::sim::World& world, const MapStage& stage,
                             ro::sim::WalkPlanner& planner, EffectsInPlay& effects,
                             std::uint32_t now_ms) {
  // UN ACTEUR PARTI OUBLIE SA TRACE, sans quoi le compte d'un ancien retiendrait
  // la première empreinte de celui qui reprendra son identifiant.
  std::erase_if(traces_, [&world](const auto& kept) { return world.Find(kept.first) == nullptr; });

  const auto period_ms = static_cast<std::uint32_t>(ro::render::kFootprintPeriodTicks) *
                         static_cast<std::uint32_t>(ro::render::kEffectTickMs);
  // Les effets de l'acteur en cours, relus pour chacun dans le même tampon.
  std::vector<ro::render::ActorEffectPlay> played;
  for (const auto& [id, actor] : world.actors()) {
    PlayedEffectsOf(actor, now_ms, played);
    if (!ro::render::TrailAfterEffects(played).has_value()) {
      traces_.erase(id);
      continue;
    }
    const ro::sim::FractionalCell cell =
        ro::sim::InterpolatedCell(actor, planner.PathFor(actor), now_ms);
    const std::array<float, 3> ground = GroundPositionAt(stage, cell.x, cell.y);
    const glm::vec3 here(ground[0], ground[1], ground[2]);

    Trace& trace = traces_[id];
    if (!trace.started) {
      trace.started = true;
      trace.last_ground = here;
      trace.last_sown_ms = now_ms;
      continue;
    }
    if (now_ms - trace.last_sown_ms < period_ms) {
      continue;
    }
    trace.last_sown_ms = now_ms;
    if (glm::length(here - trace.last_ground) < kMovedEnough) {
      // Arrêté : rien à laisser, et la position de référence ne bouge pas non
      // plus — repartir laissera bien une trace au premier pas.
      continue;
    }
    // L'empreinte se pose LÀ OÙ IL ÉTAIT, puis le relevé avance.
    effects.PlayById(ro::render::kFootprintEffects[trace.next], trace.last_ground, now_ms);
    trace.next = (trace.next + 1) % ro::render::kFootprintEffects.size();
    trace.last_ground = here;
  }
}

}  // namespace moonclient
