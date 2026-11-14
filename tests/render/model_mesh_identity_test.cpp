// L'IDENTITÉ DU MAILLAGE D'UN MODÈLE : ce qui permet au téléversement de garder
// les tampons d'un modèle inchangé.

#include "ro/render/model_mesh_identity.h"

#include <memory>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/image.h"

using ro::render::MeshIdentityOf;
using ro::render::ModelInstances;

TEST_CASE("Identité du maillage : ce qui la change, et ce qui ne la change pas",
          "[render][mapedit]") {
  ModelInstances model;
  model.rsm_file = "decor\\pont.rsm";
  model.textures.push_back(std::make_shared<const ro::formats::Image>());
  const ro::render::ModelMeshIdentity identity = MeshIdentityOf(model);

  // Les poses ne font pas le maillage : seule la cuisson les regarde.
  ModelInstances moved = model;
  moved.transforms.emplace_back(1.0F);
  CHECK(MeshIdentityOf(moved) == identity);

  // Une variante, un sommet déplacé, une image relue : un autre maillage.
  ModelInstances variant = model;
  variant.variant_rank = 3;
  CHECK_FALSE(MeshIdentityOf(variant) == identity);
  ModelInstances reshaped = model;
  reshaped.applied_moves.push_back(
      ro::mapedit::VertexMove{.node = "planche", .vertex = 0, .origin = {}, .position = {}});
  CHECK_FALSE(MeshIdentityOf(reshaped) == identity);
  ModelInstances reread = model;
  reread.textures[0] = std::make_shared<const ro::formats::Image>();
  CHECK_FALSE(MeshIdentityOf(reread) == identity);
}
