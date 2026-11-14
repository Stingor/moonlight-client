#include "support/gr2_builder.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/core/byte_writer.h"

namespace ro::test_support {
namespace {

// Le genre d'un membre, dans la numérotation du format.
enum class Kind : std::uint32_t {
  kInline = 1,
  kReference = 2,
  kReferenceToArray = 3,
  kArrayOfReferences = 4,
  kReferenceToVariantArray = 7,
  kString = 8,
  kTransform = 9,
  kReal32 = 10,
  kUInt8 = 12,
  kNormalUInt8 = 14,
  kInt32 = 19,
};

std::uint32_t KindBytes(Kind kind) {
  switch (kind) {
    case Kind::kReferenceToArray:
    case Kind::kArrayOfReferences:
      return 8;
    case Kind::kReferenceToVariantArray:
      return 12;
    case Kind::kTransform:
      return 68;
    case Kind::kUInt8:
    case Kind::kNormalUInt8:
      return 1;
    default:
      return 4;
  }
}

struct TypeDef;

struct Member {
  Kind kind = Kind::kInt32;
  std::string name;
  const TypeDef* type = nullptr;
  std::uint32_t width = 0;
};

struct TypeDef {
  std::vector<Member> members;
  std::uint32_t offset = 0;
};

std::uint32_t SizeOf(const TypeDef& type);

std::uint32_t MemberBytes(const Member& member) {
  const std::uint32_t element =
      member.kind == Kind::kInline ? SizeOf(*member.type) : KindBytes(member.kind);
  return element * std::max<std::uint32_t>(member.width, 1);
}

std::uint32_t SizeOf(const TypeDef& type) {
  std::uint32_t size = 0;
  for (const Member& member : type.members) {
    size += MemberBytes(member);
  }
  return size;
}

// La section en cours d'écriture, et les mots qui y sont des pointeurs.
class Section {
 public:
  Section() { Allocate(kReservedHead); }

  std::uint32_t Allocate(std::uint32_t size) {
    bytes_.resize((bytes_.size() + 3) & ~std::size_t{3});
    const auto at = static_cast<std::uint32_t>(bytes_.size());
    bytes_.resize(bytes_.size() + size, 0);
    return at;
  }
  void U32(std::uint32_t at, std::uint32_t value) {
    for (std::size_t byte = 0; byte < sizeof(value); ++byte) {
      bytes_[at + byte] = static_cast<std::uint8_t>(value >> (8 * byte));
    }
  }
  void F32(std::uint32_t at, float value) { U32(at, std::bit_cast<std::uint32_t>(value)); }
  void U8(std::uint32_t at, std::uint8_t value) { bytes_[at] = value; }
  void Pointer(std::uint32_t at, std::uint32_t target) { fixups_.emplace_back(at, target); }
  std::uint32_t String(std::string_view text) {
    const std::uint32_t at = Allocate(static_cast<std::uint32_t>(text.size() + 1));
    std::copy(text.begin(), text.end(), bytes_.begin() + at);
    return at;
  }

  [[nodiscard]] const std::vector<std::uint8_t>& bytes() const { return bytes_; }
  [[nodiscard]] const std::vector<std::pair<std::uint32_t, std::uint32_t>>& fixups() const {
    return fixups_;
  }

 private:
  // Rien ne se pose à l'offset 0 : un type nul s'y reconnaît.
  static constexpr std::uint32_t kReservedHead = 16;
  std::vector<std::uint8_t> bytes_;
  std::vector<std::pair<std::uint32_t, std::uint32_t>> fixups_;
};

constexpr std::uint32_t kMemberDefinitionBytes = 32;

void ReserveTypes(Section& section, std::initializer_list<TypeDef*> types) {
  for (TypeDef* type : types) {
    type->offset = section.Allocate(kMemberDefinitionBytes *
                                    static_cast<std::uint32_t>(type->members.size() + 1));
  }
  for (TypeDef* type : types) {
    std::uint32_t at = type->offset;
    for (const Member& member : type->members) {
      section.U32(at, static_cast<std::uint32_t>(member.kind));
      section.Pointer(at + 4, section.String(member.name));
      if (member.type != nullptr) {
        section.Pointer(at + 8, member.type->offset);
      }
      section.U32(at + 12, member.width);
      at += kMemberDefinitionBytes;
    }
  }
}

// Un objet posé dans la section : ses champs s'écrivent par leur nom.
class Object {
 public:
  Object(Section& section, const TypeDef& type)
      : section_(&section), type_(&type), at_(section.Allocate(SizeOf(type))) {}
  Object(Section& section, const TypeDef& type, std::uint32_t at)
      : section_(&section), type_(&type), at_(at) {}

