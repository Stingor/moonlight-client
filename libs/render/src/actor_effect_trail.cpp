#include <cstdint>

#include "ro/render/actor_effect_table.h"

namespace ro::render {
namespace {

// LE BIT DU COUREUR — `EF_RUN`, la course du Taekwon.
//
// Le rendu du sprite principal le lit et appelle le semeur de traces : tous
// les sept rendus, si le coureur a BOUGÉ depuis le dernier passage, il pose une
// empreinte à l'endroit qu'il quitte, orientée selon sa course, et alterne
// entre deux effets.
inline constexpr std::uint32_t kRunningBody = 0x8000;

// LE CHAMP QUE LE SEMEUR EXIGE. `EF_RUN` y recopie l'horloge de son instance, et
// le semeur ne pose rien tant qu'il ne dépasse pas sa période : la course laisse
// donc des traces seulement une fois lancée.
inline constexpr std::uint16_t kRunClockField = 0xF7;

}  // namespace

std::optional<ActorTrail> TrailAfterEffects(std::span<const ActorEffectPlay> played) {
  for (const ActorEffectPlay& play : played) {
    const ActorEffectChange* change = ActorEffectChangeFor(play.effect_id);
    if (change == nullptr || (RenderBitsAt(*change, play.age_ticks) & kRunningBody) == 0) {
      continue;
    }
    // ⚠ LE SEMEUR ATTEND QUE LE COMPTEUR DÉPASSE SA PÉRIODE, et l'effet l'écrit
    // depuis sa propre horloge : un coureur ne laisse rien pendant ses sept
    // premiers tics. Sans cette garde, une empreinte partirait à l'instant même
    // où la course commence, avant que le pied n'ait bougé.
    bool lance = false;
    for (const ActorFieldWrite& write : change->writes) {
      if (write.field == kRunClockField) {
        lance = play.age_ticks > kFootprintPeriodTicks;
      }
    }
    if (lance) {
      return ActorTrail{};
    }
  }
  return std::nullopt;
}

}  // namespace ro::render
