// LES BONS DE TRAVAIL DU STAFF sur les albums d'objets.
//
// 🔴 LE FICHIER EST LU PAR UN SCRIPT, et c'est lui le vrai lecteur : sa clé
// racine, ses champs et l'absence de poids sur un retrait sont un contrat.

#include "items/card_album_delta.h"

#include <fstream>
#include <sstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "support/temp_directory.h"

using moonclient::AlbumDeltaAction;
using moonclient::AlbumDeltaEntry;
using moonclient::CardAlbumDelta;
using ro::test_support::TempDirectory;

namespace {

constexpr std::uint32_t kOldAlbum = 616;
constexpr std::uint32_t kMysticAlbum = 12246;

[[nodiscard]] std::string FileText(const TempDirectory& root) {
  std::ifstream in(root.path() / "savedata" / "card_album_delta.yml");
  std::stringstream text;
  text << in.rdbuf();
  return text.str();
}

}  // namespace

TEST_CASE("Bons de travail : absents, rien n'est demandé", "[app][album]") {
  TempDirectory root;
  CardAlbumDelta delta(root.path());
  CHECK(delta.entries().empty());
  CHECK(delta.Find(kOldAlbum, 4001) == nullptr);
  // Retirer une intention absente est un succès.
  CHECK(delta.Clear(kOldAlbum, 4001));
}

TEST_CASE("Bons de travail : une intention s'écrit et se relit", "[app][album]") {
  TempDirectory root;
  {
    CardAlbumDelta delta(root.path());
    REQUIRE(delta.Set(AlbumDeltaEntry{
        .album_id = kOldAlbum, .card_id = 4001, .action = AlbumDeltaAction::kAdd, .rate = 10}));
    REQUIRE(delta.Set(AlbumDeltaEntry{
        .album_id = kMysticAlbum, .card_id = 4121, .action = AlbumDeltaAction::kRemove}));
    CHECK(delta.revision() == 2);
  }
  const std::string text = FileText(root);
  CHECK(text.find("card_album_delta:") != std::string::npos);
  CHECK(text.find("action: remove") != std::string::npos);
  CHECK(text.find("rate: 10") != std::string::npos);

  CardAlbumDelta again(root.path());
  REQUIRE(again.entries().size() == 2);
  const AlbumDeltaEntry* added = again.Find(kOldAlbum, 4001);
  REQUIRE(added != nullptr);
  CHECK(added->action == AlbumDeltaAction::kAdd);
  CHECK(added->rate == 10);
  const AlbumDeltaEntry* removed = again.Find(kMysticAlbum, 4121);
  REQUIRE(removed != nullptr);
  CHECK(removed->action == AlbumDeltaAction::kRemove);
}

TEST_CASE("Bons de travail : une intention remplace celle du même couple", "[app][album]") {
  TempDirectory root;
  CardAlbumDelta delta(root.path());
  REQUIRE(delta.Set(AlbumDeltaEntry{
      .album_id = kOldAlbum, .card_id = 4001, .action = AlbumDeltaAction::kAdd, .rate = 10}));
  REQUIRE(delta.Set(AlbumDeltaEntry{
      .album_id = kOldAlbum, .card_id = 4001, .action = AlbumDeltaAction::kAdd, .rate = 3}));
  REQUIRE(delta.entries().size() == 1);
  CHECK(delta.Find(kOldAlbum, 4001)->rate == 3);
  REQUIRE(delta.Clear(kOldAlbum, 4001));
  CHECK(delta.entries().empty());
}
