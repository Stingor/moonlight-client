// Le manifeste du miroir de fichiers, l'adresse d'un fichier, et le cache
// local des empreintes.

#include "patcher/mirror_manifest.h"

#include <string>

#include <catch2/catch_test_macros.hpp>

#include "patcher/mirror_cache.h"

using moonclient::patcher::FormatMirrorCache;
using moonclient::patcher::KnownFile;
using moonclient::patcher::MirrorCache;
using moonclient::patcher::MirrorFileUrl;
using moonclient::patcher::ParseMirrorCache;
using moonclient::patcher::ParseMirrorManifest;

namespace {

const std::string kShaA(64, 'a');
const std::string kShaB(64, 'b');

[[nodiscard]] std::string Manifest(const std::string& lines) {
  return "moonclient-mirror 1\n" + lines;
}

}  // namespace

TEST_CASE("Miroir : le manifeste se lit, chemin espacé et fins de ligne Windows compris",
          "[app][patcher][mirror]") {
  const auto files = ParseMirrorManifest(
      Manifest(kShaA + " 12 SystemEN/table.lua\r\n\n" + kShaB + " 0 Données/mon fichier.txt\n"));
  REQUIRE(files.has_value());
  REQUIRE(files->size() == 2);
  CHECK((*files)[0].relative_path == "SystemEN/table.lua");
  CHECK((*files)[0].size_bytes == 12);
  CHECK((*files)[0].sha256_hex == kShaA);
  CHECK((*files)[1].relative_path == "Données/mon fichier.txt");
  CHECK((*files)[1].size_bytes == 0);
}

TEST_CASE("Miroir : un manifeste vide de fichiers est valide, une page sans en-tête ne l'est pas",
          "[app][patcher][mirror]") {
  const auto empty = ParseMirrorManifest(Manifest(""));
  REQUIRE(empty.has_value());
  CHECK(empty->empty());
  CHECK_FALSE(ParseMirrorManifest("").has_value());
  CHECK_FALSE(ParseMirrorManifest("<html>404</html>\n").has_value());
}

TEST_CASE("Miroir : une ligne mal formée refuse le manifeste et se nomme",
          "[app][patcher][mirror]") {
  const auto broken =
      ParseMirrorManifest(Manifest(kShaA + " 1 a.txt\n" + kShaA + " douze b.txt\n"));
  REQUIRE_FALSE(broken.has_value());
  CHECK(broken.error().detail.find("ligne 3") != std::string::npos);

  CHECK_FALSE(ParseMirrorManifest(Manifest(kShaA + " 1\n")).has_value());
  CHECK_FALSE(ParseMirrorManifest(Manifest("abc 1 a.txt\n")).has_value());
  CHECK_FALSE(ParseMirrorManifest(Manifest(std::string(64, 'A') + " 1 a.txt\n")).has_value());
  CHECK_FALSE(ParseMirrorManifest(Manifest(kShaA + " -1 a.txt\n")).has_value());
}

TEST_CASE("Miroir : un chemin qui sort du dossier du jeu, ou répété, est refusé",
          "[app][patcher][mirror]") {
  for (const char* outside : {"../hors.txt", "a/../../hors.txt", "/abs.txt", "C:/jeu.txt",
                              "a\\b.txt", "a//b.txt", "./a.txt", "a/"}) {
    INFO(outside);
    CHECK_FALSE(ParseMirrorManifest(Manifest(kShaA + " 1 " + outside + "\n")).has_value());
  }
  const auto repeated =
      ParseMirrorManifest(Manifest(kShaA + " 1 Data/A.txt\n" + kShaB + " 1 data/a.txt\n"));
  REQUIRE_FALSE(repeated.has_value());
  CHECK(repeated.error().detail.find("répété") != std::string::npos);
}

TEST_CASE("Miroir : l'adresse d'un fichier échappe ce qui n'est pas sûr, sauf les barres",
          "[app][patcher][mirror]") {
  CHECK(MirrorFileUrl("https://m.example/files/", "SystemEN/table.lua") ==
        "https://m.example/files/SystemEN/table.lua");
  CHECK(MirrorFileUrl("https://m.example/files/", "é t/a+b#c.txt") ==
        "https://m.example/files/%C3%A9%20t/a%2Bb%23c.txt");
}

TEST_CASE("Miroir : le cache des empreintes fait l'aller-retour, et illisible il est vide",
          "[app][patcher][mirror]") {
  MirrorCache cache;
  cache.emplace("Données/mon fichier.txt", KnownFile{12, -42, kShaA});
  cache.emplace("a.txt", KnownFile{0, 1, kShaB});
  CHECK(ParseMirrorCache(FormatMirrorCache(cache)) == cache);

  CHECK(ParseMirrorCache("").empty());
  CHECK(ParseMirrorCache("autre chose\n").empty());
  CHECK(ParseMirrorCache("moonclient-mirror-cache 1\n" + kShaA + " x 1 a.txt\n").empty());
}
