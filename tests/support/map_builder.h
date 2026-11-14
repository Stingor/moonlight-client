#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace ro::test_support {

// Fabriques de cartes synthétiques. Même raison d'être que le constructeur
// d'archives : aucun asset Gravity n'est versionné, donc les cas limites
// (dimensions absurdes, section eau absente, version hors plage serveur) se
// fabriquent ici plutôt que de se chercher dans de vraies données.

struct GatCellSpec {
  std::array<float, 4> corner_height{};
  std::int32_t type = 0;
};

[[nodiscard]] std::vector<std::uint8_t> BuildGat(std::int32_t width, std::int32_t height,
                                                 const std::vector<GatCellSpec>& cells,
                                                 std::uint8_t major = 1, std::uint8_t minor = 2);

struct RswModelSpec {
  std::string name;
  std::string model_name;
  std::string node_name;
  std::array<float, 3> position{};
  std::array<float, 3> rotation{};
  std::array<float, 3> scale{1.0F, 1.0F, 1.0F};
  float animation_speed = 1.0F;
};

struct RswSoundSpec {
  std::string name;
  std::string wave_name;
  std::array<float, 3> position{};
  float volume = 1.0F;
  std::int32_t width = 0;
  std::int32_t height = 0;
  float range = 0.0F;
  float cycle_seconds = 0.0F;
};

struct RswBuildOptions {
  std::uint8_t major = 2;
  std::uint8_t minor = 1;
  std::int32_t build_number = 0;
  std::uint8_t render_flag = 1;
  std::string ini_file;
  std::string ground_file = "essai.gnd";
  std::string altitude_file = "essai.gat";
  std::string source_file;
  float water_level = 0.0F;
  std::int32_t water_type = 0;
  float wave_height = 1.0F;
  float wave_speed = 2.0F;
  float wave_pitch = 50.0F;
  std::int32_t texture_cycling = 3;

  // Section lumière, écrite à partir de la version 1.5.
  std::int32_t light_longitude = 45;
  std::int32_t light_latitude = 45;
  std::array<float, 3> light_diffuse{1.0F, 1.0F, 1.0F};
  std::array<float, 3> light_ambient{1.0F, 1.0F, 1.0F};
  float light_shadow_intensity = 0.5F;
  // Faux : le fichier s'arrête avant la lumière. Sert à vérifier qu'un RSW
  // tronqué là reste exploitable pour son en-tête et son eau.
  bool write_light = true;

  // Emprise du sol, écrite à partir de la version 1.6.
  std::int32_t ground_top = -500;
  std::int32_t ground_bottom = 500;
  std::int32_t ground_left = -500;
  std::int32_t ground_right = 500;

  // Liste d'objets. Les lumières, sons et effets sont écrits comme des blocs de
  // zéros de la bonne longueur : ce qui compte est qu'ils soient TRAVERSÉS sans
  // décaler les modèles qui les suivent.
  std::vector<RswModelSpec> models;
  std::int32_t light_object_count = 0;
  std::int32_t sound_object_count = 0;
  // Des sources de son écrites champ par champ, APRÈS les blocs de zéros.
  std::vector<RswSoundSpec> sounds;
  std::int32_t effect_object_count = 0;
  // Écrit un type d'objet inconnu après les autres : le parseur doit s'arrêter
  // là plutôt que de lire n'importe quoi comme un modèle.
  bool write_unknown_object = false;
  bool write_objects = true;
  // Écrits après la liste d'objets, douze réels par nœud dans l'ordre du
  // fichier : le coin haut, le coin bas, la demi-taille, le centre.
  std::vector<std::array<float, 12>> quad_tree;
};

[[nodiscard]] std::vector<std::uint8_t> BuildRsw(const RswBuildOptions& options);

struct GndCubeSpec {
  std::array<float, 4> corner_height{};
  std::int32_t tile_up = -1;
  std::int32_t tile_front = -1;
  std::int32_t tile_side = -1;
};

struct GndBuildOptions {
  std::uint8_t major = 1;
  std::uint8_t minor = 9;
  std::int32_t width = 2;
  std::int32_t height = 2;
  float proportion_ratio = 10.0F;

  std::vector<std::string> texture_paths;
  std::int32_t texture_path_size = 80;

  std::int32_t lightmap_count = 0;
  std::int32_t lightmap_width = 8;
  std::int32_t lightmap_height = 8;
  // Écrit tel quel dans l'en-tête. Le parseur doit l'IGNORER pour calculer la
  // taille des données — d'où l'intérêt de pouvoir y mettre autre chose que 1.
  std::int32_t lightmap_cells = 1;
  // Le texel que portent TOUTES les lightmaps, dans l'ordre du fichier : l'ombre
  // d'abord, puis la couleur en R, G, B. Trois valeurs distinctes révèlent un
  // ordre de canaux inversé, qu'un gris uniforme laisse passer.
  std::uint8_t lightmap_shadow = 0x7F;
  std::array<std::uint8_t, 3> lightmap_rgb = {0x7F, 0x7F, 0x7F};

  std::int32_t tile_count = 0;
  // Index de texture par tuile. Vide : toutes les tuiles pointent la texture 0.
  // Sert à vérifier le regroupement des sommets par texture.
  std::vector<std::int16_t> tile_texture_indices;

  // Couleur par tuile, rangée BLEU, VERT, ROUGE, ALPHA comme dans le fichier.
  // Vide : la même couleur pour toutes.
  std::vector<std::array<std::uint8_t, 4>> tile_colors;

  // Vide : des cubes plats sont générés pour remplir width × height.
  std::vector<GndCubeSpec> cubes;

  // L'eau n'est écrite qu'à partir de la version 1.8.
  std::int32_t water_split_width = 1;
  std::int32_t water_split_height = 1;
  // Vide : une zone unique au niveau 0.
  std::vector<float> water_levels;
};

[[nodiscard]] std::vector<std::uint8_t> BuildGnd(const GndBuildOptions& options);

}  // namespace ro::test_support
