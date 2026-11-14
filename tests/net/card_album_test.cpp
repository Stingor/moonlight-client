// L'ALBUM DE CARTES SUR LE FIL — `ZC 0x0F33`, `CZ 0x0F34` et l'annonce `CZ 0x0F24`.
//
// 🔴 CE BANC PORTE TROIS CHOSES QU'UNE SESSION NE MONTRERAIT PAS VITE :
//
//   · la taille d'une entrée se DÉDUIT de la trame : onze octets chez un serveur
//     d'avant la nature du monstre, douze après — et la lire en dur décalerait
//     chaque carte d'un octet de plus que la précédente ;
//   · un compte annoncé plus grand que ce que la trame porte ne lit pas au-delà ;
//   · « tenu par un autre compte de jeu » VIDE l'album au lieu de montrer une
//     réserve qui n'est pas la nôtre.

#include "ro/net/card_album.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/net/ui_caps.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/bank.h"
#include "ro/sim/card_album.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/wire_bytes.h"

using ro::net::ApplyCardAlbumFrame;
using ro::net::CardAlbumCommand;
using ro::net::DecodeCardAlbum;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::sim::CardAlbum;
using ro::sim::CardAlbumResult;
using ro::test::Connect;
using ro::test::LoopbackServer;
using ro::test_support::PutU16;
using ro::test_support::PutU32;

namespace {

struct Entry {
  std::uint32_t card_id;
  std::uint16_t reserve;
  bool unlocked;
  std::uint32_t equip;
  std::uint8_t dropper;
};

// Une trame d'album. `with_dropper` : l'octet de nature en queue d'entrée.
// `announced` : le compte écrit dans l'en-tête, s'il doit mentir.
[[nodiscard]] Frame AlbumFrame(std::uint8_t result, const std::vector<Entry>& entries,
                               bool with_dropper, int announced = -1) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, ro::proto::ZcMoonlightCardAlbum::kOpcode);
  PutU16(bytes, 0);
  bytes.push_back(result);
  PutU16(bytes, static_cast<std::uint16_t>(announced < 0 ? entries.size()
                                                         : static_cast<std::size_t>(announced)));
  for (const Entry& e : entries) {
    PutU32(bytes, e.card_id);
    PutU16(bytes, e.reserve);
    bytes.push_back(e.unlocked ? 1 : 0);
    PutU32(bytes, e.equip);
    if (with_dropper) {
      bytes.push_back(e.dropper);
    }
  }
  bytes[2] = static_cast<std::uint8_t>(bytes.size() & 0xFFU);
  bytes[3] = static_cast<std::uint8_t>(bytes.size() >> 8U);
  return Frame{.opcode = ro::proto::ZcMoonlightCardAlbum::kOpcode, .bytes = bytes};
}

const std::vector<Entry> kTwoCards = {
    {4001, 3, true, 0x10, 0},
    {4121, 0, false, 0x02, 2},
};

}  // namespace

TEST_CASE("Album : une entrée de douze octets porte la nature du monstre", "[net][album]") {
  const auto reply = DecodeCardAlbum(AlbumFrame(0, kTwoCards, true).bytes);
  REQUIRE(reply.has_value());
  CHECK(reply->dropper_known);
  REQUIRE(reply->cards.size() == 2);
  CHECK(reply->cards[0].card_id == 4001);
  CHECK(reply->cards[0].reserve == 3);
  CHECK(reply->cards[0].unlocked);
  CHECK(reply->cards[0].equip_mask == 0x10);
  CHECK(reply->cards[1].card_id == 4121);
  CHECK_FALSE(reply->cards[1].unlocked);
  CHECK(reply->cards[1].dropper_rank == ro::sim::kCardDropperMvp);
}

