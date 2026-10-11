#include "ro/sim/death_rules.h"

namespace ro::sim {
namespace {

[[nodiscard]] constexpr bool Within(std::uint32_t job, std::uint32_t first,
                                    std::uint32_t last) noexcept {
  return job >= first && job <= last;
}

[[nodiscard]] bool IsNonPlayer(const Actor& actor) noexcept {
  const std::uint32_t job = actor.look.job;
  const bool announced =
      actor.object_type == kObjectTypeNonPlayerEvent || actor.object_type == kObjectTypeNonPlayer;
  const bool dressed = Within(job, kFirstNonPlayerJob, kLastNonPlayerJob) ||
                       Within(job, kFirstExtraNonPlayerJob, kLastExtraNonPlayerJob);
  return (announced || dressed) && actor.object_type != kObjectTypeMonsterLookingNonPlayer &&
         job != kDyingNonPlayerJob;
}

[[nodiscard]] bool LeavesCorpse(const Actor& actor) noexcept {
  const std::uint32_t job = actor.look.job;
  return Within(job, kFirstCorpseBodyJob, kLastCorpseBodyJob) ||
         Within(job, kFirstMonsterJob, kLastMonsterJob) ||
         Within(job, kFirstExtendedMonsterJob, kLastExtendedMonsterJob) ||
         Within(job, kFirstSpecialUnitJob, kLastSpecialUnitJob) ||
         actor.object_type == kObjectTypeMercenary;
}

}  // namespace

DeathAftermath DeathAftermathOf(const Actor& actor) noexcept {
  // Un joueur — et tout ce que le paquet annonce avec un corps de joueur — ne
  // passe pas par la règle des créatures.
  if (actor.kind == ActorKind::kPlayer) {
    return DeathAftermath::kStaysDown;
  }
  if (IsNonPlayer(actor)) {
    return DeathAftermath::kStandsUp;
  }
  return LeavesCorpse(actor) ? DeathAftermath::kLeavesCorpse : DeathAftermath::kStaysDown;
}

}  // namespace ro::sim
