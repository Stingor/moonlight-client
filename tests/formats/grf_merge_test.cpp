// La fusion d'un patch THOR dans une archive GRF, et l'écrivain qu'elle emploie.

#include "ro/formats/grf_merge.h"

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
#include "support/thor_builder.h"

using ro::core::MemoryByteSink;
using ro::core::MemoryByteSource;
using ro::formats::GrfArchive;
using ro::formats::MergeThorIntoGrf;
using ro::formats::ThorArchive;
using ro::test_support::Bytes;

namespace {

[[nodiscard]] ThorArchive OpenThor(const std::vector<ro::test_support::ThorFileSpec>& files) {
  auto patch =
      ThorArchive::Open(std::make_unique<MemoryByteSource>(ro::test_support::BuildThor(files)));
  REQUIRE(patch.has_value());
  return std::move(*patch);
}

[[nodiscard]] GrfArchive OpenGrf(const std::vector<std::uint8_t>& bytes) {
  auto archive = GrfArchive::Open(std::make_unique<MemoryByteSource>(bytes));
  REQUIRE(archive.has_value());
  return std::move(*archive);
}

[[nodiscard]] std::string Text(GrfArchive& archive, std::string_view path) {
  auto content = archive.Read(path);
  REQUIRE(content.has_value());
  return {content->begin(), content->end()};
}

}  // namespace

TEST_CASE("Écrivain GRF : ce qu'il écrit se relit, contenu, noms et taille alignée",
          "[formats][grf][writer]") {
  MemoryByteSink sink;
  auto writer = ro::formats::GrfWriter::Begin(sink);
  REQUIRE(writer.has_value());
  const auto stream = ro::test_support::Deflate(Bytes("bonjour"));
  REQUIRE(writer->AddCompressed("data\\Texte.TXT", stream, 7).has_value());
  REQUIRE(writer->Finish().has_value());

  MemoryByteSource source(sink.bytes());
  const auto listing = ro::formats::ListGrfEntries(source);
  REQUIRE(listing.has_value());
  CHECK(listing->version == 0x200);
  REQUIRE(listing->files.size() == 1);
  // La casse est gardée telle qu'écrite.
  CHECK(listing->files[0].name == "data\\Texte.TXT");
  CHECK(listing->files[0].entry.aligned_size % 8 == 0);
  CHECK(listing->files[0].entry.aligned_size >= stream.size());

  GrfArchive archive = OpenGrf(sink.bytes());
  CHECK(Text(archive, "data/texte.txt") == "bonjour");
}

TEST_CASE("Écrivain GRF : une sortie déjà entamée est refusée", "[formats][grf][writer]") {
  MemoryByteSink sink;
  REQUIRE(sink.Append(Bytes("x")).has_value());
  CHECK_FALSE(ro::formats::GrfWriter::Begin(sink).has_value());
}

TEST_CASE("Fusion : remplacer, ajouter, retirer, et garder le reste octet pour octet",
          "[formats][grf][merge]") {
  const auto previous_bytes = ro::test_support::BuildGrf({
      {"data\\Garde.txt", Bytes("inchangé")},
      {"data\\remplace.txt", Bytes("ancien")},
      {"data\\retire.txt", Bytes("à retirer")},
  });
  MemoryByteSource previous(previous_bytes);
  ThorArchive patch = OpenThor({
      // Même chemin dans une autre casse, et l'autre séparateur.
      {"DATA/Remplace.txt", Bytes("nouveau")},
      {"data\\ajout.txt", Bytes("ajouté")},
      {"data\\retire.txt", {}, true},
      // Une suppression d'un fichier absent ne compte pas.
      {"data\\jamais.txt", {}, true},
  });

  MemoryByteSink merged;
  const auto report = MergeThorIntoGrf(patch, &previous, merged);
  REQUIRE(report.has_value());
  CHECK(report->kept_files == 1);
  CHECK(report->replaced_files == 1);
  CHECK(report->added_files == 1);
  CHECK(report->removed_files == 1);

  GrfArchive archive = OpenGrf(merged.bytes());
  CHECK(archive.entry_count() == 3);
  CHECK(Text(archive, "data\\garde.txt") == "inchangé");
  CHECK(Text(archive, "data\\remplace.txt") == "nouveau");
  CHECK(Text(archive, "data\\ajout.txt") == "ajouté");
  CHECK_FALSE(archive.Contains("data\\retire.txt"));

  // Le fichier gardé porte toujours son nom d'origine.
  MemoryByteSource merged_source(merged.bytes());
  const auto listing = ro::formats::ListGrfEntries(merged_source);
  REQUIRE(listing.has_value());
  CHECK(listing->files[0].name == "data\\Garde.txt");
}

TEST_CASE("Fusion : pour un même chemin, la dernière entrée du patch l'emporte",
          "[formats][grf][merge]") {
  ThorArchive patch = OpenThor({
      {"a.txt", Bytes("un")},
      {"b.txt", Bytes("b")},
      {"A.txt", Bytes("deux")},
      {"b.txt", {}, true},
      {"B.txt", Bytes("revenu")},
  });
  MemoryByteSink merged;
  const auto report = MergeThorIntoGrf(patch, nullptr, merged);
  REQUIRE(report.has_value());
  CHECK(report->added_files == 2);

  GrfArchive archive = OpenGrf(merged.bytes());
  CHECK(archive.entry_count() == 2);
  CHECK(Text(archive, "a.txt") == "deux");
  CHECK(Text(archive, "b.txt") == "revenu");
}

TEST_CASE("Fusion : une archive autre que 2.0 n'est pas réécrite", "[formats][grf][merge]") {
  ro::test_support::GrfBuildOptions version3;
  version3.major = 3;
  const auto previous_bytes = ro::test_support::BuildGrf({{"a.txt", Bytes("a")}}, version3);
  MemoryByteSource previous(previous_bytes);
  ThorArchive patch = OpenThor({{"b.txt", Bytes("b")}});
  MemoryByteSink merged;
  const auto report = MergeThorIntoGrf(patch, &previous, merged);
  REQUIRE_FALSE(report.has_value());
  CHECK(report.error().code == ro::core::ErrorCode::kUnsupportedVersion);
}
