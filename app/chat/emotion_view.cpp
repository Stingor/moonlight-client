#include "chat/emotion_view.h"

#include <cstddef>
#include <utility>

#include "ro/core/log.h"
#include "ro/render/sprite_billboard.h"
#include "ro/render/sprite_canvas.h"
#include "ro/sim/world.h"

namespace moonclient {

std::span<const ro::render::DrawnActor> EmotionBubbles::Drawn(
    std::uint32_t now_ms, std::span<const ro::render::DrawnActor> drawn,
    std::span<const std::uint32_t> ids, const ro::sim::World& world) {
  frames_.clear();
  shown_.clear();
  if (sprite_ == nullptr || animation_ == nullptr) {
    return {};
  }
  // Réservé d'avance : on garde un pointeur sur chaque composition, et une
  // réallocation en cours de route les invaliderait toutes.
  frames_.reserve(ids.size());

  for (std::size_t index = 0; index < drawn.size() && index < ids.size(); ++index) {
    const ro::sim::Actor* actor = world.Find(ids[index]);
    if (actor == nullptr || !actor->emotion.has_value() || drawn[index].actor == nullptr) {
      continue;
    }
    // Un lot de boutique a son propre sprite, que ce client ne charge pas.
    if (actor->emotion->pack != ro::sim::kBaseEmotionPack) {
      if (reported_.insert(actor->emotion->pack).second) {
        ro::core::LogWarning(
            "émoticônes : le lot {} a son propre sprite, que ce client ne charge pas — ces "
            "bulles-là ne paraîtront pas",
            actor->emotion->pack);
      }
      continue;
    }
    if (ro::render::EmotionLifeMs(*animation_, actor->emotion->id) <= 0.0F) {
      if (reported_.insert(actor->emotion->id).second) {
        ro::core::LogWarning(
            "émoticône {} : le sprite n'a pas cette action (il en porte {}) — la bulle ne "
            "paraîtra pas",
            actor->emotion->id, animation_->actions().size());
      }
      continue;
    }
    const float elapsed_ms = now_ms > actor->emotion_since_ms
                                 ? static_cast<float>(now_ms - actor->emotion_since_ms)
                                 : 0.0F;
    ro::render::ComposedActor frame =
        ro::render::ComposeEmotion(*sprite_, *animation_, actor->emotion->id, elapsed_ms);
    if (frame.quads.empty()) {
      continue;
    }
    frames_.push_back(std::move(frame));

    // Le sommet de ce que le porteur occupe : dans un sprite, le haut est le y
    // le plus PETIT, et l'ancre est aux pieds. La hauteur se compte donc à
    // partir de zéro vers le négatif, échelle du corps comprise.
    const ro::render::SpriteBounds bounds = ro::render::ComputeBounds(drawn[index].actor->quads);
    const float height_world_units =
        -bounds.min.y * drawn[index].scale / ro::render::kSpritePixelsPerWorldUnit;
    glm::vec3 above = drawn[index].ground_position;
    above.y += height_world_units;
    shown_.push_back(ro::render::DrawnActor{&frames_.back(), nullptr, above});
  }
  return shown_;
}

}  // namespace moonclient
