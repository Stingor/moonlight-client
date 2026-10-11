#include "items/doll_body.h"

#include <algorithm>
#include <array>

#include "world/actor_tint.h"

namespace moonclient {

bool ComposeChangedDollBody(std::span<const ro::render::ActorEffectPlay> played,
                            const DollBody& body, std::uint32_t bearer_id, glm::vec3 feet,
                            ActorGhosts& ghosts, std::uint32_t now_ms,
                            std::vector<ro::render::DrawnActor>& out) {
  out.clear();
  const bool changes_actor =
      std::ranges::any_of(played, [](const ro::render::ActorEffectPlay& play) {
        return ro::render::ActorEffectChangeFor(play.effect_id) != nullptr;
      });
  if (!changes_actor) {
    return false;
  }
  const std::array<std::uint8_t, 4> tint = ro::render::TintAfterEffectsAt(kPlainTint, played);
  // LES RÉMANENCES, semées au tic comme en partie et vivant un tour de
  // l'animation que le pantin joue. Elles passent derrière lui.
  ghosts.Notice(bearer_id, played, body.composed, feet, tint, body.animation_duration_ms, now_ms);
  ghosts.Drawn(now_ms, out);
  const ro::render::ActorBodyPose pose = ro::render::BodyPoseAfterEffects(played);
  if (pose.hidden) {
    return true;
  }
  ro::render::DrawnActor drawn{.actor = &body.composed,
                               .ground_position = feet,
                               .tint = tint,
                               .scale = ro::render::BodyScaleAfterEffects(played),
                               .height_scale = pose.height_scale};
  // LE MÉLANGE DE SES CALQUES. Il ne double pas d'arme : il n'en porte pas.
  drawn.additive_parts = pose.additive_parts;
  out.push_back(drawn);
  return true;
}

}  // namespace moonclient
