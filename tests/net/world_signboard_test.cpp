#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/world_update.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/packed_position.h"
#include "ro/sim/world.h"

#include "support/net_frames.h"

// LES ENSEIGNES : l'échoppe qu'un joueur tient, le salon qu'il a ouvert.
//
// ⚠ CETTE FAMILLE A UN PIÈGE, ET C'EST POURQUOI ELLE A SON FICHIER. Une échoppe
// se ferme sous l'identifiant de SON VENDEUR ; un salon sous le sien propre,
// qui n'est pas celui de son hôte. Les deux paquets se ressemblent à s'y
// méprendre, et les essais ci-dessous existent d'abord pour cela.

using ro::net::ApplyFrame;
using ro::net::FrameOutcome;
using ro::sim::CellPosition;
using ro::sim::World;
using ro::test_support::FrameOf;
using ro::test_support::IdleUnit;

TEST_CASE("Traduction : l'enseigne d'une échoppe se pose sur son vendeur", "[net][update]") {
  World world;
  REQUIRE(ApplyFrame(FrameOf(Encode(IdleUnit(150'001, CellPosition{10, 10, 0}))), world, 0) ==
          FrameOutcome::kApplied);

  ro::proto::ZcStoreEntry board;
  board.makerAID = 150'001;
  board.storeName = "Potions pas cheres";
  CHECK(ApplyFrame(FrameOf(Encode(board)), world, 0) == FrameOutcome::kApplied);

  const auto* actor = world.Find(150'001);
  REQUIRE(actor->signboard.has_value());
  CHECK(actor->signboard->kind == ro::sim::SignboardKind::kVendingShop);
  CHECK(actor->signboard->title == "Potions pas cheres");

  // Elle se ferme sous le MEME identifiant : ZC_DISAPPEAR_ENTRY nomme le
  // vendeur, pas une enseigne.
  ro::proto::ZcDisappearEntry closed;
  closed.GID = 150'001;
  CHECK(ApplyFrame(FrameOf(Encode(closed)), world, 0) == FrameOutcome::kApplied);
  CHECK_FALSE(world.Find(150'001)->signboard.has_value());
  // Redite, elle ne trouve plus rien à retirer, et ce n'est pas une panne.
  CHECK(ApplyFrame(FrameOf(Encode(closed)), world, 0) == FrameOutcome::kIgnored);
}

TEST_CASE("Traduction : un salon se pose sur son hôte et se ferme par son propre numéro",
          "[net][update]") {
  World world;
  REQUIRE(ApplyFrame(FrameOf(Encode(IdleUnit(150'001, CellPosition{10, 10, 0}))), world, 0) ==
          FrameOutcome::kApplied);

  ro::proto::ZcRoomNewentry room;
  room.owner = 150'001;
  room.id = 77'000;
  room.limit = 12;
  room.users = 3;
  room.type = ro::sim::kRoomPublic;
  room.title = "Groupe pour Orcs";
  CHECK(ApplyFrame(FrameOf(Encode(room)), world, 0) == FrameOutcome::kApplied);

  const auto* actor = world.Find(150'001);
  REQUIRE(actor->signboard.has_value());
  CHECK(actor->signboard->kind == ro::sim::SignboardKind::kChatRoom);
  CHECK(actor->signboard->title == "Groupe pour Orcs");
  // ⚠ L'ENSEIGNE PORTE LE NUMÉRO DU SALON, PAS CELUI DE L'HÔTE.
  CHECK(actor->signboard->room_id == 77'000);
  CHECK(actor->signboard->users == 3);
  CHECK(actor->signboard->limit == 12);

  ro::proto::ZcDestroyRoom closed;
  closed.chatId = 77'000;
  CHECK(ApplyFrame(FrameOf(Encode(closed)), world, 0) == FrameOutcome::kApplied);
  CHECK_FALSE(world.Find(150'001)->signboard.has_value());
}

TEST_CASE("Traduction : l'enseigne d'une échoppe d'achat se pose sur son acheteur",
          "[net][update][buying_store]") {
  World world;
  REQUIRE(ApplyFrame(FrameOf(Encode(IdleUnit(150'001, CellPosition{10, 10, 0}))), world, 0) ==
          FrameOutcome::kApplied);

  // La trame, octet par octet : le compte de l'acheteur, puis quatre-vingts
  // octets de titre.
  std::vector<std::uint8_t> entry = {0x14, 0x08, 0xF1, 0x49, 0x02, 0x00};
  const std::string title = "Achète Jellopy";
  entry.insert(entry.end(), title.begin(), title.end());
  entry.resize(ro::proto::ZcBuyingStoreEntry::kWireSize, 0);
  CHECK(ApplyFrame(FrameOf(entry), world, 0) == FrameOutcome::kApplied);

  const auto* actor = world.Find(150'001);
  REQUIRE(actor->signboard.has_value());
  CHECK(actor->signboard->kind == ro::sim::SignboardKind::kBuyingStore);
  // Un titre écrit en UTF-8 revient tel quel, sans passer par la table 1252.
  CHECK(actor->signboard->title == title);

  // Elle se ferme sous le MÊME identifiant.
  const std::vector<std::uint8_t> gone = {0x16, 0x08, 0xF1, 0x49, 0x02, 0x00};
  CHECK(ApplyFrame(FrameOf(gone), world, 0) == FrameOutcome::kApplied);
  CHECK_FALSE(world.Find(150'001)->signboard.has_value());
  CHECK(ApplyFrame(FrameOf(gone), world, 0) == FrameOutcome::kIgnored);
}

TEST_CASE("Traduction : l'enseigne d'achat d'un inconnu n'invente pas d'acteur",
          "[net][update][buying_store]") {
  World world;
  ro::proto::ZcBuyingStoreEntry board;
  board.makerAID = 999'999;
  board.storeName = "Hors de vue";
  CHECK(ApplyFrame(FrameOf(Encode(board)), world, 0) == FrameOutcome::kIgnored);
  CHECK(world.Find(999'999) == nullptr);
}

TEST_CASE("Traduction : l'enseigne d'un inconnu n'invente pas d'acteur", "[net][update]") {
  World world;
  ro::proto::ZcStoreEntry board;
  board.makerAID = 999'999;
  board.storeName = "Une echoppe hors de vue";

  CHECK(ApplyFrame(FrameOf(Encode(board)), world, 0) == FrameOutcome::kIgnored);
  CHECK(world.Find(999'999) == nullptr);
}
