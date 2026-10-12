#include "ro/formats/thor.h"

#include <algorithm>
#include <charconv>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <zlib.h>

#include "ro/core/byte_reader.h"
#include "ro/formats/grf.h"

namespace ro::formats {
namespace {

using core::ByteReader;
using core::ErrorCode;
using core::Fail;
using core::Result;

constexpr std::string_view kSignature{"ASSF (C) 2007 Aeomin DEV"};

// Octet de cible : 0 pour le dossier du jeu, 1 pour une GRF.
constexpr std::uint8_t kTargetGameDirectory = 0;
constexpr std::uint8_t kTargetGrf = 1;

// Ce qui précède le nom de la GRF : signature, cible, nombre d'entrées, mode,
// longueur du nom. 24 + 1 + 4 + 2 + 1.
constexpr std::size_t kFixedHeaderBytes = kSignature.size() + 1 + 4 + 2 + 1;
// Ce qui suit le nom : taille compressée et position de la table.
constexpr std::size_t kTableLocatorBytes = 4 + 4;

// Bornes confrontées aux tailles annoncées AVANT toute allocation. Une entrée de
// table pèse au plus 1 + 255 + 1 + 12 octets ; 64 Mio en admettent des
// centaines de milliers, bien au-delà de toute archive de patch.
constexpr std::size_t kMaxTableSize = 64U * 1024U * 1024U;
constexpr std::uint32_t kMaxEntrySize = 512U * 1024U * 1024U;
// Morceau de sortie ajouté à chaque tour : la taille décompressée de la table
// n'est écrite nulle part.
constexpr std::size_t kInflateChunkBytes = 64U * 1024U;

constexpr int kHexBase = 16;

// Décompresse en exigeant EXACTEMENT `expected_size` octets et la fin du flux.
// La sortie a un octet de plus que prévu : un flux trop long le remplit et se
// trahit, et zlib refuse un tampon de sortie nul, cas d'un fichier vide.
Result<std::vector<std::uint8_t>> InflateExactly(std::span<const std::uint8_t> input,
                                                 std::uint32_t expected_size) {
  std::vector<std::uint8_t> output(static_cast<std::size_t>(expected_size) + 1);
  z_stream stream{};
  if (inflateInit(&stream) != Z_OK) {
    return Fail(ErrorCode::kCorruptData, "inflateInit a échoué");
  }
  // zlib ne modifie pas l'entrée, mais son API ne le dit pas dans le type.
  stream.next_in = const_cast<Bytef*>(static_cast<const Bytef*>(input.data()));
  stream.avail_in = static_cast<uInt>(input.size());
  stream.next_out = output.data();
  stream.avail_out = static_cast<uInt>(output.size());
  const int status = inflate(&stream, Z_FINISH);
  const uLong produced = stream.total_out;
  inflateEnd(&stream);
  if (status != Z_STREAM_END || produced != expected_size) {
    return Fail(ErrorCode::kCorruptData, "flux zlib : " + std::to_string(produced) +
                                             " octets produits sur " +
                                             std::to_string(expected_size) + " attendus");
  }
  output.resize(expected_size);
  return output;
}

// Décompresse un flux dont la taille finale est inconnue, sans dépasser
// `max_size` : une table forgée ne doit pas devenir une demande de gibioctets.
Result<std::vector<std::uint8_t>> InflateBounded(std::span<const std::uint8_t> input,
                                                 std::size_t max_size) {
  std::vector<std::uint8_t> output;
  z_stream stream{};
  if (inflateInit(&stream) != Z_OK) {
    return Fail(ErrorCode::kCorruptData, "inflateInit a échoué");
  }
  stream.next_in = const_cast<Bytef*>(static_cast<const Bytef*>(input.data()));
  stream.avail_in = static_cast<uInt>(input.size());
  int status = Z_OK;
  while (status == Z_OK) {
    if (output.size() >= max_size) {
      inflateEnd(&stream);
      return Fail(ErrorCode::kCorruptData,
                  "table THOR au-delà de " + std::to_string(max_size) + " octets");
    }
    const std::size_t produced_before = output.size();
    const std::size_t chunk = std::min(kInflateChunkBytes, max_size - produced_before);
    output.resize(produced_before + chunk);
    stream.next_out = output.data() + produced_before;
    stream.avail_out = static_cast<uInt>(chunk);
    status = inflate(&stream, Z_NO_FLUSH);
    output.resize(produced_before + (chunk - stream.avail_out));
  }
  inflateEnd(&stream);
  if (status != Z_STREAM_END) {
    return Fail(ErrorCode::kCorruptData, "table THOR : flux zlib incomplet ou corrompu");
  }
  return output;
}

bool IsSeparator(char character) { return character == '\\' || character == '/'; }

// Un chemin s'applique SOUS la racine de la cible, jamais ailleurs : ni absolu,
// ni lecteur, ni composant « .. ». Un octet CP949 de queue peut valoir « \ »,
// mais il suit alors un octet haut : il ne fabrique pas de composant « .. ».
bool IsContainedRelativePath(std::string_view path) {
  if (path.empty() || IsSeparator(path.front()) || path.find(':') != std::string_view::npos) {
    return false;
  }
  std::size_t component_start = 0;
  while (component_start <= path.size()) {
    std::size_t component_end = component_start;
    while (component_end < path.size() && !IsSeparator(path[component_end])) {
      ++component_end;
    }
    if (path.substr(component_start, component_end - component_start) == "..") {
      return false;
    }
    component_start = component_end + 1;
  }
  return true;
}

// Une ligne « chemin=0x1234abcd ». Le chemin peut contenir « = » : c'est le
// DERNIER qui sépare.
Result<ThorChecksum> ParseChecksumLine(std::string_view line) {
  const std::size_t equals = line.rfind('=');
  if (equals == std::string_view::npos || equals == 0) {
    return Fail(ErrorCode::kCorruptData, "ligne d'intégrité sans « = » : " + std::string(line));
  }
  std::string_view digits = line.substr(equals + 1);
  if (digits.starts_with("0x") || digits.starts_with("0X")) {
    digits.remove_prefix(2);
  }
  std::uint32_t crc32_value = 0;
  const auto [end, error] =
      std::from_chars(digits.data(), digits.data() + digits.size(), crc32_value, kHexBase);
  if (digits.empty() || error != std::errc{} || end != digits.data() + digits.size()) {
    return Fail(ErrorCode::kCorruptData, "somme d'intégrité illisible : " + std::string(line));
  }
  return ThorChecksum{std::string(line.substr(0, equals)), crc32_value};
}

}  // namespace

Result<ThorArchive> ThorArchive::OpenFile(const std::filesystem::path& path) {
  auto source = core::FileByteSource::Open(path);
  if (!source) {
    return std::unexpected(source.error());
  }
  return Open(std::move(*source));
}

Result<ThorArchive> ThorArchive::Open(std::unique_ptr<core::ByteSource> source) {
  if (!source) {
    return Fail(ErrorCode::kIoFailure, "source nulle");
  }
  auto fixed = source->ReadBlock(0, kFixedHeaderBytes);
  if (!fixed) {
    return std::unexpected(fixed.error());
  }
  ByteReader reader(*fixed);
  const std::span<const std::uint8_t> signature_bytes = reader.Bytes(kSignature.size());
  const std::string_view signature(reinterpret_cast<const char*>(signature_bytes.data()),
                                   signature_bytes.size());
  if (signature != kSignature) {
    return Fail(ErrorCode::kBadSignature, "pas une archive THOR");
  }
  const std::uint8_t target_byte = reader.U8();
  const std::uint32_t declared_entries = reader.U32();
  const std::uint16_t table_mode = reader.U16();
  const std::uint8_t target_name_bytes = reader.U8();
  if (target_byte != kTargetGameDirectory && target_byte != kTargetGrf) {
    return Fail(ErrorCode::kCorruptData, "octet de cible THOR " + std::to_string(target_byte));
  }
  if (table_mode != kThorTableModeCompressed) {
    return Fail(ErrorCode::kUnsupportedVersion, "mode de table THOR " + std::to_string(table_mode));
  }
  auto locator = source->ReadBlock(kFixedHeaderBytes, target_name_bytes + kTableLocatorBytes);
  if (!locator) {
    return std::unexpected(locator.error());
  }
  ByteReader locator_reader(*locator);
  const std::span<const std::uint8_t> target_name = locator_reader.Bytes(target_name_bytes);
  const std::uint32_t table_compressed_size = locator_reader.U32();
  const std::uint32_t table_offset = locator_reader.U32();
  const std::uint64_t header_end = kFixedHeaderBytes + locator->size();
  if (table_offset < header_end ||
      static_cast<std::uint64_t>(table_offset) + table_compressed_size > source->size()) {
    return Fail(ErrorCode::kCorruptData, "table THOR hors du fichier (position " +
                                             std::to_string(table_offset) + ", " +
                                             std::to_string(table_compressed_size) + " octets)");
  }
  auto compressed_table = source->ReadBlock(table_offset, table_compressed_size);
  if (!compressed_table) {
    return std::unexpected(compressed_table.error());
  }
  auto table = InflateBounded(*compressed_table, kMaxTableSize);
  if (!table) {
    return std::unexpected(table.error());
  }

  ThorArchive archive;
  archive.target_ = target_byte == kTargetGrf ? ThorTarget::kGrf : ThorTarget::kGameDirectory;
  archive.target_grf_name_.assign(target_name.begin(), target_name.end());

  std::uint64_t table_entries = 0;
  ByteReader entry_reader(*table);
  while (entry_reader.remaining() > 0) {
    ThorEntry entry;
    const std::span<const std::uint8_t> path = entry_reader.Bytes(entry_reader.U8());
    entry.path.assign(path.begin(), path.end());
    const std::uint8_t flags = entry_reader.U8();
    if ((flags & ~kThorFlagRemoved) != 0) {
      return Fail(ErrorCode::kUnsupportedVersion,
                  "drapeaux d'entrée THOR " + std::to_string(flags) + " : " + entry.path);
    }
    entry.is_removed = (flags & kThorFlagRemoved) != 0;
    if (!entry.is_removed) {
      entry.data_offset = entry_reader.U32();
      entry.compressed_size = entry_reader.U32();
      entry.real_size = entry_reader.U32();
    }
    if (!entry_reader.ok()) {
      return Fail(ErrorCode::kCorruptData,
                  "table THOR tronquée après " + std::to_string(table_entries) + " entrées");
    }
    if (!IsContainedRelativePath(entry.path)) {
      return Fail(ErrorCode::kCorruptData, "chemin THOR hors de la cible : « " + entry.path + " »");
    }
    const std::uint64_t data_end =
        static_cast<std::uint64_t>(entry.data_offset) + entry.compressed_size;
    if (!entry.is_removed && (entry.data_offset < header_end || data_end > table_offset ||
                              entry.real_size > kMaxEntrySize)) {
      return Fail(ErrorCode::kCorruptData, "données THOR incohérentes : " + entry.path);
    }
    ++table_entries;
    if (!entry.is_removed && entry.path == kThorIntegrityEntryName) {
      archive.integrity_ = std::move(entry);
    } else {
      archive.entries_.push_back(std::move(entry));
    }
  }
  if (table_entries != declared_entries) {
    return Fail(ErrorCode::kCorruptData, "table THOR : " + std::to_string(table_entries) +
                                             " entrées pour " + std::to_string(declared_entries) +
                                             " annoncées");
  }
  archive.source_ = std::move(source);
  return archive;
}

const ThorEntry* ThorArchive::Find(std::string_view path) const {
  const std::string wanted = NormalizeGrfPath(path);
  const auto found = std::ranges::find_if(
      entries_.rbegin(), entries_.rend(),
      [&wanted](const ThorEntry& entry) { return NormalizeGrfPath(entry.path) == wanted; });
  return found == entries_.rend() ? nullptr : &*found;
}

Result<std::vector<std::uint8_t>> ThorArchive::Read(const ThorEntry& entry) {
  if (entry.is_removed) {
    return Fail(ErrorCode::kInvalidArgument, "entrée supprimée, sans contenu : " + entry.path);
  }
  auto compressed = source_->ReadBlock(entry.data_offset, entry.compressed_size);
  if (!compressed) {
    return std::unexpected(compressed.error());
  }
  auto content = InflateExactly(*compressed, entry.real_size);
  if (!content) {
    return Fail(content.error().code, entry.path + " : " + content.error().detail);
  }
  return content;
}

Result<std::vector<std::uint8_t>> ThorArchive::ReadCompressed(const ThorEntry& entry) {
  if (entry.is_removed) {
    return Fail(ErrorCode::kInvalidArgument, "entrée supprimée, sans contenu : " + entry.path);
  }
  return source_->ReadBlock(entry.data_offset, entry.compressed_size);
}

Result<std::vector<ThorChecksum>> ThorArchive::ReadChecksums() {
  std::vector<ThorChecksum> checksums;
  if (!integrity_) {
    return checksums;
  }
  auto text = Read(*integrity_);
  if (!text) {
    return std::unexpected(text.error());
  }
  const std::string_view lines(reinterpret_cast<const char*>(text->data()), text->size());
  std::size_t line_start = 0;
  while (line_start < lines.size()) {
    std::size_t line_end = lines.find('\n', line_start);
    if (line_end == std::string_view::npos) {
      line_end = lines.size();
    }
    std::string_view line = lines.substr(line_start, line_end - line_start);
    if (line.ends_with('\r')) {
      line.remove_suffix(1);
    }
    line_start = line_end + 1;
    if (line.empty()) {
      continue;
    }
    auto checksum = ParseChecksumLine(line);
    if (!checksum) {
      return std::unexpected(checksum.error());
    }
    checksums.push_back(std::move(*checksum));
  }
  return checksums;
}

Result<void> ThorArchive::VerifyIntegrity() {
  if (!integrity_) {
    return {};
  }
  auto checksums = ReadChecksums();
  if (!checksums) {
    return std::unexpected(checksums.error());
  }
  // ⚠ L'INDEX SE BÂTIT UNE FOIS : `Find` normalise toute la table à chaque
  // appel, et une somme par fichier en ferait un parcours quadratique. La
  // dernière entrée d'un chemin l'emporte, comme dans `Find`.
  std::unordered_map<std::string, const ThorEntry*> by_path;
  by_path.reserve(entries_.size());
  for (const ThorEntry& entry : entries_) {
    by_path.insert_or_assign(NormalizeGrfPath(entry.path), &entry);
  }
  std::unordered_set<std::string> listed;
  for (const ThorChecksum& checksum : *checksums) {
    std::string wanted = NormalizeGrfPath(checksum.path);
    const auto found = by_path.find(wanted);
    const ThorEntry* entry = found == by_path.end() ? nullptr : found->second;
    if (entry == nullptr || entry->is_removed) {
      return Fail(ErrorCode::kCorruptData, "somme d'une entrée absente : " + checksum.path);
    }
    auto content = Read(*entry);
    if (!content) {
      return std::unexpected(content.error());
    }
    const auto computed = static_cast<std::uint32_t>(
        crc32(crc32(0L, Z_NULL, 0), content->data(), static_cast<uInt>(content->size())));
    if (computed != checksum.crc32) {
      return Fail(ErrorCode::kCorruptData, "somme fausse : " + checksum.path);
    }
    listed.insert(std::move(wanted));
  }
  for (const ThorEntry& entry : entries_) {
    if (!entry.is_removed && !listed.contains(NormalizeGrfPath(entry.path))) {
      return Fail(ErrorCode::kCorruptData, "entrée sans somme d'intégrité : " + entry.path);
    }
  }
  return {};
}

}  // namespace ro::formats
