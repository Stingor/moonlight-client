#pragma once

#include <array>
#include <cstddef>
#include <span>

#include "ro/render/actor_model_renderer.h"
#include "ro/render/camera.h"
#include "ro/render/model_renderer.h"
#include "ro/render/scene_light.h"
#include "ro/render/sprite_renderer.h"
#include "ro/render/sun_shadow_map.h"
#include "ro/render/terrain_mesh.h"
#include "ro/render/terrain_renderer.h"

namespace ro::render {

// Les lots d'acteurs en sprite qu'une passe d'ombre reçoit au plus.
inline constexpr std::size_t kSunShadowActorLots = 2;

// CE QUI PORTE OMBRE dans une image. Un renderer nul ne peint rien : le décor
// que GreyWorld cache ne porte pas d'ombre.
struct SunShadowCasters {
  const TerrainRenderer* terrain = nullptr;
  const ModelRenderer* models = nullptr;
  // La pose des modèles animés, comme dans la passe de couleur.
  const ModelClock* clock = nullptr;
  // Les poses du décor qui portent ombre, pleines, les autres à zéro ; nul :
  // toutes. Voir `ShadowCastingOpacities`.
  const ModelOpacities* model_casting = nullptr;
  SpriteRenderer* sprites = nullptr;
  // Les lots d'acteurs — les corps, puis les compagnons, charrettes et faucons —,
  // dont la silhouette va
  // dans la couche des acteurs.
  std::array<std::span<const DrawnActor>, kSunShadowActorLots> actor_lots{};
  // Les acteurs en modèle — Emperium, gardiens, drapeaux —, dans la même couche.
  ActorModelRenderer* actor_model_renderer = nullptr;
  std::span<const DrawnActorModel> actor_models;
};

// LA PASSE D'OMBRE D'UNE IMAGE : ce que portent `casters`, peint dans `map`,
// vu du soleil de `light`, cadré sur ce que `camera` voit de `terrain`.
//
// À appeler AVANT la passe de couleur, dont la vue rendue nourrit les `Draw`.
// L'eau n'y entre pas.
//
// `strength` dit ce que l'ombre retire, voir `SunShadowView::strength`.
[[nodiscard]] SunShadowView DrawSunShadow(SunShadowMap& map, const SunShadowCasters& casters,
                                          const TerrainMesh& terrain, const SceneLight& light,
                                          const Camera& camera, float aspect_ratio, float strength);

}  // namespace ro::render
