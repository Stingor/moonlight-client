#include "ro/formats/grf.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/byte_source.h"
#include "ro/core/error.h"

#include "support/grf_builder.h"

using ro::core::ErrorCode;
using ro::core::MemoryByteSource;
using ro::formats::GrfArchive;
using ro::test_support::BuildGrf;
using ro::test_support::Bytes;
using ro::test_support::GrfBuildOptions;
using ro::test_support::GrfFileSpec;

namespace {

ro::core::Result<GrfArchive> OpenBytes(std::vector<std::uint8_t> archive) {
  return GrfArchive::Open(std::make_unique<MemoryByteSource>(std::move(archive)));
}

std::vector<GrfFileSpec> SampleFiles() {
  return {
      {"data\\readme.txt", Bytes("Bonjour depuis le GRF")},
      {"data\\sprite\\poring.spr", Bytes("des octets de sprite")},
      {"data\\vide.txt", {}},
  };
}

}  // namespace

TEST_CASE("GRF 2.0 : ouverture et lecture", "[formats][grf]") {
  auto archive = OpenBytes(BuildGrf(SampleFiles()));
  REQUIRE(archive.has_value());

  CHECK(archive->version() == 0x0200);
  CHECK(archive->major_version() == 2);
  CHECK(archive->entry_count() == 3);

  auto content = archive->Read("data\\readme.txt");
  REQUIRE(content.has_value());
  CHECK(std::string(content->begin(), content->end()) == "Bonjour depuis le GRF");
}

TEST_CASE("GRF 3.0 : ouverture et lecture", "[formats][grf]") {
  // Le 3.0 « Event Horizon » n'est pas chiffré : c'est le 2.0 avec un offset de
  // table sur 64 bits, un dword de rembourrage et des entrées de 21 octets.
  auto archive = OpenBytes(BuildGrf(SampleFiles(), GrfBuildOptions{.major = 3}));
  REQUIRE(archive.has_value());

  CHECK(archive->version() == 0x0300);
  CHECK(archive->entry_count() == 3);

  auto content = archive->Read("data\\sprite\\poring.spr");
  REQUIRE(content.has_value());
  CHECK(std::string(content->begin(), content->end()) == "des octets de sprite");
}

TEST_CASE("GRF 3.0 portant encore un seed 2.0", "[formats][grf]") {
  // Un fichier signé « Event Horizon » dont les octets 35..37 ne sont pas nuls
  // porte le seed du 2.0 là où le 3.0 met les octets hauts de son offset : lire
  // un u64 donnerait une position absurde. Le lecteur doit retomber sur 32 bits.
  auto archive =
      OpenBytes(BuildGrf(SampleFiles(), GrfBuildOptions{.major = 3, .legacy_seed_under_v3 = true}));
  REQUIRE(archive.has_value());
  CHECK(archive->entry_count() == 3);
  CHECK(archive->Contains("data\\readme.txt"));
}

TEST_CASE("GRF : la recherche est insensible à la casse et au séparateur", "[formats][grf]") {
  auto archive = OpenBytes(BuildGrf(SampleFiles()));
  REQUIRE(archive.has_value());

  CHECK(archive->Contains("data\\readme.txt"));
  CHECK(archive->Contains("DATA\\README.TXT"));
  CHECK(archive->Contains("data/readme.txt"));
  CHECK(archive->Contains("DATA/Sprite/Poring.SPR"));
  CHECK_FALSE(archive->Contains("data\\absent.txt"));
}

TEST_CASE("GRF : les noms sont triés", "[formats][grf]") {
  auto archive = OpenBytes(BuildGrf(SampleFiles()));
  REQUIRE(archive.has_value());

  const std::vector<std::string>& names = archive->names();
  REQUIRE(names.size() == 3);
  CHECK(std::is_sorted(names.begin(), names.end()));
  CHECK(names.front() == "data\\readme.txt");
}

TEST_CASE("GRF : l'inventaire par préfixe ne rend que ce qui commence ainsi", "[formats][grf]") {
  // ⚠ SANS LA LISTE TRIÉE : `names()` copie et trie toute l'archive, quand un
  // inventaire filtré n'en garde qu'une poignée. Le préfixe s'écrit dans
  // n'importe quelle casse, avec l'un ou l'autre séparateur.
  auto archive = OpenBytes(BuildGrf(SampleFiles()));
  REQUIRE(archive.has_value());

  const std::vector<std::string_view> sprites = archive->NamesStartingWith("DATA/Sprite/");
  REQUIRE(sprites.size() == 1);
  CHECK(sprites.front() == "data\\sprite\\poring.spr");

  std::vector<std::string_view> everything = archive->NamesStartingWith("data\\");
  std::ranges::sort(everything);
  CHECK(everything == std::vector<std::string_view>{"data\\readme.txt", "data\\sprite\\poring.spr",
                                                    "data\\vide.txt"});
  CHECK(archive->NamesStartingWith("data\\absent").empty());
}

