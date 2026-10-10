// Deux poses que le client impose par-dessus les autres : la reprise d'attaque
// d'un enchaînement et la feinte de mort. Voir `ReplayedSwingAt` et
// `TrickDeadMotionAt`, déclarées avec les autres motions dans `actor_view.h`.

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "ro/formats/act.h"
#include "ro/render/floating_number.h"
#include "ro/render/skill_damage_timing.h"
#include "ro/render/skill_hit_extra.h"
#include "ro/render/skill_impl.h"
#include "ro/sim/world.h"

#include "world/actor_view.h"

namespace moonclient {
namespace {

// L'état d'action « attaque » du client, que la reprise lui redonne.
constexpr std::int8_t kAttackActionState = 2;

// Le temps écoulé qu'on prête à une chute dont on ne sait pas l'instant : bien
// plus long que toute animation de mort, l'acteur y est donc couché.
constexpr float kAlreadyLyingMs = 60000.0F;

}  // namespace

std::optional<PlayedMotion> ReplayedSwingAt(ro::render::ActorWardrobe* wardrobe,
                                            const ro::sim::World& world,
                                            const ro::sim::Actor& actor,
                                            ro::render::ActorOutfit& outfit,
                                            ro::render::SpriteLayout layout, std::uint32_t now_ms) {
  // ⚠ NI UN MORT NI QUI FAIT LE MORT : le client ne change la pose ni de l'un
  // ni de l'autre.
  if (wardrobe == nullptr || !actor.attack.has_value() ||
      actor.posture == ro::sim::Posture::kDead ||
      actor.status_effects.contains(ro::sim::kEfstTrickDead)) {
    return std::nullopt;
  }
  const ro::sim::Strike& blow = *actor.attack;
  const std::uint16_t replays = ro::render::SwingReplaysOf(blow.skill_id, blow.type, blow.hits);
  if (replays == 0 || layout != ro::render::SpriteLayout::kPlayer ||
      world.Find(blow.target) == nullptr) {
    return std::nullopt;
  }
  // LA PREMIÈRE FRAPPE TOUCHE à l'instant où ses dégâts paraissent ; chaque
  // suivante un pas plus tard, et c'est elle qui relance la pose.
  const float first_hit_ms =
      static_cast<float>(blow.started_ms) + HitLandsAtMs(*wardrobe, world, blow);
  const float step_ms = ro::render::kMultiHitIntervalMs +
                        static_cast<float>(ro::render::SkillHitStepExtraMs(blow.skill_id));
  const float since_first_ms = static_cast<float>(now_ms) - first_hit_ms;
  if (!(step_ms > 0.0F) || since_first_ms < step_ms) {
    return std::nullopt;
  }
  const auto reached = static_cast<std::uint16_t>(
      std::min(static_cast<float>(replays), std::floor(since_first_ms / step_ms)));
  const float since_ms = since_first_ms - static_cast<float>(reached) * step_ms;
  // 🔴 LE RANG DE L'ATTAQUE QUE L'ARME DÉCIDE, et non celui de la table : la
  // reprise repose l'état d'attaque comme un coup ordinaire, et le katar de
  // Sonic Blow n'a d'images qu'à ce rang-là. Chaque reprise tire au sort le
  // rang d'un taekwon.
  const auto replayed_ms =
      static_cast<std::uint32_t>(first_hit_ms + static_cast<float>(reached) * step_ms);
  const std::optional<std::size_t> attack =
      PosedAnimationOf(&wardrobe->catalog(), actor, kAttackActionState, layout, replayed_ms);
  if (!attack.has_value()) {
    return std::nullopt;
  }
  // UNE IMAGE PAR TIC : la reprise ne lit pas la cadence du fichier, et passe
  // l'attaque entière en autant de tics qu'elle a d'images.
  const std::size_t frames = outfit.FrameCount(*attack);
  if (since_ms >= static_cast<float>(frames) * ro::formats::kMillisecondsPerSpeedUnit) {
    return std::nullopt;
  }
  PlayedMotion played;
  played.motion = ro::render::Motion::kAttack;
  played.animation = *attack;
  // Le temps du FICHIER, que la composition attend : chaque tic y vaut une image.
  played.elapsed_ms =
      since_ms * outfit.FrameIntervalMs(*attack) / ro::formats::kMillisecondsPerSpeedUnit;
  played.loops = false;
  return played;
}

std::optional<PlayedMotion> TrickDeadMotionAt(const ro::sim::Actor& actor,
                                              ro::render::SpriteLayout layout,
                                              std::uint32_t now_ms) {
  if (!actor.status_effects.contains(ro::sim::kEfstTrickDead)) {
    return std::nullopt;
  }
  PlayedMotion played;
  played.motion = ro::render::Motion::kDie;
  played.animation = ro::render::AnimationFor(ro::render::Motion::kDie, layout);
  // LA CHUTE depuis l'instant où l'état a été posé ; sans lui, l'acteur est
  // déjà couché.
  const auto since = actor.status_since_ms.find(ro::sim::kEfstTrickDead);
  played.elapsed_ms = since != actor.status_since_ms.end() && now_ms >= since->second
                          ? static_cast<float>(now_ms - since->second)
                          : kAlreadyLyingMs;
  played.loops = false;
  return played;
}

}  // namespace moonclient
