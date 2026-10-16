#include "ro/render/model_mesh_identity.h"

#include <memory>

namespace ro::render {

ModelMeshIdentity MeshIdentityOf(const ModelInstances& model) {
  ModelMeshIdentity identity{.rsm_file = model.rsm_file,
                             .variant_rank = model.variant_rank,
                             .applied_moves = model.applied_moves,
                             .images = {},
                             .rsm_version = model.rsm_version,
                             .shade_type = model.shade_type};
  identity.images.reserve(model.textures.size());
  for (const std::shared_ptr<const formats::Image>& image : model.textures) {
    identity.images.push_back(image.get());
  }
  return identity;
}

}  // namespace ro::render
