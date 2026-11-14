// La liste des patchs du serveur et l'état que l'installation en garde.

#include "patcher/patch_list.h"

#include <string>

#include <catch2/catch_test_macros.hpp>

#include "patcher/patcher_state.h"

using moonclient::patcher::ParsePatchList;
using moonclient::patcher::PatchEntry;
using moonclient::patcher::PendingPatches;

namespace {

constexpr const char* kHash = "0123456789ABCDEF0123456789abcdef0123456789abcdef0123456789abcdef";

}  // namespace

TEST_CASE("Liste des patchs : index, archive et empreinte facultative, triés", "[app][patcher]") {
  const std::string text = std::string("// chaîne de Moonlight\r\n") +
                           "102 2026-09-24-grf-1.thor\r\n"
                           "\n"
                           "# commentaire\n"
                           "101 2026-09-24-sys-1.thor " +
                           kHash +
                           " colonne ignorée\n"
                           "103 2026-09-24-sys-2.thor";
  const auto patches = ParsePatchList(text);
  REQUIRE(patches.has_value());
  REQUIRE(patches->size() == 3);
  CHECK((*patches)[0].patch_index == 101);
  CHECK((*patches)[0].archive_name == "2026-09-24-sys-1.thor");
  // L'empreinte se compare en minuscules, quelle que soit la casse publiée.
  CHECK((*patches)[0].sha256_hex ==
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
  CHECK((*patches)[1].patch_index == 102);
  CHECK((*patches)[1].sha256_hex.empty());
  // La dernière ligne, sans saut de ligne final, compte.
  CHECK((*patches)[2].archive_name == "2026-09-24-sys-2.thor");
}

TEST_CASE("Liste des patchs : une ligne mal formée est refusée et nommée", "[app][patcher]") {
  const auto bad_index = ParsePatchList("1 a.thor\ndeux b.thor\n");
  REQUIRE_FALSE(bad_index.has_value());
  CHECK(bad_index.error().detail.find("ligne 2") != std::string::npos);

  CHECK_FALSE(ParsePatchList("1\n").has_value());
  CHECK_FALSE(ParsePatchList("1 a.thor court\n").has_value());
  CHECK_FALSE(ParsePatchList("-1 a.thor\n").has_value());
  CHECK(ParsePatchList("").value().empty());
}

TEST_CASE("Liste des patchs : un nom qui sort de son dossier est refusé", "[app][patcher]") {
  CHECK_FALSE(ParsePatchList("1 ../patcher.exe\n").has_value());
  CHECK_FALSE(ParsePatchList("1 data/a.thor\n").has_value());
  CHECK_FALSE(ParsePatchList("1 data\\a.thor\n").has_value());
  CHECK_FALSE(ParsePatchList("1 C:a.thor\n").has_value());
  CHECK_FALSE(ParsePatchList("1 .cache\n").has_value());
}

TEST_CASE("Patchs à appliquer : ceux qui suivent le dernier appliqué", "[app][patcher]") {
  const auto patches = ParsePatchList("1 a.thor\n2 b.thor\n3 c.thor\n").value();

  const auto after_two = PendingPatches(patches, 2);
  REQUIRE(after_two.size() == 1);
  CHECK(after_two[0].archive_name == "c.thor");

  CHECK(PendingPatches(patches, 3).empty());
  CHECK(PendingPatches(patches, std::nullopt).size() == 3);
  // Un index que la liste ne connaît plus ne prouve rien : tout se réapplique.
  CHECK(PendingPatches(patches, 7).size() == 3);
}

TEST_CASE("Patchs à appliquer : une chaîne qui commence à zéro", "[app][patcher]") {
  const auto patches = ParsePatchList("0 a.thor\n1 b.thor\n").value();

  // Aucun état : le patch 0 fait partie de ce qui reste à appliquer.
  const auto fresh = PendingPatches(patches, std::nullopt);
  REQUIRE(fresh.size() == 2);
  CHECK(fresh[0].patch_index == 0);

  // Le patch 0 appliqué n'est PAS « aucun état » : seul le suivant reste.
  const auto after_zero = PendingPatches(patches, 0);
  REQUIRE(after_zero.size() == 1);
  CHECK(after_zero[0].archive_name == "b.thor");
}

TEST_CASE("État du patcher : un index en JSON, aller et retour", "[app][patcher]") {
  using moonclient::patcher::FormatPatcherState;
  using moonclient::patcher::ParsePatcherState;

  CHECK(FormatPatcherState(102) == R"({"last_patch_index":102})");
  const auto read = ParsePatcherState(R"({"last_patch_index":102})");
  REQUIRE(read.has_value());
  CHECK(*read == 102);

  CHECK_FALSE(ParsePatcherState("").has_value());
  CHECK_FALSE(ParsePatcherState(R"({"last_patch_index":"102"})").has_value());
  CHECK_FALSE(ParsePatcherState(R"({"last_patch_index":-1})").has_value());
  CHECK_FALSE(ParsePatcherState(R"({"autre":1})").has_value());
}
