#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/camera.h"
#include "ro/render/model_renderer.h"
#include "ro/render/scene_fog.h"
#include "ro/sim/survivors_state.h"
#include "ro/sim/world.h"
#include "ro/vfs/vfs.h"

#include "world/ground_unit_view.h"
#include "world/map_stage.h"

namespace moonclient {

// LES PIÈGES ARMÉS DE L'ARÈNE, en modèles : chacun pose dans le monde privé
// des effets l'unité au sol que le jeu pose pour lui (`TrapUnitOf`), et la
// vue des unités du jeu la dessine au milieu de sa zone, figée sur son image
// de pose, comme sur une carte.
class ArenaTrapModels {
 public:
  explicit ArenaTrapModels(ro::vfs::Vfs& vfs);

  // Pose dans `world` une unité par piège armé de `zones`, lève celles dont
  // la zone a disparu, et téléverse les modèles quand l'ensemble a bougé.
  // `origin_cells` est le coin de la fenêtre de sol, en cases du champ.
  void Follow(ro::sim::World& world, std::span<const ro::sim::SurvivorsZone> zones,
              const MapStage& stage, glm::ivec2 origin_cells, std::uint32_t now_ms);

  // Lève toutes ses unités : à « Rejouer », et quand la fenêtre de sol se
  // recentre, où elles se reposent à leur case nouvelle.
  void Forget(ro::sim::World& world);

  // Les modèles posés, derrière les corps.
  void Draw(const ro::sim::World& world, const ro::render::Camera& camera, float aspect_ratio,
            const ro::render::SceneFog& fog);

  [[nodiscard]] std::size_t placed() const noexcept { return placed_ids_.size(); }

 private:
  GroundUnits units_;
  std::optional<ro::render::ModelRenderer> renderer_;
  // Les unités posées, par numéro : celui de la zone, décalé hors de ceux
  // des acteurs du monde privé.
  std::vector<std::uint32_t> placed_ids_;
  std::uint32_t now_ms_ = 0;
};

}  // namespace moonclient
