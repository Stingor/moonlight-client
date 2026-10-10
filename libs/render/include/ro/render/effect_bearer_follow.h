#pragma once

#include <cstdint>
#include <limits>

namespace ro::render {

// QUAND UNE INSTANCE D'EFFET REPREND LA PLACE DE SON PORTEUR.
//
// Un effet joué sur un acteur naît sur la place de cet acteur, et tout ce qu'il
// sème se place ensuite sur la place de l'EFFET plus son propre écart. L'effet,
// lui, ne suit son porteur que si sa mise à jour recopie la place de celui-ci —
// à chaque tic pour une aura qui marche avec son porteur, jamais pour un impact
// ou une bouffée qui restent où ils sont nés, et à quelques tics précis pour
// une poignée : la copie a lieu juste avant un semis, et tout ce qui est déjà né
// saute avec elle.
//
// ⚠ SUIVRE N'EST PAS APPARTENIR. Un effet qui reste où il est né appartient
// toujours à son porteur : il disparaît avec lui, et on le retire par lui. Seule
// la PLACE ne suit pas.
//
// L'horaire est une progression : les tics `premier`, `premier + période`, …
// jusqu'à `dernier`. Une période nulle ne recale qu'une fois, au premier tic.
struct EffectBearerFollow {
  std::uint32_t effect_id = 0;
  std::int32_t first_tick = 0;
  std::int32_t last_tick = 0;
  std::int32_t period_ticks = 0;
};

// Le dernier tic d'un horaire qui court jusqu'à la fin de l'effet.
inline constexpr std::int32_t kEffectFollowsToTheEnd = std::numeric_limits<std::int32_t>::max();

// L'horaire d'un effet qui suit son porteur à chaque tic. C'est celui d'un nœud
// semé hors de tout effet nommé : rien ne dit alors qu'il resterait en place.
inline constexpr EffectBearerFollow kEffectFollowsEveryTick{0, 0, kEffectFollowsToTheEnd, 1};

// L'horaire d'un effet qui ne reprend jamais la place de son porteur après sa
// naissance.
inline constexpr EffectBearerFollow kEffectStaysWhereBorn{0, -1, -1, 0};

// L'horaire d'un effet, lu dans la table engendrée — nul pour un effet qui ne
// reprend jamais la place de son porteur.
[[nodiscard]] const EffectBearerFollow* EffectBearerFollowFor(std::uint32_t effect_id) noexcept;

// L'horaire d'un effet, jamais nul : `kEffectStaysWhereBorn` pour un effet
// absent de la table.
[[nodiscard]] inline EffectBearerFollow EffectBearerFollowOf(std::uint32_t effect_id) noexcept {
  const EffectBearerFollow* lu = EffectBearerFollowFor(effect_id);
  return lu != nullptr ? *lu : kEffectStaysWhereBorn;
}

// Ce tic-là, l'effet reprend-il la place de son porteur ?
[[nodiscard]] constexpr bool EffectRejoinsBearerAt(const EffectBearerFollow& follow,
                                                   std::int32_t tick) {
  if (follow.first_tick < 0 || tick < follow.first_tick) {
    return false;
  }
  if (follow.period_ticks <= 0) {
    return tick == follow.first_tick;
  }
  return tick <= follow.last_tick && (tick - follow.first_tick) % follow.period_ticks == 0;
}

// L'a-t-il reprise au moins une fois APRÈS `after_tick` et jusqu'à `through_tick`
// compris ? C'est la question d'une image qui a vu passer plusieurs tics.
[[nodiscard]] constexpr bool EffectRejoinsBearerBetween(const EffectBearerFollow& follow,
                                                        std::int32_t after_tick,
                                                        std::int32_t through_tick) {
  if (follow.first_tick < 0 || through_tick <= after_tick) {
    return false;
  }
  // Le premier tic de l'horaire qui vient APRÈS `after_tick`.
  std::int32_t prochain = follow.first_tick;
  if (prochain <= after_tick) {
    if (follow.period_ticks <= 0) {
      return false;
    }
    const std::int32_t steps_taken = (after_tick - follow.first_tick) / follow.period_ticks + 1;
    if (steps_taken > (follow.last_tick - follow.first_tick) / follow.period_ticks) {
      return false;
    }
    prochain = follow.first_tick + steps_taken * follow.period_ticks;
  }
  return prochain <= through_tick;
}

// Le suit-il pas à pas à ce tic, c'est-à-dire à chaque tic d'un intervalle ?
// Un tel effet se déplace alors à chaque IMAGE avec son porteur, sans attendre
// le tic : c'est ce qui le garde collé à un acteur qui marche.
[[nodiscard]] constexpr bool EffectFollowsBearerStepByStep(const EffectBearerFollow& follow,
                                                           std::int32_t tick) {
  return follow.period_ticks == 1 && follow.first_tick >= 0 && tick >= follow.first_tick &&
         tick <= follow.last_tick;
}

}  // namespace ro::render
