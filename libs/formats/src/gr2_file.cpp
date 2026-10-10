#include "gr2_file.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "ro/core/byte_reader.h"
#include "ro/formats/gr2_codecs.h"

namespace ro::formats::gr2 {
namespace {

// LA SIGNATURE : seize octets, suivis de la taille de l'en-tête et de son
// format, puis de huit octets réservés.
constexpr std::array<std::uint8_t, 16> kMagic = {0xB8, 0x67, 0xB0, 0xCA, 0xF8, 0x6D, 0xB1, 0x0F,
                                                 0x84, 0x72, 0x8C, 0x7E, 0x5E, 0x19, 0x00, 0x1E};
// La même, écrite par une machine à l'autre boutisme.
constexpr std::array<std::uint8_t, 16> kReversedMagic = {
    0xCA, 0xB0, 0x67, 0xB8, 0x0F, 0xB1, 0x6D, 0xF8, 0x7E, 0x8C, 0x72, 0x84, 0x1E, 0x00, 0x19, 0x5E};
constexpr std::size_t kMagicBlockBytes = 32;
constexpr std::uint32_t kSupportedRevision = 6;

// Une section : ses onze mots, puis ses corrections de pointeurs, de douze
// octets chacune — l'endroit dans la section, la section visée, le décalage
// visé.
constexpr std::size_t kSectionHeaderBytes = 44;
constexpr std::size_t kFixupBytes = 12;
enum class Compression : std::uint32_t { kNone = 0, kDictionary = 1 };

// Les sections se posent à partir de cette adresse, alignées : l'adresse 0
// reste le pointeur nul.
constexpr std::uint32_t kFirstSectionAddress = 16;
constexpr std::uint32_t kSectionAlignment = 16;
// Au-delà, un fichier n'est pas un modèle d'acteur : on refuse plutôt que
// d'allouer ce qu'il annonce.
constexpr std::uint64_t kLargestLoadedBytes = 256ULL * 1024 * 1024;

struct SectionHeader {
  std::uint32_t compression = 0;
  std::uint32_t data_offset = 0;
  std::uint32_t packed_size = 0;
  std::uint32_t unpacked_size = 0;
  std::array<std::uint32_t, 2> stops{};
  std::uint32_t fixup_offset = 0;
  std::uint32_t fixup_count = 0;
};

core::Result<std::vector<std::uint8_t>> SectionBytes(std::span<const std::uint8_t> file,
                                                     const SectionHeader& section) {
  if (std::uint64_t{section.data_offset} + section.packed_size > file.size()) {
    return core::Fail(core::ErrorCode::kUnexpectedEnd, "Granny : section hors du fichier");
  }
  const std::span<const std::uint8_t> packed =
      file.subspan(section.data_offset, section.packed_size);
  switch (static_cast<Compression>(section.compression)) {
    case Compression::kNone: {
      std::vector<std::uint8_t> bytes(section.unpacked_size, 0);
      std::copy_n(packed.begin(), std::min<std::size_t>(packed.size(), bytes.size()),
                  bytes.begin());
      return bytes;
    }
    case Compression::kDictionary:
      return DecompressGr2Section(packed, section.stops, section.unpacked_size);
  }
  return core::Fail(core::ErrorCode::kUnsupportedVersion,
                    "Granny : compression inconnue " + std::to_string(section.compression));
}

}  // namespace

std::uint32_t Gr2Memory::U32(std::uint32_t address) {
  if (!Holds(address, sizeof(std::uint32_t))) {
    ok_ = false;
    return 0;
  }
  core::ByteReader reader(std::span<const std::uint8_t>(bytes_).subspan(address));
  return reader.U32();
}

std::uint16_t Gr2Memory::U16(std::uint32_t address) {
  if (!Holds(address, sizeof(std::uint16_t))) {
    ok_ = false;
    return 0;
  }
  core::ByteReader reader(std::span<const std::uint8_t>(bytes_).subspan(address));
  return reader.U16();
}

std::uint8_t Gr2Memory::U8(std::uint32_t address) {
  if (!Holds(address, 1)) {
    ok_ = false;
    return 0;
  }
  return bytes_[address];
}

float Gr2Memory::F32(std::uint32_t address) { return std::bit_cast<float>(U32(address)); }

std::string Gr2Memory::String(std::uint32_t address) {
  if (address == 0) {
    return {};
  }
  if (!Holds(address, 1)) {
    ok_ = false;
    return {};
  }
  std::string text;
  for (std::size_t at = address; at < bytes_.size(); ++at) {
    if (bytes_[at] == 0) {
      return text;
    }
    text.push_back(static_cast<char>(bytes_[at]));
  }
  ok_ = false;
  return {};
}

bool Gr2Memory::Holds(std::uint64_t address, std::uint64_t size) const noexcept {
  return address <= bytes_.size() && size <= bytes_.size() - address;
}

std::span<const std::uint8_t> Gr2Memory::Bytes(std::uint32_t address, std::uint32_t size) {
  if (!Holds(address, size)) {
    ok_ = false;
    return {};
  }
  return std::span<const std::uint8_t>(bytes_).subspan(address, size);
}

core::Result<Gr2File> LoadGr2File(std::span<const std::uint8_t> bytes) {
  if (bytes.size() < kMagicBlockBytes) {
    return core::Fail(core::ErrorCode::kUnexpectedEnd, "Granny : fichier trop court");
  }
  if (std::equal(kReversedMagic.begin(), kReversedMagic.end(), bytes.begin())) {
    return core::Fail(core::ErrorCode::kUnsupportedVersion, "Granny : fichier d'autre boutisme");
  }
  if (!std::equal(kMagic.begin(), kMagic.end(), bytes.begin())) {
    return core::Fail(core::ErrorCode::kBadSignature, "Granny : signature absente");
  }
  core::ByteReader reader(bytes);
  reader.Seek(kMagic.size());
  const std::uint32_t header_size = reader.U32();
  const std::uint32_t header_format = reader.U32();
  if (header_format != 0) {
    return core::Fail(core::ErrorCode::kUnsupportedVersion, "Granny : en-tête d'un autre format");
  }
  const std::span<const std::uint8_t> header = bytes.subspan(
      kMagicBlockBytes, std::min<std::size_t>(header_size, bytes.size() - kMagicBlockBytes));
  core::ByteReader fields(header);
  const std::uint32_t revision = fields.U32();
  fields.Skip(2 * sizeof(std::uint32_t));  // la taille totale, puis la somme de contrôle
  const std::uint32_t section_array = fields.U32();
  const std::uint32_t section_count = fields.U32();
  const std::array<std::uint32_t, 2> root_type_ref = {fields.U32(), fields.U32()};
  const std::array<std::uint32_t, 2> root_object_ref = {fields.U32(), fields.U32()};
  if (!fields.ok()) {
    return core::Fail(core::ErrorCode::kUnexpectedEnd, "Granny : en-tête tronqué");
  }
  if (revision != kSupportedRevision) {
    return core::Fail(core::ErrorCode::kUnsupportedVersion,
                      "Granny : révision " + std::to_string(revision));
  }
  std::vector<SectionHeader> sections(fields.AtMostRemaining(section_count));
  fields.Seek(section_array);
  for (SectionHeader& section : sections) {
    section.compression = fields.U32();
    section.data_offset = fields.U32();
    section.packed_size = fields.U32();
    section.unpacked_size = fields.U32();
    fields.Skip(sizeof(std::uint32_t));  // l'alignement demandé
    section.stops = {fields.U32(), fields.U32()};
    section.fixup_offset = fields.U32();
    section.fixup_count = fields.U32();
    fields.Skip(2 * sizeof(std::uint32_t));  // les corrections d'un autre boutisme
  }
  if (!fields.ok() || sections.size() != section_count) {
    return core::Fail(core::ErrorCode::kUnexpectedEnd, "Granny : table des sections tronquée");
  }

  // Les sections, posées bout à bout ; une section vide n'a pas d'adresse, et
  // tout pointeur vers elle devient nul.
  std::vector<std::uint32_t> bases(sections.size(), 0);
  std::uint64_t end = kFirstSectionAddress;
  for (std::size_t index = 0; index < sections.size(); ++index) {
    if (sections[index].unpacked_size == 0) {
      continue;
    }
    end = (end + kSectionAlignment - 1) & ~std::uint64_t{kSectionAlignment - 1};
    bases[index] = static_cast<std::uint32_t>(end);
    end += sections[index].unpacked_size;
    if (end > kLargestLoadedBytes) {
      return core::Fail(core::ErrorCode::kCorruptData, "Granny : sections démesurées");
    }
  }
  std::vector<std::uint8_t> memory(end, 0);
  for (std::size_t index = 0; index < sections.size(); ++index) {
    if (bases[index] == 0) {
      continue;
    }
    auto section = SectionBytes(bytes, sections[index]);
    if (!section) {
      return std::unexpected(section.error());
    }
    std::copy(section->begin(), section->end(), memory.begin() + bases[index]);
  }
  for (std::size_t index = 0; index < sections.size(); ++index) {
    const SectionHeader& section = sections[index];
    if (bases[index] == 0 || section.fixup_count == 0) {
      continue;
    }
    if (std::uint64_t{section.fixup_offset} + std::uint64_t{section.fixup_count} * kFixupBytes >
        bytes.size()) {
      return core::Fail(core::ErrorCode::kUnexpectedEnd, "Granny : corrections tronquées");
    }
    core::ByteReader fixups(bytes.subspan(section.fixup_offset));
    for (std::uint32_t fixup = 0; fixup < section.fixup_count; ++fixup) {
      const std::uint32_t at = fixups.U32();
      const std::uint32_t target_section = fixups.U32();
      const std::uint32_t target_offset = fixups.U32();
      if (target_section >= bases.size() ||
          std::uint64_t{at} + sizeof(std::uint32_t) > section.unpacked_size) {
        return core::Fail(core::ErrorCode::kCorruptData, "Granny : correction hors section");
      }
      const std::uint32_t target_base = bases[target_section];
      const std::uint32_t pointer = target_base == 0 ? 0 : target_base + target_offset;
      std::uint8_t* slot = memory.data() + bases[index] + at;
      for (std::size_t byte = 0; byte < sizeof(std::uint32_t); ++byte) {
        slot[byte] = static_cast<std::uint8_t>(pointer >> (8 * byte));
      }
    }
  }
  const auto address_of = [&](const std::array<std::uint32_t, 2>& reference) -> std::uint32_t {
    if (reference[0] >= bases.size() || bases[reference[0]] == 0) {
      return 0;
    }
    return bases[reference[0]] + reference[1];
  };
  const Gr2Object root{address_of(root_type_ref), address_of(root_object_ref)};
  if (!root.present()) {
    return core::Fail(core::ErrorCode::kCorruptData, "Granny : racine absente");
  }
  return Gr2File(Gr2Memory(std::move(memory)), root);
}

}  // namespace ro::formats::gr2
