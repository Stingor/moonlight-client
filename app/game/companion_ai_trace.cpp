#include "game/companion_ai_trace.h"

#include <format>
#include <string>

#include "ro/core/log.h"
#include "ro/sim/actor.h"
#include "ro/sim/companion_sight.h"
#include "ro/sim/movement.h"
#include "ro/sim/world.h"

namespace moonclient {
namespace {

// Un acteur décrit pour le journal : numéro, classe, distance au compagnon et
// au maître, et s'il est mort.
[[nodiscard]] std::string Describe(const ro::sim::Actor& actor, const ro::sim::Actor& self,
                                   const ro::sim::Actor* owner) {
  const ro::sim::CellPosition cell = ro::sim::StandingCell(actor);
  const int from_self = ro::sim::WholeCellDistance(ro::sim::StandingCell(self), cell);
  const int from_owner =
      owner != nullptr ? ro::sim::WholeCellDistance(ro::sim::StandingCell(*owner), cell) : -1;
  return std::format("{} (classe {}, à {} case(s) du compagnon, {} du maître{})", actor.id,
                     actor.look.job, from_self, from_owner,
                     actor.posture == ro::sim::Posture::kDead ? ", mort" : "");
}

}  // namespace

void CompanionAiTrace::Note(std::string_view companion, const ro::sim::World& world,
                            const ro::sim::CompanionFacts& facts,
                            const ro::sim::CompanionBrain& brain,
                            const std::optional<ro::sim::CompanionOrder>& order,
                            std::uint32_t now_ms) {
  Decision decision{.target_id = brain.target_id(),
                    .reason = brain.last_reason(),
                    .kind = said_.has_value() ? said_->kind : ro::sim::CompanionOrder::Kind::kNone};
  if (order.has_value()) {
    decision.kind = order->kind;
    last_order_ms_ = now_ms;
    silence_said_ = false;
  } else if (!last_order_ms_.has_value()) {
    last_order_ms_ = now_ms;
  }
  if (!said_.has_value() || *said_ != decision) {
    said_ = decision;
    ro::core::LogInfo("IA {} : cible {}, raison {}, ordre {}", companion, decision.target_id,
                      static_cast<int>(decision.reason), static_cast<int>(decision.kind));
  }
  if (silence_said_ || now_ms - *last_order_ms_ < kCompanionAiSilenceMs) {
    return;
  }
  silence_said_ = true;
  const ro::sim::Actor* self = world.Find(facts.actor_id);
  if (self == nullptr) {
    return;
  }
  const ro::sim::Actor* owner = world.player();
  const ro::sim::Actor* target = decision.target_id != 0 ? world.Find(decision.target_id) : nullptr;
  const ro::sim::Actor* nearest = nullptr;
  int nearest_cells = 0;
  for (const auto& [actor_id, actor] : world.actors()) {
    if (actor.kind != ro::sim::ActorKind::kMonster) {
      continue;
    }
    const int cells =
        ro::sim::WholeCellDistance(ro::sim::StandingCell(*self), ro::sim::StandingCell(actor));
    if (nearest == nullptr || cells < nearest_cells) {
      nearest = &actor;
      nearest_cells = cells;
    }
  }
  // Le refus du plus proche : « - » quand ce tic n'a pas cherché de cible.
  const auto refusal = nearest != nullptr ? brain.RefusalOf(nearest->id) : std::nullopt;
  ro::core::LogInfo(
      "IA {} : aucun ordre depuis {} ms (mode {}) — cible {}, monstre le plus proche {}, refus {}",
      companion, now_ms - *last_order_ms_, brain.mode_number(),
      target != nullptr ? Describe(*target, *self, owner) : std::string("aucune"),
      nearest != nullptr ? Describe(*nearest, *self, owner) : std::string("aucun"),
      refusal.has_value() ? std::to_string(static_cast<int>(*refusal)) : std::string("-"));
}

}  // namespace moonclient
