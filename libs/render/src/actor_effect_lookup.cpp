#include <algorithm>
#include <limits>
#include <optional>

#include "ro/render/actor_effect_table.h"
#include "ro/render/effect_duration.h"
#include "ro/render/effect_frame_beat.h"
#include "ro/render/effect_node.h"

namespace ro::render {
namespace {

// La table range alpha, rouge, vert, bleu — l'ordre des arguments du client.
// DrawnActor range rouge, vert, bleu, alpha. Les confondre mettrait l'alpha à
// la place du rouge.
constexpr std::array<std::size_t, 4> kToDrawnActor = {3, 0, 1, 2};

constexpr std::int32_t kChannelMax = 255;

// CE QUE L'ACTEUR RAJOUTE À CHAQUE COMPOSANTE DE SA COULEUR, à chaque tic et à
// chaque image sans tic, jusqu'à `kChannelMax` : c'est ce qui ramène un corps
// teint vers le blanc une fois que plus rien ne le teint.
constexpr std::int32_t kTintRecoveryPerTick = 8;

[[nodiscard]] bool Blinks(const ActorEffectChange& change) noexcept {
  return std::ranges::any_of(change.odd_tick_tint,
                             [](std::int16_t channel) { return channel != kTintUnchanged; });
}

constexpr std::array<std::int16_t, 4> kNoTint = {kTintUnchanged, kTintUnchanged, kTintUnchanged,
                                                 kTintUnchanged};

// Le bit du mot de rendu qui rend additifs les calques du corps.
constexpr std::uint32_t kAdditiveLayersBit = 0x1;

// Vrai quand ce tic tombe dans la seconde moitié de la période d'un effet qui
// clignote : celle de `odd_tick_tint`.
[[nodiscard]] bool InOddBlinkPhase(const ActorEffectChange& change, std::uint32_t tick) noexcept {
  const std::uint32_t period = std::max<std::uint32_t>(change.blink_period_ticks, 1U);
  return tick % period >= (period + 1U) / 2U;
}

// La teinte qu'un effet écrit au tic `tick` : aucune avant son premier tic.
[[nodiscard]] std::array<std::int16_t, 4> TintWrittenAt(const ActorEffectChange& change,
                                                        std::uint32_t tick) noexcept {
  if (tick < change.first_tick) {
    return kNoTint;
  }
  std::array<std::int16_t, 4> tint = change.tint;
  if (Blinks(change)) {
    tint = InOddBlinkPhase(change, tick) ? change.odd_tick_tint : change.tint;
  }
  // LA RAMPE, composante par composante : voir `ActorEffectChange::tint_slope_per_tick`.
  for (std::size_t channel = 0; channel < tint.size(); ++channel) {
    const std::int16_t slope = change.tint_slope_per_tick[channel];
    if (slope == 0 || tint[channel] == kTintUnchanged || tint[channel] == kTintRandom) {
      continue;
    }
    const std::int32_t value = tint[channel] + slope * static_cast<std::int32_t>(tick);
    tint[channel] = static_cast<std::int16_t>(std::clamp(value, 0, kChannelMax));
  }
  return tint;
}

// LE PREMIER TIC où l'effet n'écrit plus sa couleur : sa vie, ou la mort de son
// instance si elle vient avant. Rien quand il écrit tant qu'il est là.
[[nodiscard]] std::optional<std::uint32_t> TintEndTick(const ActorEffectChange& change) {
  std::optional<std::uint32_t> end;
  if (change.life_ticks != 0) {
    end = change.life_ticks;
  }
  if (!InstanceAliveAt(change.effect_id, std::numeric_limits<std::uint32_t>::max())) {
    const auto death = static_cast<std::uint32_t>(EffectDurationTicks(change.effect_id));
    end = end.has_value() ? std::min(*end, death) : death;
  }
  return end;
}

// La dernière écriture qu'un effet a faite de sa couleur : à quel tic, et quoi.
struct LastTintWrite {
  std::uint32_t tick = 0;
  std::array<std::int16_t, 4> tint = kNoTint;
};

[[nodiscard]] std::optional<LastTintWrite> LastWriteOf(const ActorEffectChange& change,
                                                       std::uint32_t age_ticks) {
  if (age_ticks < change.first_tick) {
    return std::nullopt;
  }
  std::uint32_t tick = age_ticks;
  if (const std::optional<std::uint32_t> end = TintEndTick(change); end.has_value()) {
    if (*end <= change.first_tick) {
      return std::nullopt;
    }
    tick = std::min(age_ticks, *end - 1U);
  }
  return LastTintWrite{tick, TintWrittenAt(change, tick)};
}

}  // namespace

const ActorEffectChange* ActorEffectChangeFor(std::uint32_t effect_id) {
  const std::span<const ActorEffectChange> table = ActorEffectChangeTable();
  const auto found = std::ranges::lower_bound(table, effect_id, {}, &ActorEffectChange::effect_id);
  if (found == table.end() || found->effect_id != effect_id) {
    return nullptr;
  }
  return &*found;
}

bool ActsAt(const ActorEffectChange& change, std::uint32_t age_ticks) noexcept {
  return age_ticks >= change.first_tick &&
         (change.life_ticks == 0 || age_ticks < change.life_ticks);
}

bool InstanceAliveAt(std::uint32_t effect_id, std::uint32_t age_ticks) {
  if (std::ranges::find(kAgeRewindingActorEffects, effect_id) != kAgeRewindingActorEffects.end() ||
      !EffectDurationIsKnown(effect_id)) {
    return true;
  }
  const std::int32_t life_ticks = EffectDurationTicks(effect_id);
  if (life_ticks <= 0 || life_ticks >= kEffectLifeForever) {
    return true;
  }
  return age_ticks < static_cast<std::uint32_t>(life_ticks);
}

std::uint32_t RenderBitsAt(const ActorEffectChange& change, std::uint32_t age_ticks) {
  if (age_ticks < change.first_tick || !InstanceAliveAt(change.effect_id, age_ticks)) {
    return 0;
  }
  if (change.life_ticks != 0 && age_ticks >= change.life_ticks) {
    return change.render_bits & change.unbounded_render_bits;
  }
  if (change.render_bits_period_ticks > 1 && age_ticks % change.render_bits_period_ticks != 0) {
    return 0;
  }
  // 🔴 UN EFFET QUI CLIGNOTE NE REND SES CALQUES ADDITIFS QU'AVEC SA TEINTE
  // PRINCIPALE : le client lève ce bit dans la même branche que la couleur des
  // tics pairs, et le mot de rendu retombe à zéro à chaque image. Levé à chaque
  // tic, le corps resterait lumineux, et le clignotement ne se verrait plus —
  // Shrink jaunissait sans clignoter. Les autres bits, eux, tiennent tout le tic.
  if (Blinks(change) && InOddBlinkPhase(change, age_ticks)) {
    return change.render_bits & ~kAdditiveLayersBit;
  }
  return change.render_bits;
}

bool TintsActor(const ActorEffectChange& change) noexcept {
  return std::ranges::any_of(change.tint,
                             [](std::int16_t channel) { return channel != kTintUnchanged; });
}

std::array<std::uint8_t, 4> TintAfterEffects(std::array<std::uint8_t, 4> base,
                                             std::span<const std::uint32_t> effect_ids) {
  for (const std::uint32_t effect_id : effect_ids) {
    const ActorEffectChange* change = ActorEffectChangeFor(effect_id);
    if (change == nullptr) {
      continue;
    }
    for (std::size_t channel = 0; channel < change->tint.size(); ++channel) {
      // ⚠ UNE COMPOSANTE TIRÉE AU SORT NE SE COMPOSE PAS ICI : voir
      // `TintAfterEffectsAt`.
      if (change->tint[channel] == kTintUnchanged || change->tint[channel] == kTintRandom) {
        continue;
      }
      base[kToDrawnActor[channel]] = static_cast<std::uint8_t>(change->tint[channel]);
    }
  }
  return base;
}

std::array<std::uint8_t, 4> TintAfterEffectsAt(std::array<std::uint8_t, 4> base,
                                               std::span<const ActorEffectPlay> played,
                                               const TintDraw& draw) {
  return TintAfterEffectsWith(base, played, TintContext{.draw = draw});
}

std::array<std::uint8_t, 4> TintAfterEffectsWith(std::array<std::uint8_t, 4> base,
                                                 std::span<const ActorEffectPlay> played,
                                                 const TintContext& context) {
  // PAR COMPOSANTE, LA DERNIÈRE ÉCRITURE L'EMPORTE, et elle remonte ensuite de
  // huit par tic. À tic égal, l'effet le plus récent de la liste passe après
  // les autres, comme chez le client qui les met à jour dans cet ordre.
  struct Latest {
    std::uint32_t ticks_since = std::numeric_limits<std::uint32_t>::max();
    std::int32_t value = 0;
    // Vrai quand l'écriture se refait à chaque image, et non au tic : aucune
    // image sans tic ne la laisse alors remonter.
    bool every_frame = false;
  };
  std::array<std::optional<Latest>, 4> latest;
  const auto keep = [&latest](std::size_t channel, const Latest& write) {
    std::optional<Latest>& slot = latest[kToDrawnActor[channel]];
    if (!slot.has_value() || write.ticks_since <= slot->ticks_since) {
      slot = write;
    }
  };
  for (std::size_t rank = 0; rank < played.size(); ++rank) {
    const ActorEffectPlay& play = played[rank];
    const ActorEffectChange* change = ActorEffectChangeFor(play.effect_id);
    const std::optional<LastTintWrite> write =
        change != nullptr ? LastWriteOf(*change, play.age_ticks) : std::nullopt;
    if (write.has_value()) {
      const std::uint32_t ticks_since = play.age_ticks - write->tick;
      const bool every_frame = EffectInstanceBeatsEveryFrame(play.effect_id);
      for (std::size_t channel = 0; channel < write->tint.size(); ++channel) {
        const std::int16_t written = write->tint[channel];
        if (written == kTintUnchanged) {
          continue;
        }
        // ⚠ `render` RESTE SANS HASARD — une image de référence ne se compare à
        // rien sinon : une composante tirée au sort vient de l'appelant, qui la
        // tire. Sans tireur, elle ne se compose pas.
        std::int32_t value = written;
        if (written == kTintRandom) {
          if (!context.draw) {
            continue;
          }
          value = context.draw(rank, write->tick, channel);
        }
        keep(channel, Latest{ticks_since, value, every_frame});
      }
    }
    // L'ÉCRITURE HORS TABLE se fait au rang de son effet, à ce tic même.
    if (context.every_tick_write.has_value() && context.every_tick_write->rank == rank) {
      for (std::size_t channel = 0; channel < context.every_tick_write->tint.size(); ++channel) {
        const std::int16_t written = context.every_tick_write->tint[channel];
        if (written != kTintUnchanged) {
          keep(channel, Latest{0, written, false});
        }
      }
    }
  }
  for (std::size_t channel = 0; channel < latest.size(); ++channel) {
    if (!latest[channel].has_value()) {
      continue;
    }
    const std::uint32_t ticks_since = latest[channel]->ticks_since;
    const std::uint32_t idle_frames =
        !context.idle_frames_since || (latest[channel]->every_frame && ticks_since == 0)
            ? 0
            : context.idle_frames_since(ticks_since);
    const std::uint64_t recovered = static_cast<std::uint64_t>(latest[channel]->value) +
                                    static_cast<std::uint64_t>(kTintRecoveryPerTick) *
                                        (static_cast<std::uint64_t>(ticks_since) + idle_frames);
    base[channel] = static_cast<std::uint8_t>(
        std::min<std::uint64_t>(recovered, static_cast<std::uint64_t>(kChannelMax)));
  }
  return base;
}

}  // namespace ro::render