TEST_CASE("GRF : une archive déplacée reste consultable", "[formats][grf]") {
  // ⚠ CE QUE CET ESSAI GARDE : les clés de l'index sont des VUES sur la table
  // décompressée que l'archive garde. Un déplacement doit donc emporter les
  // octets SANS les bouger. Le VFS range ses archives dans un `vector`, qui les
  // déplace dès qu'il grandit : une table qui se réallouerait au passage
  // laisserait un index pointant dans le vide, et la panne ne se verrait qu'au
  // second GRF monté.
  std::vector<ro::formats::GrfArchive> shelved;
  for (int fois = 0; fois < 4; ++fois) {
    auto archive = OpenBytes(BuildGrf(SampleFiles()));
    REQUIRE(archive.has_value());
    shelved.push_back(std::move(*archive));
  }

  for (const ro::formats::GrfArchive& archive : shelved) {
    CHECK(archive.entry_count() == 3);
    CHECK(archive.Contains("data\\readme.txt"));
    CHECK(archive.Contains("DATA/Sprite/Poring.SPR"));
    REQUIRE(archive.names().size() == 3);
    CHECK(archive.names().front() == "data\\readme.txt");
  }
}

TEST_CASE("GRF : un fichier vide rend un contenu vide", "[formats][grf]") {
  auto archive = OpenBytes(BuildGrf(SampleFiles()));
  REQUIRE(archive.has_value());

  auto content = archive->Read("data\\vide.txt");
  REQUIRE(content.has_value());
  CHECK(content->empty());
}

TEST_CASE("GRF : les répertoires ne sont pas indexés", "[formats][grf]") {
  std::vector<GrfFileSpec> files = SampleFiles();
  files.push_back({"data\\sprite", {}, 0x00});  // bit 0 absent : répertoire

  auto archive = OpenBytes(BuildGrf(files));
  REQUIRE(archive.has_value());
  CHECK(archive->entry_count() == 3);
  CHECK_FALSE(archive->Contains("data\\sprite"));
}

TEST_CASE("GRF : une entrée chiffrée est refusée, pas devinée", "[formats][grf]") {
  std::vector<GrfFileSpec> files = SampleFiles();
  files.push_back({"data\\protege.txt", Bytes("chiffre"), ro::formats::kGrfFlagFile | 0x02});

  auto archive = OpenBytes(BuildGrf(files));
  REQUIRE(archive.has_value());
  // Elle est bien indexée — c'est un fichier — mais illisible sans la clé DES.
  CHECK(archive->Contains("data\\protege.txt"));

  auto content = archive->Read("data\\protege.txt");
  REQUIRE_FALSE(content.has_value());
  CHECK(content.error().code == ErrorCode::kEncrypted);
}

TEST_CASE("GRF : entrée absente", "[formats][grf]") {
  auto archive = OpenBytes(BuildGrf(SampleFiles()));
  REQUIRE(archive.has_value());

  auto content = archive->Read("data\\jamais_vu.txt");
  REQUIRE_FALSE(content.has_value());
  CHECK(content.error().code == ErrorCode::kNotFound);
}

TEST_CASE("GRF : signature invalide", "[formats][grf]") {
  std::vector<std::uint8_t> archive = BuildGrf(SampleFiles());
  archive[0] = 'X';

  auto opened = OpenBytes(std::move(archive));
  REQUIRE_FALSE(opened.has_value());
  CHECK(opened.error().code == ErrorCode::kBadSignature);
}

TEST_CASE("GRF : version non gérée", "[formats][grf]") {
  std::vector<std::uint8_t> archive = BuildGrf(SampleFiles());
  archive[43] = 0x04;  // version 0x0400

  auto opened = OpenBytes(std::move(archive));
  REQUIRE_FALSE(opened.has_value());
  CHECK(opened.error().code == ErrorCode::kUnsupportedVersion);
}

TEST_CASE("GRF : offset de table au-delà du fichier", "[formats][grf]") {
  std::vector<std::uint8_t> archive = BuildGrf(SampleFiles());
  archive[33] = 0xFF;  // octet haut de l'offset de table

  auto opened = OpenBytes(std::move(archive));
  REQUIRE_FALSE(opened.has_value());
  CHECK(opened.error().code == ErrorCode::kCorruptData);
}

