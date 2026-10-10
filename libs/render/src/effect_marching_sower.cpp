#include "ro/render/effect_marching_sower.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "ro/render/effect_launch.h"

namespace ro::render {
namespace {

// LES SEMEURS QUI MARCHENT.
//
// Un même mouvement sert les deux : le trait de Frost Diver et sa variante
// d'apparat, que le répartiteur sépare par le paramètre de l'instance. Le
// premier pas vaut cinq fois le vecteur avant, le pas suivant deux fois le
// même, et l'arrivée se juge à deux unités et demie de la cible.
//
// La durée propre de l'effet vient de `client_effect_durations.json`.
//
// Le projectile de pierre part de deux unités, en fait deux de plus par tic et
// ne sème qu'un tic sur trois ; son arrivée se juge depuis la SOURCE. Sa durée
// propre est de deux cent cinquante tics.
constexpr std::array<EffectMarchingSower, 3> kMarchingSowers = {{
    {27, 5.0F, 2.0F, 2.5F, 150},
    {801, 5.0F, 2.0F, 2.5F, 150},
    {123, 2.0F, 2.0F, 2.5F, 250, 3, true},
}};

}  // namespace

const EffectMarchingSower* EffectMarchingSowerFor(std::uint32_t effect_id) noexcept {
  const auto found = std::ranges::find(kMarchingSowers, effect_id, &EffectMarchingSower::effect_id);
  return found != kMarchingSowers.end() ? &*found : nullptr;
}

float EffectGroundDistance(glm::vec3 from, glm::vec3 to) noexcept {
  const float dx = to.x - from.x;
  const float dz = to.z - from.z;
  return std::sqrt(dx * dx + dz * dz);
}

std::int32_t EffectMarchingSowerLastSowTick(const EffectMarchingSower& sower,
                                            float distance_to_target) noexcept {
  if (sower.distance_per_tick <= 0.0F) {
    return 0;
  }
  // DEPUIS LA SOURCE, l'arrivée est jugée une fois pour toutes : au premier
  // semis, ou jamais.
  if (sower.arrival_measured_at_source) {
    return distance_to_target <= sower.arrival_distance ? 0 : sower.last_tick;
  }
  // LE PREMIER TIC OÙ LA MARCHE ENTRE DANS LA BANDE D'ARRIVÉE. Elle s'écarte
  // d'un pas constant, si bien que les tics qui touchent au but forment un
  // intervalle : le premier en est le plancher.
  const float plancher = (distance_to_target - sower.arrival_distance - sower.first_distance) /
                         sower.distance_per_tick;
  const auto arrivee = std::max<std::int32_t>(0, static_cast<std::int32_t>(std::ceil(plancher)));
  // ⚠ ET IL PEUT ENJAMBER LA BANDE. Une cible trop proche — ou pas de cible du
  // tout, la source lui servant alors de cible — laisse le semeur hors
  // d'atteinte dès son premier pas : il marche jusqu'au bout de sa vie, et c'est
  // le client qui fait cela. Sans ce contrôle, l'arrivée à zéro le ferait semer
  // une seule fois, ce qui ressemble à une figure oubliée.
  const float atteint =
      sower.first_distance + sower.distance_per_tick * static_cast<float>(arrivee);
  if (atteint > distance_to_target + sower.arrival_distance) {
    return sower.last_tick;
  }
  return std::min(arrivee, sower.last_tick);
}

bool EffectMarchingSowerSowsAt(const EffectMarchingSower& sower, std::int32_t tick,
                               float distance_to_target) noexcept {
  const std::int32_t cadence = std::max<std::int32_t>(sower.sowing_period_ticks, 1);
  return tick % cadence == 0 && tick <= EffectMarchingSowerLastSowTick(sower, distance_to_target);
}

glm::vec3 EffectMarchingSowerPoint(const EffectMarchingSower& sower, glm::vec3 source,
                                   glm::vec3 target, std::int32_t tick) noexcept {
  const float offset = EffectGroundDistance(source, target);
  if (offset <= 0.0F) {
    return source;
  }
  const glm::vec3 vers = EffectClientAimedHeading(target - source, 0.0F);
  return source +
         vers * (sower.first_distance + sower.distance_per_tick * static_cast<float>(tick));
}

}  // namespace ro::render
