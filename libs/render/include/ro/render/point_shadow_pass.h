#pragma once

#include <cstddef>
#include <cstdint>

#include "ro/render/camera.h"
#include "ro/render/point_light_stage.h"
#include "ro/render/point_lights.h"
#include "ro/render/sun_shadow_pass.h"

namespace ro::render {

// CE QU'A COÛTÉ LA PASSE D'UNE IMAGE, pour le banc.
struct PointShadowTally {
  std::size_t visible_lights = 0;
  std::size_t shadowed_lights = 0;
  std::size_t painted_faces = 0;
  // Les faces qui auraient dû se peindre, et attendent l'image suivante. Celles
  // qu'un modèle animé fait seul repeindre n'y comptent pas : elles ont leur
  // propre cadence.
  std::size_t deferred_faces = 0;
  // Celles qui ne se repeignent que parce qu'un modèle s'anime dans leur champ.
  std::size_t moving_faces = 0;
};

// LES LUMIÈRES PONCTUELLES D'UNE IMAGE, prêtes pour la passe de couleur.
//
// Garde de `points` celles dont la portée touche ce que voit `camera`, et les
// téléverse dans `stage`. Avec `casters`, chacune reçoit une carte d'ombre
// (voir `PointShadowSlots`), où les porteurs peignent leur profondeur — le sol,
// le décor, puis les corps, en une seule couche.
//
// UNE FACE NE SE PEINT QUE SI CE QU'ELLE VOIT A CHANGÉ : une carte neuve, le
// sol ou le décor d'une autre génération (voir `PointShadowScene`), un acteur
// entré, sorti ou changé dans son champ — ou, après toutes celles-là et pas
// plus d'une image sur `kPointShadowAnimatedPeriod`, un modèle qui s'anime.
// Et elle ne peint que ce qu'elle voit : voir `ShadowSight`. Au plus
// `kPointShadowFacesPerFrame` faces par image, de la lumière la plus proche à
// la plus lointaine.
//
// Rend les lumières, leurs cartes et `stage` : les dessins qui les reçoivent
// masquent chaque lumière ombrée par sa carte. À appeler AVANT la passe de
// couleur. `camera` sert aussi aux élargissements des sprites comptés en
// pixels d'écran.
[[nodiscard]] PointLightSet DrawPointShadows(PointLightStage& stage,
                                             const SunShadowCasters* casters,
                                             const PointLightSet& points, const Camera& camera,
                                             float aspect_ratio, PointShadowTally* tally = nullptr);

}  // namespace ro::render
