#include "support/model_builder.h"

#include <algorithm>
#include <bit>
#include <string_view>

namespace ro::test_support {
namespace {

void AppendU16(std::vector<std::uint8_t>& out, std::uint16_t value) {
  out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
  out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFU));
}

void AppendU32(std::vector<std::uint8_t>& out, std::uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8) {
    out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFU));
  }
}

void AppendI32(std::vector<std::uint8_t>& out, std::int32_t value) {
  AppendU32(out, static_cast<std::uint32_t>(value));
}

void AppendF32(std::vector<std::uint8_t>& out, float value) {
  AppendU32(out, std::bit_cast<std::uint32_t>(value));
}

void AppendVector3(std::vector<std::uint8_t>& out, const std::array<float, 3>& value) {
  for (const float component : value) {
    AppendF32(out, component);
  }
}

void AppendFixed(std::vector<std::uint8_t>& out, const std::string& text, std::size_t width) {
  for (std::size_t index = 0; index < width; ++index) {
    out.push_back(index < text.size() ? static_cast<std::uint8_t>(text[index])
                                      : static_cast<std::uint8_t>(0));
  }
}

// Nom préfixé de sa longueur, à partir de la version 2.2. Le NUL final est
// compté dans la longueur, comme dans les fichiers réels.
void AppendPrefixed(std::vector<std::uint8_t>& out, const std::string& text) {
  AppendI32(out, static_cast<std::int32_t>(text.size() + 1));
  out.insert(out.end(), text.begin(), text.end());
  out.push_back(0);
}

void AppendName(std::vector<std::uint8_t>& out, const std::string& text, std::int32_t version) {
  if (version >= 0x0202) {
    AppendPrefixed(out, text);
  } else {
    AppendFixed(out, text, 40);
  }
}

void AppendMesh(std::vector<std::uint8_t>& out, const RsmMeshSpec& mesh, std::int32_t version) {
  AppendName(out, mesh.name, version);
  AppendName(out, mesh.parent_name, version);

  if (version >= 0x0203) {
    AppendI32(out, static_cast<std::int32_t>(mesh.textures.size()));
    for (const std::string& texture : mesh.textures) {
      AppendPrefixed(out, texture);
    }
  } else {
    AppendI32(out, static_cast<std::int32_t>(mesh.texture_indices.size()));
    for (const std::int32_t index : mesh.texture_indices) {
      AppendI32(out, index);
    }
  }

  for (const float component : mesh.transform) {
    AppendF32(out, component);
  }
  AppendVector3(out, mesh.local_position);

  if (version < 0x0202) {
    AppendVector3(out, mesh.global_position);
    AppendF32(out, mesh.global_rotation_angle);
    AppendVector3(out, mesh.global_rotation_axis);
    AppendVector3(out, mesh.global_scale);
  }

  AppendI32(out, static_cast<std::int32_t>(mesh.vertices.size()));
  for (const auto& vertex : mesh.vertices) {
    AppendVector3(out, vertex);
  }

  AppendI32(out, static_cast<std::int32_t>(mesh.texture_vertices.size()));
  for (std::size_t index = 0; index < mesh.texture_vertices.size(); ++index) {
    if (version >= 0x0102) {
      AppendU32(out, index < mesh.texture_vertex_colors.size() ? mesh.texture_vertex_colors[index]
                                                               : 0xFFFFFFFFU);
    }
    AppendF32(out, mesh.texture_vertices[index][0]);
    AppendF32(out, mesh.texture_vertices[index][1]);
  }

  AppendI32(out, static_cast<std::int32_t>(mesh.faces.size()));
  for (const RsmFaceSpec& face : mesh.faces) {
    if (version >= 0x0202) {
      // Longueur déclarée : jusqu'au premier groupe de lissage inclus.
      AppendI32(out, 24);
    }
    for (const std::uint16_t vertex : face.vertex_ids) {
      AppendU16(out, vertex);
    }
    for (const std::uint16_t vertex : face.texture_vertex_ids) {
      AppendU16(out, vertex);
    }
    AppendU16(out, face.texture_id);
    AppendU16(out, 0);  // bourrage
    AppendI32(out, face.two_sided_word.value_or(face.two_sided ? 1 : 0));
    if (version >= 0x0102) {
      AppendI32(out, face.smooth_group);
    }
  }

  if (version >= 0x0106) {
    AppendI32(out, static_cast<std::int32_t>(mesh.scale_keys.size()));
    for (const RsmScaleKeySpec& key : mesh.scale_keys) {
      AppendI32(out, key.frame);
      AppendVector3(out, key.scale);
      AppendF32(out, 0.0F);
    }
  }

  AppendI32(out, static_cast<std::int32_t>(mesh.rotation_keys.size()));
  for (const RsmRotationKeySpec& key : mesh.rotation_keys) {
    AppendI32(out, key.frame);
    for (const float component : key.quaternion) {
      AppendF32(out, component);
    }
  }

  if (version >= 0x0202) {
    AppendI32(out, static_cast<std::int32_t>(mesh.position_keys.size()));
    for (const RsmPositionKeySpec& key : mesh.position_keys) {
      AppendI32(out, key.frame);
      AppendVector3(out, key.position);
      AppendI32(out, 0);
    }
  }

  if (version >= 0x0203) {
    // Le fichier regroupe par texture : l'ordre de première apparition suffit.
    std::vector<std::int32_t> textures;
    for (const RsmTextureAnimationSpec& animation : mesh.texture_animations) {
      if (std::find(textures.begin(), textures.end(), animation.texture_index) == textures.end()) {
        textures.push_back(animation.texture_index);
      }
    }
    AppendI32(out, static_cast<std::int32_t>(textures.size()));
    for (const std::int32_t texture : textures) {
      AppendI32(out, texture);
      const auto count = std::count_if(
          mesh.texture_animations.begin(), mesh.texture_animations.end(),
          [texture](const RsmTextureAnimationSpec& spec) { return spec.texture_index == texture; });
      AppendI32(out, static_cast<std::int32_t>(count));
      for (const RsmTextureAnimationSpec& animation : mesh.texture_animations) {
        if (animation.texture_index != texture) {
          continue;
        }
        AppendI32(out, animation.motion);
        AppendI32(out, static_cast<std::int32_t>(animation.keys.size()));
        for (const RsmTextureKeySpec& key : animation.keys) {
          AppendI32(out, key.frame);
          AppendF32(out, key.value);
        }
      }
    }
  }
}

}  // namespace

