#include "support/effect_builder.h"

#include <bit>
#include <cstring>

namespace ro::test_support {
namespace {

void PutU8(std::vector<std::uint8_t>& out, std::uint8_t value) { out.push_back(value); }

void PutI32(std::vector<std::uint8_t>& out, std::int32_t value) {
  const auto raw = static_cast<std::uint32_t>(value);
  for (int shift = 0; shift < 32; shift += 8) {
    out.push_back(static_cast<std::uint8_t>((raw >> shift) & 0xFFU));
  }
}

void PutF32(std::vector<std::uint8_t>& out, float value) {
  PutI32(out, static_cast<std::int32_t>(std::bit_cast<std::uint32_t>(value)));
}

// Un nom de texture occupe toujours cent vingt-huit octets, complétés de zéros.
void PutTextureName(std::vector<std::uint8_t>& out, const std::string& name) {
  constexpr std::size_t kTextureNameBytes = 128;
  for (std::size_t index = 0; index < kTextureNameBytes; ++index) {
    out.push_back(index < name.size() ? static_cast<std::uint8_t>(name[index]) : 0U);
  }
}

void PutKeyFrame(std::vector<std::uint8_t>& out, const StrKeyFrameSpec& key) {
  PutI32(out, key.frame_index);
  PutI32(out, key.type);
  PutF32(out, key.offset_x);
  PutF32(out, key.offset_y);
  for (const float value : key.uv) {
    PutF32(out, value);
  }
  for (const float value : key.corners) {
    PutF32(out, value);
  }
  PutF32(out, key.texture_index);
  PutI32(out, key.animation);
  PutF32(out, key.delay);
  PutF32(out, key.angle_units);
  for (const float value : key.color) {
    PutF32(out, value);
  }
  PutI32(out, key.blend_source);
  PutI32(out, key.blend_destination);
  PutI32(out, key.multi_texture);
}

}  // namespace

std::vector<std::uint8_t> BuildStr(const StrBuildOptions& options) {
  std::vector<std::uint8_t> out;
  for (const char letter : std::string("STRM")) {
    out.push_back(static_cast<std::uint8_t>(letter));
  }
  PutU8(out, options.major);
  PutU8(out, options.minor);
  PutU8(out, 0);
  PutU8(out, 0);
  PutI32(out, options.frames_per_second);
  PutI32(out, options.last_frame);
  PutI32(out, options.declared_layers >= 0 ? options.declared_layers
                                           : static_cast<std::int32_t>(options.layers.size()));
  for (int reserved = 0; reserved < 4; ++reserved) {
    PutI32(out, 0);
  }

  for (const StrLayerSpec& layer : options.layers) {
    PutI32(out, static_cast<std::int32_t>(layer.texture_names.size()));
    for (const std::string& name : layer.texture_names) {
      PutTextureName(out, name);
    }
    PutI32(out, static_cast<std::int32_t>(layer.key_frames.size()));
    for (const StrKeyFrameSpec& key : layer.key_frames) {
      PutKeyFrame(out, key);
    }
  }
  return out;
}

}  // namespace ro::test_support
