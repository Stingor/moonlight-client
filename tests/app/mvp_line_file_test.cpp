// LES LIGNES DÉTACHÉES DU CARNET DE CHASSE MVP, sur le disque.

#include "mvp/mvp_line_file.h"

#include <filesystem>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "support/temp_directory.h"

using moonclient::kMvpLineFileName;
using moonclient::LoadMvpLines;
using moonclient::MvpPinnedLine;
using moonclient::ReadMvpLines;
using moonclient::SaveMvpLines;
using moonclient::WriteMvpLines;
using ro::test_support::TempDirectory;

namespace {

[[nodiscard]] bool Same(const MvpPinnedLine& left, const MvpPinnedLine& right) {
  return left.mob_id == right.mob_id && left.map == right.map && left.x == right.x &&
         left.y == right.y;
}

[[nodiscard]] bool Same(const std::vector<MvpPinnedLine>& left,
                        const std::vector<MvpPinnedLine>& right) {
  if (left.size() != right.size()) {
    return false;
  }
  for (std::size_t at = 0; at < left.size(); ++at) {
    if (!Same(left[at], right[at])) {
      return false;
    }
  }
  return true;
}

const std::vector<MvpPinnedLine> kTwoLines = {
    {.mob_id = 1039, .map = "gef_dun03", .x = 412, .y = 96},
    {.mob_id = 0, .map = "lhz_dun03", .x = 0, .y = 0},
};

}  // namespace

TEST_CASE("Lignes détachées : le texte s'écrit et se relit à l'identique", "[app][mvp]") {
  const std::string text = WriteMvpLines(kTwoLines);
  // Les clés sont anglaises, l'en-tête dit le format.
  CHECK(text.starts_with("# Les lignes détachées du carnet de chasse MVP"));
  CHECK(text.find("lines:") != std::string::npos);
  CHECK(text.find("mob: 1039") != std::string::npos);
  CHECK(text.find("gef_dun03") != std::string::npos);
  std::vector<MvpPinnedLine> read;
  REQUIRE(ReadMvpLines(text, read));
  CHECK(Same(read, kTwoLines));
}

TEST_CASE("Lignes détachées : une entrée sans carte ou hors de l'écran est sautée", "[app][mvp]") {
  std::vector<MvpPinnedLine> read;
  REQUIRE(
      ReadMvpLines("lines:\n"
                   "  - {mob: 1039, map: gef_dun03, x: 10, y: 20}\n"
                   "  - {mob: 1038, map: \"\", x: 10, y: 20}\n"
                   "  - {mob: 1038, x: 10, y: 20}\n"
                   "  - {mob: 1087, map: prt_maze03, x: -1, y: 20}\n"
                   "  - {mob: 1087, map: prt_maze03, x: 1, y: -5}\n"
                   "  - {mob: 70000, map: prt_maze03, x: 1, y: 5}\n"
                   "  - {mob: 1039, map: gef_dun03, x: 99, y: 99}\n"
                   "  - not a line\n"
                   "  - {mob: 0, map: niflheim, x: 3, y: 4}\n",
                   read));
  const std::vector<MvpPinnedLine> kept = {
      {.mob_id = 1039, .map = "gef_dun03", .x = 10, .y = 20},
      {.mob_id = 0, .map = "niflheim", .x = 3, .y = 4},
  };
  CHECK(Same(read, kept));
}

TEST_CASE("Lignes détachées : un texte illisible laisse la liste intacte", "[app][mvp]") {
  std::vector<MvpPinnedLine> kept = kTwoLines;
  CHECK_FALSE(ReadMvpLines("lines: [unclosed", kept));
  CHECK(Same(kept, kTwoLines));
  CHECK_FALSE(ReadMvpLines("- just\n- a list\n", kept));
  CHECK(Same(kept, kTwoLines));
  CHECK_FALSE(ReadMvpLines("lines: 12\n", kept));
  CHECK(Same(kept, kTwoLines));
  // Un texte vide, lui, est un fichier sans ligne.
  CHECK(ReadMvpLines("", kept));
  CHECK(kept.empty());
}

TEST_CASE("Lignes détachées : le disque, absent puis écrit puis relu", "[app][mvp]") {
  const TempDirectory client;
  std::vector<MvpPinnedLine> read = kTwoLines;
  // ABSENT : une liste vide, et un succès — c'est la première partie.
  REQUIRE(LoadMvpLines(client.path(), read));
  CHECK(read.empty());

  REQUIRE(SaveMvpLines(client.path(), kTwoLines));
  CHECK(std::filesystem::exists(client.path() / "savedata" / kMvpLineFileName));
  REQUIRE(LoadMvpLines(client.path(), read));
  CHECK(Same(read, kTwoLines));

  // ILLISIBLE : faux, et ce qu'on tenait reste.
  client.WriteText(std::filesystem::path("savedata") / kMvpLineFileName, "lines: [");
  CHECK_FALSE(LoadMvpLines(client.path(), read));
  CHECK(Same(read, kTwoLines));
}
