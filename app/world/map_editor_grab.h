#pragma once

#include <cstddef>
#include <optional>

#include <glm/glm.hpp>

#include "ro/render/ground_pick.h"
#include "ro/ui/map_editor_panel.h"

namespace moonclient {

// SAISIR UN MODÈLE PAR SON CORPS. Avec les outils Déplacer, Tourner et
// Échelle, un appui sur un modèle le désigne ; un glissé qui suit le fait
// suivre la souris sur le plan horizontal de son pivot, sur x et z du RSW.
// Les touches tenues pendant le glissé changent ce qu'il fait :
//   - Maj : le glissé vertical l'incline, autour de l'axe x du RSW ;
//   - Alt : le glissé horizontal le fait tourner sur lui-même, autour de la
//     verticale ;
//   - les deux : les deux à la fois. Tant que l'une est tenue, il ne glisse
//     plus ;
//   - Ctrl : la position et les angles s'aimantent — voir `Snapped`.
// Prendre ou lâcher une touche en cours de geste ne fait pas sauter le
// modèle : le glissé repart de là où il en est (`BodyAnchor`).
struct BodyDragKeys {
  bool tilt = false;
  bool turn = false;
  bool snap = false;
};

// Les axes de rotation du RSW que tiennent Maj et Alt.
inline constexpr std::size_t kTiltAxis = 0;
inline constexpr std::size_t kTurnAxis = 1;
// En deçà de ce trajet depuis l'appui, en pixels, le geste reste un clic qui
// désigne, et rien ne bouge.
inline constexpr float kBodyDragDeadZonePixels = 4.0F;
// En deçà de cette pente du rayon sur l'horizontale, il frôle le plan : son
// point de croisement fuirait à l'horizon.
inline constexpr float kGrazingRaySlope = 0.05F;

// Le point du monde où `ray` croise le plan horizontal de hauteur
// `world_height` ; rien s'il le frôle ou le croise derrière l'œil.
[[nodiscard]] std::optional<glm::vec3> HorizontalHit(const ro::render::ScreenRay& ray,
                                                     float world_height);

// L'ANCRE D'UN GLISSÉ PAR LE CORPS : le placement, le pointeur, et le point du
// plan sous lui dans le repère du RSW, au moment où le geste a pris sa forme
// actuelle.
struct BodyAnchor {
  ro::ui::map_editor::Place place;
  glm::vec2 pointer{0.0F, 0.0F};
  std::optional<glm::vec3> native_hit;
};

// Le placement que donne le glissé depuis `anchor` jusqu'en `pointer`, dont
// le point du plan est `native_hit`.
[[nodiscard]] ro::ui::map_editor::Place BodyDragged(const BodyAnchor& anchor, BodyDragKeys keys,
                                                    glm::vec2 pointer,
                                                    const std::optional<glm::vec3>& native_hit);

// LE PAS DES RACCOURCIS DE ROTATION, Ctrl+Q et Ctrl+E : `degrees` mené au
// multiple suivant de `step_degrees`, dans le sens de son signe, puis ramené
// entre -180 et 180. Un angle déjà sur un multiple passe au suivant.
inline constexpr float kTurnStepDegrees = 45.0F;
[[nodiscard]] float SteppedAngle(float degrees, float step_degrees);

}  // namespace moonclient
