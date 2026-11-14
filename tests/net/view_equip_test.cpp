// « VOIR L'ÉQUIPEMENT » SUR LE FIL : la demande telle qu'elle part, la fiche et
// le refus tels qu'ils arrivent.
//
// ⚠ LA FICHE EST FABRIQUÉE À LA MAIN, octet par octet, comme dans
// `inventory_test.cpp` : c'est la DISPOSITION DU FIL qu'on mesure, pas l'accord
// de notre encodeur avec notre décodeur, qui se tromperaient ensemble.

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/net/inventory.h"
#include "ro/proto/player_packets.h"
#include "ro/sim/bank.h"
#include "ro/sim/equip_location.h"
#include "ro/sim/viewed_equipment.h"

#include "support/loopback_server.h"
#include "support/net_frames.h"
#include "support/wire_bytes.h"

using ro::net::ApplyViewEquipFrame;
using ro::net::FrameOutcome;
using ro::sim::ViewedEquipmentBook;
using ro::test::LoopbackServer;
using ro::test_support::FrameOf;
using ro::test_support::ReadU16;
using ro::test_support::ReadU32;

namespace {

constexpr std::uint16_t kSheetOpcode = 0x0B37;
constexpr std::uint16_t kMessageOpcode = 0x0291;
constexpr std::uint16_t kRefusedMessage = 1357;
constexpr std::uint32_t kTarget = 2000123;
constexpr std::size_t kNameBytes = 24;
constexpr std::size_t kOptionBytes = 25;

constexpr std::uint8_t kIdentified = 0x01;
constexpr std::uint8_t kDamaged = 0x02;

class Wire {
 public:
  void U8(std::uint8_t value) { bytes_.push_back(value); }

  void U16(std::uint16_t value) {
    bytes_.push_back(static_cast<std::uint8_t>(value & 0xFFU));
    bytes_.push_back(static_cast<std::uint8_t>(value >> 8U));
  }

  void U32(std::uint32_t value) {
    U16(static_cast<std::uint16_t>(value & 0xFFFFU));
    U16(static_cast<std::uint16_t>(value >> 16U));
  }

  void Zeros(std::size_t count) { bytes_.insert(bytes_.end(), count, 0U); }

  void Text(std::string_view text, std::size_t width) {
    bytes_.insert(bytes_.end(), text.begin(), text.end());
    Zeros(width - text.size());
  }

  void SealLength() {
    const auto length = static_cast<std::uint16_t>(bytes_.size());
    bytes_[2] = static_cast<std::uint8_t>(length & 0xFFU);
    bytes_[3] = static_cast<std::uint8_t>(length >> 8U);
  }

  [[nodiscard]] std::vector<std::uint8_t> take() { return std::move(bytes_); }

 private:
  std::vector<std::uint8_t> bytes_;
};

// L'EN-TÊTE, 47 octets, dans l'ordre du fil.
struct SheetHead {
  std::string_view name = "Alice";
  std::uint16_t job = 4054;
  std::uint16_t hair = 12;
  std::uint16_t accessory = 0;
  std::uint16_t accessory2 = 0;
  std::uint16_t accessory3 = 0;
  std::uint16_t robe = 0;
  std::uint16_t hair_color = 0;
  std::uint16_t clothes_color = 0;
  std::uint16_t body2 = 0;
  std::uint8_t sex = 0;
};

void PutHead(Wire& wire, const SheetHead& head) {
  wire.U16(kSheetOpcode);
  wire.U16(0);  // longueur, scellée à la fin
  wire.Text(head.name, kNameBytes);
  wire.U16(head.job);
  wire.U16(head.hair);
  wire.U16(head.accessory);
  wire.U16(head.accessory2);
  wire.U16(head.accessory3);
  wire.U16(head.robe);
  wire.U16(head.hair_color);
  wire.U16(head.clothes_color);
  wire.U16(head.body2);
  wire.U8(head.sex);
}

// UNE ENTRÉE, 68 octets. Le raffinement se pose APRÈS les options.
struct PieceWire {
  std::uint16_t index = 0;
  std::uint32_t item_id = 0;
  std::uint32_t worn_at = 0;
  std::uint8_t refine = 0;
  std::uint8_t flags = kIdentified;
};

void PutPiece(Wire& wire, const PieceWire& piece) {
  wire.U16(piece.index);
  wire.U32(piece.item_id);
  wire.U8(4);               // type : armure
  wire.U32(piece.worn_at);  // où elle PEUT se porter
  wire.U32(piece.worn_at);  // où elle EST portée
  wire.Zeros(16);           // quatre cartes
  wire.U32(0);              // fin de location
  wire.U16(0);              // liaison
  wire.U16(0);              // apparence
  wire.U8(0);               // compte d'options : toujours zéro sur le fil
  wire.Zeros(kOptionBytes);
  wire.U8(piece.refine);
  wire.U8(0);  // grade
  wire.U8(piece.flags);
}

[[nodiscard]] std::vector<std::uint8_t> Sheet(const SheetHead& head,
                                              const std::vector<PieceWire>& pieces) {
  Wire wire;
  PutHead(wire, head);
  for (const PieceWire& piece : pieces) {
    PutPiece(wire, piece);
  }
  wire.SealLength();
  return wire.take();
}

[[nodiscard]] std::vector<std::uint8_t> Message(std::uint16_t message_id) {
  Wire wire;
  wire.U16(kMessageOpcode);
  wire.U16(message_id);
  return wire.take();
}

}  // namespace

