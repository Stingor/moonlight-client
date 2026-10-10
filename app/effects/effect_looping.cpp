// UN EFFET EN FICHIER QUI REBOUCLE TANT QUE SON PORTEUR EST LÀ, et qui se
// déplace avec lui.
//
// Sorti de `effect_view.cpp` au plafond de huit cents lignes, et la coupure
// suit une famille que le fichier tenait déjà à part : ces deux gestes ne
// servent qu'aux colonnes de butin au sol — voir `world/item_drops.cpp` —, là
// où tout le reste du fichier parle d'effets qu'un acteur ANNONCE.
//
// La différence n'est pas la durée mais QUI décide de la fin : un effet joué
// s'éteint au bout de son fichier, celui-ci s'éteint quand son porteur s'en va.

#include <string>

#include "effects/effect_view.h"

namespace moonclient {

bool EffectsInPlay::PlayLooping(std::string_view path, glm::vec3 ground_position,
                                std::uint32_t now_ms, float lift_pixels, std::uint32_t while_actor,
                                bool behind_actors) {
  const ro::render::LoadedEffect* effect = catalog_->Load(path);
  if (effect == nullptr) {
    return false;
  }
  Playing sown;
  sown.effect = effect;
  sown.ground_position = ground_position;
  sown.started_ms = now_ms;
  sown.loops_with_actor = while_actor;
  sown.lift_pixels = lift_pixels;
  sown.behind_actors = behind_actors;
  per_map_.playing.push_back(sown);
  return true;
}

void EffectsInPlay::MoveLooping(std::uint32_t while_actor, glm::vec3 ground_position) {
  if (while_actor == 0) {
    return;
  }
  for (Playing& playing : per_map_.playing) {
    if (playing.loops_with_actor == while_actor) {
      playing.ground_position = ground_position;
    }
  }
}

}  // namespace moonclient
