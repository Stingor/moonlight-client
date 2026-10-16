#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "ro/core/error.h"

namespace ro::formats {

// Fichier .rsm : un modèle 3D du décor — bâtiments, arbres, mobilier.
//
// Le format existe en DEUX familles qui ne se lisent pas pareil, et il faut les
// gérer toutes les deux : le data.grf du serveur contient 7103 fichiers .rsm et
// 2244 .rsm2. N'en gérer qu'une laisserait un quart du décor absent.
//
// Les différences principales, par seuil de version :
//   1.2  les sommets de texture portent une couleur, les faces un groupe de
//        lissage ;
//   1.4  un octet d'opacité globale ;
//   1.6  des images-clés d'échelle ;
//   2.2  les noms sont préfixés de leur longueur au lieu d'occuper 40 octets,
//        les faces sont préfixées de leur taille, les transformations globales
//        disparaissent au profit d'images-clés de position ;
//   2.3  chaque maillage porte SES noms de textures ; la table globale du
//        modèle disparaît.
//
// Référence : GRF/FileFormats/RsmFormat/ de GRFEditor.

struct RsmTextureVertex {
  // Avant la version 1.2 le format n'en porte pas ; on prend le blanc opaque.
  std::uint32_t color = 0xFFFFFFFFU;
  float u = 0.0F;
  float v = 0.0F;
};

struct RsmFace {
  std::array<std::uint16_t, 3> vertex_ids{};
  std::array<std::uint16_t, 3> texture_vertex_ids{};
  std::uint16_t texture_id = 0;
  bool two_sided = false;
  // Un groupe par sommet. Les versions anciennes n'en écrivent qu'un, recopié
  // sur les trois.
  std::array<std::int32_t, 3> smooth_group{};
  // Le mot du fichier derrière `two_sided`, tel quel : le client de la première
  // famille tient pour double toute valeur non nulle, celui de la seconde
  // seulement 1.
  std::int32_t two_sided_word = 0;
};

struct RsmScaleKey {
  std::int32_t frame = 0;
  std::array<float, 3> scale{1.0F, 1.0F, 1.0F};
  float data = 0.0F;
};

struct RsmRotationKey {
  std::int32_t frame = 0;
  // Quaternion, dans l'ordre du fichier.
  std::array<float, 4> quaternion{};
};

struct RsmPositionKey {
  std::int32_t frame = 0;
  std::array<float, 3> position{};
  std::int32_t data = 0;
};

// Une image-clé d'animation de texture, à partir de 2.3 : à l'image `frame`, la
// grandeur que l'animation fait varier vaut `value`.
struct RsmTextureKey {
  std::int32_t frame = 0;
  float value = 0.0F;
};

// Une grandeur de texture animée — un décalage, une échelle ou un angle des
// coordonnées de texture — pour UNE texture du maillage.
struct RsmTextureAnimation {
  // Index dans RsmMesh::textures.
  std::int32_t texture_index = 0;
  // Numéro de la grandeur, tel que le fichier l'écrit. La référence lit 0 et 1
  // comme des décalages, 2 et 3 comme des échelles, 4 comme un angle ; le sens
  // se fixe là où on les applique, pas dans le parseur.
  std::int32_t motion = 0;
  std::vector<RsmTextureKey> keys;
};

struct RsmMesh {
  std::string name;
  std::string parent_name;

  // Jusqu'en 2.2, ce sont des index dans la table de textures du MODÈLE.
  // À partir de 2.3, le maillage porte ses propres noms et les index valent
  // simplement 0..n-1.
  std::vector<std::int32_t> texture_indices;
  std::vector<std::string> textures;

  // Matrice 3 × 3, rangée par rangée.
  std::array<float, 9> transform{};
  std::array<float, 3> local_position{};

  // Absentes à partir de 2.2, où les images-clés de position les remplacent.
  std::array<float, 3> global_position{};
  float global_rotation_angle = 0.0F;
  std::array<float, 3> global_rotation_axis{};
  std::array<float, 3> global_scale{1.0F, 1.0F, 1.0F};

  std::vector<std::array<float, 3>> vertices;
  std::vector<RsmTextureVertex> texture_vertices;
  std::vector<RsmFace> faces;

  std::vector<RsmScaleKey> scale_keys;
  std::vector<RsmRotationKey> rotation_keys;
  std::vector<RsmPositionKey> position_keys;
  // À partir de 2.3 seulement.
  std::vector<RsmTextureAnimation> texture_animations;
};

class RsmModel {
 public:
  [[nodiscard]] static core::Result<RsmModel> Parse(std::span<const std::uint8_t> data);

  [[nodiscard]] std::uint8_t major_version() const noexcept { return major_version_; }
  [[nodiscard]] std::uint8_t minor_version() const noexcept { return minor_version_; }
  [[nodiscard]] std::int32_t version() const noexcept {
    return (static_cast<std::int32_t>(major_version_) << 8) |
           static_cast<std::int32_t>(minor_version_);
  }

  [[nodiscard]] std::int32_t animation_length() const noexcept { return animation_length_; }
  [[nodiscard]] std::int32_t shade_type() const noexcept { return shade_type_; }
  [[nodiscard]] std::uint8_t alpha() const noexcept { return alpha_; }
  [[nodiscard]] float frame_rate() const noexcept { return frame_rate_; }

  // Table de textures du modèle. Vide à partir de la version 2.3, où chaque
  // maillage porte les siennes.
  [[nodiscard]] const std::vector<std::string>& textures() const noexcept { return textures_; }
  [[nodiscard]] const std::vector<std::string>& main_mesh_names() const noexcept {
    return main_mesh_names_;
  }
  [[nodiscard]] const std::vector<RsmMesh>& meshes() const noexcept { return meshes_; }
  // Déplace le sommet `vertex` du nœud de rang `mesh_index`, dans le repère du
  // nœud. Hors des listes, rien.
  void MoveVertex(std::size_t mesh_index, std::size_t vertex,
                  const std::array<float, 3>& position) noexcept;

  // Nombre total de faces, tous maillages confondus — la mesure la plus utile
  // pour juger du coût d'un modèle.
  [[nodiscard]] std::size_t face_count() const noexcept;

 private:
  RsmModel() = default;

  std::vector<std::string> textures_;
  std::vector<std::string> main_mesh_names_;
  std::vector<RsmMesh> meshes_;
  std::int32_t animation_length_ = 0;
  std::int32_t shade_type_ = 0;
  float frame_rate_ = 0.0F;
  std::uint8_t alpha_ = 0xFF;
  std::uint8_t major_version_ = 0;
  std::uint8_t minor_version_ = 0;
};

}  // namespace ro::formats
