// Les documents d'édition de carte, en morceaux. Voir l'en-tête.

#include "ro/proto/map_edit_packets.h"

#include <algorithm>
#include <format>
#include <utility>

#include "ro/proto/map_packets.h"

namespace ro::proto {
namespace {

// Où la longueur s'écrit dans une trame : juste après l'opcode.
constexpr std::size_t kLengthOffset = 2;
// La plus longue trame que porte un champ de longueur signé de 16 bits.
constexpr std::size_t kMostSignedFrame = 0x7FFF;

// La tête suivie de `bytes`, la longueur repatchée sur le tout.
std::vector<std::uint8_t> WithTail(std::vector<std::uint8_t> head,
                                   std::span<const std::uint8_t> bytes) {
  head.insert(head.end(), bytes.begin(), bytes.end());
  const auto length = static_cast<std::uint16_t>(head.size());
  head[kLengthOffset] = static_cast<std::uint8_t>(length & 0xFF);
  head[kLengthOffset + 1] = static_cast<std::uint8_t>(length >> 8);
  return head;
}

// Le flux et ses plafonds tiennent-ils ?
bool Sendable(std::string_view map, std::uint32_t text_size, std::span<const std::uint8_t> zipped) {
  return IsMapEditName(map) && !zipped.empty() && zipped.size() <= kMapEditMostZippedBytes &&
         text_size > 0 && text_size <= kMapEditMostTextBytes;
}

// Les morceaux de `zipped`, chacun au plus `chunk_bytes` octets, en trames.
template <typename Head>
std::vector<std::vector<std::uint8_t>> Chunked(Head head, std::span<const std::uint8_t> zipped,
                                               std::size_t chunk_bytes) {
  std::vector<std::vector<std::uint8_t>> frames;
  head.zipped_size = static_cast<std::uint32_t>(zipped.size());
  for (std::size_t offset = 0; offset < zipped.size(); offset += chunk_bytes) {
    head.offset = static_cast<std::uint32_t>(offset);
    std::vector<std::uint8_t> encoded = Encode(head);
    if (encoded.empty()) {
      return {};
    }
    frames.push_back(WithTail(
        std::move(encoded), zipped.subspan(offset, std::min(chunk_bytes, zipped.size() - offset))));
  }
  return frames;
}

// La longueur que la trame annonce est-elle la sienne ?
bool LengthMatches(std::span<const std::uint8_t> frame) {
  if (frame.size() < kLengthOffset + 2 || frame.size() > kMostSignedFrame) {
    return false;
  }
  const std::size_t announced = static_cast<std::size_t>(frame[kLengthOffset]) |
                                (static_cast<std::size_t>(frame[kLengthOffset + 1]) << 8);
  return announced == frame.size();
}

}  // namespace

bool IsMapEditName(std::string_view map) {
  if (map.empty() || map.size() >= kMapEditNameBytes) {
    return false;
  }
  return std::ranges::all_of(map, [](char letter) {
    return (letter >= 'a' && letter <= 'z') || (letter >= 'A' && letter <= 'Z') ||
           (letter >= '0' && letter <= '9') || letter == '_' || letter == '-' || letter == '@';
  });
}

std::vector<std::vector<std::uint8_t>> EncodeMapEditPut(std::string_view map,
                                                        std::uint32_t base_revision,
                                                        std::uint32_t text_size,
                                                        std::span<const std::uint8_t> zipped) {
  if (!Sendable(map, text_size, zipped)) {
    return {};
  }
  CzMoonclientMapEditPut head;
  head.map = std::string(map);
  head.base_revision = base_revision;
  head.text_size = text_size;
  return Chunked(std::move(head), zipped, kMapEditPutChunkBytes);
}

std::vector<std::vector<std::uint8_t>> EncodeMapEditDoc(std::string_view map,
                                                        std::uint32_t revision,
                                                        std::string_view author,
                                                        std::uint32_t text_size,
                                                        std::span<const std::uint8_t> zipped) {
  if (!Sendable(map, text_size, zipped)) {
    return {};
  }
  ZcMoonclientMapEditDoc head;
  head.map = std::string(map);
  head.revision = revision;
  head.text_size = text_size;
  head.author = std::string(author);
  return Chunked(std::move(head), zipped, kMapEditDocChunkBytes);
}

std::optional<MapEditChunk> DecodeMapEditPut(std::span<const std::uint8_t> frame) {
  CzMoonclientMapEditPut head;
  if (!LengthMatches(frame) || !Decode(frame, head)) {
    return std::nullopt;
  }
  const std::span<const std::uint8_t> tail = frame.subspan(CzMoonclientMapEditPut::kWireSize);
  return MapEditChunk{.map = std::move(head.map),
                      .revision = head.base_revision,
                      .zipped_size = head.zipped_size,
                      .text_size = head.text_size,
                      .offset = head.offset,
                      .author = {},
                      .bytes = {tail.begin(), tail.end()}};
}

std::optional<MapEditChunk> DecodeMapEditDoc(std::span<const std::uint8_t> frame) {
  ZcMoonclientMapEditDoc head;
  if (!LengthMatches(frame) || !Decode(frame, head)) {
    return std::nullopt;
  }
  const std::span<const std::uint8_t> tail = frame.subspan(ZcMoonclientMapEditDoc::kWireSize);
  return MapEditChunk{.map = std::move(head.map),
                      .revision = head.revision,
                      .zipped_size = head.zipped_size,
                      .text_size = head.text_size,
                      .offset = head.offset,
                      .author = std::move(head.author),
                      .bytes = {tail.begin(), tail.end()}};
}

MapEditAssembler::Step MapEditAssembler::Feed(MapEditChunk chunk) {
  if (chunk.bytes.empty() || chunk.zipped_size == 0 ||
      chunk.zipped_size > kMapEditMostZippedBytes || chunk.text_size == 0 ||
      chunk.text_size > kMapEditMostTextBytes || chunk.offset > chunk.zipped_size ||
      chunk.bytes.size() > chunk.zipped_size - chunk.offset) {
    return Refuse(std::format("morceau de {} hors des plafonds (décalage {}, {} octets sur {})",
                              chunk.map, chunk.offset, chunk.bytes.size(), chunk.zipped_size));
  }
  if (chunk.offset == 0) {
    // LE DÉCALAGE 0 OUVRE UN TRANSFERT, et remplace celui qui était en cours.
    receiving_ = true;
    zipped_size_ = chunk.zipped_size;
    building_ = AssembledMapEdit{.map = std::move(chunk.map),
                                 .revision = chunk.revision,
                                 .text_size = chunk.text_size,
                                 .author = std::move(chunk.author),
                                 .zipped = {}};
    building_.zipped.reserve(zipped_size_);
  } else if (!receiving_ || chunk.map != building_.map || chunk.revision != building_.revision ||
             chunk.zipped_size != zipped_size_ || chunk.text_size != building_.text_size ||
             chunk.offset != building_.zipped.size()) {
    return Refuse(std::format("morceau de {} hors d'ordre (décalage {}, {} attendu)", chunk.map,
                              chunk.offset, building_.zipped.size()));
  }
  building_.zipped.insert(building_.zipped.end(), chunk.bytes.begin(), chunk.bytes.end());
  if (building_.zipped.size() < zipped_size_) {
    return Step::kPending;
  }
  receiving_ = false;
  complete_ = std::exchange(building_, AssembledMapEdit());
  return Step::kComplete;
}

AssembledMapEdit MapEditAssembler::Take() {
  AssembledMapEdit taken = complete_.has_value() ? std::move(*complete_) : AssembledMapEdit();
  complete_.reset();
  return taken;
}

void MapEditAssembler::Reset() {
  receiving_ = false;
  building_ = AssembledMapEdit();
  complete_.reset();
  zipped_size_ = 0;
}

MapEditAssembler::Step MapEditAssembler::Refuse(std::string why) {
  receiving_ = false;
  building_ = AssembledMapEdit();
  refusal_ = std::move(why);
  return Step::kRefused;
}

}  // namespace ro::proto
