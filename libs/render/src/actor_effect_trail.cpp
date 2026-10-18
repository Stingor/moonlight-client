#include <cmath>
#include <cstdint>

#include "ro/render/actor_effect_table.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// LE BIT DU COUREUR — `EF_RUN`, la course du Taekwon.
//
// Le rendu du sprite principal le lit et appelle le semeur de traces : tous
// les sept rendus, si le coureur a BOUGÉ depuis le dernier passage, il pose une
// empreinte sous ses pieds, orientée selon sa course, et alterne entre deux
// effets.
inline constexpr std::uint32_t kRunningBody = 0x8000;

// LE CHAMP QUE LE SEMEUR EXIGE. `EF_RUN` y recopie l'horloge de son instance, et
// le semeur ne pose rien tant qu'il ne dépasse pas sa période : la course laisse
// donc des traces seulement une fois lancée.
inline constexpr std::uint16_t kRunClockField = 0xF7;

// Ce qu'un octet de l'acteur garde d'une horloge qui court.
inline constexpr std::int32_t kClockByteMask = 0xFF;

// Le demi-tour que l'angle de la marche parcourt, et le quart de tour et demi
// que le client y ajoute pour coucher l'empreinte dans le sens du pas.
inline constexpr std::int32_t kHalfTurnDegrees = 180;
inline constexpr std::int32_t kFullTurnDegrees = 360;
inline constexpr std::int32_t kFootprintTurnDegrees = 135;

}  // namespace

std::optional<ActorTrail> TrailAfterEffects(std::span<const ActorEffectPlay> played) {
  for (const ActorEffectPlay& play : played) {
    const ActorEffectChange* change = ActorEffectChangeFor(play.effect_id);
    if (change == nullptr || (RenderBitsAt(*change, play.age_ticks) & kRunningBody) == 0) {
      continue;
    }
    // ⚠ LE SEMEUR ATTEND QUE L'HORLOGE DÉPASSE SA PÉRIODE, et l'effet l'écrit
    // depuis sa propre horloge dans un OCTET de l'acteur : voir `ActorTrail`.
    ActorTrail trail;
    for (const ActorFieldWrite& write : change->writes) {
      if (write.field == kRunClockField) {
        const auto clock_byte = static_cast<std::uint8_t>(
            ValueAt(write, play.age_ticks, change->life_ticks) & kClockByteMask);
        trail.clock_past_period = clock_byte > kFootprintPeriodTicks;
      }
    }
    return trail;
  }
  return std::nullopt;
}

std::optional<std::int32_t> FootprintAzimuthDegrees(float dx, float dz) {
  if (dx == 0.0F && dz == 0.0F) {
    return std::nullopt;
  }
  const float length = std::sqrt(dz * dz + dx * dx);
  std::int32_t degrees = 0;
  std::int32_t azimuth = kHalfTurnDegrees;
  bool found = true;
  while (dx / length < CosOfDegrees(degrees)) {
    if (++degrees > kHalfTurnDegrees) {
      found = false;
      break;
    }
  }
  if (found) {
    azimuth = dz / length < 0.0F ? kFullTurnDegrees - degrees : degrees;
  }
  return (azimuth + kFootprintTurnDegrees) % kFullTurnDegrees;
}

}  // namespace ro::render
