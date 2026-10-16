#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "ro/formats/image.h"
#include "ro/mapedit/map_edit.h"
#include "ro/render/model_mesh.h"

namespace ro::render {

// CE QUI FAIT LE MAILLAGE D'UN MODÈLE TÉLÉVERSÉ : deux modèles de même identité
// ont le même maillage, et les mêmes images. Le téléversement s'en sert pour
// garder les tampons d'un modèle inchangé — voir `ModelRenderer::Upload`.
struct ModelMeshIdentity {
  std::string rsm_file;
  std::optional<std::size_t> variant_rank;
  std::vector<mapedit::VertexMove> applied_moves;
  // Les images, par adresse : une image relue est une autre image.
  std::vector<const formats::Image*> images;
  std::int32_t rsm_version = 0;
  std::int32_t shade_type = 0;

  friend bool operator==(const ModelMeshIdentity&, const ModelMeshIdentity&) = default;
};

[[nodiscard]] ModelMeshIdentity MeshIdentityOf(const ModelInstances& model);

}  // namespace ro::render
