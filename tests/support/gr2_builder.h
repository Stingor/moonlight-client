#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ro/formats/gr2_arith.h"

namespace ro::test_support {

// FABRIQUE DE FICHIERS GRANNY 2 SYNTHÉTIQUES, en mémoire : un conteneur d'une
// section, l'arborescence d'un modèle et la description de ses types. Aucun
// fichier du jeu n'entre dans les essais.

struct Gr2BoneSpec {
  std::string name;
  std::int32_t parent = -1;
  std::array<float, 3> position{0.0F, 0.0F, 0.0F};
};

// Une texture de 256 pixels au plus, rangée brute comme le format le prévoit.
struct Gr2TextureSpec {
  std::string file_name;
  std::int32_t width = 0;
  std::int32_t height = 0;
  bool alpha = true;
  // RGBA ; sans alpha, le quatrième octet de chaque pixel est ignoré.
  std::vector<std::uint8_t> rgba;
  // Posé, la texture s'écrit BRUTE — quatre octets par pixel, tels que `rgba`
  // les donne — sous ce pas de ligne, en octets. Zéro laisse le lecteur le
  // déduire de la largeur.
  std::optional<std::int32_t> raw_stride;
};

struct Gr2VertexSpec {
  std::array<float, 3> position{0.0F, 0.0F, 0.0F};
  std::array<float, 3> normal{0.0F, 0.0F, 1.0F};
  std::array<float, 2> uv{0.0F, 0.0F};
  std::array<std::uint8_t, 4> bone_indices{0, 0, 0, 0};
  std::array<std::uint8_t, 4> bone_weights{255, 0, 0, 0};
};

struct Gr2MeshSpec {
  std::string name = "maillage";
  std::vector<std::string> bone_names;
  // Le nom de fichier de la texture diffuse ; vide, le maillage n'a pas de
  // matériau.
  std::string diffuse_texture;
  std::vector<Gr2VertexSpec> vertices;
  std::vector<std::uint32_t> indices;
};

struct Gr2CurveSpec {
  std::int32_t degree = 0;
  std::vector<float> knots;
  std::vector<float> controls;
};

struct Gr2TrackSpec {
  std::string name;
  Gr2CurveSpec position;
  Gr2CurveSpec orientation;
  Gr2CurveSpec scale_shear;
};

struct Gr2AnimationSpec {
  std::string name = "animation";
  float duration_seconds = 1.0F;
  std::string group_name = "Dummy01";
  std::vector<Gr2TrackSpec> tracks;
};

struct Gr2Spec {
  std::string model_name = "Dummy01";
  std::vector<Gr2BoneSpec> bones;
  std::vector<Gr2TextureSpec> textures;
  std::vector<Gr2MeshSpec> meshes;
  std::vector<Gr2AnimationSpec> animations;
  // Vrai : la section passe par le codage par dictionnaire.
  bool compress = false;
};

[[nodiscard]] std::vector<std::uint8_t> BuildGr2(const Gr2Spec& spec);

// L'ENCODEUR D'INTERVALLE, symétrique du décodeur : les bits sortent dans
// l'ordre où le décodeur les lira.
class Gr2RangeEncoder {
 public:
  void Encode(const formats::Gr2Interval& interval);
  void Uniform(std::uint32_t value, std::uint32_t count) { Encode({value, 1, count}); }
  // Un symbole du modèle ; neuf, sa valeur suit, uniforme dans [0, count).
  void Symbol(formats::Gr2AdaptiveModel& model, std::uint16_t value, std::uint32_t escape_count);
  // Clôt le flux : ce qui suit ses derniers bits n'y change plus rien.
  [[nodiscard]] std::vector<std::uint8_t> Finish();

 private:
  void Emit(std::uint32_t bit);
  void EmitWithPending(std::uint32_t bit);

  std::uint32_t low_ = 0;
  std::uint32_t high_ = 0x7FFFFFFFU;
  std::uint32_t pending_ = 0;
  std::vector<std::uint8_t> bytes_;
  std::uint64_t bit_count_ = 0;
};

// Compresse une section comme le fait le format, avec des littéraux et des
// copies trouvées naïvement. `stops` découpe la sortie en trois plages.
[[nodiscard]] std::vector<std::uint8_t> CompressGr2Section(
    std::span<const std::uint8_t> data, const std::array<std::uint32_t, 2>& stops);

// Une texture UNIE codée par ondelettes : dans chaque couche, la bande
// grossière vaut `coarse[couche]` et tous les détails sont nuls. Trois
// couches, ou quatre avec l'alpha.
[[nodiscard]] std::vector<std::uint8_t> EncodeFlatGr2Texture(std::uint32_t height,
                                                             std::span<const std::int16_t> coarse);

}  // namespace ro::test_support
