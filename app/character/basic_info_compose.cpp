#include "character/basic_info_compose.h"

#include <algorithm>
#include <cstdint>
#include <utility>

#include "ro/sim/actor.h"
#include "ro/sim/player_status.h"
#include "ro/sim/world.h"

namespace moonclient {
namespace {

// Une valeur dite, ou zéro. ⚠ LA SENTINELLE « INCONNU » EST NÉGATIVE.
[[nodiscard]] std::int64_t Said(std::int64_t value) { return std::max<std::int64_t>(value, 0); }

}  // namespace

bool ComposeBasicInfo(ro::ui::BasicInfoView& into, const ro::sim::World& world,
                      const ro::vfs::JobNames& jobs, std::string toggle_keys) {
  into = ro::ui::BasicInfoView();
  const ro::sim::Actor* self = world.player();
  if (self == nullptr) {
    return false;
  }
  const ro::sim::PlayerStatus& status = world.player_status();
  into.name = self->name;
  // LE MÉTIER EXERCÉ, celui que la fiche nomme : une monture ne le change pas.
  into.job = std::string(jobs.Find(self->look.job));
  into.base_level = Said(status.base_level);
  into.job_level = Said(status.job_level);
  // LA VIE VIENT DE L'ACTEUR, comme pour les jauges libres et les barres
  // au-dessus des têtes.
  into.hp = Said(self->health);
  into.max_hp = Said(self->max_health);
  into.sp = Said(status.sp);
  into.max_sp = Said(status.max_sp);
  into.base_exp = Said(status.base_exp);
  into.next_base_exp = Said(status.next_base_exp);
  into.job_exp = Said(status.job_exp);
  into.next_job_exp = Said(status.next_job_exp);
  into.zeny = Said(status.zeny);
  into.weight_tenths = Said(status.weight);
  into.max_weight_tenths = Said(status.max_weight);
  into.overweight_percent = Said(status.overweight_percent);
  into.toggle_keys = std::move(toggle_keys);
  return true;
}

}  // namespace moonclient
