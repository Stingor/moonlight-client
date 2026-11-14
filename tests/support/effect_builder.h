#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace ro::test_support {

// Fabrique d'effets STR synthétiques. Même raison d'être que les autres : aucun
// asset Gravity n'est versionné, et les cas limites — plus d'images clés que le
// fichier n'en porte, un mot laissé à la place de la dernière image, un délai
// non numérique — se fabriquent ici.

struct StrKeyFrameSpec {
  std::int32_t frame_index = 0;
  std::int32_t type = 0;
  float offset_x = 0.0F;
  float offset_y = 0.0F;
  std::array<float, 8> uv = {0.0F, 0.0F, 1.0F, 1.0F, 0.0F, 0.0F, 1.0F, 1.0F};
  std::array<float, 8> corners = {-64.0F, 64.0F, 64.0F, -64.0F, -64.0F, -64.0F, 64.0F, 64.0F};
  float texture_index = 0.0F;
  std::int32_t animation = 0;
  float delay = 0.0F;
  // En 1024e de tour, comme le fichier l'écrit.
  float angle_units = 0.0F;
  std::array<float, 4> color = {255.0F, 255.0F, 255.0F, 255.0F};
  std::int32_t blend_source = 5;
  std::int32_t blend_destination = 7;
  std::int32_t multi_texture = 0;
};

struct StrLayerSpec {
  std::vector<std::string> texture_names;
  std::vector<StrKeyFrameSpec> key_frames;
};

struct StrBuildOptions {
  std::uint8_t major = 148;
  std::uint8_t minor = 0;
  std::int32_t frames_per_second = 60;
  std::int32_t last_frame = 0;
  std::vector<StrLayerSpec> layers;
  // Écrit un nombre de couches qui ne correspond PAS à celles qui suivent : de
  // quoi vérifier qu'un fichier tronqué se refuse au lieu de se deviner.
  std::int32_t declared_layers = -1;
};

[[nodiscard]] std::vector<std::uint8_t> BuildStr(const StrBuildOptions& options);

}  // namespace ro::test_support
