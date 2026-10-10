#include "world/actor_tint.h"

#include <algorithm>
#include <cstddef>

#include "ro/render/ailment_sprites.h"
#include "ro/render/effect_duration.h"
#include "ro/render/effect_frame.h"
#include "ro/render/ground_light.h"
#include "ro/render/item_drop.h"
#include "ro/sim/status_effects.h"

namespace moonclient {
namespace {

// Un alpha qui ne cache rien.
constexpr int kOpaqueAlpha = 255;

// LE SCINTILLEMENT d'une teinte tirée au sort : combien de valeurs, et les deux
// pas du mélange qui la tire. Le client tire rand() % 255 : deux cent
// cinquante-cinq valeurs, de zéro à deux cent cinquante-quatre.
constexpr std::uint32_t kFlickerValues = 255;
constexpr std::uint32_t kFlickerEffectStride = 2246822519U;
constexpr std::uint32_t kFlickerTickStride = 3266489917U;

// La teinte que l'ÉTAT du corps et celui de la SANTÉ imposent, ou rien s'ils
// n'en imposent aucune — voir `ro::render::AilmentTint`.
std::optional<std::array<std::uint8_t, 4>> StateTint(const ro::sim::Actor& actor,
                                                     std::uint32_t now_ms) {
  return ro::render::AilmentTint(
      actor.body_state, actor.health_state,
      StillCarriesEffect(actor, ro::render::kPoisonTintYieldingEffect, now_ms));
}

}  // namespace

bool StillCarriesEffect(const ro::sim::Actor& actor, std::uint32_t effect_id,
                        std::uint32_t now_ms) {
  const std::int32_t life_ticks = ro::render::EffectDurationTicks(effect_id);
  return std::ranges::any_of(actor.effects, [&](const ro::sim::PlayedEffect& effect) {
    return effect.id == effect_id &&
           static_cast<std::int32_t>((now_ms - effect.since_ms) /
                                     static_cast<std::uint32_t>(ro::render::kEffectTickMs)) <
               life_ticks;
  });
}

std::uint32_t ScrambleSeed(std::uint32_t seed) {
  seed ^= seed << 13U;
  seed ^= seed >> 17U;
  seed ^= seed << 5U;
  return seed;
}

std::uint32_t BlurFlickerSeed(std::uint32_t actor_id, std::uint32_t age_ticks) {
  return ScrambleSeed(actor_id * kFlickerEffectStride + age_ticks * kFlickerTickStride);
}

std::array<std::uint8_t, 4> TintOf(const ro::sim::Actor& actor, std::uint32_t now_ms) {
  // L'ÉTAT DU CORPS ET CELUI DE LA SANTÉ PASSENT DEVANT. Un pétrifié est gris
  // quoi qu'un effet ait dit de lui : le serveur le tient, et un corps vert qui
  // survivrait à sa pétrification dirait le contraire de ce qu'il se passe.
  if (const std::optional<std::array<std::uint8_t, 4>> state = StateTint(actor, now_ms)) {
    return *state;
  }

  // Les effets, du plus ancien au plus récent, chacun à son âge. `render`
  // compose — la dernière écriture de chaque composante l'emporte, puis la
  // couleur revient vers le blanc : c'est la frontière habituelle — render ne
  // connaît pas sim —, et elle met la composition sous test.
  std::vector<ro::render::ActorEffectPlay> played;
  PlayedEffectsOf(actor, now_ms, played);
  return ro::render::TintAfterEffectsAt(kPlainTint, played);
}

std::array<std::uint8_t, 4> TintAt(const ro::sim::Actor& actor, std::uint32_t now_ms) {
  return TintAt(actor, now_ms, std::nullopt, {});
}

std::array<std::uint8_t, 4> TintAt(
    const ro::sim::Actor& actor, std::uint32_t now_ms,
    const std::optional<ro::render::TintEveryTickWrite>& every_tick_write,
    const ro::render::TintIdleFramesSince& idle_frames_since) {
  // L'état du corps passe devant TOUT, le scintillement compris : un pétrifié
  // ne clignote pas.
  if (const std::optional<std::array<std::uint8_t, 4>> state = StateTint(actor, now_ms)) {
    return *state;
  }
  return EffectsTintAt(actor, now_ms, every_tick_write, idle_frames_since);
}

std::array<std::uint8_t, 4> EffectsTintAt(
    const ro::sim::Actor& actor, std::uint32_t now_ms,
    const std::optional<ro::render::TintEveryTickWrite>& every_tick_write,
    const ro::render::TintIdleFramesSince& idle_frames_since) {
  std::vector<ro::render::ActorEffectPlay> played;
  PlayedEffectsOf(actor, now_ms, played);
  // ⚠ LE SCINTILLEMENT S'ARRÊTE AVEC L'EFFET, et ne part qu'à son premier tic :
  // le client cesse d'écrire la couleur à sa borne, et la dernière qu'il a
  // tirée revient ensuite vers le blanc comme toute autre. Le tirage change à
  // chaque TIC, pas à chaque image : le client refait sa couleur dans sa mise à
  // jour, et non dans son dessin.
  const auto draw = [&actor](std::size_t rank, std::uint32_t tick, std::size_t channel) {
    const ro::sim::PlayedEffect& effect = actor.effects[rank];
    const std::uint32_t seed =
        effect.id * kFlickerEffectStride + (effect.since_ms + tick) * kFlickerTickStride;
    return static_cast<std::uint8_t>(ScrambleSeed(seed + static_cast<std::uint32_t>(channel)) %
                                     kFlickerValues);
  };
  return ro::render::TintAfterEffectsWith(
      kPlainTint, played,
      ro::render::TintContext{.draw = draw,
                              .every_tick_write = every_tick_write,
                              .idle_frames_since = idle_frames_since});
}

// Les effets qu'un acteur porte, avec leur ÂGE en tics d'effet.
//
// ⚠ L'ÂGE EST CE QUI SÉPARE UNE POSE D'UNE TEINTE. Une couleur ne bouge pas ;
// une hauteur de saut, si — le corps est à mi-chemin au dixième tic et parti au
// vingtième. C'est `app` qui tient l'horloge, `render` ne la lit jamais.
void PlayedEffectsOf(const ro::sim::Actor& actor, std::uint32_t now_ms,
                     std::vector<ro::render::ActorEffectPlay>& played) {
  played.clear();
  played.reserve(actor.effects.size());
  for (const ro::sim::PlayedEffect& effect : actor.effects) {
    const std::uint32_t elapsed_ms = now_ms - effect.since_ms;
    played.push_back(ro::render::ActorEffectPlay{
        effect.id, elapsed_ms / static_cast<std::uint32_t>(ro::render::kEffectTickMs)});
  }
}

std::array<std::uint8_t, 4> BodyTintOf(const ro::sim::Actor& actor, std::uint32_t now_ms,
                                       const BodyTintInputs& inputs) {
  // 🔴 LA TEINTE QUI RESTE, quand la scène la tient : voir `AilmentTints`.
  // Sans elle, celle que les états imposent à cet instant.
  const std::optional<std::array<std::uint8_t, 4>> state =
      inputs.ailment_tints != nullptr ? inputs.ailment_tints->TintOf(actor.id)
                                      : StateTint(actor, now_ms);
  std::array<std::uint8_t, 4> tint =
      state.has_value() ? *state
                        : EffectsTintAt(actor, now_ms, inputs.blur_write, inputs.idle_frames_since);
  // ⚠ L'ALPHA DE LA POSE MULTIPLIE CELUI DE LA TEINTE, il ne le remplace pas :
  // un corps camouflé qui saute doit rester camouflé en montant.
  tint[3] = static_cast<std::uint8_t>(tint[3] * inputs.pose_alpha / kOpaqueAlpha);
  // ET LE FONDU DE LA DISPARITION MULTIPLIE À SON TOUR : une créature morte
  // s'efface au lieu de s'éteindre d'un coup, et un corps déjà translucide le
  // reste en s'effaçant.
  tint[3] = static_cast<std::uint8_t>(tint[3] * inputs.vanish_alpha / kOpaqueAlpha);
  // L'ÉCLAT D'UN OBJET AU SOL REMPLACE TOUT, la lumière de la carte comprise.
  if (inputs.sparkling) {
    return ro::render::kItemSparkleTint;
  }
  if (inputs.ground_light.has_value()) {
    return ro::render::LitTint(tint, *inputs.ground_light, inputs.cold);
  }
  return tint;
}

}  // namespace moonclient
