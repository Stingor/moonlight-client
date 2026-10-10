#include "ro/formats/palette.h"

#include <string>

namespace ro::formats {

core::Result<Palette> DecodePalette(std::span<const std::uint8_t> data) {
  if (data.size() < kPaletteBytes) {
    return core::Fail(core::ErrorCode::kUnexpectedEnd,
                      "palette de " + std::to_string(data.size()) + " octets, 1024 attendus");
  }

  Palette palette;
  for (std::size_t index = 0; index < kPaletteEntryCount; ++index) {
    for (std::size_t channel = 0; channel < 4; ++channel) {
      palette.entries[index][channel] = data[index * 4 + channel];
    }
  }
  return palette;
}

}  // namespace ro::formats
