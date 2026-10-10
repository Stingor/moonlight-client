#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "ro/render/day_night_light.h"
#include "ro/render/model_mesh.h"
#include "ro/render/scene_light.h"
#include "ro/sim/world.h"

#include "world/map_stage.h"

namespace moonclient {

// LA NUIT DES CARTES, côté jeu : la carte dont la lumière sert de jour, les
// états du joueur, et le temps qui passe. Voir `ro/render/day_night_light.h`
// pour la règle, et `ro::sim::DarkensTheMap` pour ce qui fait tomber la nuit.
//
// ⚠ LE SERVEUR DÉCIDE, ET LUI SEUL : c'est lui qui connaît l'heure et le
// drapeau `nightenabled` des cartes. Le client n'en lit rien et ne fait que
// suivre l'état posé sur le joueur — et le retirer rend le jour.
class NightLightView {
 public:
  // Une image : la carte affichée, le monde, l'instant.
  void Advance(const MapStage& stage, const ro::sim::World& world, std::uint32_t now_ms);

  // LA LUMIÈRE À PEINDRE pour le sol et le décor : celle du RSW, teintée.
  [[nodiscard]] const ro::render::SceneLight& light() const noexcept { return light_; }

 private:
  // LA CARTE DONT LA LUMIÈRE SERT DE JOUR : en changer repart du jour, sans
  // fondu.
  struct Entered {
    std::string map_name;
    const ro::render::ModelInstances* models = nullptr;
  };

  ro::render::DayNightLight day_night_;
  ro::render::SceneLight light_;
  std::optional<Entered> entered_;
  std::optional<std::uint32_t> last_ms_;
};

}  // namespace moonclient