  [[nodiscard]] std::uint32_t at() const { return at_; }

  std::uint32_t Offset(std::string_view name) const {
    std::uint32_t offset = at_;
    for (const Member& member : type_->members) {
      if (member.name == name) {
        return offset;
      }
      offset += MemberBytes(member);
    }
    return offset;
  }
  void Int(std::string_view name, std::int32_t value) {
    section_->U32(Offset(name), static_cast<std::uint32_t>(value));
  }
  void Reals(std::string_view name, std::span<const float> values) {
    for (std::size_t index = 0; index < values.size(); ++index) {
      section_->F32(Offset(name) + static_cast<std::uint32_t>(4 * index), values[index]);
    }
  }
  void Bytes(std::string_view name, std::span<const std::uint8_t> values) {
    for (std::size_t index = 0; index < values.size(); ++index) {
      section_->U8(Offset(name) + static_cast<std::uint32_t>(index), values[index]);
    }
  }
  void Text(std::string_view name, std::string_view text) {
    section_->Pointer(Offset(name), section_->String(text));
  }
  void Reference(std::string_view name, std::uint32_t target) {
    section_->Pointer(Offset(name), target);
  }
  void Array(std::string_view name, std::size_t count, std::uint32_t first) {
    section_->U32(Offset(name), static_cast<std::uint32_t>(count));
    if (count != 0) {
      section_->Pointer(Offset(name) + 4, first);
    }
  }

 private:
  Section* section_;
  const TypeDef* type_;
  std::uint32_t at_;
};

// Un tableau de `count` objets d'un type, contigus.
std::vector<Object> Objects(Section& section, const TypeDef& type, std::size_t count) {
  const std::uint32_t first =
      section.Allocate(SizeOf(type) * static_cast<std::uint32_t>(std::max<std::size_t>(count, 1)));
  std::vector<Object> objects;
  for (std::size_t index = 0; index < count; ++index) {
    objects.emplace_back(section, type, first + static_cast<std::uint32_t>(index) * SizeOf(type));
  }
  return objects;
}

// Un tableau de références : `count` pointeurs contigus.
std::uint32_t PointerArray(Section& section, std::span<const std::uint32_t> targets) {
  const std::uint32_t first =
      section.Allocate(4 * static_cast<std::uint32_t>(std::max<std::size_t>(targets.size(), 1)));
  for (std::size_t index = 0; index < targets.size(); ++index) {
    section.Pointer(first + static_cast<std::uint32_t>(4 * index), targets[index]);
  }
  return first;
}

void WriteNumbers(Section& section, Object& owner, std::string_view name, const TypeDef& type,
                  std::span<const float> values) {
  const std::vector<Object> items = Objects(section, type, values.size());
  for (std::size_t index = 0; index < values.size(); ++index) {
    section.F32(items[index].at(), values[index]);
  }
  owner.Array(name, values.size(), items.empty() ? 0 : items.front().at());
}

// Les types qu'un modèle d'acteur emploie, tels que le fichier les décrit.
struct Types {
  TypeDef real{{{Kind::kReal32, "Real32"}}};
  TypeDef integer{{{Kind::kInt32, "Int32"}}};
  TypeDef byte{{{Kind::kUInt8, "UInt8"}}};
  TypeDef curve;
  TypeDef track;
  TypeDef group;
  TypeDef animation;
  TypeDef layout;
  TypeDef mip;
  TypeDef image;
  TypeDef texture;
  TypeDef material;
  TypeDef map;
  TypeDef bone;
  TypeDef skeleton;
  TypeDef vertex;
  TypeDef vertex_data;
  TypeDef topology;
  TypeDef bone_binding;
  TypeDef material_binding;
  TypeDef mesh;
  TypeDef mesh_binding;
  TypeDef model;
  TypeDef root;

