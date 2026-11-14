#include "ro/formats/thor.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/byte_source.h"
#include "ro/core/error.h"

#include "support/grf_builder.h"
#include "support/thor_builder.h"

using ro::core::ErrorCode;
using ro::core::MemoryByteSource;
using ro::formats::ThorArchive;
using ro::formats::ThorTarget;
using ro::test_support::BuildThor;
using ro::test_support::Bytes;
using ro::test_support::ThorBuildOptions;
using ro::test_support::ThorFileSpec;

namespace {

ro::core::Result<ThorArchive> OpenBytes(std::vector<std::uint8_t> archive) {
  return ThorArchive::Open(std::make_unique<MemoryByteSource>(std::move(archive)));
}

std::string Text(const std::vector<std::uint8_t>& bytes) { return {bytes.begin(), bytes.end()}; }

std::vector<ThorFileSpec> SampleFiles() {
  return {
      {"data\\readme.txt", Bytes("Bonjour depuis le patch"), false},
      {"data\\vide.txt", {}, false},
      // Un nom coréen en CP949 : ses octets traversent sans conversion.
      {"data\\texture\\\xC0\xAF\xC0\xFA.bmp", Bytes("BM"), false},
  };
}

// Une archive produite par l'outil de patch de référence, sur un contenu choisi :
// « a.txt » = « hello », « sub\b.txt » = quarante « A », avec l'entrée
// d'intégrité. Aucun octet du jeu.
const std::vector<std::uint8_t> kReferenceArchive = {
    0x41, 0x53, 0x53, 0x46, 0x20, 0x28, 0x43, 0x29, 0x20, 0x32, 0x30, 0x30, 0x37, 0x20, 0x41,
    0x65, 0x6f, 0x6d, 0x69, 0x6e, 0x20, 0x44, 0x45, 0x56, 0x01, 0x03, 0x00, 0x00, 0x00, 0x30,
    0x00, 0x00, 0x48, 0x00, 0x00, 0x00, 0x6c, 0x00, 0x00, 0x00, 0x78, 0x9c, 0xcb, 0x48, 0xcd,
    0xc9, 0xc9, 0x07, 0x00, 0x06, 0x2c, 0x02, 0x15, 0x78, 0x9c, 0x73, 0x74, 0x24, 0x0e, 0x00,
    0x00, 0xd0, 0x5c, 0x0a, 0x29, 0x78, 0x9c, 0x4b, 0xd4, 0x2b, 0xa9, 0x28, 0xb1, 0x35, 0xa8,
    0x30, 0x36, 0x33, 0x34, 0x48, 0x34, 0xb3, 0x30, 0xe3, 0xe5, 0x2a, 0x2e, 0x4d, 0x8a, 0x49,
    0x82, 0x8a, 0x1a, 0x25, 0xa6, 0x5a, 0x5a, 0x24, 0x1b, 0x1b, 0xf0, 0x72, 0x01, 0x00, 0xf6,
    0xb8, 0x0b, 0x7c, 0x78, 0x9c, 0x2d, 0x8a, 0x31, 0x0a, 0xc0, 0x20, 0x10, 0x04, 0xaf, 0x49,
    0x11, 0x48, 0xf2, 0x8e, 0x40, 0xc0, 0x2e, 0x0f, 0xf0, 0x1f, 0x36, 0x27, 0x8a, 0xd8, 0x58,
    0xe8, 0x0a, 0xfa, 0x7b, 0x4f, 0xb1, 0x18, 0xd8, 0x59, 0xe6, 0x71, 0x0c, 0x56, 0x31, 0xc1,
    0x87, 0x1c, 0xd1, 0x49, 0x13, 0xd1, 0x27, 0xbc, 0xc2, 0xc1, 0x0a, 0x0d, 0x6b, 0xde, 0x53,
    0x85, 0xb3, 0x54, 0x6b, 0xec, 0xba, 0x7f, 0xd1, 0x6b, 0x97, 0x03, 0x19, 0x74, 0x0c, 0x80,
};

}  // namespace

