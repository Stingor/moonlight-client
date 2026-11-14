#include "ro/core/file_write.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <sstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/byte_source.h"

#include "support/temp_directory.h"

namespace {

[[nodiscard]] std::string Contents(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  std::ostringstream all;
  all << file.rdbuf();
  return all.str();
}

}  // namespace

TEST_CASE("l'écriture atomique pose le contenu demandé", "[core][fichier]") {
  const ro::test_support::TempDirectory folder;
  const std::filesystem::path target = folder.path() / "reglages.yaml";

  REQUIRE(ro::core::WriteFileAtomically(target, "musique: true\n").has_value());
  CHECK(Contents(target) == "musique: true\n");
}

TEST_CASE("elle crée le dossier qui manque", "[core][fichier]") {
  const ro::test_support::TempDirectory folder;
  // La première partie d'un joueur : `savedata` n'existe pas encore.
  const std::filesystem::path target = folder.path() / "savedata" / "reglages.yaml";

  REQUIRE(ro::core::WriteFileAtomically(target, "x: 1\n").has_value());
  CHECK(std::filesystem::exists(target));
}

TEST_CASE("elle remplace un fichier existant sans laisser de temporaire", "[core][fichier]") {
  const ro::test_support::TempDirectory folder;
  const std::filesystem::path target = folder.path() / "reglages.yaml";
  REQUIRE(ro::core::WriteFileAtomically(target, "ancien\n").has_value());

  REQUIRE(ro::core::WriteFileAtomically(target, "neuf\n").has_value());
  CHECK(Contents(target) == "neuf\n");
  // ⚠ LE VOISIN TEMPORAIRE NE DOIT PAS SURVIVRE. Un `.tmp` laissé derrière est
  // un fichier que le joueur trouve dans son dossier de sauvegarde sans savoir
  // s'il peut le supprimer.
  std::filesystem::path pending = target;
  pending += ".tmp";
  CHECK_FALSE(std::filesystem::exists(pending));
}

TEST_CASE("elle écrit les octets tels quels, sans traduire les fins de ligne", "[core][fichier]") {
  const ro::test_support::TempDirectory folder;
  const std::filesystem::path target = folder.path() / "brut.txt";

  REQUIRE(ro::core::WriteFileAtomically(target, "une\ndeux\n").has_value());
  // En mode texte sous Windows, ce serait « une\r\ndeux\r\n » : les comparaisons
  // d'un essai ne tiendraient plus d'un système à l'autre.
  CHECK(Contents(target) == "une\ndeux\n");
}

TEST_CASE("un chemin impossible échoue sans toucher à la cible", "[core][fichier]") {
  const ro::test_support::TempDirectory folder;
  const std::filesystem::path target = folder.path() / "reglages.yaml";
  REQUIRE(ro::core::WriteFileAtomically(target, "ancien\n").has_value());

  // Un DOSSIER porte déjà le nom du temporaire : le flux ne peut pas s'ouvrir.
  std::filesystem::path pending = target;
  pending += ".tmp";
  std::filesystem::create_directories(pending);

  CHECK_FALSE(ro::core::WriteFileAtomically(target, "neuf\n").has_value());
  // ⚠ CE QUI COMPTE : l'ancien contenu est INTACT. Une écriture qui échoue vaut
  // mieux qu'une écriture qui réussit à moitié.
  CHECK(Contents(target) == "ancien\n");
}

TEST_CASE("l'écriture par morceaux remplace la cible, qu'elle peut relire", "[core][fichier]") {
  const ro::test_support::TempDirectory folder;
  const std::filesystem::path target = folder.path() / "archive.grf";
  REQUIRE(ro::core::WriteFileAtomically(target, "ancien").has_value());

  const auto written = ro::core::WriteFileAtomicallyWith(
      target, [&](ro::core::ByteSink& sink) -> ro::core::Result<void> {
        // La cible se lit PENDANT l'écriture, et se referme avant la fin.
        const std::string previous = Contents(target);
        const std::string grown = previous + "+neuf";
        return sink.Append(
            std::span(reinterpret_cast<const std::uint8_t*>(grown.data()), grown.size()));
      });
  REQUIRE(written.has_value());
  CHECK(Contents(target) == "ancien+neuf");
  CHECK(std::distance(std::filesystem::directory_iterator(folder.path()),
                      std::filesystem::directory_iterator()) == 1);
}

TEST_CASE("l'écriture par morceaux qui échoue laisse la cible intacte", "[core][fichier]") {
  const ro::test_support::TempDirectory folder;
  const std::filesystem::path target = folder.path() / "archive.grf";
  REQUIRE(ro::core::WriteFileAtomically(target, "ancien").has_value());

  const auto written = ro::core::WriteFileAtomicallyWith(
      target, [](ro::core::ByteSink& sink) -> ro::core::Result<void> {
        static_cast<void>(sink.Append(std::span<const std::uint8_t>()));
        return ro::core::Fail(ro::core::ErrorCode::kCorruptData, "patch illisible");
      });
  REQUIRE_FALSE(written.has_value());
  CHECK(Contents(target) == "ancien");
  // Le voisin à moitié écrit a disparu avec l'échec.
  CHECK(std::distance(std::filesystem::directory_iterator(folder.path()),
                      std::filesystem::directory_iterator()) == 1);
}

TEST_CASE("un échec dans un dossier hors page ANSI se dit sans lever", "[core][fichier]") {
  // Ces caractères manquent à la plupart des pages ANSI : `path::string()` y lève.
  const ro::test_support::TempDirectory folder;
  const std::filesystem::path blocker = folder.path() / "fichier";
  REQUIRE(ro::core::WriteFileAtomically(blocker, "x").has_value());
  const std::filesystem::path impossible = blocker / std::filesystem::path(u8"日本語한글.yaml");

  CHECK_FALSE(ro::core::WriteFileAtomically(impossible, "x").has_value());
  CHECK_FALSE(ro::core::FileByteSource::Open(impossible).has_value());
}
