#include "support/grf_builder.h"

#include <cstring>
#include <span>
#include <string_view>

#include <zlib.h>

namespace ro::test_support {
namespace {

void AppendU32(std::vector<std::uint8_t>& out, std::uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8) {
    out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFU));
  }
}

void AppendU64(std::vector<std::uint8_t>& out, std::uint64_t value) {
  for (unsigned shift = 0; shift < 64; shift += 8) {
    out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFU));
  }
}

void PutU32(std::vector<std::uint8_t>& out, std::size_t at, std::uint32_t value) {
  for (unsigned index = 0; index < 4; ++index) {
    out[at + index] = static_cast<std::uint8_t>((value >> (index * 8)) & 0xFFU);
  }
}

void PutU64(std::vector<std::uint8_t>& out, std::size_t at, std::uint64_t value) {
  for (unsigned index = 0; index < 8; ++index) {
    out[at + index] = static_cast<std::uint8_t>((value >> (index * 8)) & 0xFFU);
  }
}

}  // namespace

std::vector<std::uint8_t> Deflate(std::span<const std::uint8_t> input) {
  uLongf produced = compressBound(static_cast<uLong>(input.size()));
  std::vector<std::uint8_t> output(produced);
  const int status =
      compress(output.data(), &produced, input.data(), static_cast<uLong>(input.size()));
  if (status != Z_OK) {
    return {};
  }
  output.resize(produced);
  return output;
}

std::vector<std::uint8_t> Bytes(std::string_view text) { return {text.begin(), text.end()}; }

std::vector<std::uint8_t> BuildGrf(const std::vector<GrfFileSpec>& files,
                                   const GrfBuildOptions& options) {
  const bool is_v3 = options.major == 3;
  const auto header_size = static_cast<std::size_t>(formats::kGrfHeaderSize);

  std::vector<std::uint8_t> archive(header_size, 0);
  const std::string_view signature =
      is_v3 ? std::string_view{"Event Horizon\0c", 15} : std::string_view{"Master of Magic", 15};
  std::memcpy(archive.data(), signature.data(), signature.size());

  // 1. Les blocs de données, chacun compressé, à la suite de l'en-tête.
  struct PlacedFile {
    std::uint64_t offset = 0;
    std::uint32_t compressed_size = 0;
    std::uint32_t real_size = 0;
  };
  std::vector<PlacedFile> placed;
  placed.reserve(files.size());
  for (const GrfFileSpec& file : files) {
    const std::vector<std::uint8_t> deflated = Deflate(file.content);
    placed.push_back({static_cast<std::uint64_t>(archive.size()) - formats::kGrfHeaderSize,
                      static_cast<std::uint32_t>(deflated.size()),
                      static_cast<std::uint32_t>(file.content.size())});
    archive.insert(archive.end(), deflated.begin(), deflated.end());
  }

  // 2. La table, non compressée pour l'instant.
  std::vector<std::uint8_t> table;
  for (std::size_t index = 0; index < files.size(); ++index) {
    table.insert(table.end(), files[index].name.begin(), files[index].name.end());
    table.push_back(0);
    AppendU32(table, placed[index].compressed_size);
    // Taille alignée : identique à la taille compressée hors chiffrement DES.
    AppendU32(table, placed[index].compressed_size);
    AppendU32(table, placed[index].real_size);
    table.push_back(files[index].flags);
    if (is_v3) {
      AppendU64(table, placed[index].offset);
    } else {
      AppendU32(table, static_cast<std::uint32_t>(placed[index].offset));
    }
  }

  // 3. L'offset de table pointe sur le REMBOURRAGE en 3.0, pas sur les tailles.
  const std::uint64_t table_offset =
      static_cast<std::uint64_t>(archive.size()) - formats::kGrfHeaderSize;
  if (is_v3) {
    archive.insert(archive.end(), 4, 0);
  }
  const std::vector<std::uint8_t> deflated_table = Deflate(table);
  AppendU32(archive, static_cast<std::uint32_t>(deflated_table.size()));
  AppendU32(archive, static_cast<std::uint32_t>(table.size()));
  archive.insert(archive.end(), deflated_table.begin(), deflated_table.end());

  // 4. Retour dans l'en-tête, une fois les positions connues.
  if (is_v3 && !options.legacy_seed_under_v3) {
    PutU64(archive, 30, table_offset);
  } else {
    PutU32(archive, 30, static_cast<std::uint32_t>(table_offset));
    if (options.legacy_seed_under_v3) {
      // Un seed dont l'octet 36 est non nul : c'est ce que le lecteur inspecte
      // pour refuser l'interprétation 64 bits.
      PutU32(archive, 34, 0x00A50000U);
    }
  }
  // Le 2.0 encode le compte comme entrées + seed + 7 ; le lecteur ne s'y fie
  // pas, mais l'archive doit rester fidèle au format.
  PutU32(archive, 38, static_cast<std::uint32_t>(files.size()) + (is_v3 ? 0U : 7U));
  PutU32(archive, 42, is_v3 ? 0x0300U : 0x0200U);

  return archive;
}

}  // namespace ro::test_support
