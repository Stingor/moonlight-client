#include "ro/formats/rsm.h"

#include <string_view>
#include <utility>

#include "ro/core/byte_reader.h"

#include "read_floats.h"

namespace ro::formats {
namespace {

using core::ByteReader;
using core::ErrorCode;
using core::Fail;

// Deux signatures existent. GRSX apparaît sur quelques modèles et se lit
// exactement comme GRSM.
constexpr std::string_view kSignatureGrsm{"GRSM", 4};
constexpr std::string_view kSignatureGrsx{"GRSX", 4};

// Seuils de version, en 0xMMmm.
constexpr std::int32_t kVersionVertexColor = 0x0102;
constexpr std::int32_t kVersionAlpha = 0x0104;
constexpr std::int32_t kVersionScaleKeys = 0x0106;
constexpr std::int32_t kVersionLengthPrefixedNames = 0x0202;
constexpr std::int32_t kVersionPerMeshTextures = 0x0203;

// Longueur des noms à taille fixe, avant la version 2.2.
constexpr std::size_t kFixedNameLength = 40;
// Octets réservés de l'en-tête, avant la version 2.2.
constexpr std::size_t kReservedLength = 16;
// Taille d'une face jusqu'aux trois groupes de lissage inclus. Au-delà, le
// préfixe de longueur dit combien d'octets supplémentaires sauter.
constexpr std::int32_t kFaceSizeWithoutSmoothGroups = 24;
constexpr std::int32_t kFaceSizeWithSecondGroup = 28;
constexpr std::int32_t kFaceSizeWithThirdGroup = 32;

// Bornes de cohérence, confrontées aux valeurs annoncées AVANT toute
// allocation. Le plus lourd modèle officiel reste très en dessous.
constexpr std::int32_t kMaxCount = 1 << 20;
constexpr std::int32_t kMaxNameLength = 1 << 16;

// Lit un compte et refuse d'emblée une valeur qui ne peut pas être vraie.
// Rendre -1 laisse l'appelant s'arrêter sans avoir à répéter le test.
std::int32_t ReadCount(ByteReader& reader) {
  const std::int32_t count = reader.I32();
  if (!reader.ok() || count < 0 || count > kMaxCount) {
    reader.Invalidate();
    return -1;
  }
  return count;
}

// Nom préfixé de sa longueur, à partir de la version 2.2.
std::string ReadPrefixedName(ByteReader& reader) {
  const std::int32_t length = reader.I32();
  if (!reader.ok() || length < 0 || length > kMaxNameLength) {
    reader.Invalidate();
    return {};
  }
  return std::string(reader.FixedString(static_cast<std::size_t>(length)));
}

std::string ReadName(ByteReader& reader, std::int32_t version) {
  return version >= kVersionLengthPrefixedNames ? ReadPrefixedName(reader)
                                                : std::string(reader.FixedString(kFixedNameLength));
}

void ReadFaces(ByteReader& reader, std::int32_t version, std::size_t texture_count, RsmMesh& mesh) {
  const std::int32_t count = ReadCount(reader);
  if (count < 0) {
    return;
  }
  mesh.faces.reserve(reader.AtMostRemaining(static_cast<std::size_t>(count)));

  for (std::int32_t index = 0; index < count; ++index) {
    // À partir de 2.2 la face annonce sa longueur : c'est elle qui dit si les
    // deuxième et troisième groupes de lissage sont présents, et combien
    // d'octets inconnus la suivent.
    std::int32_t declared_length = -1;
    if (version >= kVersionLengthPrefixedNames) {
      declared_length = reader.I32();
    }

    RsmFace face;
    for (std::uint16_t& vertex : face.vertex_ids) {
      vertex = reader.U16();
    }
    for (std::uint16_t& vertex : face.texture_vertex_ids) {
      vertex = reader.U16();
    }
    face.texture_id = reader.U16();
    // Un index de texture hors table est ramené à zéro plutôt que de faire
    // échouer le modèle : le décor s'affiche avec une texture inattendue au
    // lieu de disparaître.
    if (texture_count == 0 || face.texture_id >= texture_count) {
      face.texture_id = 0;
    }
    reader.Skip(2);  // bourrage
    face.two_sided_word = reader.I32();
    face.two_sided = face.two_sided_word > 0;

    if (version >= kVersionVertexColor) {
      const std::int32_t group = reader.I32();
      face.smooth_group = {group, group, group};
      if (declared_length > kFaceSizeWithoutSmoothGroups) {
        face.smooth_group[1] = reader.I32();
      }
      if (declared_length > kFaceSizeWithSecondGroup) {
        face.smooth_group[2] = reader.I32();
      }
      if (declared_length > kFaceSizeWithThirdGroup) {
        reader.Skip(static_cast<std::size_t>(declared_length - kFaceSizeWithThirdGroup));
      }
    }

    if (!reader.ok()) {
      return;
    }
    mesh.faces.push_back(face);
  }
}

void ReadKeyFrames(ByteReader& reader, std::int32_t version, RsmMesh& mesh) {
  if (version >= kVersionScaleKeys) {
    const std::int32_t count = ReadCount(reader);
    if (count < 0) {
      return;
    }
    mesh.scale_keys.reserve(reader.AtMostRemaining(static_cast<std::size_t>(count)));
    for (std::int32_t index = 0; index < count; ++index) {
      RsmScaleKey key;
      key.frame = reader.I32();
      key.scale = ReadVector3(reader);
      key.data = reader.F32();
      mesh.scale_keys.push_back(key);
    }
  }

  const std::int32_t rotation_count = ReadCount(reader);
  if (rotation_count < 0) {
    return;
  }
  mesh.rotation_keys.reserve(reader.AtMostRemaining(static_cast<std::size_t>(rotation_count)));
  for (std::int32_t index = 0; index < rotation_count; ++index) {
    RsmRotationKey key;
    key.frame = reader.I32();
    ReadFloats(reader, key.quaternion);
    mesh.rotation_keys.push_back(key);
  }

  if (version >= kVersionLengthPrefixedNames) {
    const std::int32_t count = ReadCount(reader);
    if (count < 0) {
      return;
    }
    mesh.position_keys.reserve(reader.AtMostRemaining(static_cast<std::size_t>(count)));
    for (std::int32_t index = 0; index < count; ++index) {
      RsmPositionKey key;
      key.frame = reader.I32();
      key.position = ReadVector3(reader);
      key.data = reader.I32();
      mesh.position_keys.push_back(key);
    }
  }

  // Animations de texture, à partir de 2.3 : pour chaque texture animée, une
  // liste de grandeurs, et pour chacune ses images-clés.
  if (version >= kVersionPerMeshTextures) {
    const std::int32_t textures = ReadCount(reader);
    if (textures < 0) {
      return;
    }
    for (std::int32_t index = 0; index < textures; ++index) {
      const std::int32_t texture_index = reader.I32();
      const std::int32_t animations = ReadCount(reader);
      if (animations < 0) {
        return;
      }
      for (std::int32_t animation = 0; animation < animations; ++animation) {
        RsmTextureAnimation motion;
        motion.texture_index = texture_index;
        motion.motion = reader.I32();
        const std::int32_t frames = ReadCount(reader);
        if (frames < 0) {
          return;
        }
        motion.keys.reserve(reader.AtMostRemaining(static_cast<std::size_t>(frames)));
        for (std::int32_t frame = 0; frame < frames; ++frame) {
          RsmTextureKey key;
          key.frame = reader.I32();
          key.value = reader.F32();
          motion.keys.push_back(key);
        }
        if (!reader.ok()) {
          return;
        }
        mesh.texture_animations.push_back(std::move(motion));
      }
    }
  }
}

bool ReadMesh(ByteReader& reader, std::int32_t version, std::size_t model_texture_count,
              RsmMesh& mesh) {
  mesh.name = ReadName(reader, version);
  mesh.parent_name = ReadName(reader, version);
  if (!reader.ok()) {
    return false;
  }

  std::size_t texture_count = 0;
  if (version >= kVersionPerMeshTextures) {
    const std::int32_t count = ReadCount(reader);
    if (count < 0) {
      return false;
    }
    mesh.textures.reserve(reader.AtMostRemaining(static_cast<std::size_t>(count)));
    mesh.texture_indices.reserve(reader.AtMostRemaining(static_cast<std::size_t>(count)));
    for (std::int32_t index = 0; index < count; ++index) {
      mesh.textures.push_back(ReadPrefixedName(reader));
      mesh.texture_indices.push_back(index);
    }
    texture_count = mesh.textures.size();
  } else {
    const std::int32_t count = ReadCount(reader);
    if (count < 0) {
      return false;
    }
    mesh.texture_indices.reserve(reader.AtMostRemaining(static_cast<std::size_t>(count)));
    for (std::int32_t index = 0; index < count; ++index) {
      mesh.texture_indices.push_back(reader.I32());
    }
    texture_count = mesh.texture_indices.size();
  }

  ReadFloats(reader, mesh.transform);
  mesh.local_position = ReadVector3(reader);

  // À partir de 2.2 la transformation globale disparaît : les images-clés de
  // position la remplacent.
  if (version < kVersionLengthPrefixedNames) {
    mesh.global_position = ReadVector3(reader);
    mesh.global_rotation_angle = reader.F32();
    mesh.global_rotation_axis = ReadVector3(reader);
    mesh.global_scale = ReadVector3(reader);
  }

  const std::int32_t vertex_count = ReadCount(reader);
  if (vertex_count < 0) {
    return false;
  }
  mesh.vertices.reserve(reader.AtMostRemaining(static_cast<std::size_t>(vertex_count)));
  for (std::int32_t index = 0; index < vertex_count; ++index) {
    mesh.vertices.push_back(ReadVector3(reader));
  }

  const std::int32_t texture_vertex_count = ReadCount(reader);
  if (texture_vertex_count < 0) {
    return false;
  }
  mesh.texture_vertices.reserve(
      reader.AtMostRemaining(static_cast<std::size_t>(texture_vertex_count)));
  for (std::int32_t index = 0; index < texture_vertex_count; ++index) {
    RsmTextureVertex vertex;
    if (version >= kVersionVertexColor) {
      vertex.color = reader.U32();
    }
    vertex.u = reader.F32();
    vertex.v = reader.F32();
    mesh.texture_vertices.push_back(vertex);
  }

  ReadFaces(reader, version, texture_count > 0 ? texture_count : model_texture_count, mesh);
  ReadKeyFrames(reader, version, mesh);
  return reader.ok();
}

}  // namespace

std::size_t RsmModel::face_count() const noexcept {
  std::size_t total = 0;
  for (const RsmMesh& mesh : meshes_) {
    total += mesh.faces.size();
  }
  return total;
}

void RsmModel::MoveVertex(std::size_t mesh_index, std::size_t vertex,
                          const std::array<float, 3>& position) noexcept {
  if (mesh_index < meshes_.size() && vertex < meshes_[mesh_index].vertices.size()) {
    meshes_[mesh_index].vertices[vertex] = position;
  }
}

core::Result<RsmModel> RsmModel::Parse(std::span<const std::uint8_t> data) {
  ByteReader reader(data);

  const std::span<const std::uint8_t> signature_bytes = reader.Bytes(kSignatureGrsm.size());
  const std::string_view signature(reinterpret_cast<const char*>(signature_bytes.data()),
                                   signature_bytes.size());
  if (signature != kSignatureGrsm && signature != kSignatureGrsx) {
    return Fail(ErrorCode::kBadSignature, "en-tête RSM attendu « GRSM » ou « GRSX »");
  }

  RsmModel model;
  model.major_version_ = reader.U8();
  model.minor_version_ = reader.U8();
  model.animation_length_ = reader.I32();
  model.shade_type_ = reader.I32();
  if (!reader.ok()) {
    return Fail(ErrorCode::kUnexpectedEnd, "en-tête RSM tronqué");
  }

  const std::int32_t version = model.version();
  if (version >= kVersionAlpha) {
    model.alpha_ = reader.U8();
  }

  std::int32_t mesh_count = 0;
  if (version >= kVersionPerMeshTextures) {
    // Les textures ont migré dans les maillages ; le modèle ne garde que les
    // noms de ses maillages principaux.
    model.frame_rate_ = reader.F32();
    const std::int32_t names = ReadCount(reader);
    if (names < 0) {
      return Fail(ErrorCode::kCorruptData, "nombre de maillages principaux invalide");
    }
    for (std::int32_t index = 0; index < names; ++index) {
      model.main_mesh_names_.push_back(ReadPrefixedName(reader));
    }
    mesh_count = ReadCount(reader);
  } else if (version >= kVersionLengthPrefixedNames) {
    model.frame_rate_ = reader.F32();
    const std::int32_t textures = ReadCount(reader);
    if (textures < 0) {
      return Fail(ErrorCode::kCorruptData, "nombre de textures invalide");
    }
    for (std::int32_t index = 0; index < textures; ++index) {
      model.textures_.push_back(ReadPrefixedName(reader));
    }
    const std::int32_t names = ReadCount(reader);
    if (names < 0) {
      return Fail(ErrorCode::kCorruptData, "nombre de maillages principaux invalide");
    }
    for (std::int32_t index = 0; index < names; ++index) {
      model.main_mesh_names_.push_back(ReadPrefixedName(reader));
    }
    mesh_count = ReadCount(reader);
  } else {
    reader.Skip(kReservedLength);
    const std::int32_t textures = ReadCount(reader);
    if (textures < 0) {
      return Fail(ErrorCode::kCorruptData, "nombre de textures invalide");
    }
    for (std::int32_t index = 0; index < textures; ++index) {
      model.textures_.emplace_back(reader.FixedString(kFixedNameLength));
    }
    model.main_mesh_names_.emplace_back(reader.FixedString(kFixedNameLength));
    mesh_count = ReadCount(reader);
  }

  if (mesh_count <= 0) {
    return Fail(ErrorCode::kCorruptData, "RSM sans maillage exploitable");
  }

  model.meshes_.reserve(reader.AtMostRemaining(static_cast<std::size_t>(mesh_count)));
  for (std::int32_t index = 0; index < mesh_count; ++index) {
    RsmMesh mesh;
    if (!ReadMesh(reader, version, model.textures_.size(), mesh)) {
      // Un modèle tronqué garde ses maillages complets : un bâtiment auquel il
      // manque un auvent reste préférable à une carte sans bâtiment.
      break;
    }
    model.meshes_.push_back(std::move(mesh));
  }

  if (model.meshes_.empty()) {
    return Fail(ErrorCode::kUnexpectedEnd, "aucun maillage RSM lisible");
  }
  return model;
}

}  // namespace ro::formats
