#pragma once

#include "ro/render/camera.h"
#include "ro/render/point_lights.h"
#include "ro/render/point_shadow_maps.h"
#include "ro/render/sun_shadow_pass.h"

namespace ro::render {

// LA PASSE D'OMBRE DES LUMIÈRES PONCTUELLES d'une image : pour chacune des
// `ShadowedPointLightCount(points)` premières lumières, les six faces de sa
// carte cubique, où `casters` peignent leur profondeur — les mêmes porteurs
// que pour le soleil, en une seule couche : le sol, le décor, puis les corps.
//
// Rend `points` avec ses cartes : les dessins qui le reçoivent masquent chaque
// lumière ombrée par sa carte. Sans carte valide ni lumière, ne peint rien et
// rend `points` sans ombre.
//
// À appeler AVANT la passe de couleur. `camera` ne sert qu'aux élargissements
// des sprites comptés en pixels d'écran.
//
// LE COÛT : six passes de profondeur par carte repeinte, chacune sur tout ce
// que porte `casters`, à `kPointShadowMapTexels` de côté. Une lumière immobile
// ne se repeint qu'une image sur `kPointShadowRepaintPeriod` : voir
// `PointShadowRepaints`. Deux lumières fixes coûtent six passes par image.
[[nodiscard]] PointLightSet DrawPointShadows(PointShadowMaps& maps, const SunShadowCasters& casters,
                                             const PointLightSet& points, const Camera& camera);

}  // namespace ro::render