  Types() {
    curve.members = {{Kind::kInt32, "Degree"},
                     {Kind::kReferenceToArray, "Knots", &real},
                     {Kind::kReferenceToArray, "Controls", &real}};
    track.members = {{Kind::kString, "Name"},
                     {Kind::kInline, "PositionCurve", &curve},
                     {Kind::kInline, "OrientationCurve", &curve},
                     {Kind::kInline, "ScaleShearCurve", &curve}};
    group.members = {{Kind::kString, "Name"}, {Kind::kReferenceToArray, "TransformTracks", &track}};
    animation.members = {{Kind::kString, "Name"},
                         {Kind::kReal32, "Duration"},
                         {Kind::kReal32, "TimeStep"},
                         {Kind::kArrayOfReferences, "TrackGroups", &group}};
    layout.members = {{Kind::kInt32, "BytesPerPixel"},
                      {Kind::kInt32, "ShiftForComponent", nullptr, 4},
                      {Kind::kInt32, "BitsForComponent", nullptr, 4}};
    mip.members = {{Kind::kInt32, "Stride"}, {Kind::kReferenceToArray, "Pixels", &byte}};
    image.members = {{Kind::kReferenceToArray, "MIPLevels", &mip}};
    texture.members = {{Kind::kString, "FromFileName"},
                       {Kind::kInt32, "TextureType"},
                       {Kind::kInt32, "Width"},
                       {Kind::kInt32, "Height"},
                       {Kind::kInt32, "Encoding"},
                       {Kind::kInt32, "SubFormat"},
                       {Kind::kInline, "Layout", &layout},
                       {Kind::kReferenceToArray, "Images", &image}};
    material.members = {{Kind::kString, "Name"},
                        {Kind::kReferenceToArray, "Maps", &map},
                        {Kind::kReference, "Texture", &texture}};
    map.members = {{Kind::kString, "Usage"}, {Kind::kReference, "Map", &material}};
    bone.members = {{Kind::kString, "Name"},
                    {Kind::kInt32, "ParentIndex"},
                    {Kind::kTransform, "Transform"},
                    {Kind::kReal32, "InverseWorldTransform", nullptr, 16}};
    skeleton.members = {{Kind::kString, "Name"}, {Kind::kReferenceToArray, "Bones", &bone}};
    vertex.members = {{Kind::kReal32, "Position", nullptr, 3},
                      {Kind::kNormalUInt8, "BoneWeights", nullptr, 4},
                      {Kind::kUInt8, "BoneIndices", nullptr, 4},
                      {Kind::kReal32, "Normal", nullptr, 3},
                      {Kind::kReal32, "TextureCoordinates0", nullptr, 2}};
    vertex_data.members = {{Kind::kReferenceToVariantArray, "Vertices"}};
    topology.members = {{Kind::kReferenceToArray, "Indices", &integer}};
    bone_binding.members = {{Kind::kString, "BoneName"}};
    material_binding.members = {{Kind::kReference, "Material", &material}};
    mesh.members = {{Kind::kString, "Name"},
                    {Kind::kReference, "PrimaryVertexData", &vertex_data},
                    {Kind::kReference, "PrimaryTopology", &topology},
                    {Kind::kReferenceToArray, "MaterialBindings", &material_binding},
                    {Kind::kReferenceToArray, "BoneBindings", &bone_binding}};
    mesh_binding.members = {{Kind::kReference, "Mesh", &mesh}};
    model.members = {{Kind::kString, "Name"},
                     {Kind::kReference, "Skeleton", &skeleton},
                     {Kind::kReferenceToArray, "MeshBindings", &mesh_binding}};
    root.members = {{Kind::kArrayOfReferences, "Textures", &texture},
                    {Kind::kArrayOfReferences, "Materials", &material},
                    {Kind::kArrayOfReferences, "Models", &model},
                    {Kind::kArrayOfReferences, "Animations", &animation}};
  }

