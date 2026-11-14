#include "support/sprite_builder.h"

#include <algorithm>
#include <bit>

namespace ro::test_support {
namespace {

constexpr std::size_t kPaletteEntryCount = 256;

// Un compte de transparence tient sur un octet : une plage plus longue se
// découpe en plusieurs paquets.
constexpr std::size_t kMaxTransparentRun = 255;

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

// Octets réservés du format .act, aux trois endroits où il en porte.
constexpr std::size_t kActHeaderReserved = 10;
constexpr std::size_t kActFrameReserved = 32;
constexpr std::size_t kActAnchorReserved = 4;
constexpr std::size_t kActSoundNameBytes = 40;

// Compresse par plages : un index non nul s'écrit tel quel, une suite de zéros
// devient un octet nul suivi de sa longueur.
std::vector<std::uint8_t> PackRunLength(const std::vector<std::uint8_t>& indices) {
  std::vector<std::uint8_t> packed;
  std::size_t position = 0;
  while (position < indices.size()) {
    if (indices[position] != 0) {
      packed.push_back(indices[position]);
      ++position;
      continue;
    }
    std::size_t run = 0;
    while (position + run < indices.size() && indices[position + run] == 0 &&
           run < kMaxTransparentRun) {
      ++run;
    }
    packed.push_back(0);
    packed.push_back(static_cast<std::uint8_t>(run));
    position += run;
  }
  return packed;
}

void AppendPalette(std::vector<std::uint8_t>& out, const std::vector<SpriteColor>& colors) {
  for (std::size_t index = 0; index < kPaletteEntryCount; ++index) {
    if (index < colors.size()) {
      out.push_back(colors[index].red);
      out.push_back(colors[index].green);
      out.push_back(colors[index].blue);
      out.push_back(colors[index].fourth);
    } else {
      out.insert(out.end(), 4, 0);
    }
  }
}

}  // namespace

std::vector<std::uint8_t> BuildSpr(const SprBuildOptions& options) {
  const std::int32_t version =
      static_cast<std::int32_t>(options.major) * 10 + static_cast<std::int32_t>(options.minor);

  std::vector<std::uint8_t> spr;
  spr.push_back('S');
  spr.push_back('P');
  spr.push_back(options.minor);
  spr.push_back(options.major);

  AppendU16(spr, static_cast<std::uint16_t>(options.indexed.size()));
  if (version >= 20) {
    AppendU16(spr, static_cast<std::uint16_t>(options.bgra.size()));
  }

  for (const SprIndexedFrameSpec& frame : options.indexed) {
    AppendU16(spr, static_cast<std::uint16_t>(frame.width));
    AppendU16(spr, static_cast<std::uint16_t>(frame.height));
    if (version < 21) {
      spr.insert(spr.end(), frame.indices.begin(), frame.indices.end());
      continue;
    }
    const std::vector<std::uint8_t> packed = PackRunLength(frame.indices);
    if (version >= 22) {
      AppendU32(spr, static_cast<std::uint32_t>(packed.size()));
    } else {
      AppendU16(spr, static_cast<std::uint16_t>(packed.size()));
    }
    spr.insert(spr.end(), packed.begin(), packed.end());
  }

  for (const SprBgraFrameSpec& frame : options.bgra) {
    AppendU16(spr, static_cast<std::uint16_t>(frame.width));
    AppendU16(spr, static_cast<std::uint16_t>(frame.height));
    // Rangées inversées, composantes en alpha, bleu, vert, rouge.
    for (std::int32_t y = frame.height - 1; y >= 0; --y) {
      for (std::int32_t x = 0; x < frame.width; ++x) {
        const auto at = static_cast<std::size_t>(y) * static_cast<std::size_t>(frame.width) +
                        static_cast<std::size_t>(x);
        const SpriteColor& color = frame.pixels[at];
        spr.push_back(at < frame.alpha.size() ? frame.alpha[at] : 255);
        spr.push_back(color.blue);
        spr.push_back(color.green);
        spr.push_back(color.red);
      }
    }
  }

  if (!options.indexed.empty()) {
    AppendPalette(spr, options.palette);
  }
  spr.insert(spr.end(), options.trailing_bytes, 0);
  return spr;
}

std::vector<std::uint8_t> BuildPal(const std::vector<SpriteColor>& colors,
                                   std::size_t trailing_bytes) {
  std::vector<std::uint8_t> pal;
  AppendPalette(pal, colors);
  pal.insert(pal.end(), trailing_bytes, 0);
  return pal;
}

std::vector<std::uint8_t> BuildAct(const ActBuildOptions& options) {
  const std::int32_t version =
      static_cast<std::int32_t>(options.major) * 10 + static_cast<std::int32_t>(options.minor);

  std::vector<std::uint8_t> act;
  act.push_back('A');
  act.push_back('C');
  act.push_back(options.minor);
  act.push_back(options.major);
  AppendU16(act, static_cast<std::uint16_t>(options.actions.size()));
  act.insert(act.end(), kActHeaderReserved, 0);

  for (const ActActionSpec& action : options.actions) {
    AppendI32(act, static_cast<std::int32_t>(action.frames.size()));
    for (const ActFrameSpec& frame : action.frames) {
      act.insert(act.end(), kActFrameReserved, 0);
      AppendI32(act, static_cast<std::int32_t>(frame.layers.size()));

      for (const ActLayerSpec& layer : frame.layers) {
        if (version >= 26) {
          AppendF32(act, layer.offset_x);
          AppendF32(act, layer.offset_y);
        } else {
          AppendI32(act, static_cast<std::int32_t>(layer.offset_x));
          AppendI32(act, static_cast<std::int32_t>(layer.offset_y));
        }
        AppendI32(act, layer.frame_index);
        AppendI32(act, layer.mirrored ? 1 : 0);
        if (version >= 20) {
          act.insert(act.end(), layer.tint.begin(), layer.tint.end());
          AppendF32(act, layer.scale_x);
          if (version >= 24) {
            AppendF32(act, layer.scale_y);
          }
          AppendI32(act, layer.rotation_degrees);
          AppendI32(act, layer.section);
          if (version >= 25) {
            AppendI32(act, layer.declared_width);
            AppendI32(act, layer.declared_height);
          }
        }
      }

      if (version >= 20) {
        AppendI32(act, frame.sound_index);
      }
      if (version >= 23) {
        AppendI32(act, options.absurd_anchor_count
                           ? 999999
                           : static_cast<std::int32_t>(frame.anchors.size()));
        for (const ActAnchorSpec& anchor : frame.anchors) {
          act.insert(act.end(), kActAnchorReserved, 0);
          AppendI32(act, anchor.x);
          AppendI32(act, anchor.y);
          AppendI32(act, anchor.attribute);
        }
      }
    }
  }

  if (version >= 21) {
    AppendI32(act, static_cast<std::int32_t>(options.sound_files.size()));
    for (const std::string& name : options.sound_files) {
      const std::size_t kept = std::min(name.size(), kActSoundNameBytes);
      act.insert(act.end(), name.begin(), name.begin() + static_cast<std::ptrdiff_t>(kept));
      act.insert(act.end(), kActSoundNameBytes - kept, 0);
    }
  }
  if (version >= 22) {
    for (const float speed : options.speeds) {
      AppendF32(act, speed);
    }
  }
  return act;
}

}  // namespace ro::test_support