TEST_CASE("THOR : aller-retour d'une archive de fusion GRF", "[formats][thor]") {
  auto archive = OpenBytes(BuildThor(SampleFiles()));
  REQUIRE(archive.has_value());
  CHECK(archive->merges_into_grf());
  CHECK(archive->target() == ThorTarget::kGrf);
  CHECK(archive->target_grf_name().empty());
  CHECK_FALSE(archive->has_integrity());
  REQUIRE(archive->entries().size() == 3);

  const std::vector<ThorFileSpec> expected = SampleFiles();
  for (std::size_t index = 0; index < expected.size(); ++index) {
    const auto& entry = archive->entries()[index];
    CHECK(entry.path == expected[index].path);
    CHECK_FALSE(entry.is_removed);
    CHECK(entry.real_size == expected[index].content.size());
    auto content = archive->Read(entry);
    REQUIRE(content.has_value());
    CHECK(*content == expected[index].content);
  }
  CHECK(archive->VerifyIntegrity().has_value());
}

TEST_CASE("THOR : dossier du jeu et GRF cible nommée", "[formats][thor]") {
  auto game = OpenBytes(BuildThor(SampleFiles(), ThorBuildOptions{.merge_into_grf = false}));
  REQUIRE(game.has_value());
  CHECK(game->target() == ThorTarget::kGameDirectory);

  auto named = OpenBytes(BuildThor(SampleFiles(), ThorBuildOptions{.target_grf_name = "moon.grf"}));
  REQUIRE(named.has_value());
  CHECK(named->target_grf_name() == "moon.grf");
  CHECK(Text(*named->Read(named->entries().front())) == "Bonjour depuis le patch");
}

TEST_CASE("THOR : une entrée supprimée n'a pas de contenu", "[formats][thor]") {
  auto archive = OpenBytes(BuildThor({
      {"data\\ancien.txt", {}, true},
      {"data\\neuf.txt", Bytes("neuf"), false},
  }));
  REQUIRE(archive.has_value());
  REQUIRE(archive->entries().size() == 2);
  const auto& removed = archive->entries()[0];
  CHECK(removed.is_removed);
  CHECK(removed.real_size == 0);
  auto content = archive->Read(removed);
  REQUIRE_FALSE(content.has_value());
  CHECK(content.error().code == ErrorCode::kInvalidArgument);
}

TEST_CASE("THOR : un chemin répété, c'est la dernière entrée qui gagne", "[formats][thor]") {
  auto archive = OpenBytes(BuildThor({
      {"ddraw.dll", Bytes("premier"), false},
      {"autre.txt", Bytes("x"), false},
      {"ddraw.dll", Bytes("second"), false},
  }));
  REQUIRE(archive.has_value());
  const auto* found = archive->Find("DDRAW.DLL");
  REQUIRE(found != nullptr);
  CHECK(Text(*archive->Read(*found)) == "second");
  CHECK(archive->Find("data/readme.txt") == nullptr);
}

TEST_CASE("THOR : Find ignore la casse ASCII et le sens des séparateurs", "[formats][thor]") {
  auto archive = OpenBytes(BuildThor(SampleFiles()));
  REQUIRE(archive.has_value());
  const auto* found = archive->Find("DATA/Readme.TXT");
  REQUIRE(found != nullptr);
  CHECK(found->path == "data\\readme.txt");
}

TEST_CASE("THOR : l'entrée d'intégrité se range à part et se vérifie", "[formats][thor]") {
  std::vector<ThorFileSpec> files = SampleFiles();
  files.push_back({"data\\parti.txt", {}, true});
  auto archive = OpenBytes(BuildThor(files, ThorBuildOptions{.include_integrity = true}));
  REQUIRE(archive.has_value());
  CHECK(archive->has_integrity());
  CHECK(archive->entries().size() == files.size());
  CHECK(archive->Find(ro::formats::kThorIntegrityEntryName) == nullptr);

  auto checksums = archive->ReadChecksums();
  REQUIRE(checksums.has_value());
  REQUIRE(checksums->size() == 3);
  CHECK(checksums->front().path == "data\\readme.txt");
  CHECK(archive->VerifyIntegrity().has_value());
}