  void Emit(Section& section) {
    ReserveTypes(section, {&real,    &integer,      &byte,     &curve,        &track,
                           &group,   &animation,    &layout,   &mip,          &image,
                           &texture, &material,     &map,      &bone,         &skeleton,
                           &vertex,  &vertex_data,  &topology, &bone_binding, &material_binding,
                           &mesh,    &mesh_binding, &model,    &root});
  }
};

// La forme brute d'une petite texture : RGBA, ou RGB sans alpha.
constexpr std::uint32_t kRawEncoding = 1;
constexpr std::uint32_t kWaveletEncoding = 3;
constexpr std::int32_t kRgbaBytes = 4;
constexpr std::int32_t kRgbBytes = 3;

std::uint32_t WriteTexture(Section& section, const Types& types, const Gr2TextureSpec& spec) {
  Object texture(section, types.texture);
  texture.Text("FromFileName", spec.file_name);
  texture.Int("Width", spec.width);
  texture.Int("Height", spec.height);
  texture.Int("Encoding", static_cast<std::int32_t>(
                              spec.raw_stride.has_value() ? kRawEncoding : kWaveletEncoding));
  Object layout(section, types.layout, texture.Offset("Layout"));
  // Brute, chaque pixel garde ses quatre octets ; sous l'ondelette, une petite
  // texture sans alpha n'en range que trois.
  const std::int32_t stride = spec.alpha || spec.raw_stride.has_value() ? kRgbaBytes : kRgbBytes;
  layout.Int("BytesPerPixel", kRgbaBytes);
  for (std::size_t lane = 0; lane < 4; ++lane) {
    section.U32(layout.Offset("ShiftForComponent") + static_cast<std::uint32_t>(4 * lane),
                static_cast<std::uint32_t>(8 * lane));
    const std::uint32_t bits = lane == 3 && !spec.alpha ? 0 : 8;
    section.U32(layout.Offset("BitsForComponent") + static_cast<std::uint32_t>(4 * lane), bits);
  }
  std::vector<std::uint8_t> pixels;
  for (std::size_t pixel = 0; pixel * kRgbaBytes < spec.rgba.size(); ++pixel) {
    for (std::int32_t lane = 0; lane < stride; ++lane) {
      pixels.push_back(spec.rgba[pixel * kRgbaBytes + static_cast<std::size_t>(lane)]);
    }
  }
  std::vector<Object> images = Objects(section, types.image, 1);
  std::vector<Object> mips = Objects(section, types.mip, 1);
  const std::uint32_t bytes = section.Allocate(static_cast<std::uint32_t>(pixels.size()));
  for (std::size_t index = 0; index < pixels.size(); ++index) {
    section.U8(bytes + static_cast<std::uint32_t>(index), pixels[index]);
  }
  mips[0].Array("Pixels", pixels.size(), bytes);
  if (spec.raw_stride.has_value()) {
    mips[0].Int("Stride", *spec.raw_stride);
  }
  images[0].Array("MIPLevels", 1, mips[0].at());
  texture.Array("Images", 1, images[0].at());
  return texture.at();
}

std::uint32_t WriteMaterial(Section& section, const Types& types, std::uint32_t texture) {
  // Le matériau du maillage ne porte pas la texture : sa carte diffuse la
  // porte, par un second matériau.
  Object holder(section, types.material);
  holder.Text("Name", "carte");
  holder.Reference("Texture", texture);
  Object material(section, types.material);
  material.Text("Name", "matériau");
  std::vector<Object> maps = Objects(section, types.map, 1);
  maps[0].Text("Usage", "Diffuse Color");
  maps[0].Reference("Map", holder.at());
  material.Array("Maps", 1, maps[0].at());
  return material.at();
}

std::uint32_t WriteMesh(Section& section, const Types& types, const Gr2MeshSpec& spec,
                        std::uint32_t material) {
  Object mesh(section, types.mesh);
  mesh.Text("Name", spec.name);
  Object data(section, types.vertex_data);
  std::vector<Object> vertices = Objects(section, types.vertex, spec.vertices.size());
  for (std::size_t index = 0; index < spec.vertices.size(); ++index) {
    const Gr2VertexSpec& vertex = spec.vertices[index];
    vertices[index].Reals("Position", vertex.position);
    vertices[index].Reals("Normal", vertex.normal);
    vertices[index].Reals("TextureCoordinates0", vertex.uv);
    vertices[index].Bytes("BoneWeights", vertex.bone_weights);
    vertices[index].Bytes("BoneIndices", vertex.bone_indices);
  }
  const std::uint32_t variant = data.Offset("Vertices");
  section.Pointer(variant, types.vertex.offset);
  section.U32(variant + 4, static_cast<std::uint32_t>(spec.vertices.size()));
  if (!vertices.empty()) {
    section.Pointer(variant + 8, vertices.front().at());
  }
  mesh.Reference("PrimaryVertexData", data.at());
  Object topology(section, types.topology);
  std::vector<Object> indices = Objects(section, types.integer, spec.indices.size());
  for (std::size_t index = 0; index < spec.indices.size(); ++index) {
    section.U32(indices[index].at(), spec.indices[index]);
  }
  topology.Array("Indices", indices.size(), indices.empty() ? 0 : indices.front().at());
  mesh.Reference("PrimaryTopology", topology.at());
  std::vector<Object> bones = Objects(section, types.bone_binding, spec.bone_names.size());
  for (std::size_t index = 0; index < bones.size(); ++index) {
    bones[index].Text("BoneName", spec.bone_names[index]);
  }
  mesh.Array("BoneBindings", bones.size(), bones.empty() ? 0 : bones.front().at());
  if (material != 0) {
    std::vector<Object> bindings = Objects(section, types.material_binding, 1);
    bindings[0].Reference("Material", material);
    mesh.Array("MaterialBindings", 1, bindings[0].at());
  }
  return mesh.at();
}

void WriteCurve(Section& section, const Types& types, Object& curve, const Gr2CurveSpec& spec) {
  curve.Int("Degree", spec.degree);
  WriteNumbers(section, curve, "Knots", types.real, spec.knots);
  WriteNumbers(section, curve, "Controls", types.real, spec.controls);
}

std::uint32_t WriteAnimation(Section& section, const Types& types, const Gr2AnimationSpec& spec) {
  Object animation(section, types.animation);
  animation.Text("Name", spec.name);
  const std::array<float, 2> timing = {spec.duration_seconds, 1.0F / 30.0F};
  animation.Reals("Duration", std::span<const float>(timing).first(1));
  animation.Reals("TimeStep", std::span<const float>(timing).subspan(1));
  Object group(section, types.group);
  group.Text("Name", spec.group_name);
  std::vector<Object> tracks = Objects(section, types.track, spec.tracks.size());
  for (std::size_t index = 0; index < tracks.size(); ++index) {
    const Gr2TrackSpec& track = spec.tracks[index];
    tracks[index].Text("Name", track.name);
    Object position(section, types.curve, tracks[index].Offset("PositionCurve"));
    WriteCurve(section, types, position, track.position);
    Object orientation(section, types.curve, tracks[index].Offset("OrientationCurve"));
    WriteCurve(section, types, orientation, track.orientation);
    Object scale(section, types.curve, tracks[index].Offset("ScaleShearCurve"));
    WriteCurve(section, types, scale, track.scale_shear);
  }
  group.Array("TransformTracks", tracks.size(), tracks.empty() ? 0 : tracks.front().at());
  const std::array<std::uint32_t, 1> groups = {group.at()};
  animation.Array("TrackGroups", 1, PointerArray(section, groups));
  return animation.at();
}

// L'en-tête : la signature, puis la révision, la table d'une section et les
// deux racines.
constexpr std::array<std::uint8_t, 16> kMagic = {0xB8, 0x67, 0xB0, 0xCA, 0xF8, 0x6D, 0xB1, 0x0F,
                                                 0x84, 0x72, 0x8C, 0x7E, 0x5E, 0x19, 0x00, 0x1E};
constexpr std::uint32_t kRevision = 6;
constexpr std::uint32_t kHeaderFieldsBytes = 56;
constexpr std::uint32_t kSectionHeaderBytes = 44;
constexpr std::uint32_t kMagicBlockBytes = 32;
constexpr std::uint32_t kTypeTag = 0x8000000FU;

}  // namespace

std::vector<std::uint8_t> BuildGr2(const Gr2Spec& spec) {
  Section section;
  Types types;
  types.Emit(section);
  Object root(section, types.root);

  std::vector<std::uint32_t> textures;
  for (const Gr2TextureSpec& texture : spec.textures) {
    textures.push_back(WriteTexture(section, types, texture));
  }
  root.Array("Textures", textures.size(), PointerArray(section, textures));

  Object model(section, types.model);
  model.Text("Name", spec.model_name);
  Object skeleton(section, types.skeleton);
  skeleton.Text("Name", spec.model_name);
  std::vector<Object> bones = Objects(section, types.bone, spec.bones.size());
  for (std::size_t index = 0; index < bones.size(); ++index) {
    const Gr2BoneSpec& bone = spec.bones[index];
    bones[index].Text("Name", bone.name);
    bones[index].Int("ParentIndex", bone.parent);
    const std::uint32_t transform = bones[index].Offset("Transform");
    const std::array<float, 16> identity = {bone.position[0],
                                            bone.position[1],
                                            bone.position[2],
                                            0,
                                            0,
                                            0,
                                            1,
                                            1,
                                            0,
                                            0,
                                            0,
                                            1,
                                            0,
                                            0,
                                            0,
                                            1};
    for (std::size_t lane = 0; lane < identity.size(); ++lane) {
      section.F32(transform + 4 + static_cast<std::uint32_t>(4 * lane), identity[lane]);
    }
    const std::array<float, 16> inverse = {
        1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -bone.position[0], -bone.position[1], -bone.position[2],
        1};
    bones[index].Reals("InverseWorldTransform", inverse);
  }
  skeleton.Array("Bones", bones.size(), bones.empty() ? 0 : bones.front().at());
  model.Reference("Skeleton", skeleton.at());

  std::vector<std::uint32_t> materials;
  std::vector<Object> bindings = Objects(section, types.mesh_binding, spec.meshes.size());
  for (std::size_t index = 0; index < spec.meshes.size(); ++index) {
    const Gr2MeshSpec& mesh = spec.meshes[index];
    std::uint32_t material = 0;
    for (std::size_t texture = 0; texture < spec.textures.size(); ++texture) {
      if (spec.textures[texture].file_name == mesh.diffuse_texture) {
        material = WriteMaterial(section, types, textures[texture]);
        materials.push_back(material);
        break;
      }
    }
    bindings[index].Reference("Mesh", WriteMesh(section, types, mesh, material));
  }
  model.Array("MeshBindings", bindings.size(), bindings.empty() ? 0 : bindings.front().at());
  root.Array("Materials", materials.size(), PointerArray(section, materials));
  const std::array<std::uint32_t, 1> models = {model.at()};
  root.Array("Models", 1, PointerArray(section, models));

  std::vector<std::uint32_t> animations;
  for (const Gr2AnimationSpec& animation : spec.animations) {
    animations.push_back(WriteAnimation(section, types, animation));
  }
  root.Array("Animations", animations.size(), PointerArray(section, animations));

  const std::vector<std::uint8_t>& raw = section.bytes();
  const auto size = static_cast<std::uint32_t>(raw.size());
  const std::vector<std::uint8_t> packed =
      spec.compress ? CompressGr2Section(raw, {size, size}) : raw;
  const std::uint32_t data_offset = kMagicBlockBytes + kHeaderFieldsBytes + kSectionHeaderBytes;
  const auto fixup_offset = static_cast<std::uint32_t>(data_offset + packed.size());
  const auto fixup_count = static_cast<std::uint32_t>(section.fixups().size());

  std::vector<std::uint8_t> file;
  core::ByteWriter out(file);
  out.Bytes(kMagic);
  out.U32(kHeaderFieldsBytes + kSectionHeaderBytes);
  out.U32(0);
  out.U64(0);
  out.U32(kRevision);
  out.U32(fixup_offset + fixup_count * 12);
  out.U32(0);  // somme de contrôle
  out.U32(kHeaderFieldsBytes);
  out.U32(1);
  out.U32(0);
  out.U32(types.root.offset);
  out.U32(0);
  out.U32(root.at());
  out.U32(kTypeTag);
  while (file.size() < kMagicBlockBytes + kHeaderFieldsBytes) {
    out.U32(0);
  }
  out.U32(spec.compress ? 1 : 0);
  out.U32(data_offset);
  out.U32(static_cast<std::uint32_t>(packed.size()));
  out.U32(size);
  out.U32(4);
  out.U32(size);
  out.U32(size);
  out.U32(fixup_offset);
  out.U32(fixup_count);
  out.U32(fixup_offset + fixup_count * 12);
  out.U32(0);
  out.Bytes(packed);
  for (const auto& [at, target] : section.fixups()) {
    out.U32(at);
    out.U32(0);
    out.U32(target);
  }
  return file;
}

}  // namespace ro::test_support