TEST_CASE("GRF : flux de données corrompu", "[formats][grf]") {
  std::vector<std::uint8_t> archive = BuildGrf(SampleFiles());
  // Le premier bloc de données commence juste après l'en-tête de 46 octets ;
  // on abîme son en-tête zlib.
  archive[48] ^= 0xFF;

  auto opened = OpenBytes(std::move(archive));
  REQUIRE(opened.has_value());  // la table, elle, reste saine

  auto content = opened->Read("data\\readme.txt");
  REQUIRE_FALSE(content.has_value());
  CHECK(content.error().code == ErrorCode::kCorruptData);
}

TEST_CASE("GRF : aucune troncature ne fait planter le lecteur", "[formats][grf]") {
  // Mini-fuzz déterministe, et le test qui compte le plus de cette suite.
  //
  // Une archive tronquée à n'importe quelle longueur doit rendre une erreur ou
  // s'ouvrir, jamais lire hors bornes. C'est le mode d'échec classique d'un
  // parseur d'archive, et un GRF vient du disque du joueur : il PEUT être
  // incomplet (téléchargement interrompu, disque abîmé). Sous ASan, ce test
  // transforme le moindre débordement en échec de CI.
  const std::vector<std::uint8_t> complete = BuildGrf(SampleFiles());

  for (std::size_t length = 0; length <= complete.size(); ++length) {
    std::vector<std::uint8_t> truncated(complete.begin(),
                                        complete.begin() + static_cast<std::ptrdiff_t>(length));
    auto opened = OpenBytes(std::move(truncated));
    if (!opened.has_value()) {
      continue;
    }
    // Ouverte malgré la troncature : chaque lecture doit rester bornée.
    for (const std::string& name : opened->names()) {
      static_cast<void>(opened->Read(name));
    }
  }
  SUCCEED("aucune troncature n'a provoqué de lecture hors bornes");
}

TEST_CASE("GRF : aucune inversion d'octet ne fait planter le lecteur", "[formats][grf]") {
  // Même intention que la troncature, sur l'autre axe : un octet abîmé n'importe
  // où dans l'en-tête ou la table.
  const std::vector<std::uint8_t> complete = BuildGrf(SampleFiles());
  const std::size_t probe_limit = std::min<std::size_t>(complete.size(), 200);

  for (std::size_t index = 0; index < probe_limit; ++index) {
    std::vector<std::uint8_t> damaged = complete;
    damaged[index] ^= 0xFF;
    auto opened = OpenBytes(std::move(damaged));
    if (!opened.has_value()) {
      continue;
    }
    for (const std::string& name : opened->names()) {
      static_cast<void>(opened->Read(name));
    }
  }
  SUCCEED("aucune corruption d'octet n'a provoqué de lecture hors bornes");
}

TEST_CASE("NormalizeGrfPath garde la queue d'une paire CP949", "[formats][grf]") {
  // 81 41 et 81 61 sont deux syllabes différentes ; la lettre qui suit la paire,
  // elle, est bien une lettre.
  CHECK(ro::formats::NormalizeGrfPath("\x81\x41Z") == "\x81\x41z");
  CHECK(ro::formats::NormalizeGrfPath("\x81\x61Z") == "\x81\x61z");
  // Un séparateur n'est jamais une queue, et un octet haut en fin de nom n'a pas
  // de paire.
  CHECK(ro::formats::NormalizeGrfPath("\x81/A") == "\x81\\a");
  CHECK(ro::formats::NormalizeGrfPath("A\x81") == "a\x81");
}

TEST_CASE("GRF : deux syllabes qui ne diffèrent que par la casse de la queue restent deux",
          "[formats][grf]") {
  const std::string upper_tail = "data\\\x81\x41.bmp";
  const std::string lower_tail = "data\\\x81\x61.bmp";
  auto archive =
      OpenBytes(BuildGrf({{upper_tail, Bytes("majuscule")}, {lower_tail, Bytes("minuscule")}}));
  REQUIRE(archive.has_value());
  CHECK(archive->entry_count() == 2);
  auto upper_read = archive->Read(upper_tail);
  REQUIRE(upper_read.has_value());
  CHECK(*upper_read == Bytes("majuscule"));
  auto lower_read = archive->Read(lower_tail);
  REQUIRE(lower_read.has_value());
  CHECK(*lower_read == Bytes("minuscule"));
}

TEST_CASE("NormalizeGrfPath", "[formats][grf]") {
  CHECK(ro::formats::NormalizeGrfPath("DATA/Sprite/Foo.SPR") == "data\\sprite\\foo.spr");
  CHECK(ro::formats::NormalizeGrfPath("") == "");
  // Les octets coréens traversent sans être touchés.
  CHECK(ro::formats::NormalizeGrfPath("data\\\xC0\xCE\xB0\xA3") == "data\\\xC0\xCE\xB0\xA3");
}
