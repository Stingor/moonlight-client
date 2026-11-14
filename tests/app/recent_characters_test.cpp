// LA MÉMOIRE DES PERSONNAGES JOUÉS, et le siège qu'elle désigne.
//
// 🔴 CE QU'ELLE ACHÈTE EST UNE TOUCHE, et ce qu'elle rate coûte un clic à
// chaque connexion. Le défaut qu'on redoute ici n'est pas une panne : c'est une
// désignation PLAUSIBLE mais fausse — le premier siège au lieu du dernier joué
// —, que personne ne remarque parce qu'un siège est bien désigné.

#include "character/recent_characters.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "support/temp_directory.h"

using moonclient::kRecentCharactersMax;
using moonclient::LoadRecentCharacters;
using moonclient::NoteRecentCharacter;
using moonclient::PreferredCharacter;

namespace {

// Les identifiants d'une tablée, dans l'ordre des sièges.
[[nodiscard]] std::vector<std::uint32_t> Seated(std::initializer_list<std::uint32_t> ids) {
  return std::vector<std::uint32_t>(ids);
}

}  // namespace

TEST_CASE("Personnages récents : le dernier joué l'emporte sur le premier siège",
          "[app][recents]") {
  const std::vector<std::uint32_t> recent = Seated({150'002, 150'000});
  const std::vector<std::uint32_t> present = Seated({150'000, 150'001, 150'002});
  // 🔴 LE RANG DEUX, et non zéro : sans mémoire on s'ouvrirait sur le premier
  // siège, ce qui a l'air juste et ne l'est pas.
  CHECK(PreferredCharacter(recent, present) == 2);
}

TEST_CASE("Personnages récents : un disparu laisse la place au suivant", "[app][recents]") {
  // Le plus récent a été supprimé — ou appartient à un autre compte. On
  // descend, plutôt que de retomber sur le siège zéro.
  const std::vector<std::uint32_t> recent = Seated({999'999, 150'001});
  const std::vector<std::uint32_t> present = Seated({150'000, 150'001});
  CHECK(PreferredCharacter(recent, present) == 1);
}

TEST_CASE("Personnages récents : sans mémoire, le premier occupé", "[app][recents]") {
  // ⚠ JAMAIS « RIEN » : une tablée sans siège désigné oblige à viser avant de
  // pouvoir jouer, ce qui est précisément ce qu'on évite.
  CHECK(PreferredCharacter({}, Seated({150'000, 150'001})) == 0);
  CHECK(PreferredCharacter(Seated({999'999}), Seated({150'000})) == 0);
}

TEST_CASE("Personnages récents : une tablée vide ne désigne personne", "[app][recents]") {
  CHECK(PreferredCharacter(Seated({150'000}), {}) == -1);
}

TEST_CASE("Personnages récents : noter repousse en tête et dédoublonne", "[app][recents]") {
  const ro::test_support::TempDirectory root;
  REQUIRE(NoteRecentCharacter(root.path(), 150'000));
  REQUIRE(NoteRecentCharacter(root.path(), 150'001));
  // Le même que le premier : il remonte, et n'apparaît pas deux fois.
  REQUIRE(NoteRecentCharacter(root.path(), 150'000));

  const std::vector<std::uint32_t> listed = LoadRecentCharacters(root.path());
  REQUIRE(listed.size() == 2);
  CHECK(listed[0] == 150'000);
  CHECK(listed[1] == 150'001);
}

TEST_CASE("Personnages récents : la liste est bornée", "[app][recents]") {
  const ro::test_support::TempDirectory root;
  // ⚠ UN DE PLUS QUE LA BORNE : c'est le seul cas qui prouve qu'elle mord.
  for (std::uint32_t rank = 0; rank <= static_cast<std::uint32_t>(kRecentCharactersMax); ++rank) {
    REQUIRE(NoteRecentCharacter(root.path(), 150'000 + rank));
  }
  const std::vector<std::uint32_t> listed = LoadRecentCharacters(root.path());
  CHECK(listed.size() == kRecentCharactersMax);
  // Le dernier noté est en tête, et le tout premier est tombé.
  CHECK(listed.front() == 150'000 + static_cast<std::uint32_t>(kRecentCharactersMax));
  CHECK(std::ranges::find(listed, 150'000U) == listed.end());
}

TEST_CASE("Personnages récents : un identifiant nul ne se note pas", "[app][recents]") {
  // Zéro est ce qu'une entrée illisible rend : le noter ferait chercher un
  // siège qui n'existe pas.
  const ro::test_support::TempDirectory root;
  CHECK_FALSE(NoteRecentCharacter(root.path(), 0));
  CHECK(LoadRecentCharacters(root.path()).empty());
}

TEST_CASE("Personnages récents : un premier lancement n'est pas une panne", "[app][recents]") {
  const ro::test_support::TempDirectory root;
  CHECK(LoadRecentCharacters(root.path()).empty());
}
