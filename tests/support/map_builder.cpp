#include "support/map_builder.h"

#include <bit>
#include <cstring>

namespace ro::test_support {
namespace {

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

void AppendText(std::vector<std::uint8_t>& out, const std::string& text, std::size_t width) {
  for (std::size_t index = 0; index < width; ++index) {
    out.push_back(index < text.size() ? static_cast<std::uint8_t>(text[index])
                                      : static_cast<std::uint8_t>(0));
  }
}

void AppendSignature(std::vector<std::uint8_t>& out, std::string_view signature) {
  out.insert(out.end(), signature.begin(), signature.end());
}

constexpr std::size_t kRswFileNameLength = 40;

}  // namespace

std::vector<std::uint8_t> BuildGat(std::int32_t width, std::int32_t height,
                                   const std::vector<GatCellSpec>& cells, std::uint8_t major,
                                   std::uint8_t minor) {
  std::vector<std::uint8_t> gat;
  AppendSignature(gat, "GRAT");
  gat.push_back(major);
  gat.push_back(minor);
  AppendI32(gat, width);
  AppendI32(gat, height);

  for (const GatCellSpec& cell : cells) {
    for (const float corner : cell.corner_height) {
      AppendF32(gat, corner);
    }
    AppendI32(gat, cell.type);
  }
  return gat;
}

std::vector<std::uint8_t> BuildRsw(const RswBuildOptions& options) {
  std::vector<std::uint8_t> rsw;
  AppendSignature(rsw, "GRSW");
  rsw.push_back(options.major);
  rsw.push_back(options.minor);

  const std::int32_t version =
      (static_cast<std::int32_t>(options.major) << 8) | static_cast<std::int32_t>(options.minor);

  if (version >= 0x0205) {
    AppendI32(rsw, options.build_number);
  }
  if (version >= 0x0202) {
    rsw.push_back(options.render_flag);
  }

  AppendText(rsw, options.ini_file, kRswFileNameLength);
  AppendText(rsw, options.ground_file, kRswFileNameLength);
  if (version >= 0x0104) {
    AppendText(rsw, options.altitude_file, kRswFileNameLength);
  }
  AppendText(rsw, options.source_file, kRswFileNameLength);

  if (version < 0x0206 && version >= 0x0103) {
    AppendF32(rsw, options.water_level);
    if (version >= 0x0108) {
      AppendI32(rsw, options.water_type);
      AppendF32(rsw, options.wave_height);
      AppendF32(rsw, options.wave_speed);
      AppendF32(rsw, options.wave_pitch);
    }
    if (version >= 0x0109) {
      AppendI32(rsw, options.texture_cycling);
    }
  }

  if (version >= 0x0105 && options.write_light) {
    AppendI32(rsw, options.light_longitude);
    AppendI32(rsw, options.light_latitude);
    for (const float channel : options.light_diffuse) {
      AppendF32(rsw, channel);
    }
    for (const float channel : options.light_ambient) {
      AppendF32(rsw, channel);
    }
    if (version >= 0x0107) {
      AppendF32(rsw, options.light_shadow_intensity);
    }
  }

  if (version >= 0x0106) {
    AppendI32(rsw, options.ground_top);
    AppendI32(rsw, options.ground_bottom);
    AppendI32(rsw, options.ground_left);
    AppendI32(rsw, options.ground_right);
  }

  if (!options.write_objects) {
    return rsw;
  }

  if (version >= 0x0207) {
    AppendI32(rsw, 0);  // bloc supplémentaire vide
  }

  const auto object_count = static_cast<std::int32_t>(options.models.size()) +
                            options.light_object_count + options.sound_object_count +
                            static_cast<std::int32_t>(options.sounds.size()) +
                            options.effect_object_count + (options.write_unknown_object ? 1 : 0);
  AppendI32(rsw, object_count);

  // Les autres types sont écrits AVANT les modèles : si le parseur se trompe de
  // longueur en les traversant, aucun modèle ne sortira juste.
  for (std::int32_t index = 0; index < options.light_object_count; ++index) {
    AppendI32(rsw, 2);
    rsw.insert(rsw.end(), 80 + 12 + 12 + 4, 0);
  }
  for (std::int32_t index = 0; index < options.sound_object_count; ++index) {
    AppendI32(rsw, 3);
    rsw.insert(rsw.end(), 80 + 80 + 12 + 4 + 4 + 4 + 4, 0);
    if (version >= 0x0200) {
      rsw.insert(rsw.end(), 4, 0);
    }
  }
  for (const RswSoundSpec& sound : options.sounds) {
    AppendI32(rsw, 3);
    AppendText(rsw, sound.name, 80);
    AppendText(rsw, sound.wave_name, 80);
    for (const float component : sound.position) {
      AppendF32(rsw, component);
    }
    AppendF32(rsw, sound.volume);
    AppendI32(rsw, sound.width);
    AppendI32(rsw, sound.height);
    AppendF32(rsw, sound.range);
    if (version >= 0x0200) {
      AppendF32(rsw, sound.cycle_seconds);
    }
  }
  for (std::int32_t index = 0; index < options.effect_object_count; ++index) {
    AppendI32(rsw, 4);
    rsw.insert(rsw.end(), 80 + 12 + 4 + 4 + 16, 0);
  }

  for (const RswModelSpec& model : options.models) {
    AppendI32(rsw, 1);
    if (version >= 0x0103) {
      AppendText(rsw, model.name, 40);
      AppendI32(rsw, 0);  // type d'animation
      AppendF32(rsw, model.animation_speed);
      AppendI32(rsw, 0);  // type de blocage
      if (version >= 0x0206 && options.build_number >= 186) {
        rsw.push_back(0);
      }
      if (version >= 0x0207) {
        AppendI32(rsw, 0);
      }
    }
    AppendText(rsw, model.model_name, 80);
    AppendText(rsw, model.node_name, 80);
    for (const float component : model.position) {
      AppendF32(rsw, component);
    }
    for (const float component : model.rotation) {
      AppendF32(rsw, component);
    }
    for (const float component : model.scale) {
      AppendF32(rsw, component);
    }
  }

  if (options.write_unknown_object) {
    AppendI32(rsw, 99);
  }
  for (const std::array<float, 12>& node : options.quad_tree) {
    for (const float component : node) {
      AppendF32(rsw, component);
    }
  }
  return rsw;
}

std::vector<std::uint8_t> BuildGnd(const GndBuildOptions& options) {
  std::vector<std::uint8_t> gnd;
  AppendSignature(gnd, "GRGN");
  gnd.push_back(options.major);
  gnd.push_back(options.minor);
  AppendI32(gnd, options.width);
  AppendI32(gnd, options.height);
  AppendF32(gnd, options.proportion_ratio);
  AppendI32(gnd, static_cast<std::int32_t>(options.texture_paths.size()));
  AppendI32(gnd, options.texture_path_size);

  for (const std::string& path : options.texture_paths) {
    AppendText(gnd, path, static_cast<std::size_t>(options.texture_path_size));
  }

  const std::int32_t version =
      (static_cast<std::int32_t>(options.major) << 8) | static_cast<std::int32_t>(options.minor);

  if (version >= 0x0101) {
    AppendI32(gnd, options.lightmap_count);
    AppendI32(gnd, options.lightmap_width);
    AppendI32(gnd, options.lightmap_height);
    AppendI32(gnd, options.lightmap_cells);

    // La taille se calcule avec UN, jamais avec lightmap_cells : c'est le
    // piège que ce constructeur permet de tester.
    const std::size_t texels_per_lightmap = static_cast<std::size_t>(options.lightmap_width) *
                                            static_cast<std::size_t>(options.lightmap_height);
    for (std::int32_t index = 0; index < options.lightmap_count; ++index) {
      gnd.insert(gnd.end(), texels_per_lightmap, options.lightmap_shadow);
      for (std::size_t texel = 0; texel < texels_per_lightmap; ++texel) {
        gnd.insert(gnd.end(), options.lightmap_rgb.begin(), options.lightmap_rgb.end());
      }
    }

    AppendI32(gnd, options.tile_count);
    for (std::int32_t index = 0; index < options.tile_count; ++index) {
      for (int coordinate = 0; coordinate < 4; ++coordinate) {
        AppendF32(gnd, static_cast<float>(coordinate));  // u
      }
      for (int coordinate = 0; coordinate < 4; ++coordinate) {
        AppendF32(gnd, static_cast<float>(coordinate) * 2.0F);  // v
      }
      const std::int16_t texture_index =
          static_cast<std::size_t>(index) < options.tile_texture_indices.size()
              ? options.tile_texture_indices[static_cast<std::size_t>(index)]
              : static_cast<std::int16_t>(0);
      AppendU32(gnd, static_cast<std::uint32_t>(static_cast<std::uint16_t>(texture_index)));
      // Couleur du carreau, rangée BLEU, VERT, ROUGE, ALPHA.
      const std::array<std::uint8_t, 4> color =
          static_cast<std::size_t>(index) < options.tile_colors.size()
              ? options.tile_colors[static_cast<std::size_t>(index)]
              : std::array<std::uint8_t, 4>{0x11, 0x22, 0x33, 0xFF};
      gnd.insert(gnd.end(), color.begin(), color.end());
    }
  }

  std::vector<GndCubeSpec> cubes = options.cubes;
  if (cubes.empty()) {
    cubes.resize(static_cast<std::size_t>(options.width) *
                 static_cast<std::size_t>(options.height));
  }
  for (const GndCubeSpec& cube : cubes) {
    for (const float corner : cube.corner_height) {
      AppendF32(gnd, corner);
    }
    if (version >= 0x0106) {
      AppendI32(gnd, cube.tile_up);
      AppendI32(gnd, cube.tile_front);
      AppendI32(gnd, cube.tile_side);
    } else {
      AppendU32(gnd, static_cast<std::uint32_t>(cube.tile_up & 0xFFFF) |
                         (static_cast<std::uint32_t>(cube.tile_front & 0xFFFF) << 16));
      AppendU32(gnd, static_cast<std::uint32_t>(cube.tile_side & 0xFFFF));  // + 2 de bourrage
    }
  }

  if (version >= 0x0108) {
    // Jeu de valeurs par défaut, dont seul le niveau est réutilisé en 1.8.
    AppendF32(gnd, 3.5F);
    AppendI32(gnd, 1);
    AppendF32(gnd, 0.5F);
    AppendF32(gnd, 1.5F);
    AppendF32(gnd, 25.0F);
    AppendI32(gnd, 7);

    AppendI32(gnd, options.water_split_width);
    AppendI32(gnd, options.water_split_height);

    std::vector<float> levels = options.water_levels;
    if (levels.empty()) {
      levels.assign(static_cast<std::size_t>(options.water_split_width) *
                        static_cast<std::size_t>(options.water_split_height),
                    0.0F);
    }
    for (const float level : levels) {
      AppendF32(gnd, level);
      if (version >= 0x0109) {
        AppendI32(gnd, 2);
        AppendF32(gnd, 0.25F);
        AppendF32(gnd, 3.0F);
        AppendF32(gnd, 40.0F);
        AppendI32(gnd, 5);
      }
    }
  }

  return gnd;
}

}  // namespace ro::test_support
