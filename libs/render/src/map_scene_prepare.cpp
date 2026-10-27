// LA PART PURE DU TÉLÉVERSEMENT D'UNE SCÈNE, faite d'avance : ce que
// `TerrainRenderer::Upload` et `ModelRenderer::Upload` calculeraient sur le fil
// qui les appelle, et qui ne dépend que de la scène.

#include <memory>

#include "ro/render/map_scene.h"
#include "ro/render/model_bake.h"
#include "ro/render/terrain_depth_chunks.h"

namespace ro::render {

void PrepareSceneUpload(MapScene& scene) {
  scene.terrain.depth_chunks =
      std::make_shared<const TerrainDepthChunks>(BuildTerrainDepthChunks(scene.terrain));
  for (ModelInstances& model : scene.models) {
    model.baked = std::make_shared<const BakedModel>(
        BakeInstances(model.mesh, model.transforms, model.rsm_version));
    model.baked_transforms = model.transforms;
  }
}

}  // namespace ro::render
