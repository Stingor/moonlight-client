#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ro::test_support {

// Fabrique de modèles RSM synthétiques.
//
// Le format existe en deux familles qui ne se lisent pas pareil ; le
// constructeur écrit l'une ou l'autre selon la version demandée, ce qui permet
// de vérifier les deux chemins sans posséder de fichier réel.

struct RsmFaceSpec {
  std::array<std::uint16_t, 3> vertex_ids{};
  std::array<std::uint16_t, 3> texture_vertex_ids{};
  std::uint16_t texture_id = 0;
  bool two_sided = false;
  std::int32_t smooth_group = 0;
  // Le mot écrit au fichier quand on le veut autre que 0 ou 1.
  std::optional<std::int32_t> two_sided_word;
};

struct RsmScaleKeySpec {
  std::int32_t frame = 0;
  std::array<float, 3> scale{1.0F, 1.0F, 1.0F};
};

struct RsmRotationKeySpec {
  std::int32_t frame = 0;
  // x, y, z, w : l'ordre du fichier.
  std::array<float, 4> quaternion{0.0F, 0.0F, 0.0F, 1.0F};
};

struct RsmPositionKeySpec {
  std::int32_t frame = 0;
  std::array<float, 3> position{};
};

struct RsmTextureKeySpec {
  std::int32_t frame = 0;
  float value = 0.0F;
};

// Une grandeur animée d'une texture ; les animations d'une même texture
// s'écrivent regroupées, comme dans le fichier.
struct RsmTextureAnimationSpec {
  std::int32_t texture_index = 0;
  std::int32_t motion = 0;
  std::vector<RsmTextureKeySpec> keys;
};

struct RsmMeshSpec {
  std::string name = "racine";
  std::string parent_name;
  // Avant la 2.3 : index dans la table du modèle. À partir de la 2.3 : noms.
  std::vector<std::int32_t> texture_indices;
  std::vector<std::string> textures;
  std::array<float, 9> transform{1.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F};
  std::array<float, 3> local_position{};
  std::array<float, 3> global_position{};
  float global_rotation_angle = 0.0F;
  std::array<float, 3> global_rotation_axis{};
  std::array<float, 3> global_scale{1.0F, 1.0F, 1.0F};
  std::vector<std::array<float, 3>> vertices;
  // Couleur, u, v — la couleur n'est écrite qu'à partir de la version 1.2.
  std::vector<std::array<float, 2>> texture_vertices;
  std::vector<std::uint32_t> texture_vertex_colors;
  std::vector<RsmFaceSpec> faces;
  // Chaque liste n'est écrite qu'aux versions qui la portent.
  std::vector<RsmScaleKeySpec> scale_keys;
  std::vector<RsmRotationKeySpec> rotation_keys;
  std::vector<RsmPositionKeySpec> position_keys;
  std::vector<RsmTextureAnimationSpec> texture_animations;
};

struct RsmBuildOptions {
  std::uint8_t major = 1;
  std::uint8_t minor = 4;
  // « GRSX » se lit exactement comme « GRSM ».
  bool use_grsx_signature = false;
  std::int32_t animation_length = 0;
  std::int32_t shade_type = 0;
  std::uint8_t alpha = 0xFF;
  float frame_rate = 60.0F;
  // Table de textures du modèle, ignorée à partir de la version 2.3.
  std::vector<std::string> textures;
  std::vector<std::string> main_mesh_names;
  std::vector<RsmMeshSpec> meshes;
};

[[nodiscard]] std::vector<std::uint8_t> BuildRsm(const RsmBuildOptions& options);

}  // namespace ro::test_support
