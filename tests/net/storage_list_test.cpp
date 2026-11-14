// LES COFFRES DE CE PERSONNAGE, décodés — et la demande qui en ouvre un.
//
// 🔴 CE BANC PORTE QUATRE CHOSES QU'AUCUNE SESSION NE MONTRERAIT VITE :
//
//   · « aucun coffre ouvert » vaut 0xFF et non ZÉRO — zéro EST un coffre, le
//     principal, et confondre les deux marquerait l'onglet du mauvais ;
//   · un nom de coffre fait VINGT-QUATRE octets là où un nom de CANAL en fait
//     vingt : deux listes qui se ressemblent, deux mesures ;
//   · la liste et l'ouvert arrivent dans la MÊME trame, et c'est ce qui les
//     garde d'accord ;
//   · fermer efface le marqueur et GARDE la liste : les coffres qu'on peut
//     ouvrir ne dépendent pas de celui qui l'est.

#include "ro/net/storage_list.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/proto/map_packets.h"
#include "ro/sim/storage_shelf.h"
#include "ro/sim/world.h"

#include "support/wire_bytes.h"

using ro::net::ApplyStorageListFrame;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::net::kStorageNameSize;
using ro::sim::kMainStorageId;
using ro::sim::kNoStorageOpen;
using ro::sim::World;
using ro::test_support::PutU16;

namespace {

struct Told {
  std::uint8_t id = 0;
  std::string name;
};

[[nodiscard]] Frame ListFrame(std::uint8_t cur_id, const std::vector<Told>& shelves) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, ro::proto::ZcMoonlightStorageList::kOpcode);
  PutU16(bytes, 0);  // la longueur, réécrite plus bas
  bytes.push_back(cur_id);
  bytes.push_back(static_cast<std::uint8_t>(shelves.size()));
  for (const Told& told : shelves) {
    bytes.push_back(told.id);
    for (std::size_t rank = 0; rank < kStorageNameSize; ++rank) {
      bytes.push_back(rank < told.name.size() ? static_cast<std::uint8_t>(told.name[rank]) : 0);
    }
  }
  bytes[2] = static_cast<std::uint8_t>(bytes.size() & 0xFF);
  bytes[3] = static_cast<std::uint8_t>(bytes.size() >> 8);
  return Frame{.opcode = ro::proto::ZcMoonlightStorageList::kOpcode, .bytes = std::move(bytes)};
}

}  // namespace

TEST_CASE("Coffres : la liste arrive avec celui qui est OUVERT", "[net][storage]") {
  World world;
  const Frame frame =
      ListFrame(2, {Told{.id = 0, .name = "Storage"}, Told{.id = 2, .name = "Coffre de guilde"},
                    Told{.id = 5, .name = "Reserve"}});
  CHECK(ApplyStorageListFrame(frame, world) == FrameOutcome::kApplied);
  REQUIRE(world.storage_shelves().size() == 3);
  CHECK(world.storage_shelves()[0].name == "Storage");
  CHECK(world.storage_shelves()[2].id == 5);
  CHECK(world.storage_shelves()[2].name == "Reserve");
  CHECK(world.open_storage_id() == 2);
}

TEST_CASE("Coffres : AUCUN OUVERT vaut 0xFF, et zéro est un coffre", "[net][storage]") {
  // ⚠ LE SERVEUR ENVOIE SA LISTE AU LOGIN AVEC 0xFF. Lire zéro comme « aucun »
  // marquerait l'onglet du coffre principal dès l'entrée en jeu, sur une
  // fenêtre que rien n'a ouverte.
  World world;
  CHECK(ApplyStorageListFrame(ListFrame(kNoStorageOpen, {Told{.id = 0, .name = "Storage"}}),
                              world) == FrameOutcome::kApplied);
  CHECK(world.open_storage_id() == kNoStorageOpen);

  CHECK(ApplyStorageListFrame(ListFrame(kMainStorageId, {Told{.id = 0, .name = "Storage"}}),
                              world) == FrameOutcome::kApplied);
  CHECK(world.open_storage_id() == kMainStorageId);
}

TEST_CASE("Coffres : refermer garde la LISTE et perd le marqueur", "[net][storage]") {
  // 🔴 LES COFFRES QU'ON PEUT OUVRIR NE DÉPENDENT PAS DE CELUI QUI L'EST : la
  // liste vient des DROITS du personnage. La vider en refermant priverait la
  // fenêtre de ses onglets pour le reste de la séance — le serveur ne la
  // repousse qu'au login et à chaque ouverture.
  World world;
  CHECK(ApplyStorageListFrame(
            ListFrame(2, {Told{.id = 0, .name = "Storage"}, Told{.id = 2, .name = "Reserve"}}),
            world) == FrameOutcome::kApplied);
  world.OpenStorage();
  REQUIRE(world.storage_open());

  world.CloseStorage();
  CHECK_FALSE(world.storage_open());
  CHECK(world.open_storage_id() == kNoStorageOpen);
  CHECK(world.storage_shelves().size() == 2);
}

TEST_CASE("Coffres : une liste VIDE se pose quand même", "[net][storage]") {
  // « Ce personnage n'a droit à aucun coffre » est une réponse, et elle doit
  // faire disparaître les onglets d'avant.
  World world;
  CHECK(ApplyStorageListFrame(ListFrame(kNoStorageOpen, {Told{.id = 0, .name = "Storage"}}),
                              world) == FrameOutcome::kApplied);
  REQUIRE(world.storage_shelves().size() == 1);
  CHECK(ApplyStorageListFrame(ListFrame(kNoStorageOpen, {}), world) == FrameOutcome::kApplied);
  CHECK(world.storage_shelves().empty());
}

TEST_CASE("Coffres : un nom fait VINGT-QUATRE octets", "[net][storage]") {
  // ⚠ ET NON LES VINGT D'UN NOM DE CANAL. Les deux listes ont la même forme —
  // un compte, puis des entrées à nom fixe — et s'en tromper décale tout dès la
  // seconde entrée, en silence : le second coffre prendrait la fin du nom du
  // premier pour son numéro.
  World world;
  const Frame frame = ListFrame(
      kNoStorageOpen, {Told{.id = 0, .name = "Storage"}, Told{.id = 1, .name = "Deuxieme"}});
  CHECK(frame.bytes.size() == ro::net::kStorageListFixedSize + 2 * (1 + kStorageNameSize));
  CHECK(ApplyStorageListFrame(frame, world) == FrameOutcome::kApplied);
  REQUIRE(world.storage_shelves().size() == 2);
  CHECK(world.storage_shelves()[1].id == 1);
  CHECK(world.storage_shelves()[1].name == "Deuxieme");
}

TEST_CASE("Coffres : un compte plus grand que la trame ne la croit pas", "[net][storage]") {
  World world;
  Frame frame = ListFrame(kNoStorageOpen, {Told{.id = 0, .name = "Storage"}});
  frame.bytes[5] = 9;  // le compte, menteur
  CHECK(ApplyStorageListFrame(frame, world) == FrameOutcome::kApplied);
  CHECK(world.storage_shelves().size() == 1);
}

TEST_CASE("Coffres : un autre opcode ne lui appartient pas", "[net][storage]") {
  World world;
  const Frame frame{.opcode = 0x0000, .bytes = {}};
  CHECK(ApplyStorageListFrame(frame, world) == FrameOutcome::kUnhandled);
}
