#pragma once

#include <cstdint>
#include <unordered_map>

#include <glm/glm.hpp>

#include "ro/render/actor_effect_table.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world_fwd.h"

#include "effects/effect_view.h"
#include "world/map_stage.h"

// LES PAS QU'UN COUREUR LAISSE DERRIÈRE LUI.
//
// `EF_RUN` — la course du Taekwon — ne dessine rien elle-même : elle fait poser
// au coureur une EMPREINTE tous les sept battements, à l'endroit qu'il vient de
// quitter, en alternant entre deux effets.
namespace moonclient {

class ActorFootprints {
 public:
  // Pose les empreintes dues à cette image.
  //
  // ⚠ ELLES SE POSENT LÀ OÙ L'ACTEUR ÉTAIT, et non où il est : le client garde
  // la position du dernier relevé et sème DESSUS avant de la remplacer. Les
  // poser sous ses pieds les ferait marcher avec lui, ce qui n'est pas une
  // trace.
  //
  // ⚠ ET IL NE SÈME QUE S'IL A BOUGÉ. Un coureur arrêté ne laisse rien, même
  // sous l'effet : c'est le déplacement qui fait la trace.
  void Notice(const ro::sim::World& world, const MapStage& stage, ro::sim::WalkPlanner& planner,
              EffectsInPlay& effects, std::uint32_t now_ms);

  // Plus aucune trace en cours : le changement de carte emporte les coureurs
  // avec leurs empreintes, et chacun repart de zéro.
  void Forget() { traces_.clear(); }

 private:
  struct Trace {
    glm::vec3 last_ground{0.0F, 0.0F, 0.0F};
    std::uint32_t last_sown_ms = 0;
    // Laquelle des deux empreintes vient ensuite : le client alterne.
    std::size_t next = 0;
    bool started = false;
  };

  std::unordered_map<std::uint32_t, Trace> traces_;
};

}  // namespace moonclient
