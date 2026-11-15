// L'allègement d'une archive GRF de ce qu'une autre a déjà.

#include "ro/formats/grf_delta.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/byte_sink.h"
#include "ro/core/byte_source.h"
#include "ro/formats/grf.h"
#include "ro/formats/grf_writer.h"

#include "support/grf_builder.h"

using ro::core::MemoryByteSink;
using ro::core::MemoryByteSource;
using ro::formats::GrfArchive;
using ro::formats::WriteGrfDelta;
using ro::test_support::BuildGrf;
using ro::test_support::Bytes;

namespace {

constexpr ro::test_support::GrfBuildOptions kVersion3{.major = 3};

[[nodiscard]] std::vector<std::string> NamesOf(const std::vector<std::uint8_t>& archive_bytes) {
  MemoryByteSource source(archive_bytes);
  auto listing = ro::formats::ListGrfEntries(source);
  REQUIRE(listing.has_value());
  std::vector<std::string> names;
  for (const auto& file : listing->files) {
    names.push_back(file.name);
  }
  return names;
}

[[nodiscard]] std::string Text(const std::vector<std::uint8_t>& archive_bytes,
                               std::string_view path) {
  auto archive = GrfArchive::Open(std::make_unique<MemoryByteSource>(archive_bytes));
  REQUIRE(archive.has_value());
  auto content = archive->Read(path);
  REQUIRE(content.has_value());
  return {content->begin(), content->end()};
}

}  // namespace

TEST_CASE("Allègement : seuls les fichiers nouveaux ou changés restent, noms d'origine gardés",
          "[formats][grf][delta]") {
  MemoryByteSource base(BuildGrf({{"data\\un.txt", Bytes("un")},
                                  {"data\\deux.txt", Bytes("deux")},
                                  {"data\\perdu.txt", Bytes("perdu")}},
                                 kVersion3));
  MemoryByteSource newer(BuildGrf({{"DATA/Un.TXT", Bytes("un")},
                                   {"data\\deux.txt", Bytes("DEUX")},
                                   {"data\\Quatre.txt", Bytes("quatre")}},
                                  kVersion3));
  MemoryByteSink delta;
  auto report = WriteGrfDelta(base, newer, delta);
  REQUIRE(report.has_value());
  CHECK(report->identical_files == 1);
  CHECK(report->changed_files == 1);
  CHECK(report->added_files == 1);

  CHECK(NamesOf(delta.bytes()) == std::vector<std::string>{"data\\deux.txt", "data\\Quatre.txt"});
  CHECK(Text(delta.bytes(), "data/deux.txt") == "DEUX");
  CHECK(Text(delta.bytes(), "data/quatre.txt") == "quatre");
}

TEST_CASE("Allègement : un même fichier compressé autrement est identique",
          "[formats][grf][delta]") {
  // Un octet après la fin du flux zlib : un autre flux compressé, le même
  // fichier une fois décompressé.
  std::vector<std::uint8_t> padded_stream = ro::test_support::Deflate(Bytes("pareil"));
  padded_stream.push_back(0);
  MemoryByteSink base_bytes;
  auto writer = ro::formats::GrfWriter::Begin(base_bytes);
  REQUIRE(writer.has_value());
  REQUIRE(writer->AddCompressed("data\\pareil.txt", padded_stream, 6).has_value());
  REQUIRE(writer->Finish().has_value());

  MemoryByteSource base(base_bytes.bytes());
  MemoryByteSource newer(BuildGrf({{"data\\pareil.txt", Bytes("pareil")}}));
  MemoryByteSink delta;
  auto report = WriteGrfDelta(base, newer, delta);
  REQUIRE(report.has_value());
  CHECK(report->identical_files == 1);
  CHECK(NamesOf(delta.bytes()).empty());
}

TEST_CASE("Allègement : pour un même chemin, la dernière entrée de la table l'emporte",
          "[formats][grf][delta]") {
  MemoryByteSource base(BuildGrf({{"data\\a.txt", Bytes("ancien")}}));
  MemoryByteSource newer(
      BuildGrf({{"data\\a.txt", Bytes("ancien")}, {"data\\A.txt", Bytes("nouveau")}}));
  MemoryByteSink delta;
  auto report = WriteGrfDelta(base, newer, delta);
  REQUIRE(report.has_value());
  CHECK(report->changed_files == 1);
  CHECK(report->identical_files == 0);
  CHECK(NamesOf(delta.bytes()) == std::vector<std::string>{"data\\A.txt"});
  CHECK(Text(delta.bytes(), "data/a.txt") == "nouveau");
}
