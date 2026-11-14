#include "support/thor_builder.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <string_view>

#include <zlib.h>

#include "ro/formats/thor.h"

#include "support/grf_builder.h"

namespace ro::test_support {
namespace {

constexpr std::string_view kSignature{"ASSF (C) 2007 Aeomin DEV"};
// « 0x » et huit chiffres hexadécimaux, puis le NUL de snprintf.
constexpr std::size_t kChecksumTextBytes = 11;

void AppendU16(std::vector<std::uint8_t>& out, std::uint16_t value) {
  out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
  out.push_back(static_cast<std::uint8_t>(value >> 8U));
}

void AppendU32(std::vector<std::uint8_t>& out, std::uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8) {
    out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFU));
  }
}

void AppendName(std::vector<std::uint8_t>& out, std::string_view name) {
  out.push_back(static_cast<std::uint8_t>(name.size()));
  out.insert(out.end(), name.begin(), name.end());
}

std::uint32_t Crc32Of(const std::vector<std::uint8_t>& content) {
  return static_cast<std::uint32_t>(
      crc32(crc32(0L, Z_NULL, 0), content.data(), static_cast<uInt>(content.size())));
}

}  // namespace

std::vector<std::uint8_t> BuildThor(const std::vector<ThorFileSpec>& files,
                                    const ThorBuildOptions& options) {
  std::vector<ThorFileSpec> placed_files;
  if (options.include_integrity) {
    std::string listing;
    for (const ThorFileSpec& file : files) {
      if (file.removed) {
        continue;
      }
      std::array<char, kChecksumTextBytes> hex{};
      std::snprintf(hex.data(), hex.size(), "0x%08x", Crc32Of(file.content));
      listing += file.path + "=" + hex.data() + "\r\n";
    }
    placed_files.push_back({std::string(formats::kThorIntegrityEntryName), Bytes(listing), false});
  }
  placed_files.insert(placed_files.end(), files.begin(), files.end());

  std::vector<std::uint8_t> archive(kSignature.begin(), kSignature.end());
  archive.push_back(options.merge_into_grf ? 1 : 0);
  AppendU32(archive,
            options.declared_entry_count.value_or(static_cast<std::uint32_t>(placed_files.size())));
  AppendU16(archive, options.table_mode);
  AppendName(archive, options.target_grf_name);
  const std::size_t locator_at = archive.size();
  AppendU32(archive, 0);
  AppendU32(archive, 0);

  std::vector<std::uint8_t> table;
  for (const ThorFileSpec& file : placed_files) {
    AppendName(table, file.path);
    table.push_back(file.removed ? formats::kThorFlagRemoved : 0);
    if (file.removed) {
      continue;
    }
    const std::vector<std::uint8_t> deflated = Deflate(file.content);
    AppendU32(table, static_cast<std::uint32_t>(archive.size()));
    AppendU32(table, static_cast<std::uint32_t>(deflated.size()));
    AppendU32(table, static_cast<std::uint32_t>(file.content.size()));
    archive.insert(archive.end(), deflated.begin(), deflated.end());
  }

  const std::vector<std::uint8_t> deflated_table = Deflate(table);
  std::vector<std::uint8_t> locator;
  AppendU32(locator, static_cast<std::uint32_t>(deflated_table.size()));
  AppendU32(locator, static_cast<std::uint32_t>(archive.size()));
  std::ranges::copy(locator, archive.begin() + static_cast<std::ptrdiff_t>(locator_at));
  archive.insert(archive.end(), deflated_table.begin(), deflated_table.end());
  return archive;
}

}  // namespace ro::test_support
