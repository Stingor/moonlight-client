#include "ro/formats/grf_delta.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "ro/formats/grf.h"
#include "ro/formats/grf_writer.h"

namespace ro::formats {
namespace {

using core::ErrorCode;
using core::Fail;
using core::Result;

// Les octets bruts d'une entrée sur `wanted` octets, bornés à la fin de
// l'archive : la dernière entrée s'y arrête parfois avant ses zéros de
// complément.
[[nodiscard]] Result<std::vector<std::uint8_t>> RawBytes(core::ByteSource& source,
                                                         const GrfListedEntry& file,
                                                         std::uint64_t wanted) {
  const std::uint64_t data_at = kGrfHeaderSize + file.entry.data_offset;
  if (data_at > source.size()) {
    return Fail(ErrorCode::kCorruptData, file.name + " : données au-delà de l'archive");
  }
  return source.ReadBlock(data_at, std::min<std::uint64_t>(wanted, source.size() - data_at));
}

// Vrai si les deux entrées portent le même fichier. Un côté illisible n'est
// jamais identique : le fichier de `newer` est alors gardé tel quel.
[[nodiscard]] bool SameContent(core::ByteSource& base, const GrfListedEntry& base_file,
                               core::ByteSource& newer, const GrfListedEntry& newer_file) {
  const GrfEntry& base_entry = base_file.entry;
  const GrfEntry& newer_entry = newer_file.entry;
  if (base_entry.real_size != newer_entry.real_size) {
    return false;
  }
  const bool encrypted = base_entry.is_encrypted() || newer_entry.is_encrypted();
  // Le même flux compressé rend le même fichier : c'est le cas courant, et il
  // évite deux décompressions.
  if (encrypted || base_entry.compressed_size == newer_entry.compressed_size) {
    const std::uint64_t wanted =
        encrypted ? newer_entry.aligned_size : std::uint64_t{newer_entry.compressed_size};
    if (encrypted && (base_entry.flags != newer_entry.flags ||
                      base_entry.aligned_size != newer_entry.aligned_size)) {
      return false;
    }
    auto base_raw = RawBytes(base, base_file, wanted);
    auto newer_raw = RawBytes(newer, newer_file, wanted);
    if (!base_raw || !newer_raw) {
      return false;
    }
    if (*base_raw == *newer_raw) {
      return true;
    }
    if (encrypted) {
      return false;
    }
  }
  auto base_content = ReadGrfEntry(base, base_entry, base_file.name);
  auto newer_content = ReadGrfEntry(newer, newer_entry, newer_file.name);
  return base_content && newer_content && *base_content == *newer_content;
}

// Pour chaque chemin normalisé, l'indice de sa DERNIÈRE entrée dans la table.
[[nodiscard]] std::unordered_map<std::string, std::size_t> LastEntryByPath(
    const GrfListing& listing) {
  std::unordered_map<std::string, std::size_t> last_entry_by_path;
  last_entry_by_path.reserve(listing.files.size());
  for (std::size_t entry_index = 0; entry_index < listing.files.size(); ++entry_index) {
    last_entry_by_path.insert_or_assign(NormalizeGrfPath(listing.files[entry_index].name),
                                        entry_index);
  }
  return last_entry_by_path;
}

}  // namespace

Result<GrfDeltaReport> WriteGrfDelta(core::ByteSource& base, core::ByteSource& newer,
                                     core::ByteSink& delta) {
  auto base_listing = ListGrfEntries(base);
  if (!base_listing) {
    return std::unexpected(base_listing.error());
  }
  auto newer_listing = ListGrfEntries(newer);
  if (!newer_listing) {
    return std::unexpected(newer_listing.error());
  }
  const std::unordered_map<std::string, std::size_t> base_by_path = LastEntryByPath(*base_listing);
  const std::unordered_map<std::string, std::size_t> newer_by_path =
      LastEntryByPath(*newer_listing);

  auto writer = GrfWriter::Begin(delta);
  if (!writer) {
    return std::unexpected(writer.error());
  }
  GrfDeltaReport report;
  for (std::size_t entry_index = 0; entry_index < newer_listing->files.size(); ++entry_index) {
    const GrfListedEntry& file = newer_listing->files[entry_index];
    const std::string path = NormalizeGrfPath(file.name);
    if (newer_by_path.at(path) != entry_index) {
      continue;
    }
    const auto in_base = base_by_path.find(path);
    if (in_base != base_by_path.end()) {
      if (SameContent(base, base_listing->files[in_base->second], newer, file)) {
        ++report.identical_files;
        continue;
      }
      ++report.changed_files;
    } else {
      ++report.added_files;
    }
    auto raw = RawBytes(newer, file, file.entry.aligned_size);
    if (!raw) {
      return std::unexpected(raw.error());
    }
    if (auto added = writer->AddRaw(file.name, file.entry, *raw); !added) {
      return std::unexpected(added.error());
    }
  }
  if (auto finished = writer->Finish(); !finished) {
    return std::unexpected(finished.error());
  }
  return report;
}

}  // namespace ro::formats