std::vector<std::uint8_t> BuildRsm(const RsmBuildOptions& options) {
  std::vector<std::uint8_t> rsm;
  const std::string_view signature = options.use_grsx_signature ? "GRSX" : "GRSM";
  rsm.insert(rsm.end(), signature.begin(), signature.end());
  rsm.push_back(options.major);
  rsm.push_back(options.minor);

  const std::int32_t version =
      (static_cast<std::int32_t>(options.major) << 8) | static_cast<std::int32_t>(options.minor);

  AppendI32(rsm, options.animation_length);
  AppendI32(rsm, options.shade_type);
  if (version >= 0x0104) {
    rsm.push_back(options.alpha);
  }

  if (version >= 0x0203) {
    AppendF32(rsm, options.frame_rate);
    AppendI32(rsm, static_cast<std::int32_t>(options.main_mesh_names.size()));
    for (const std::string& name : options.main_mesh_names) {
      AppendPrefixed(rsm, name);
    }
  } else if (version >= 0x0202) {
    AppendF32(rsm, options.frame_rate);
    AppendI32(rsm, static_cast<std::int32_t>(options.textures.size()));
    for (const std::string& texture : options.textures) {
      AppendPrefixed(rsm, texture);
    }
    AppendI32(rsm, static_cast<std::int32_t>(options.main_mesh_names.size()));
    for (const std::string& name : options.main_mesh_names) {
      AppendPrefixed(rsm, name);
    }
  } else {
    rsm.insert(rsm.end(), 16, 0);  // octets réservés
    AppendI32(rsm, static_cast<std::int32_t>(options.textures.size()));
    for (const std::string& texture : options.textures) {
      AppendFixed(rsm, texture, 40);
    }
    AppendFixed(rsm, options.main_mesh_names.empty() ? std::string{} : options.main_mesh_names[0],
                40);
  }

  AppendI32(rsm, static_cast<std::int32_t>(options.meshes.size()));
  for (const RsmMeshSpec& mesh : options.meshes) {
    AppendMesh(rsm, mesh, version);
  }

  return rsm;
}

}  // namespace ro::test_support
