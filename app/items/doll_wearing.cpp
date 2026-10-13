#include "items/doll_wearing.h"

#include <utility>

#include "world/actor_view.h"

namespace moonclient {
namespace {

// Une marge de chaque côté de la boîte, par axe.
constexpr float kSidesPerAxis = 2.0F;

}  // namespace

DollWearing WearingOf(const ro::sim::Actor& player, std::vector<ro::sim::HatEffectId> worn,
                      std::string_view map_name) {
  return DollWearing{.worn = std::move(worn),
                     .shown_job = ShownJobOf(player),
                     .mounted = player.status_effects.contains(ro::sim::kStatusAllRiding),
                     .map_name = std::string(map_name)};
}

std::vector<ro::sim::HatEffectId> PortraitHatEffects(const ro::sim::Actor& self,
                                                     bool drawn_as_model) {
  if (drawn_as_model) {
    return {};
  }
  // L'ensemble est déjà rangé et sans doublon : c'est l'ordre de `DollWearing`.
  return {self.hat_effects.begin(), self.hat_effects.end()};
}

glm::vec2 DollCanvas::size_px() const noexcept { return box_px + kSidesPerAxis * margin_px(); }

ro::render::VignetteFraming OnDollCanvas(const ro::render::VignetteFraming& framing,
                                         const DollCanvas& canvas) {
  ro::render::VignetteFraming on_canvas = framing;
  on_canvas.origin += canvas.margin_px();
  return on_canvas;
}

}  // namespace moonclient