TEST_CASE("THOR : une somme fausse se détecte", "[formats][thor]") {
  // Le contenu change après le calcul des sommes : on bâtit deux archives et on
  // greffe la table d'intégrité de la première sur les données de la seconde.
  std::vector<ThorFileSpec> original = {{"a.txt", Bytes("hello"), false}};
  std::vector<ThorFileSpec> altered = {{"a.txt", Bytes("hellO"), false}};
  std::vector<std::uint8_t> bytes =
      BuildThor(original, ThorBuildOptions{.include_integrity = true});
  const std::vector<std::uint8_t> other =
      BuildThor(altered, ThorBuildOptions{.include_integrity = true});
  REQUIRE(bytes.size() == other.size());
  // Les deux archives ne diffèrent que dans le flux de « a.txt » et dans la
  // table : on prend le flux modifié et on garde la table d'origine.
  auto original_archive = OpenBytes(bytes);
  REQUIRE(original_archive.has_value());
  const auto& entry = original_archive->entries().front();
  const auto data_begin = static_cast<std::ptrdiff_t>(entry.data_offset);
  const auto data_end = data_begin + static_cast<std::ptrdiff_t>(entry.compressed_size);
  std::copy(other.begin() + data_begin, other.begin() + data_end, bytes.begin() + data_begin);

  auto archive = OpenBytes(std::move(bytes));
  REQUIRE(archive.has_value());
  auto verdict = archive->VerifyIntegrity();
  REQUIRE_FALSE(verdict.has_value());
  CHECK(verdict.error().code == ErrorCode::kCorruptData);
}

TEST_CASE("THOR : une archive de l'outil de référence se lit", "[formats][thor]") {
  auto archive = OpenBytes(kReferenceArchive);
  REQUIRE(archive.has_value());
  CHECK(archive->merges_into_grf());
  CHECK(archive->has_integrity());
  REQUIRE(archive->entries().size() == 2);
  const auto* hello = archive->Find("a.txt");
  const auto* letters = archive->Find("sub\\b.txt");
  REQUIRE(hello != nullptr);
  REQUIRE(letters != nullptr);
  CHECK(Text(*archive->Read(*hello)) == "hello");
  CHECK(Text(*archive->Read(*letters)) == std::string(40, 'A'));
  CHECK(archive->VerifyIntegrity().has_value());
}

TEST_CASE("THOR : toute troncature est refusée ou illisible", "[formats][thor]") {
  const std::vector<std::uint8_t> whole =
      BuildThor(SampleFiles(), ThorBuildOptions{.include_integrity = true});
  for (std::size_t kept = 0; kept < whole.size(); ++kept) {
    auto archive = OpenBytes({whole.begin(), whole.begin() + static_cast<std::ptrdiff_t>(kept)});
    CAPTURE(kept);
    // La table est à la fin : une troncature la coupe toujours.
    CHECK_FALSE(archive.has_value());
  }
}

TEST_CASE("THOR : en-têtes et tables incohérents refusés", "[formats][thor]") {
  SECTION("signature") {
    std::vector<std::uint8_t> bytes = BuildThor(SampleFiles());
    bytes[0] = 'X';
    auto archive = OpenBytes(std::move(bytes));
    REQUIRE_FALSE(archive.has_value());
    CHECK(archive.error().code == ErrorCode::kBadSignature);
  }
  SECTION("mode de table inconnu") {
    auto archive = OpenBytes(BuildThor(SampleFiles(), ThorBuildOptions{.table_mode = 0x21}));
    REQUIRE_FALSE(archive.has_value());
    CHECK(archive.error().code == ErrorCode::kUnsupportedVersion);
  }
  SECTION("nombre d'entrées démenti par la table") {
    auto archive = OpenBytes(BuildThor(SampleFiles(), ThorBuildOptions{.declared_entry_count = 7}));
    REQUIRE_FALSE(archive.has_value());
    CHECK(archive.error().code == ErrorCode::kCorruptData);
  }
  SECTION("chemin qui sort de la cible") {
    for (const char* path : {"..\\evil.dll", "data\\..\\..\\x", "\\abs.txt", "C:\\x.txt"}) {
      CAPTURE(path);
      auto archive = OpenBytes(BuildThor({{path, Bytes("x"), false}}));
      REQUIRE_FALSE(archive.has_value());
      CHECK(archive.error().code == ErrorCode::kCorruptData);
    }
  }
  SECTION("flux de données corrompu") {
    std::vector<std::uint8_t> bytes = BuildThor({{"a.txt", Bytes("hello hello hello"), false}});
    auto intact = OpenBytes(bytes);
    REQUIRE(intact.has_value());
    const std::size_t data_at = intact->entries().front().data_offset;
    // L'octet d'en-tête zlib : sa somme de contrôle ne tient plus.
    bytes[data_at] ^= 0xFFU;
    auto archive = OpenBytes(std::move(bytes));
    REQUIRE(archive.has_value());
    auto content = archive->Read(archive->entries().front());
    REQUIRE_FALSE(content.has_value());
    CHECK(content.error().code == ErrorCode::kCorruptData);
  }
}
