#include "ro/formats/grf.h"

#include <algorithm>
#include <span>
#include <utility>

#include <zlib.h>

#include "ro/core/ascii.h"
#include "ro/core/byte_reader.h"

namespace ro::formats {
namespace {

using core::ByteReader;
using core::ErrorCode;
using core::Fail;
using core::Result;

// Les deux signatures font 15 octets. Celle du 3.0 contient un NUL en position
// 13 : elle ne peut pas s'écrire comme une chaîne C, d'où la longueur explicite.
constexpr std::string_view kSignatureV2{"Master of Magic", 15};
constexpr std::string_view kSignatureV3{"Event Horizon\0c", 15};

// LA DISPOSITION DE L'EN-TÊTE : signature et clé sur trente octets, puis
// l'offset de la table (quatre octets en 2.0, huit en 3.0), le `seed` du 2.0,
// le nombre de fichiers et la version.
constexpr std::size_t kGrfTableOffsetOffset = 30;
constexpr std::size_t kGrfVersionOffset = 42;
// En 3.0, les octets hauts de l'offset de table recouvrent le `seed` du 2.0 :
// ceux-là décident de la largeur de l'offset. Voir `Open`.
constexpr std::size_t kGrfSeedHighOffset = 35;
constexpr std::size_t kGrfSeedHighBytes = 3;
// Ce que la table 3.0 porte avant ses données compressées.
constexpr std::uint64_t kGrfV3TablePaddingBytes = 4;

// Bornes de cohérence, confrontées aux tailles annoncées AVANT toute allocation.
// Une taille corrompue dans un en-tête ne doit pas se transformer en demande de
// plusieurs gibioctets : c'est le mode d'échec classique d'un parseur d'archive.
//
// Ordres de grandeur réels : la table d'un GRF officiel pèse quelques mébioctets
// décompressée, et son plus gros fichier (BGM, grande texture) reste sous les
// 100 Mio. Les deux plafonds laissent donc une marge large.
constexpr std::uint32_t kMaxTableSize = 256U * 1024U * 1024U;
constexpr std::uint32_t kMaxEntrySize = 512U * 1024U * 1024U;

// Ce qu'une entrée porte APRÈS son nom, avant son offset de données : taille
// compressée, taille alignée, taille réelle, drapeaux. 4 + 4 + 4 + 1.
constexpr std::size_t kEntryMetadataBytes = 13;

// Décompresse en exigeant EXACTEMENT `expected_size` octets.
//
// Un flux GRF peut porter des octets au-delà de la taille réelle annoncée ; le
// client tronque, on tronque aussi. Le seul critère qui compte est donc « a-t-on
// produit la taille attendue ? » — un test unique qui couvre à la fois le flux
// corrompu, le flux tronqué et l'échec d'inflate.
Result<std::vector<std::uint8_t>> InflateExactly(std::span<const std::uint8_t> input,
                                                 std::uint32_t expected_size) {
  std::vector<std::uint8_t> output(expected_size);
  if (expected_size == 0) {
    return output;
  }
  if (input.empty()) {
    return Fail(
        ErrorCode::kCorruptData,
        "flux zlib vide alors que " + std::to_string(expected_size) + " octets sont annoncés");
  }

  z_stream stream{};
  if (inflateInit(&stream) != Z_OK) {
    return Fail(ErrorCode::kCorruptData, "inflateInit a échoué");
  }
  // zlib ne modifie pas l'entrée, mais son API ne le dit pas dans le type.
  stream.next_in = const_cast<Bytef*>(static_cast<const Bytef*>(input.data()));
  stream.avail_in = static_cast<uInt>(input.size());
  stream.next_out = output.data();
  stream.avail_out = static_cast<uInt>(output.size());

  static_cast<void>(inflate(&stream, Z_FINISH));
  const uLong produced = stream.total_out;
  inflateEnd(&stream);

  if (produced != expected_size) {
    return Fail(ErrorCode::kCorruptData, "flux zlib : " + std::to_string(produced) +
                                             " octets produits sur " +
                                             std::to_string(expected_size) + " attendus");
  }
  return output;
}

// La même normalisation que `NormalizeGrfPath`, mais SUR PLACE, dans la table
// décompressée. C'est ce qui permet aux clés de n'être que des vues.
std::string_view NormalizeInPlace(std::span<std::uint8_t> name) {
  for (std::uint8_t& byte : name) {
    byte = byte == '/' ? static_cast<std::uint8_t>('\\') : core::AsciiLower(byte);
  }
  return {reinterpret_cast<const char*>(name.data()), name.size()};
}

// L'EN-TÊTE LU ET LA TABLE DÉCOMPRESSÉE, ce que partagent la lecture pour
// chercher et la lecture pour réécrire.
struct DecodedTable {
  std::uint32_t version = 0;
  std::vector<std::uint8_t> table;
  bool entry_offsets_are_64bit = false;
};

Result<DecodedTable> DecodeTable(core::ByteSource& source) {
  auto header = source.ReadBlock(0, kGrfHeaderSize);
  if (!header) {
    return std::unexpected(header.error());
  }

  ByteReader reader(*header);
  const std::span<const std::uint8_t> signature_bytes = reader.Bytes(kSignatureV2.size());
  const std::string_view signature(reinterpret_cast<const char*>(signature_bytes.data()),
                                   signature_bytes.size());
  if (signature != kSignatureV2 && signature != kSignatureV3) {
    return Fail(ErrorCode::kBadSignature,
                "ni « Master of Magic » ni « Event Horizon » — GRF chiffré ou format inconnu");
  }

  reader.Seek(kGrfVersionOffset);
  DecodedTable decoded;
  decoded.version = reader.U32();
  const std::uint32_t major = decoded.version >> 8;

  // Deux décisions INDÉPENDANTES ; les confondre casse le cas du 2.0 déguisé.
  //
  // La largeur des offsets d'ENTRÉES ne dépend que de la version majeure : une
  // table 3.0 porte des entrées de 21 octets, toujours.
  //
  // La largeur de l'offset de TABLE dépend en plus des octets 35..37. S'ils ne
  // sont pas tous nuls, le fichier porte encore le champ `seed` du 2.0 là où le
  // 3.0 met les octets hauts de son offset : le lire en u64 donnerait une
  // position absurde. C'est le comportement de GRFEditor.
  bool table_offset_is_64bit = false;
  std::uint64_t table_padding = 0;
  switch (major) {
    case 2:
      break;
    case 3:
      decoded.entry_offsets_are_64bit = true;
      table_offset_is_64bit =
          std::ranges::all_of(std::span(*header).subspan(kGrfSeedHighOffset, kGrfSeedHighBytes),
                              [](std::uint8_t byte) { return byte == 0; });
      table_padding = kGrfV3TablePaddingBytes;
      break;
    default:
      return Fail(ErrorCode::kUnsupportedVersion, "GRF version " + std::to_string(decoded.version));
  }

  reader.Seek(kGrfTableOffsetOffset);
  const std::uint64_t table_offset = table_offset_is_64bit ? reader.U64() : reader.U32();
  if (!reader.ok()) {
    return Fail(ErrorCode::kUnexpectedEnd, "en-tête GRF tronqué");
  }
  if (table_offset > source.size()) {
    return Fail(ErrorCode::kCorruptData,
                "offset de table " + std::to_string(table_offset) + " au-delà du fichier");
  }

  const std::uint64_t table_header_at = kGrfHeaderSize + table_offset + table_padding;
  auto table_header = source.ReadBlock(table_header_at, 8);
  if (!table_header) {
    return std::unexpected(table_header.error());
  }
  ByteReader table_reader(*table_header);
  const std::uint32_t compressed_size = table_reader.U32();
  const std::uint32_t uncompressed_size = table_reader.U32();
  if (uncompressed_size > kMaxTableSize) {
    return Fail(ErrorCode::kCorruptData,
                "table annoncée à " + std::to_string(uncompressed_size) + " octets");
  }

  auto compressed_table = source.ReadBlock(table_header_at + 8, compressed_size);
  if (!compressed_table) {
    return std::unexpected(compressed_table.error());
  }
  auto table = InflateExactly(*compressed_table, uncompressed_size);
  if (!table) {
    return std::unexpected(table.error());
  }
  decoded.table = std::move(*table);
  return decoded;
}

// PARCOURT LES ENTRÉES DE LA TABLE, répertoires compris, en passant à `visit`
// la position et la longueur du nom, puis ses métadonnées.
//
// On avance jusqu'au bout du tampon plutôt que de faire confiance au compteur
// d'entrées de l'en-tête : il se lit différemment d'une version à l'autre
// (le 2.0 lui applique `- seed - 7`), et la table est de toute façon la seule
// source qui fasse foi. GRFEditor procède pareil.
template <typename Visit>
Result<void> WalkEntries(std::span<const std::uint8_t> table, bool entry_offsets_are_64bit,
                         Visit&& visit) {
  ByteReader entry_reader(table);
  std::size_t walked_entries = 0;
  while (entry_reader.remaining() > 0) {
    const std::size_t name_at = entry_reader.offset();
    const std::size_t name_size = entry_reader.CString().size();
    // 4 + 4 + 4 + 1 = 13 octets, puis l'offset : 17 en 2.0, 21 en 3.0.
    GrfEntry entry;
    entry.compressed_size = entry_reader.U32();
    entry.aligned_size = entry_reader.U32();
    entry.real_size = entry_reader.U32();
    entry.flags = entry_reader.U8();
    entry.data_offset = entry_offsets_are_64bit ? entry_reader.U64() : entry_reader.U32();
    if (!entry_reader.ok()) {
      return Fail(ErrorCode::kCorruptData,
                  "table d'entrées tronquée après " + std::to_string(walked_entries) + " entrées");
    }
    visit(name_at, name_size, entry);
    ++walked_entries;
  }
  return {};
}

}  // namespace

Result<GrfListing> ListGrfEntries(core::ByteSource& source) {
  auto decoded = DecodeTable(source);
  if (!decoded) {
    return std::unexpected(decoded.error());
  }
  GrfListing listing;
  listing.version = decoded->version;
  const std::span<const std::uint8_t> table(decoded->table);
  auto walked = WalkEntries(
      table, decoded->entry_offsets_are_64bit,
      [&](std::size_t name_at, std::size_t name_size, const GrfEntry& entry) {
        if ((entry.flags & kGrfFlagFile) == 0) {
          return;
        }
        listing.files.push_back(GrfListedEntry{
            std::string(reinterpret_cast<const char*>(table.data() + name_at), name_size), entry});
      });
  if (!walked) {
    return std::unexpected(walked.error());
  }
  return listing;
}

std::string NormalizeGrfPath(std::string_view path) {
  std::string normalized;
  normalized.reserve(path.size());
  for (const char character : path) {
    const auto byte = static_cast<std::uint8_t>(character);
    // 0x2F ne peut pas être le second octet d'une paire CP949 (ils commencent à
    // 0x41), donc cette substitution ne peut pas abîmer un nom coréen.
    normalized.push_back(byte == '/' ? '\\' : static_cast<char>(core::AsciiLower(byte)));
  }
  return normalized;
}

Result<GrfArchive> GrfArchive::OpenFile(const std::filesystem::path& path) {
  auto source = core::FileByteSource::Open(path);
  if (!source) {
    return std::unexpected(source.error());
  }
  return Open(std::move(*source));
}

Result<GrfArchive> GrfArchive::Open(std::unique_ptr<core::ByteSource> source) {
  if (!source) {
    return Fail(ErrorCode::kIoFailure, "source nulle");
  }

  auto decoded = DecodeTable(*source);
  if (!decoded) {
    return std::unexpected(decoded.error());
  }
  const bool entry_offsets_are_64bit = decoded->entry_offsets_are_64bit;

  GrfArchive archive;
  archive.version_ = decoded->version;
  archive.table_ = std::move(decoded->table);

  // ⚠ COMPTER AVANT D'INSÉRER. Une archive officielle porte 339 000 entrées :
  // sans réservation, la table de hachage se redimensionne une vingtaine de
  // fois et REHACHE tout à chaque fois. Ce passage n'avance que des pointeurs
  // dans un tampon déjà en mémoire — il n'alloue rien, ne décode rien, et ne
  // juge de rien : il compte au plus large, répertoires compris, ce qui est
  // exactement ce qu'une réservation demande.
  ByteReader counter(archive.table_);
  const std::size_t offset_bytes = entry_offsets_are_64bit ? 8 : 4;
  std::size_t announced_entries = 0;
  while (counter.remaining() > 0) {
    static_cast<void>(counter.CString());
    counter.Skip(kEntryMetadataBytes + offset_bytes);
    if (!counter.ok()) {
      break;
    }
    ++announced_entries;
  }
  archive.entries_.reserve(announced_entries);

  // Le nom est lu POUR SES BORNES : ce qui entre dans la carte est une vue sur
  // ces mêmes octets, minusculés en place juste après.
  auto walked = WalkEntries(
      archive.table_, entry_offsets_are_64bit,
      [&](std::size_t name_at, std::size_t name_size, const GrfEntry& entry) {
        if ((entry.flags & kGrfFlagFile) == 0) {
          return;  // répertoire
        }
        archive.entries_.insert_or_assign(
            NormalizeInPlace(std::span(archive.table_).subspan(name_at, name_size)), entry);
      });
  if (!walked) {
    return std::unexpected(walked.error());
  }

  archive.source_ = std::move(source);
  return archive;
}

const std::vector<std::string>& GrfArchive::names() const {
  if (!names_listed_) {
    names_.reserve(entries_.size());
    for (const auto& [name, entry] : entries_) {
      names_.emplace_back(name);
    }
    std::ranges::sort(names_);
    names_listed_ = true;
  }
  return names_;
}

std::vector<std::string_view> GrfArchive::NamesStartingWith(std::string_view prefix) const {
  const std::string wanted = NormalizeGrfPath(prefix);
  std::vector<std::string_view> kept;
  for (const auto& [name, entry] : entries_) {
    if (name.starts_with(wanted)) {
      kept.push_back(name);
    }
  }
  return kept;
}

const GrfEntry* GrfArchive::Find(std::string_view path) const {
  const auto found = entries_.find(NormalizeGrfPath(path));
  return found == entries_.end() ? nullptr : &found->second;
}

Result<std::vector<std::uint8_t>> GrfArchive::Read(std::string_view path) {
  const GrfEntry* entry = Find(path);
  if (entry == nullptr) {
    return Fail(ErrorCode::kNotFound, std::string(path));
  }
  return Read(*entry, path);
}

Result<std::vector<std::uint8_t>> GrfArchive::Read(const GrfEntry& entry,
                                                   std::string_view reported_path) {
  if (entry.is_encrypted()) {
    return Fail(ErrorCode::kEncrypted,
                std::string(reported_path) + " (flags " + std::to_string(entry.flags) + ")");
  }
  if (entry.real_size > kMaxEntrySize) {
    return Fail(ErrorCode::kCorruptData, std::string(reported_path) + " annoncé à " +
                                             std::to_string(entry.real_size) + " octets");
  }
  if (entry.real_size == 0) {
    return std::vector<std::uint8_t>{};
  }
  if (entry.data_offset > source_->size()) {
    return Fail(ErrorCode::kCorruptData, std::string(reported_path) + " : offset " +
                                             std::to_string(entry.data_offset) +
                                             " au-delà du fichier");
  }

  auto compressed = source_->ReadBlock(kGrfHeaderSize + entry.data_offset, entry.compressed_size);
  if (!compressed) {
    return std::unexpected(compressed.error());
  }
  return InflateExactly(*compressed, entry.real_size);
}

}  // namespace ro::formats
