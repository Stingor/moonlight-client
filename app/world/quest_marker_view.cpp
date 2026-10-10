#include "world/quest_marker_view.h"

#include <cstddef>
#include <optional>
#include <utility>

#include "ro/render/quest_marker.h"
#include "ro/render/sleep_bubbles.h"
#include "ro/sim/quest_markers.h"
#include "ro/sim/world.h"

namespace moonclient {
namespace {

// Seul un PNJ porte une marque : ni un joueur, ni un monstre, ni un familier.
[[nodiscard]] bool CarriesQuestMarker(const ro::sim::Actor& actor) {
  return actor.kind == ro::sim::ActorKind::kNonPlayer && actor.familiar == ro::sim::Familiar::kNone;
}

}  // namespace

std::span<const ro::render::DrawnActor> QuestMarkerSprites::Drawn(
    std::uint32_t now_ms, std::span<const ro::render::DrawnActor> drawn,
    std::span<const std::uint32_t> ids, const ro::sim::World& world, bool translucent) {
  frames_.clear();
  shown_.clear();
  // UNE MARQUE MEURT AVEC SON PNJ, ou quand le serveur la retire : un PNJ qui
  // revient en reçoit une neuve, dont l'animation repart de zéro.
  std::erase_if(held_, [&world](const auto& entry) {
    return world.Find(entry.first) == nullptr ||
           world.quest_markers().ShownFor(entry.first) == nullptr;
  });
  if (sprite_ == nullptr || animation_ == nullptr) {
    return {};
  }
  // Réservé d'avance : on garde un pointeur sur chaque composition, et une
  // réallocation en cours de route les invaliderait toutes.
  frames_.reserve(ids.size());
  const std::uint8_t alpha = translucent ? ro::render::kSleepBubblesTranslucentAlpha
                                         : ro::render::kSleepBubblesOpaqueAlpha;
  for (std::size_t index = 0; index < drawn.size() && index < ids.size(); ++index) {
    const ro::sim::Actor* actor = world.Find(ids[index]);
    if (actor == nullptr || !CarriesQuestMarker(*actor)) {
      continue;
    }
    const ro::sim::QuestMarker* marker = world.quest_markers().ShownFor(ids[index]);
    if (marker == nullptr) {
      continue;
    }
    const std::optional<std::size_t> action = ro::render::QuestMarkerAction(marker->effect);
    if (!action.has_value()) {
      continue;
    }
    // L'HORLOGE REPART quand l'effet OU la couleur change, même si l'image reste
    // la même.
    const Held told{marker->effect, marker->color, now_ms};
    auto [held, born] = held_.try_emplace(ids[index], told);
    if (!born && (held->second.effect != told.effect || held->second.color != told.color)) {
      held->second = told;
    }
    const std::uint32_t since_ms = held->second.since_ms;
    const float held_ms = now_ms > since_ms ? static_cast<float>(now_ms - since_ms) : 0.0F;
    ro::render::ComposedActor frame =
        ro::render::ComposeHeldChildSprite(*sprite_, *animation_, *action, held_ms);
    if (frame.quads.empty()) {
      continue;
    }
    frames_.push_back(std::move(frame));
    glm::vec3 above = drawn[index].ground_position;
    above.x += ro::render::kQuestMarkerShiftXUnits;
    above.y += ro::render::kQuestMarkerRiseUnits;
    shown_.push_back(
        ro::render::DrawnActor{&frames_.back(), nullptr, above, {255, 255, 255, alpha}});
  }
  return shown_;
}

}  // namespace moonclient
