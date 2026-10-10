#pragma once

#include <optional>

#include <glm/glm.hpp>

#include "ro/render/camera.h"
#include "ro/render/floating_number.h"
#include "ro/ui/damage_texts.h"

namespace moonclient {

// ── LES NOMBRES EN POLICE, PROJETÉS À L'ÉCRAN ───────────────────────────────
//
// Un nombre de dégâts se pose dans le monde, aux pieds de sa cible, et ses
// chiffres sont un panneau face à la caméra. Écrit en police, il se peint à
// l'écran par l'étage d'interface : il faut donc dire où tombe son milieu et
// combien de pixels d'écran vaut un pixel de sprite À CETTE PROFONDEUR — un
// nombre au loin s'écrit plus petit, comme ses chiffres de sprite.

// La vue où l'on projette.
struct NumberScreen {
  const ro::render::Camera* camera = nullptr;
  float aspect_ratio = 1.0F;
  float viewport_width = 0.0F;
  float viewport_height = 0.0F;
};

// Le nombre `text`, posé à `placed` — les pieds de sa cible, dérive comprise —,
// tel que l'étage d'interface l'écrit. `digit` est la boîte d'un chiffre du
// sprite, que le texte épouse ; `outline_pixels` le cerne, en pixels de sprite
// avant l'échelle. Rien derrière l'œil, ou sans caméra.
[[nodiscard]] std::optional<ro::ui::DamageTextView> DamageTextOnScreen(
    const NumberScreen& screen, const glm::vec3& placed, const ro::render::NumberText& text,
    const ro::render::GlyphBox& digit, float outline_pixels);

}  // namespace moonclient