TEST_CASE("Album : un serveur d'avant la nature se lit quand même", "[net][album]") {
  const auto reply = DecodeCardAlbum(AlbumFrame(0, kTwoCards, false).bytes);
  REQUIRE(reply.has_value());
  CHECK_FALSE(reply->dropper_known);
  REQUIRE(reply->cards.size() == 2);
  // 🔴 SANS DÉCALAGE : la seconde carte est bien 4121, et pas un mélange de
  // la queue de la première et de la tête de la seconde.
  CHECK(reply->cards[1].card_id == 4121);
  CHECK(reply->cards[1].equip_mask == 0x02);
  CHECK(reply->cards[1].dropper_rank == ro::sim::kCardDropperOrdinary);
}

TEST_CASE("Album : un compte qui ment ne fait pas lire au-delà de la trame", "[net][album]") {
  const auto reply = DecodeCardAlbum(AlbumFrame(0, kTwoCards, true, 40).bytes);
  REQUIRE(reply.has_value());
  CHECK(reply->cards.size() <= 2);
}

TEST_CASE("Album : l'état se remplace, les comptes se refont", "[net][album]") {
  CardAlbum album;
  REQUIRE(ApplyCardAlbumFrame(AlbumFrame(0, kTwoCards, true), album) == FrameOutcome::kApplied);
  CHECK(album.cards().size() == 2);
  CHECK(album.unlocked_count() == 1);
  CHECK(album.total_reserve() == 3);
  CHECK(album.arrival() == 1);
  REQUIRE(album.Find(4121) != nullptr);
  CHECK(album.Find(9999) == nullptr);

  // Une seconde réponse REMPLACE : la carte retirée du catalogue disparaît.
  REQUIRE(ApplyCardAlbumFrame(AlbumFrame(0, {{4001, 5, true, 0x10, 0}}, true), album) ==
          FrameOutcome::kApplied);
  CHECK(album.cards().size() == 1);
  CHECK(album.Find(4121) == nullptr);
  CHECK(album.total_reserve() == 5);
  CHECK(album.arrival() == 2);
}

TEST_CASE("Album : tenu par un autre compte de jeu, il se vide", "[net][album]") {
  CardAlbum album;
  REQUIRE(ApplyCardAlbumFrame(AlbumFrame(0, kTwoCards, true), album) == FrameOutcome::kApplied);
  const auto in_use = static_cast<std::uint8_t>(CardAlbumResult::kInUse);
  REQUIRE(ApplyCardAlbumFrame(AlbumFrame(in_use, kTwoCards, true), album) ==
          FrameOutcome::kApplied);
  CHECK(album.blocked());
  CHECK(album.cards().empty());
  CHECK(album.unlocked_count() == 0);
}

TEST_CASE("Album : une autre trame n'est pas la sienne", "[net][album]") {
  CardAlbum album;
  const Frame other{.opcode = ro::proto::ZcMoonlightMvpState::kOpcode, .bytes = {}};
  CHECK(ApplyCardAlbumFrame(other, album) == FrameOutcome::kUnhandled);
  const Frame cut{.opcode = ro::proto::ZcMoonlightCardAlbum::kOpcode, .bytes = {0x33, 0x0F}};
  CHECK(ApplyCardAlbumFrame(cut, album) == FrameOutcome::kUndecodable);
}

TEST_CASE("Album : la commande et l'annonce, octet pour octet", "[net][album]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);

  REQUIRE(ro::net::RequestCardAlbum(link, CardAlbumCommand::kPut, 7, 12).has_value());
  const std::vector<std::uint8_t> put =
      server.Receive(ro::proto::CzMoonlightCardAlbumCmd::kWireSize);
  // [op:2][len:2][cmd:1][arg:4][amount:2] — la longueur ÉCRITE, que le serveur lit.
  const std::vector<std::uint8_t> expected = {0x34, 0x0F, 11, 0, 2, 7, 0, 0, 0, 12, 0};
  CHECK(put == expected);

  REQUIRE(ro::net::AnnounceUiCaps(link, ro::net::kUiCapCardAlbum | ro::net::kUiCapCardAlbumDropper)
              .has_value());
  const std::vector<std::uint8_t> caps = server.Receive(ro::proto::CzMoonlightUiCaps::kWireSize);
  const std::vector<std::uint8_t> announced = {0x24, 0x0F, 8, 0, 0x18, 0, 0, 0};
  CHECK(caps == announced);
}
