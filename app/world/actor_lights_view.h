#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "ro/render/actor_lights.h"
#include "ro/render/point_lights.h"
#include "ro/render/sprite_tables.h"

namespace moonclient {

// LES LUMIÈRES QUE PORTENT LES ACTEURS DE L'IMAGE : la lampe d'un monstre, les
// feux de Sight. Voir docs/adr/0120.
//
// Le catalogue des monstres se lit à travers la table des classes une fois,
// et de nouveau quand on lui en donne une autre.
class SceneActorLights {
 public:
  // Les lumières de `bearers`, les corps que la scène vient de composer, à
  // l'instant `time_ms`. Aucune sans `tables`, ou quand `asked` est faux — le
  // réglage « Les sorts éclairent ».
  [[nodiscard]] std::span<const ro::render::PointLight> Gather(
      std::span<const ro::render::ActorLightBearer> bearers, const ro::render::SpriteTables* tables,
      bool asked, double time_ms);

 private:
  ro::render::ActorLightCatalogue catalogue_;
  const std::unordered_map<std::uint32_t, std::string>* read_from_ = nullptr;
  std::vector<ro::render::PointLight> lights_;
};

}  // namespace moonclient
