#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>

#include <glm/glm.hpp>

#include "ro/render/skill_named_recipe.h"
#include "ro/sim/actor.h"

namespace moonclient {

// CE QUE LES EFFETS DEMANDENT À LA VUE DES ACTEURS, qui vit hors de leur
// bibliothèque : l'allure d'un acteur (`ro::render::NamedEffectPaceOf`), la
// direction de son sprite, s'il bronche, et où sa tête est passée. Vides, rien
// ne se lit.
struct EffectActorView {
  std::function<std::optional<ro::render::NamedEffectPaceOf>(
      const ro::sim::Actor& actor, std::uint16_t skill_id,
      std::optional<std::uint32_t> attack_motion_ms)>
      pace_of;
  std::function<std::optional<std::size_t>(const ro::sim::Actor& actor, std::uint32_t now_ms)>
      sprite_direction_of;
  std::function<bool(const ro::sim::Actor& actor, std::uint32_t now_ms)> flinches_at;
  // LE DÉPLACEMENT DE LA TÊTE de chaque acteur composé à cette image, en pixels
  // de sprite : voir `ComposedScene::head_shifts`. Nul, aucune tête ne bouge.
  const std::map<std::uint32_t, glm::vec2>* head_shifts = nullptr;

  // Celui de cet acteur ; nul s'il n'a pas de tête composée à cette image.
  [[nodiscard]] glm::vec2 HeadShiftOf(std::uint32_t actor_id) const {
    if (head_shifts == nullptr) {
      return glm::vec2(0.0F);
    }
    const auto found = head_shifts->find(actor_id);
    return found != head_shifts->end() ? found->second : glm::vec2(0.0F);
  }
};

}  // namespace moonclient
