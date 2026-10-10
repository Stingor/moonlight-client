#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include <glm/glm.hpp>

#include "ro/render/camera.h"
#include "ro/render/sprite_renderer.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/pick_boxes.h"
#include "ro/ui/staff_tools.h"

#include "world/ground_unit_pick.h"

// CE QUE LE CLIC ATTRAPE : le réglage des emprises, posé sur les acteurs
// dessinés, et leur contour pour l'établi du staff.
namespace moonclient {

// Les acteurs d'une image et leurs identifiants, APPARIÉS, et les unités au
// sol que la souris peut désigner avec le plancher de leur emprise.
struct PickScene {
  std::span<const ro::render::DrawnActor> drawn;
  std::span<const std::uint32_t> ids;
  std::span<const GroundUnitPick> ground_units;
  float ground_unit_floor_pixels = ro::render::kGroundPickFloorFactor;
};

// POSE SUR LES ACTEURS CE QUE LE RÉGLAGE DEMANDE : le plancher de chacun selon
// sa famille — quarante pour un acteur, trente-quatre pour un objet au sol —
// et la part d'un acteur au numéro négatif. À appeler après la composition,
// avant toute désignation.
void ApplyPickTuning(const ro::ui::PickTuning& tuning, std::span<ro::render::DrawnActor> drawn,
                     std::span<const std::uint32_t> ids, const ro::sim::World& world);

// LES ZONES CLIQUABLES DE L'IMAGE, projetées : celle de chaque acteur, par la
// règle même qui le désigne, puis celle de chaque unité au sol. La désignée est
// celle qu'un clic prendrait : l'unité quand elle passe devant l'acteur
// désigné, sinon lui. Voir `GroundUnitOutranks`.
[[nodiscard]] ro::ui::PickBoxesView PickBoxesOnScreen(const ro::render::Camera& camera,
                                                      float aspect_ratio, const PickScene& scene,
                                                      const ro::sim::World& world,
                                                      std::optional<std::size_t> hovered_rank,
                                                      glm::vec2 mouse_px, float viewport_width,
                                                      float viewport_height);

}  // namespace moonclient
