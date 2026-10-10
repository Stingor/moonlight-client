#include "world/sleep_bubbles_view.h"

#include <cstddef>
#include <optional>
#include <utility>

#include "ro/render/ailment_sprites.h"
#include "ro/render/silence_dots.h"
#include "ro/render/sleep_bubbles.h"
#include "ro/render/sprite_frame.h"
#include "ro/sim/motion_events.h"
#include "ro/sim/movement.h"
#include "ro/sim/world.h"

#include "world/hit_popup_rules.h"

namespace moonclient {
namespace {

// DEPUIS QUAND L'ÉTAT TIENT UN ACTEUR, ou rien quand il ne le tient pas.
using HeldSince = std::optional<std::uint32_t> (*)(const ro::sim::Actor& actor);

// CE QUE L'ÉTAT FAIT PARAÎTRE, ET OÙ : l'action jouée, sa hauteur au-dessus des
// pieds, et son avance vers la caméra — nulle pour ce qui se peint en avant de
// toute la scène.
struct HeldLook {
  std::size_t action_index = 0;
  float rise_units = 0.0F;
  float depth_bias = 0.0F;
};

// LA TENUE COMMUNE : pour chaque acteur dessiné que l'état tient, l'action
// `look.action_index` à son âge, `look.rise_units` au-dessus de ses pieds.
std::span<const ro::render::DrawnActor> DrawHeld(
    const ro::formats::SprSprite* sprite, const ro::formats::ActAnimation* animation,
    const HeldLook& look, HeldSince held_since, std::uint32_t now_ms,
    std::span<const ro::render::DrawnActor> drawn, std::span<const std::uint32_t> ids,
    const ro::sim::World& world, bool translucent, std::vector<ro::render::ComposedActor>& frames,
    std::vector<ro::render::DrawnActor>& shown) {
  frames.clear();
  shown.clear();
  if (sprite == nullptr || animation == nullptr) {
    return {};
  }
  // Réservé d'avance : on garde un pointeur sur chaque composition, et une
  // réallocation en cours de route les invaliderait toutes.
  frames.reserve(ids.size());
  const std::uint8_t alpha = translucent ? ro::render::kSleepBubblesTranslucentAlpha
                                         : ro::render::kSleepBubblesOpaqueAlpha;
  for (std::size_t index = 0; index < drawn.size() && index < ids.size(); ++index) {
    const ro::sim::Actor* actor = world.Find(ids[index]);
    if (actor == nullptr) {
      continue;
    }
    const std::optional<std::uint32_t> since_ms = held_since(*actor);
    if (!since_ms.has_value()) {
      continue;
    }
    const float held_ms = now_ms > *since_ms ? static_cast<float>(now_ms - *since_ms) : 0.0F;
    ro::render::ComposedActor frame =
        ro::render::ComposeHeldChildSprite(*sprite, *animation, look.action_index, held_ms);
    if (frame.quads.empty()) {
      continue;
    }
    frames.push_back(std::move(frame));
    glm::vec3 above = drawn[index].ground_position;
    above.y += look.rise_units;
    ro::render::DrawnActor held{&frames.back(), nullptr, above, {255, 255, 255, alpha}};
    held.depth_bias = look.depth_bias;
    shown.push_back(held);
  }
  return shown;
}

// 🔴 QUI PARAÎT ENDORMI PORTE DÉJÀ SES BULLES : l'apparition passe par le même
// changement d'état qu'un paquet reçu en direct, depuis un corps éveillé. Son
// sommeil n'a pas d'instant noté ; celui de son état du corps en tient lieu.
std::optional<std::uint32_t> AsleepSince(const ro::sim::Actor& actor) {
  if (actor.asleep_since_ms.has_value()) {
    return actor.asleep_since_ms;
  }
  return ro::sim::BodyStateHeldSince(actor, ro::sim::kBodyAsleep);
}

// ⚠ LE BIT DÉCIDE, L'INSTANT NE FAIT QU'ANIMER : un acteur apparu déjà muet
// n'a pas d'instant noté, et porte ses points dès l'apparition.
std::optional<std::uint32_t> SilencedSince(const ro::sim::Actor& actor) {
  if ((actor.health_state & ro::sim::kHealthSilenced) == 0) {
    return std::nullopt;
  }
  return actor.silenced_since_ms.value_or(0);
}

std::optional<std::uint32_t> StunnedSince(const ro::sim::Actor& actor) {
  return ro::sim::BodyStateHeldSince(actor, ro::sim::kBodyStunned);
}

std::optional<std::uint32_t> FrozenSince(const ro::sim::Actor& actor) {
  return ro::sim::BodyStateHeldSince(actor, ro::sim::kBodyFrozen);
}

}  // namespace

std::span<const ro::render::DrawnActor> CurseMarks::Drawn(
    std::uint32_t now_ms, float camera_yaw_degrees, std::span<const ro::render::DrawnActor> drawn,
    std::span<const std::uint32_t> ids, const ro::sim::World& world, bool translucent) {
  frames_.clear();
  shown_.clear();
  if (sprite_ == nullptr || animation_ == nullptr) {
    return {};
  }
  frames_.reserve(ids.size());
  const std::uint8_t held_alpha = translucent ? ro::render::kSleepBubblesTranslucentAlpha
                                              : ro::render::kSleepBubblesOpaqueAlpha;
  // ⚠ LES DEUX ALPHAS SE MULTIPLIENT : la marque est déjà à demi transparente,
  // et le réglage des sprites-enfants la voile encore.
  const auto alpha = static_cast<std::uint8_t>(held_alpha * ro::render::kCurseMarkAlpha /
                                               ro::render::kSleepBubblesOpaqueAlpha);
  for (std::size_t index = 0; index < drawn.size() && index < ids.size(); ++index) {
    const ro::sim::Actor* actor = world.Find(ids[index]);
    if (actor == nullptr || (actor->health_state & ro::sim::kHealthCursed) == 0) {
      continue;
    }
    const std::uint32_t since_ms = actor->cursed_since_ms.value_or(0);
    const float held_ms = now_ms > since_ms ? static_cast<float>(now_ms - since_ms) : 0.0F;
    // LA FACE D'ÉCRAN DU CORPS, son angle relatif à la caméra TRONQUÉ au huitième.
    const std::size_t screen_direction = ro::render::SpriteDirectionOfDegrees(
        ClientBodyDegrees(*actor, ro::sim::FacingAt(*actor, now_ms)), camera_yaw_degrees,
        /*rounded=*/false);
    const ro::render::CurseMarkPlacement placement = ro::render::CurseMarkFor(screen_direction);
    ro::render::ComposedActor frame =
        ro::render::ComposeHeldChildSprite(*sprite_, *animation_, placement.action, held_ms);
    if (frame.quads.empty()) {
      continue;
    }
    frames_.push_back(std::move(frame));
    glm::vec3 above = drawn[index].ground_position;
    above.y += ro::render::kCurseMarkRiseUnits;
    ro::render::DrawnActor held{&frames_.back(), nullptr, above, {255, 255, 255, alpha}};
    held.depth_bias = placement.depth_bias;
    shown_.push_back(held);
  }
  return shown_;
}

std::span<const ro::render::DrawnActor> SleepBubbles::Drawn(
    std::uint32_t now_ms, std::span<const ro::render::DrawnActor> drawn,
    std::span<const std::uint32_t> ids, const ro::sim::World& world, bool translucent) {
  return DrawHeld(
      sprite_, animation_,
      HeldLook{ro::render::kSleepBubblesAction, ro::render::kSleepBubblesRiseUnits, 0.0F},
      &AsleepSince, now_ms, drawn, ids, world, translucent, frames_, shown_);
}

std::span<const ro::render::DrawnActor> SilenceDots::Drawn(
    std::uint32_t now_ms, std::span<const ro::render::DrawnActor> drawn,
    std::span<const std::uint32_t> ids, const ro::sim::World& world, bool translucent) {
  return DrawHeld(sprite_, animation_,
                  HeldLook{ro::render::kSilenceDotsAction, ro::render::kSilenceDotsRiseUnits, 0.0F},
                  &SilencedSince, now_ms, drawn, ids, world, translucent, frames_, shown_);
}

std::span<const ro::render::DrawnActor> StunStars::Drawn(
    std::uint32_t now_ms, std::span<const ro::render::DrawnActor> drawn,
    std::span<const std::uint32_t> ids, const ro::sim::World& world, bool translucent) {
  return DrawHeld(sprite_, animation_,
                  HeldLook{ro::render::kStunStarsAction, ro::render::kStunStarsRiseUnits, 0.0F},
                  &StunnedSince, now_ms, drawn, ids, world, translucent, frames_, shown_);
}

std::span<const ro::render::DrawnActor> FrozenIce::Drawn(
    std::uint32_t now_ms, std::span<const ro::render::DrawnActor> drawn,
    std::span<const std::uint32_t> ids, const ro::sim::World& world, bool translucent) {
  return DrawHeld(sprite_, animation_,
                  HeldLook{ro::render::kFrozenIceAction, ro::render::kFrozenIceRiseUnits,
                           ro::render::kFrozenIceDepthBias},
                  &FrozenSince, now_ms, drawn, ids, world, translucent, frames_, shown_);
}

}  // namespace moonclient