TEST_CASE("Voir l'équipement : la demande part en 0x02D6, l'AID en deux", "[net][view_equip]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  auto link = ro::net::Connection::Open("127.0.0.1", server.port());
  REQUIRE(link.has_value());
  REQUIRE(server.Accept());
  REQUIRE(ro::net::RequestViewEquip(*link, kTarget).has_value());
  const std::vector<std::uint8_t> sent = server.Receive(ro::proto::ViewEquipRequest::kWireSize);
  REQUIRE(sent.size() == ro::proto::ViewEquipRequest::kWireSize);
  CHECK(ReadU16(sent, 0) == 0x02D6);
  CHECK(ReadU32(sent, 2) == kTarget);
}

TEST_CASE("Voir l'équipement : les coiffes de l'en-tête vont BAS, MILIEU, HAUT",
          "[net][view_equip]") {
  // Le paquet d'apparition range bas, haut, milieu ; celui-ci non. Les trois
  // valeurs sont distinctes pour qu'une permutation se voie.
  ViewedEquipmentBook book;
  const SheetHead head{.name = "Bob",
                       .job = 4060,
                       .hair = 7,
                       .accessory = 11,
                       .accessory2 = 22,
                       .accessory3 = 33,
                       .robe = 5,
                       .hair_color = 3,
                       .clothes_color = 2,
                       .body2 = 4060,
                       .sex = 1};
  REQUIRE(ApplyViewEquipFrame(FrameOf(Sheet(head, {})), book) == FrameOutcome::kApplied);
  REQUIRE(book.shown().has_value());
  const ro::sim::ViewedEquipment& sheet = *book.shown();
  CHECK(sheet.name == "Bob");
  CHECK(sheet.look.job == 4060);
  CHECK(sheet.look.hair == 7);
  CHECK(sheet.look.head_low == 11);
  CHECK(sheet.look.head_mid == 22);
  CHECK(sheet.look.head_top == 33);
  CHECK(sheet.look.robe == 5);
  CHECK(sheet.look.hair_color == 3);
  CHECK(sheet.look.clothes_color == 2);
  // Brut : sans style porté, le serveur y remet la classe.
  CHECK(sheet.look.body2 == 4060);
  CHECK(sheet.look.sex == 1);
  CHECK(sheet.pieces.empty());
}

TEST_CASE("Voir l'équipement : une entrée par emplacement, telle que le fil la donne",
          "[net][view_equip]") {
  ViewedEquipmentBook book;
  // Une arme à deux mains part DEUX FOIS sous la même case ; une pièce qui ne
  // se porte nulle part ne se garde pas.
  const std::vector<PieceWire> pieces = {
      {.index = 5, .item_id = 1160, .worn_at = ro::sim::kEquipBothHands, .refine = 7},
      {.index = 5, .item_id = 1160, .worn_at = ro::sim::kEquipBothHands, .refine = 7},
      {.index = 9, .item_id = 2301, .worn_at = 0},
      {.index = 12,
       .item_id = 2101,
       .worn_at = ro::sim::kEquipGarment,
       .flags = kIdentified | kDamaged},
  };
  REQUIRE(ApplyViewEquipFrame(FrameOf(Sheet(SheetHead{}, pieces)), book) == FrameOutcome::kApplied);
  REQUIRE(book.shown().has_value());
  const std::vector<ro::sim::InventoryItem>& shown = book.shown()->pieces;
  REQUIRE(shown.size() == 3);
  CHECK(shown[0].client_slot == 5);
  CHECK(shown[1].client_slot == 5);
  CHECK(shown[0].worn_at == ro::sim::kEquipBothHands);
  // Le raffinement se lit APRÈS les options.
  CHECK(shown[0].refine_level == 7);
  CHECK(shown[0].identified);
  CHECK(shown[2].item_id == 2101);
  CHECK(shown[2].damaged);
}

TEST_CASE("Voir l'équipement : une fiche identique compte comme une réponse neuve",
          "[net][view_equip]") {
  ViewedEquipmentBook book;
  const std::vector<std::uint8_t> bytes = Sheet(SheetHead{}, {});
  REQUIRE(ApplyViewEquipFrame(FrameOf(bytes), book) == FrameOutcome::kApplied);
  REQUIRE(ApplyViewEquipFrame(FrameOf(bytes), book) == FrameOutcome::kApplied);
  // C'est ce qui rouvre la fenêtre quand on redemande le même joueur.
  CHECK(book.received() == 2);
}

TEST_CASE("Voir l'équipement : seul le message 1357 est un refus", "[net][view_equip]") {
  ViewedEquipmentBook book;
  // Le canal des messages en porte bien d'autres, et aucun n'est à nous.
  CHECK(ApplyViewEquipFrame(FrameOf(Message(kRefusedMessage - 1)), book) ==
        FrameOutcome::kUnhandled);
  CHECK(book.refusals() == 0);
  CHECK(ApplyViewEquipFrame(FrameOf(Message(kRefusedMessage)), book) == FrameOutcome::kApplied);
  CHECK(book.refusals() == 1);
  // Un refus ne touche pas à la fiche affichée.
  CHECK_FALSE(book.shown().has_value());
}
