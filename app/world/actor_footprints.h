#pragma once

#include <cstdint>
#include <unordered_map>

#include <glm/glm.hpp>

#include "ro/render/actor_effect_table.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world_fwd.h"

#include "effects/effect_node_view.h"
#include "world/map_stage.h"

// LES PAS QU'UN COUREUR LAISSE DERRIÈRE LUI.
//
// La course du Taekwon et Chase Walk ne dessinent rien elles-mêmes : elles font
// tourner, à chaque tic, un semeur que le client tient sur l'acteur. Il compte,
// et à mi-tour comme au tour plein il pose une EMPREINTE sous les pieds de
// l'acteur, tournée dans le sens de sa marche — la gauche à mi-tour, la droite
// au tour plein.
namespace moonclient {

class ActorFootprints {
 public:
  // Fait battre le semeur de chaque acteur pour les tics écoulés, et sème les
  // empreintes dues.
  //
  // ⚠ ELLES SE POSENT SOUS SES PIEDS, là où il est à ce battement, et non là où
  // il était : le client compare au relevé précédent pour l'angle, puis sème à
  // la place qu'il vient de relever.
  //
  // ⚠ ET IL NE SÈME QUE S'IL A BOUGÉ. Un coureur arrêté ne laisse rien, même
  // sous l'effet : c'est le déplacement qui fait la trace.
  void Notice(const ro::sim::World& world, const MapStage& stage, ro::sim::WalkPlanner& planner,
              EffectNodesInPlay& nodes, std::uint32_t now_ms);

  // Plus aucune trace en cours : le changement de carte emporte les coureurs
  // avec leurs empreintes, et chacun repart de zéro.
  void Forget() { traces_.clear(); }

 private:
  struct Trace {
    // Le compte du semeur, que la course et Chase Walk partagent.
    std::int32_t count = 0;
    // La place du dernier relevé.
    glm::vec3 last_ground{0.0F, 0.0F, 0.0F};
    // Le dernier tic battu.
    std::uint32_t beaten_tick = 0;
  };

  std::unordered_map<std::uint32_t, Trace> traces_;
};

}  // namespace moonclient
