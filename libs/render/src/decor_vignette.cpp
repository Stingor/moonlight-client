#include "ro/render/decor_vignette.h"

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "ro/formats/gr2_model.h"
#include "ro/formats/rsm.h"
#include "ro/formats/rsw.h"
#include "ro/render/map_scene.h"
#include "ro/render/model_vignette.h"

namespace ro::render {
namespace {

constexpr std::string_view kModelFolder = "data\\model\\";

// LA VUE DE TROIS QUARTS : le modèle tourne d'un huitième de tour autour de sa
// verticale, et montre deux de ses faces à la fois.
constexpr float kThreeQuarterYawRadians = 0.785398F;

// Un modèle que le fichier déclare sans ombrage se peint comme sur la carte :
// à pleine lumière, sans que l'orientation des faces n'y entre.
constexpr float kUnshadedAmbient = 1.0F;
constexpr float kUnshadedDiffuse = 0.0F;

// Les index d'un lot tiennent sur seize bits : un lot plus long se découpe en
// tranches de faces entières.
constexpr std::size_t kCornersPerFace = 3;
constexpr std::size_t kLongestSlice = (std::size_t{0xFFFF} / kCornersPerFace) * kCornersPerFace;

// Le repère de la carte — Y vers le haut — vers celui de la vignette — Z vers
// le haut, X et Y au sol. Une rotation d'un quart de tour autour de X, sans
// miroir : le modèle ne se montre pas retourné.
[[nodiscard]] glm::vec3 VignetteFrameOf(const glm::vec3& map) { return {map.x, -map.z, map.y}; }

// La texture au format que la vignette lit, ou rien.
[[nodiscard]] std::optional<formats::GrannyTextureImage> PaintedTexture(
    const std::shared_ptr<const formats::Image>& image) {
  if (image == nullptr || image->empty()) {
    return std::nullopt;
  }
  formats::GrannyTextureImage painted;
  painted.width = image->width;
  painted.height = image->height;
  painted.has_alpha = true;
  painted.rgba = image->pixels;
  return painted;
}

}  // namespace

formats::Image RasterDecorVignette(const ModelMesh& mesh,
                                   std::span<const std::shared_ptr<const formats::Image>> textures,
                                   std::int32_t rsm_version, std::int32_t shade_type,
                                   std::int32_t side_px) {
  if (mesh.empty() || side_px <= 0) {
    return {};
  }
  const glm::mat4 instance = ModelInstanceMatrix(formats::RswModel{}, 0, 0, mesh, rsm_version);

  std::vector<PosedVertex> posed;
  posed.reserve(mesh.vertices.size());
  for (const ModelVertex& vertex : mesh.vertices) {
    const glm::vec3 local{vertex.position[0], vertex.position[1], vertex.position[2]};
    const glm::vec3 placed = glm::vec3(instance * glm::vec4(local, 1.0F));
    const glm::vec3 normal = TransformedNormal(
        instance, glm::vec3{vertex.normal[0], vertex.normal[1], vertex.normal[2]});
    posed.push_back(PosedVertex{.position = VignetteFrameOf(placed),
                                .normal = VignetteFrameOf(normal),
                                .uv = {vertex.texture_uv[0], vertex.texture_uv[1]}});
  }

  std::vector<std::optional<formats::GrannyTextureImage>> painted;
  painted.reserve(textures.size());
  for (const std::shared_ptr<const formats::Image>& image : textures) {
    painted.push_back(PaintedTexture(image));
  }

  std::vector<std::uint16_t> indices(std::min(kLongestSlice, posed.size()));
  std::iota(indices.begin(), indices.end(), std::uint16_t{0});

  std::vector<ModelVignetteMesh> meshes;
  for (const ModelSurface& surface : mesh.surfaces) {
    const std::size_t first = surface.first_vertex;
    const std::size_t end = std::min<std::size_t>(first + surface.vertex_count, posed.size());
    const formats::GrannyTextureImage* texture =
        surface.texture_index >= 0 &&
                static_cast<std::size_t>(surface.texture_index) < painted.size() &&
                painted[static_cast<std::size_t>(surface.texture_index)].has_value()
            ? &*painted[static_cast<std::size_t>(surface.texture_index)]
            : nullptr;
    for (std::size_t slice = first; slice < end; slice += kLongestSlice) {
      const std::size_t count = std::min(kLongestSlice, end - slice);
      meshes.push_back(
          ModelVignetteMesh{.vertices = std::span<const PosedVertex>(posed).subspan(slice, count),
                            .indices = std::span<const std::uint16_t>(indices).first(count),
                            .texture = texture});
    }
  }

  ModelVignetteView view{
      .width = side_px, .height = side_px, .yaw_radians = kThreeQuarterYawRadians};
  if (shade_type == 0) {
    view.ambient = kUnshadedAmbient;
    view.diffuse = kUnshadedDiffuse;
  }
  return RasterModelVignette(meshes, view);
}

core::Result<formats::Image> LoadDecorVignette(vfs::Vfs& vfs, std::string_view rsm_file,
                                               std::int32_t side_px) {
  std::string path(kModelFolder);
  path += rsm_file;
  auto bytes = vfs.Read(path);
  if (!bytes) {
    return std::unexpected(bytes.error());
  }
  auto model = formats::RsmModel::Parse(*bytes);
  if (!model) {
    return std::unexpected(model.error());
  }
  const ModelMesh mesh = BuildModelMesh(*model);
  std::vector<std::shared_ptr<const formats::Image>> textures;
  textures.reserve(mesh.textures.size());
  for (const std::string& texture : mesh.textures) {
    std::optional<formats::Image> image = LoadSceneTexture(vfs, texture);
    textures.push_back(image.has_value() ? std::make_shared<const formats::Image>(std::move(*image))
                                         : nullptr);
  }
  formats::Image vignette =
      RasterDecorVignette(mesh, textures, model->version(), model->shade_type(), side_px);
  if (vignette.empty()) {
    return core::Fail(core::ErrorCode::kCorruptData,
                      std::string(rsm_file) + " : aucune face à montrer");
  }
  return vignette;
}

}  // namespace ro::render
