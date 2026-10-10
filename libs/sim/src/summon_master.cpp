// Le maître d'un monstre invoqué. Voir l'en-tête.

#include "ro/sim/summon_master.h"

#include <cstdint>
#include <utility>

#include "ro/sim/actor.h"
#include "ro/sim/world.h"

namespace ro::sim {

bool TellSummonMaster(World& world, std::uint32_t actor_id, std::uint32_t master_id) {
  const Actor* known = world.Find(actor_id);
  if (known == nullptr) {
    return false;
  }
  // 🔴 PAR LA RÉANNONCE, qui ne change rien d'autre : une fiche identique à
  // celle qu'on a, sauf le maître, que `World::Spawn` garde parce qu'il est dit.
  // Le reste de la fiche traverse comme à chaque mise en marche.
  Actor told = *known;
  told.summon_master_id = master_id;
  world.Spawn(std::move(told));
  return true;
}

std::uint32_t SummonMasterOf(const World& world, std::uint32_t actor_id) {
  const Actor* actor = world.Find(actor_id);
  return actor == nullptr ? 0 : actor->summon_master_id.value_or(0);
}

}  // namespace ro::sim
