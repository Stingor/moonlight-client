#include "ro/proto/hat_effect_packets.h"

#include "ro/core/byte_reader.h"
#include "ro/core/byte_writer.h"
#include "ro/proto/map_packets.h"

namespace ro::proto {

// L'opcode écrit ici est celui que le serveur déclare.
static_assert(HatEffectMap::kOpcode == ZcMoonlightHateffectMap::kOpcode);
static_assert(HatEffectMap::kHeaderSize == ZcMoonlightHateffectMap::kWireSize);

core::Result<HatEffectMap> DecodeHatEffectMap(std::span<const std::uint8_t> frame) {
  core::ByteReader reader(frame);
  if (reader.U16() != HatEffectMap::kOpcode) {
    return core::Fail(core::ErrorCode::kBadSignature,
                      "trame d'un autre paquet que la carte des effets de chapeau");
  }
  const std::uint16_t length = reader.U16();
  const std::int16_t count = reader.I16();
  if (!reader.ok() || length != frame.size() || count < 0 ||
      (frame.size() - HatEffectMap::kHeaderSize) !=
          static_cast<std::size_t>(count) * HatEffectMap::kEntrySize) {
    return core::Fail(core::ErrorCode::kUnexpectedEnd, "carte des effets de chapeau tronquée");
  }
  HatEffectMap map;
  map.entries.resize(static_cast<std::size_t>(count));
  for (HatEffectMapEntry& entry : map.entries) {
    entry.item_id = reader.U32();
    entry.ordinal = reader.I16();
  }
  if (!reader.ok()) {
    return core::Fail(core::ErrorCode::kUnexpectedEnd, "carte des effets de chapeau tronquée");
  }
  return map;
}

std::vector<std::uint8_t> EncodeHatEffectMap(const HatEffectMap& map) {
  std::vector<std::uint8_t> frame;
  core::ByteWriter writer(frame);
  writer.U16(HatEffectMap::kOpcode);
  writer.U16(static_cast<std::uint16_t>(HatEffectMap::kHeaderSize +
                                        map.entries.size() * HatEffectMap::kEntrySize));
  writer.I16(static_cast<std::int16_t>(map.entries.size()));
  for (const HatEffectMapEntry& entry : map.entries) {
    writer.U32(entry.item_id);
    writer.I16(entry.ordinal);
  }
  return frame;
}

}  // namespace ro::proto
