#include "ro/formats/gr2_model.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/ascii.h"
#include "ro/formats/gr2_codecs.h"

#include "gr2_file.h"

namespace ro::formats {
namespace {

using gr2::Gr2File;
using gr2::Gr2Kind;
using gr2::Gr2Member;
using gr2::Gr2Object;

// Comment une texture est rangée.
enum class TextureEncoding : std::int32_t { kRaw = 1, kBlocks = 2, kWavelet = 3 };

// La disposition d'un pixel brut : ses octets, puis le décalage et le nombre
// de bits de chaque composante — rouge, vert, bleu, alpha.
constexpr std::size_t kComponents = 4;
constexpr std::size_t kAlphaComponent = 3;
constexpr std::uint32_t kLayoutShiftsOffset = 4;
constexpr std::uint32_t kLayoutBitsOffset = 20;
constexpr std::uint32_t kLayoutWordBytes = 4;
constexpr std::uint32_t kBitsPerChannel = 8;
constexpr std::uint32_t kLargestPixelBytes = 4;
constexpr std::uint8_t kOpaque = 255;
constexpr std::size_t kRgbaBytes = 4;

// Les usages d'une carte de matériau qui désignent la couleur DIFFUSE, selon
// l'outil qui a exporté le fichier.
constexpr std::array<std::string_view, 3> kDiffuseUsages = {"color", "Diffuse Color", "BaseColor"};
// Un matériau renvoie à d'autres par ses cartes : on n'en suit pas davantage.
constexpr std::uint32_t kDeepestMaterial = 16;

// Un poids réel se ramène à l'octet.
constexpr double kWeightScale = 255.0;
constexpr float kLargestByte = 255.0F;

bool SameText(std::string_view left, std::string_view right) {
  return core::AsciiLower(left) == core::AsciiLower(right);
}

// Le sous-objet rangé EN LIGNE dans `object` sous ce nom.
Gr2Object Inline(Gr2File& file, const Gr2Object& object, std::string_view name) {
  const std::optional<Gr2Member> member = file.Field(object, name);
  if (!member || member->kind != Gr2Kind::kInline) {
    return {};
  }
  return Gr2Object{member->type, object.address + member->offset};
}

std::vector<std::uint8_t> RawPixels(Gr2File& file, const Gr2Object& layout,
                                    std::span<const std::uint8_t> pixels, std::int32_t width,
                                    std::int32_t height, std::int32_t stride) {
  const auto bytes_per_pixel = static_cast<std::uint32_t>(file.Int(layout, "BytesPerPixel", 0));
  std::array<std::uint32_t, kComponents> shifts{};
  std::array<std::uint32_t, kComponents> bits{};
  if (const std::optional<Gr2Member> member = file.Field(layout, "ShiftForComponent")) {
    for (std::size_t lane = 0; lane < kComponents; ++lane) {
      shifts[lane] = static_cast<std::uint32_t>(
          file.Scalar(*member, layout.address, static_cast<std::uint32_t>(lane)));
    }
  }
  if (const std::optional<Gr2Member> member = file.Field(layout, "BitsForComponent")) {
    for (std::size_t lane = 0; lane < kComponents; ++lane) {
      bits[lane] = static_cast<std::uint32_t>(
          file.Scalar(*member, layout.address, static_cast<std::uint32_t>(lane)));
    }
  }
  if (bytes_per_pixel == 0 || bytes_per_pixel > kLargestPixelBytes) {
    return {};
  }
  const std::size_t row_bytes = stride > 0 ? static_cast<std::size_t>(stride)
                                           : static_cast<std::size_t>(width) * bytes_per_pixel;
  if (pixels.size() < row_bytes * static_cast<std::size_t>(height)) {
    return {};
  }
  std::vector<std::uint8_t> rgba(static_cast<std::size_t>(width) *
                                 static_cast<std::size_t>(height) * kRgbaBytes);
  for (std::size_t y = 0; y < static_cast<std::size_t>(height); ++y) {
    for (std::size_t x = 0; x < static_cast<std::size_t>(width); ++x) {
      std::uint32_t packed = 0;
      for (std::uint32_t byte = 0; byte < bytes_per_pixel; ++byte) {
        packed |= std::uint32_t{pixels[y * row_bytes + x * bytes_per_pixel + byte]}
                  << (kBitsPerChannel * byte);
      }
      std::uint8_t* out = rgba.data() + (y * static_cast<std::size_t>(width) + x) * kRgbaBytes;
      for (std::size_t lane = 0; lane < kComponents; ++lane) {
        const std::uint32_t count = std::min<std::uint32_t>(bits[lane], 32);
        if (count == 0) {
          out[lane] = lane == kAlphaComponent ? kOpaque : 0;
          continue;
        }
        const std::uint64_t mask = (std::uint64_t{1} << count) - 1;
        const std::uint32_t value =
            static_cast<std::uint32_t>((std::uint64_t{packed} >> (shifts[lane] % 32)) & mask);
        out[lane] = static_cast<std::uint8_t>(count >= kBitsPerChannel
                                                  ? value >> (count - kBitsPerChannel)
                                                  : value << (kBitsPerChannel - count));
      }
    }
  }
  return rgba;
}

core::Result<GrannyTextureImage> ReadTexture(Gr2File& file, const Gr2Object& texture) {
  GrannyTextureImage image;
  image.file_name = file.String(texture, "FromFileName");
  image.width = file.Int(texture, "Width", 0);
  image.height = file.Int(texture, "Height", 0);
  const Gr2Object layout = Inline(file, texture, "Layout");
  if (const std::optional<Gr2Member> bits = file.Field(layout, "BitsForComponent")) {
    image.has_alpha =
        bits->width > kAlphaComponent &&
        file.Scalar(*bits, layout.address, static_cast<std::uint32_t>(kAlphaComponent)) != 0.0;
  }
  const std::vector<Gr2Object> images = file.Array(texture, "Images");
  if (images.empty() || image.width <= 0 || image.height <= 0) {
    return image;
  }
  const std::vector<Gr2Object> levels = file.Array(images.front(), "MIPLevels");
  if (levels.empty()) {
    return image;
  }
  const Gr2Object level = levels.front();
  std::span<const std::uint8_t> pixels;
  if (const std::optional<Gr2Member> member = file.Field(level, "Pixels");
      member && member->kind == Gr2Kind::kReferenceToArray) {
    const std::uint32_t at = level.address + member->offset;
    const std::int32_t count = file.memory().I32(at);
    const std::uint32_t first = file.memory().U32(at + kLayoutWordBytes);
    if (count > 0 && first != 0) {
      pixels = file.memory().Bytes(first, static_cast<std::uint32_t>(count));
    }
  }
  switch (static_cast<TextureEncoding>(file.Int(texture, "Encoding", 0))) {
    case TextureEncoding::kWavelet: {
      auto decoded = DecodeGr2WaveletTexture(pixels, image.width, image.height, image.has_alpha);
      if (!decoded) {
        return std::unexpected(decoded.error());
      }
      image.rgba = std::move(*decoded);
      break;
    }
    case TextureEncoding::kRaw:
      image.rgba =
          RawPixels(file, layout, pixels, image.width, image.height, file.Int(level, "Stride", 0));
      if (!image.has_alpha) {
        for (std::size_t alpha = kAlphaComponent; alpha < image.rgba.size(); alpha += kRgbaBytes) {
          image.rgba[alpha] = kOpaque;
        }
      }
      break;
    case TextureEncoding::kBlocks:
    default:
      // Encodage non géré : la texture reste sans pixels, et le maillage qui la
      // porte ne se peint pas.
      break;
  }
  return image;
}

// La texture d'un matériau : la sienne, ou la première que portent ses cartes.
Gr2Object TextureOfMaterial(Gr2File& file, const Gr2Object& material, std::uint32_t depth) {
  if (!material.present() || depth > kDeepestMaterial) {
    return {};
  }
  if (const Gr2Object own = file.Reference(material, "Texture"); own.present()) {
    return own;
  }
  for (const Gr2Object& map : file.Array(material, "Maps")) {
    if (const Gr2Object found = TextureOfMaterial(file, file.Reference(map, "Map"), depth + 1);
        found.present()) {
      return found;
    }
  }
  return {};
}

// LA TEXTURE DIFFUSE d'un matériau : celle de la première de ses cartes dont
// l'usage dit la couleur, et qui en porte une. Le matériau lui-même n'est pas
// consulté.
Gr2Object DiffuseTexture(Gr2File& file, const Gr2Object& material) {
  for (const Gr2Object& map : file.Array(material, "Maps")) {
    const Gr2Object texture = TextureOfMaterial(file, file.Reference(map, "Map"), 1);
    if (!texture.present()) {
      continue;
    }
    const std::string usage = file.String(map, "Usage");
    if (std::any_of(kDiffuseUsages.begin(), kDiffuseUsages.end(),
                    [&](std::string_view diffuse) { return SameText(usage, diffuse); })) {
      return texture;
    }
  }
  return {};
}

std::uint8_t ByteOf(double value) {
  return static_cast<std::uint8_t>(std::clamp(value, 0.0, static_cast<double>(kLargestByte)));
}

GrannyMesh ReadMesh(Gr2File& file, const Gr2Object& mesh,
                    const std::vector<GrannyTextureImage>& textures) {
  GrannyMesh out;
  out.name = file.String(mesh, "Name");
  const gr2::Gr2VariantArray vertices =
      file.VariantArray(file.Reference(mesh, "PrimaryVertexData"), "Vertices");
  if (vertices.count != 0) {
    const std::uint32_t stride = file.SizeOf(vertices.type);
    const std::optional<Gr2Member> position = file.MemberNamed(vertices.type, "Position");
    const std::optional<Gr2Member> normal = file.MemberNamed(vertices.type, "Normal");
    const std::optional<Gr2Member> uv = file.MemberNamed(vertices.type, "TextureCoordinates0");
    const std::optional<Gr2Member> weights = file.MemberNamed(vertices.type, "BoneWeights");
    const std::optional<Gr2Member> bones = file.MemberNamed(vertices.type, "BoneIndices");
    const auto lanes = [&](const std::optional<Gr2Member>& member, std::uint32_t address,
                           std::span<float> target) {
      if (!member) {
        return;
      }
      const std::uint32_t count =
          std::min<std::uint32_t>(member->width, static_cast<std::uint32_t>(target.size()));
      for (std::uint32_t lane = 0; lane < count; ++lane) {
        target[lane] = static_cast<float>(file.Normalized(*member, address, lane));
      }
    };
    out.vertices.resize(vertices.count);
    for (std::uint32_t index = 0; index < vertices.count; ++index) {
      const std::uint32_t address = vertices.address + index * stride;
      GrannyVertex& vertex = out.vertices[index];
      lanes(position, address, vertex.position);
      lanes(normal, address, vertex.normal);
      lanes(uv, address, vertex.uv);
      for (std::uint32_t lane = 0; lane < vertex.bone_indices.size(); ++lane) {
        if (bones && lane < bones->width) {
          vertex.bone_indices[lane] = ByteOf(file.Scalar(*bones, address, lane));
        }
        if (weights && lane < weights->width) {
          const double weight =
              weights->kind == Gr2Kind::kReal32
                  ? std::round(file.Scalar(*weights, address, lane) * kWeightScale)
                  : file.Scalar(*weights, address, lane);
          vertex.bone_weights[lane] = ByteOf(weight);
        }
      }
    }
  }
  const Gr2Object topology = file.Reference(mesh, "PrimaryTopology");
  std::vector<std::int64_t> indices = file.IntegerArray(topology, "Indices");
  if (indices.empty()) {
    indices = file.IntegerArray(topology, "Indices16");
  }
  out.indices.reserve(indices.size());
  for (const std::int64_t index : indices) {
    out.indices.push_back(static_cast<std::uint16_t>(index));
  }
  for (const Gr2Object& binding : file.Array(mesh, "BoneBindings")) {
    out.bone_names.push_back(file.String(binding, "BoneName"));
  }
  out.rigid = out.bone_names.size() == 1;
  // La texture du premier matériau lié, retrouvée PAR LE NOM DE SON FICHIER
  // parmi celles du fichier.
  const std::vector<Gr2Object> materials = file.Array(mesh, "MaterialBindings");
  if (!materials.empty()) {
    const Gr2Object texture = DiffuseTexture(file, file.Reference(materials.front(), "Material"));
    if (texture.present()) {
      const std::string name = file.String(texture, "FromFileName");
      const auto found = std::find_if(
          textures.begin(), textures.end(),
          [&](const GrannyTextureImage& candidate) { return candidate.file_name == name; });
      if (found != textures.end()) {
        out.texture = static_cast<std::int32_t>(found - textures.begin());
      }
    }
  }
  return out;
}

GrannyCurve ReadCurve(Gr2File& file, const Gr2Object& curve) {
  GrannyCurve out;
  out.degree = file.Int(curve, "Degree", 0);
  out.knots = file.RealArray(curve, "Knots");
  out.controls = file.RealArray(curve, "Controls");
  if (!out.knots.empty()) {
    out.dimension = static_cast<std::int32_t>(out.controls.size() / out.knots.size());
  }
  return out;
}

GrannyAnimation ReadAnimation(Gr2File& file, const Gr2Object& animation) {
  GrannyAnimation out;
  out.name = file.String(animation, "Name");
  out.duration_seconds = file.Real(animation, "Duration", 0.0F);
  out.time_step_seconds = file.Real(animation, "TimeStep", 0.0F);
  for (const Gr2Object& group : file.Array(animation, "TrackGroups")) {
    if (!group.present()) {
      continue;
    }
    GrannyTrackGroup& track_group = out.track_groups.emplace_back();
    track_group.name = file.String(group, "Name");
    for (const Gr2Object& track : file.Array(group, "TransformTracks")) {
      GrannyTrack& out_track = track_group.tracks.emplace_back();
      out_track.name = file.String(track, "Name");
      out_track.position = ReadCurve(file, Inline(file, track, "PositionCurve"));
      out_track.orientation = ReadCurve(file, Inline(file, track, "OrientationCurve"));
      out_track.scale_shear = ReadCurve(file, Inline(file, track, "ScaleShearCurve"));
    }
  }
  return out;
}

}  // namespace

core::Result<GrannyScene> ReadGrannyScene(std::span<const std::uint8_t> bytes) {
  auto loaded = gr2::LoadGr2File(bytes);
  if (!loaded) {
    return std::unexpected(loaded.error());
  }
  Gr2File& file = *loaded;
  const Gr2Object root = file.root();
  GrannyScene scene;
  for (const Gr2Object& texture : file.Array(root, "Textures")) {
    if (!texture.present()) {
      scene.textures.emplace_back();
      continue;
    }
    auto image = ReadTexture(file, texture);
    if (!image) {
      return std::unexpected(image.error());
    }
    scene.textures.push_back(std::move(*image));
  }
  const std::vector<Gr2Object> models = file.Array(root, "Models");
  if (!models.empty() && models.front().present()) {
    const Gr2Object model = models.front();
    scene.model_name = file.String(model, "Name");
    const Gr2Object skeleton = file.Reference(model, "Skeleton");
    for (const Gr2Object& bone : file.Array(skeleton, "Bones")) {
      GrannyBone& out = scene.bones.emplace_back();
      out.name = file.String(bone, "Name");
      out.parent_index = file.Int(bone, "ParentIndex", -1);
      out.local = file.Transform(bone, "Transform");
      file.Reals(bone, "InverseWorldTransform", out.inverse_world);
    }
    for (const Gr2Object& binding : file.Array(model, "MeshBindings")) {
      if (const Gr2Object mesh = file.Reference(binding, "Mesh"); mesh.present()) {
        scene.meshes.push_back(ReadMesh(file, mesh, scene.textures));
      }
    }
  }
  for (const Gr2Object& animation : file.Array(root, "Animations")) {
    if (animation.present()) {
      scene.animations.push_back(ReadAnimation(file, animation));
    }
  }
  if (!file.ok()) {
    return core::Fail(core::ErrorCode::kCorruptData, "Granny : objet hors du fichier");
  }
  return scene;
}

}  // namespace ro::formats
