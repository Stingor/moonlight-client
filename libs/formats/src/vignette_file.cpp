#include "ro/formats/vignette_file.h"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <utility>

#include <zlib.h>

#include "ro/core/byte_reader.h"
#include "ro/core/byte_writer.h"

namespace ro::formats {
namespace {

using core::ErrorCode;
using core::Fail;

constexpr std::array<std::uint8_t, 4> kSignature = {'M', 'C', 'V', 'G'};
// La révision de la disposition : une autre se refuse, et la vignette se refait.
constexpr std::uint32_t kFormatRevision = 1;
// Le plus grand côté accepté, en pixels : un fichier qui en annonce plus est
// abîmé, et l'on n'alloue pas ce qu'il prétend.
constexpr std::int32_t kMostSidePx = 1024;
constexpr std::size_t kBytesPerPixel = 4;

void WriteText(core::ByteWriter& out, std::string_view text) {
  out.U16(static_cast<std::uint16_t>(text.size()));
  out.Text(text);
}

std::string ReadText(core::ByteReader& in) {
  const std::uint16_t length = in.U16();
  const std::span<const std::uint8_t> raw = in.Bytes(length);
  return {raw.begin(), raw.end()};
}

}  // namespace

std::vector<std::uint8_t> EncodeVignetteFile(const VignetteFile& file) {
  std::vector<std::uint8_t> bytes;
  core::ByteWriter out(bytes);
  out.Bytes(kSignature);
  out.U32(kFormatRevision);
  const VignetteKey& key = file.key;
  WriteText(out, key.model_file);
  out.I32(key.side_px);
  out.U32(key.render_revision);
  out.U8(static_cast<std::uint8_t>(key.stamp.origin));
  WriteText(out, key.stamp.container);
  out.U64(key.stamp.offset);
  out.U64(key.stamp.stored_size);
  out.U64(key.stamp.size);
  out.I64(key.stamp.written_ticks);
  const bool shown = !file.image.empty();
  out.I32(shown ? file.image.width : 0);
  out.I32(shown ? file.image.height : 0);
  if (!shown) {
    out.U32(0);
    return bytes;
  }
  uLongf packed_size = compressBound(static_cast<uLong>(file.image.pixels.size()));
  std::vector<std::uint8_t> packed(packed_size);
  if (compress2(packed.data(), &packed_size, file.image.pixels.data(),
                static_cast<uLong>(file.image.pixels.size()), Z_DEFAULT_COMPRESSION) != Z_OK) {
    packed_size = 0;
  }
  packed.resize(packed_size);
  out.U32(static_cast<std::uint32_t>(packed.size()));
  out.Bytes(packed);
  return bytes;
}

core::Result<VignetteFile> DecodeVignetteFile(std::span<const std::uint8_t> data) {
  core::ByteReader in(data);
  const std::span<const std::uint8_t> signature = in.Bytes(kSignature.size());
  if (!in.ok() || !std::ranges::equal(signature, kSignature)) {
    return Fail(ErrorCode::kBadSignature, "vignette : signature absente");
  }
  if (const std::uint32_t revision = in.U32(); revision != kFormatRevision) {
    return Fail(ErrorCode::kUnsupportedVersion,
                "vignette : révision " + std::to_string(revision) + " inconnue");
  }
  VignetteFile file;
  VignetteKey& key = file.key;
  key.model_file = ReadText(in);
  key.side_px = in.I32();
  key.render_revision = in.U32();
  const std::uint8_t origin = in.U8();
  if (origin > static_cast<std::uint8_t>(core::FileStamp::Origin::kArchive)) {
    return Fail(ErrorCode::kCorruptData, "vignette : origine inconnue");
  }
  key.stamp.origin = static_cast<core::FileStamp::Origin>(origin);
  key.stamp.container = ReadText(in);
  key.stamp.offset = in.U64();
  key.stamp.stored_size = in.U64();
  key.stamp.size = in.U64();
  key.stamp.written_ticks = in.I64();
  const std::int32_t width = in.I32();
  const std::int32_t height = in.I32();
  const std::uint32_t packed_size = in.U32();
  const std::span<const std::uint8_t> packed = in.Bytes(packed_size);
  if (!in.ok()) {
    return Fail(ErrorCode::kUnexpectedEnd, "vignette : fichier tronqué");
  }
  if (width == 0 && height == 0) {
    return file;
  }
  if (width <= 0 || height <= 0 || width > kMostSidePx || height > kMostSidePx) {
    return Fail(ErrorCode::kCorruptData, "vignette : dimensions incohérentes");
  }
  const std::size_t expected =
      static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * kBytesPerPixel;
  std::vector<std::uint8_t> pixels(expected);
  auto produced = static_cast<uLongf>(expected);
  if (uncompress(pixels.data(), &produced, packed.data(), static_cast<uLong>(packed.size())) !=
          Z_OK ||
      produced != expected) {
    return Fail(ErrorCode::kCorruptData, "vignette : flux zlib incomplet");
  }
  file.image.width = width;
  file.image.height = height;
  file.image.pixels = std::move(pixels);
  return file;
}

}  // namespace ro::formats
