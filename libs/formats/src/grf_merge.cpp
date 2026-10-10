#include "ro/formats/grf_merge.h"

#include <algorithm>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ro/formats/grf.h"
#include "ro/formats/grf_writer.h"

namespace ro::formats {
namespace {

using core::ErrorCode;
using core::Fail;
using core::Result;

constexpr std::uint32_t kGrfMajorVersion2 = 2;

// L'EFFET FINAL DU PATCH, chemin par chemin : l'indice de sa dernière entrée.
struct FinalChanges {
  std::unordered_map<std::string, std::size_t> last_entry_by_path;
  // Les indices retenus, dans l'ordre du patch.
  std::vector<std::size_t> ordered_entries;
};

[[nodiscard]] FinalChanges CollectFinalChanges(std::span<const ThorEntry> entries) {
  FinalChanges changes;
  std::vector<std::string> normalized_paths;
  normalized_paths.reserve(entries.size());
  for (std::size_t entry_index = 0; entry_index < entries.size(); ++entry_index) {
    normalized_paths.push_back(NormalizeGrfPath(entries[entry_index].path));
    changes.last_entry_by_path.insert_or_assign(normalized_paths.back(), entry_index);
  }
  for (std::size_t entry_index = 0; entry_index < entries.size(); ++entry_index) {
    if (changes.last_entry_by_path.at(normalized_paths[entry_index]) == entry_index) {
      changes.ordered_entries.push_back(entry_index);
    }
  }
  return changes;
}

// Recopie les fichiers de l'ancienne archive que le patch ne touche pas.
[[nodiscard]] Result<void> CopyUntouched(core::ByteSource& previous, const GrfListing& listing,
                                         const FinalChanges& changes, GrfWriter& writer,
                                         GrfMergeReport& report) {
  for (const GrfListedEntry& file : listing.files) {
    if (changes.last_entry_by_path.contains(NormalizeGrfPath(file.name))) {
      continue;
    }
    const std::uint64_t data_at = kGrfHeaderSize + file.entry.data_offset;
    if (data_at > previous.size()) {
      return Fail(ErrorCode::kCorruptData, file.name + " : données au-delà de l'archive");
    }
    const std::uint64_t readable =
        std::min<std::uint64_t>(file.entry.aligned_size, previous.size() - data_at);
    auto bytes = previous.ReadBlock(data_at, readable);
    if (!bytes) {
      return std::unexpected(bytes.error());
    }
    if (auto added = writer.AddRaw(file.name, file.entry, *bytes); !added) {
      return added;
    }
    ++report.kept_files;
  }
  return {};
}

// Écrit l'effet final du patch, et compte ce qu'il remplace ou retire.
[[nodiscard]] Result<void> WritePatchEntries(ThorArchive& patch, const FinalChanges& changes,
                                             const std::unordered_set<std::string>& previous_paths,
                                             GrfWriter& writer, GrfMergeReport& report) {
  const std::span<const ThorEntry> entries = patch.entries();
  for (const std::size_t entry_index : changes.ordered_entries) {
    const ThorEntry& entry = entries[entry_index];
    const bool was_there = previous_paths.contains(NormalizeGrfPath(entry.path));
    if (entry.is_removed) {
      if (was_there) {
        ++report.removed_files;
      }
      continue;
    }
    if (auto proven = patch.Read(entry); !proven) {
      return std::unexpected(proven.error());
    }
    auto stream = patch.ReadCompressed(entry);
    if (!stream) {
      return std::unexpected(stream.error());
    }
    if (auto added = writer.AddCompressed(entry.path, *stream, entry.real_size); !added) {
      return added;
    }
    if (was_there) {
      ++report.replaced_files;
    } else {
      ++report.added_files;
    }
  }
  return {};
}

}  // namespace

Result<GrfMergeReport> MergeThorIntoGrf(ThorArchive& patch, core::ByteSource* previous,
                                        core::ByteSink& merged) {
  const FinalChanges changes = CollectFinalChanges(patch.entries());
  std::optional<GrfListing> listing;
  std::unordered_set<std::string> previous_paths;
  if (previous != nullptr) {
    auto listed = ListGrfEntries(*previous);
    if (!listed) {
      return std::unexpected(listed.error());
    }
    if ((listed->version >> 8) != kGrfMajorVersion2) {
      return Fail(ErrorCode::kUnsupportedVersion, "seule une archive GRF 2.0 se réécrit");
    }
    for (const GrfListedEntry& file : listed->files) {
      previous_paths.insert(NormalizeGrfPath(file.name));
    }
    listing = std::move(*listed);
  }

  auto writer = GrfWriter::Begin(merged);
  if (!writer) {
    return std::unexpected(writer.error());
  }
  GrfMergeReport report;
  if (listing.has_value()) {
    if (auto copied = CopyUntouched(*previous, *listing, changes, *writer, report); !copied) {
      return std::unexpected(copied.error());
    }
  }
  if (auto written = WritePatchEntries(patch, changes, previous_paths, *writer, report); !written) {
    return std::unexpected(written.error());
  }
  if (auto finished = writer->Finish(); !finished) {
    return std::unexpected(finished.error());
  }
  return report;
}

}  // namespace ro::formats
