#include "ro/formats/grf_writer.h"

#include <array>
#include <limits>
#include <string>

#include <zlib.h>

#include "ro/core/byte_writer.h"

namespace ro::formats {
namespace {

using core::ErrorCode;
using core::Fail;
using core::Result;

// La signature, son NUL, puis la clé que portent les archives non chiffrées :
// les octets 1 à 14. Trente octets en tout.
constexpr std::string_view kSignature{"Master of Magic\0", 16};
constexpr std::size_t kKeyBytes = 14;
constexpr std::uint32_t kVersion200 = 0x200;
// Le 2.0 range le nombre d'entrées augmenté de la graine et de sept.
constexpr std::uint32_t kEntryCountBias = 7;
constexpr std::uint64_t kAlignmentBytes = 8;
constexpr std::uint64_t kU32Limit = std::numeric_limits<std::uint32_t>::max();

[[nodiscard]] std::uint64_t Aligned(std::uint64_t size) {
  return (size + kAlignmentBytes - 1) / kAlignmentBytes * kAlignmentBytes;
}

[[nodiscard]] Result<std::vector<std::uint8_t>> Deflated(std::span<const std::uint8_t> input) {
  uLongf bound = compressBound(static_cast<uLong>(input.size()));
  std::vector<std::uint8_t> output(bound);
  if (compress2(output.data(), &bound, input.data(), static_cast<uLong>(input.size()),
                Z_BEST_COMPRESSION) != Z_OK) {
    return Fail(ErrorCode::kCorruptData, "zlib a refusé de compresser la table");
  }
  output.resize(bound);
  return output;
}

}  // namespace

Result<GrfWriter> GrfWriter::Begin(core::ByteSink& sink) {
  if (sink.size() != 0) {
    return Fail(ErrorCode::kInvalidArgument, "une archive GRF s'écrit dans une sortie vide");
  }
  // L'en-tête se réserve ; il ne s'écrit qu'une fois la table placée.
  const std::array<std::uint8_t, kGrfHeaderSize> reserved{};
  if (auto written = sink.Append(reserved); !written) {
    return std::unexpected(written.error());
  }
  return GrfWriter(sink);
}

Result<void> GrfWriter::Append(std::string_view name, const GrfEntry& entry,
                               std::span<const std::uint8_t> bytes) {
  if (name.empty() || name.find('\0') != std::string_view::npos) {
    return Fail(ErrorCode::kInvalidArgument, "nom d'entrée GRF vide ou coupé par un NUL");
  }
  if (data_end_ + entry.aligned_size > kU32Limit) {
    return Fail(ErrorCode::kInvalidArgument,
                "l'archive dépasserait quatre gibioctets, la limite d'une GRF 2.0");
  }
  if (auto written = sink_->Append(bytes); !written) {
    return written;
  }
  const std::array<std::uint8_t, kAlignmentBytes> padding{};
  if (auto padded = sink_->Append(std::span(padding).first(entry.aligned_size - bytes.size()));
      !padded) {
    return padded;
  }
  core::ByteWriter table(table_);
  table_.insert(table_.end(), name.begin(), name.end());
  table.U8(0);
  table.U32(entry.compressed_size);
  table.U32(entry.aligned_size);
  table.U32(entry.real_size);
  table.U8(entry.flags);
  table.U32(static_cast<std::uint32_t>(data_end_));
  data_end_ += entry.aligned_size;
  ++entry_count_;
  return {};
}

Result<void> GrfWriter::AddCompressed(std::string_view name,
                                      std::span<const std::uint8_t> zlib_stream,
                                      std::uint32_t real_size) {
  if (zlib_stream.size() > kU32Limit - kAlignmentBytes) {
    return Fail(ErrorCode::kInvalidArgument, std::string(name) + " : flux trop grand");
  }
  GrfEntry entry;
  entry.compressed_size = static_cast<std::uint32_t>(zlib_stream.size());
  entry.aligned_size = static_cast<std::uint32_t>(Aligned(zlib_stream.size()));
  entry.real_size = real_size;
  entry.flags = kGrfFlagFile;
  return Append(name, entry, zlib_stream);
}

Result<void> GrfWriter::AddRaw(std::string_view name, const GrfEntry& entry,
                               std::span<const std::uint8_t> aligned_bytes) {
  if (aligned_bytes.size() > entry.aligned_size ||
      entry.aligned_size - aligned_bytes.size() >= kAlignmentBytes) {
    return Fail(ErrorCode::kInvalidArgument,
                std::string(name) + " : les octets ne correspondent pas à la taille alignée");
  }
  return Append(name, entry, aligned_bytes);
}

Result<void> GrfWriter::Finish() {
  auto compressed_table = Deflated(table_);
  if (!compressed_table) {
    return std::unexpected(compressed_table.error());
  }
  if (table_.size() > kU32Limit || compressed_table->size() > kU32Limit) {
    return Fail(ErrorCode::kInvalidArgument, "table GRF trop grande");
  }
  std::vector<std::uint8_t> table_block;
  core::ByteWriter block(table_block);
  block.U32(static_cast<std::uint32_t>(compressed_table->size()));
  block.U32(static_cast<std::uint32_t>(table_.size()));
  table_block.insert(table_block.end(), compressed_table->begin(), compressed_table->end());
  if (auto written = sink_->Append(table_block); !written) {
    return written;
  }

  std::vector<std::uint8_t> header(kSignature.begin(), kSignature.end());
  core::ByteWriter head(header);
  for (std::size_t key_byte = 1; key_byte <= kKeyBytes; ++key_byte) {
    head.U8(static_cast<std::uint8_t>(key_byte));
  }
  head.U32(static_cast<std::uint32_t>(data_end_));
  head.U32(0);
  head.U32(entry_count_ + kEntryCountBias);
  head.U32(kVersion200);
  return sink_->Overwrite(0, header);
}

}  // namespace ro::formats
