// LE COURRIER, décodé — et ses demandes, sur le fil.
//
// 🔴 CE BANC PORTE CE QUE LE SERVEUR NE RATTRAPE PAS :
//
//   · une trame tronquée ou incohérente, venue d'un autre joueur par le titre
//     ou le corps, est rejetée entière sans lecture hors trame ;
//   · l'envoi est construit à la main, et vérifié octet par octet ;
//   · ce qui détruirait des biens — titre vide, case du zeny — ne part pas.
//
// Les opcodes s'écrivent ici en clair : les trames sont bâties octet par octet,
// comme le serveur les pose.

#include "ro/net/mail.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/byte_writer.h"
#include "ro/net/connection.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/inventory.h"
#include "ro/sim/mailbox.h"

#include "support/loopback_server.h"
#include "support/net_frames.h"

using ro::core::ByteWriter;
using ro::net::ApplyMailFrame;
using ro::net::FrameOutcome;
using ro::sim::InventoryItem;
using ro::sim::Mailbox;
using ro::test::LoopbackServer;
using ro::test_support::FrameOf;

namespace {

constexpr std::int64_t kNowUnixS = 1700000000;
constexpr std::uint16_t kPotionSlot = 7;

struct ListEntry {
  std::uint8_t box = 0;
  std::int64_t mail_id = 0;
  std::uint8_t read = 0;
  std::uint8_t mask = 0;
  std::string sender;
  std::uint32_t expires_in_s = 0;
  std::string title;
};

std::vector<std::uint8_t> ListFrame(const std::vector<ListEntry>& entries) {
  std::vector<std::uint8_t> bytes;
  ByteWriter writer(bytes);
  writer.U16(0x0AC2);
  writer.U16(0);
  writer.U8(1);
  for (const ListEntry& entry : entries) {
    writer.U8(entry.box);
    writer.I64(entry.mail_id);
    writer.U8(entry.read);
    writer.U8(entry.mask);
    writer.FixedString(entry.sender, 24);
    writer.U32(entry.expires_in_s);
    writer.U16(static_cast<std::uint16_t>(entry.title.size() + 1));
    writer.Text(entry.title);
    writer.U8(0);
  }
  REQUIRE(writer.PatchU16(2, static_cast<std::uint16_t>(bytes.size())));
  return bytes;
}

void ItemBlock(ByteWriter& writer, std::int16_t count, std::uint32_t item_id) {
  writer.I16(count);
  writer.U32(item_id);
  writer.U8(1);  // identifié
  writer.U8(0);  // abîmé
  for (int card = 0; card < 4; ++card) {
    writer.U32(0);
  }
  writer.U32(0);  // emplacement
  writer.U8(3);   // type
  writer.U16(0);  // apparence
  writer.U16(0);  // lien
  for (int option = 0; option < 5; ++option) {
    writer.U16(0);
    writer.I16(0);
    writer.U8(0);
  }
  writer.U8(4);  // raffinement
  writer.U8(0);  // grade
}

// Un contenu : `text` tel quel (zéro final compris s'il y est), `blocks` blocs
// présents pour `item_count` annoncés.
std::vector<std::uint8_t> ContentFrame(std::int64_t mail_id, const std::string& text,
                                       std::uint8_t item_count, int blocks) {
  std::vector<std::uint8_t> bytes;
  ByteWriter writer(bytes);
  writer.U16(0x0B63);
  writer.U16(0);
  writer.U8(0);
  writer.I64(mail_id);
  writer.U16(static_cast<std::uint16_t>(text.size()));
  writer.I64(300);
  writer.U8(item_count);
  writer.Text(text);
  for (int block = 0; block < blocks; ++block) {
    ItemBlock(writer, static_cast<std::int16_t>(block + 2),
              501 + static_cast<std::uint32_t>(block));
  }
  REQUIRE(writer.PatchU16(2, static_cast<std::uint16_t>(bytes.size())));
  return bytes;
}

std::vector<std::uint8_t> AttachAck(std::uint16_t slot, std::int16_t count) {
  ro::proto::ZcAckAddItemRodex ack;
  ack.result = 0;
  ack.index = static_cast<std::int16_t>(slot);
  ack.count = count;
  ack.itemId = 501;
  ack.weight = 70;
  return ro::proto::Encode(ack);
}

std::vector<std::uint8_t> DetachAck(std::uint16_t slot, std::uint16_t count) {
  ro::proto::ZcAckRemoveItemMail ack;
  ack.result = 1;
  ack.index = static_cast<std::int16_t>(slot);
  ack.cnt = count;
  return ro::proto::Encode(ack);
}

InventoryItem Potions(std::int16_t amount, std::uint16_t slot = kPotionSlot) {
  InventoryItem item;
  item.client_slot = slot;
  item.item_id = 501;
  item.amount = amount;
  return item;
}

Mailbox Writing() {
  Mailbox mailbox;
  ro::proto::ZcAckOpenWriteMail granted;
  granted.receiveName = "Bob";
  granted.result = 1;
  REQUIRE(ApplyMailFrame(FrameOf(ro::proto::Encode(granted)), mailbox, kNowUnixS) ==
          FrameOutcome::kApplied);
  REQUIRE(mailbox.writing());
  CHECK(mailbox.write_recipient() == "Bob");
  return mailbox;
}

// Une liaison dont on capte chaque trame émise.
struct Wire {
  LoopbackServer server;
  ro::net::Connection link;
  std::vector<std::vector<std::uint8_t>> sent;

  Wire() {
    REQUIRE(server.listening());
    auto opened = ro::net::Connection::Open("127.0.0.1", server.port());
    REQUIRE(opened.has_value());
    REQUIRE(server.Accept());
    link = std::move(*opened);
    link.set_send_observer([this](std::span<const std::uint8_t> frame) {
      sent.emplace_back(frame.begin(), frame.end());
    });
  }
};

}  // namespace

TEST_CASE("Courrier : une liste à titres de longueur variable", "[net][mail]") {
  Mailbox mailbox;
  const auto frame = ListFrame({
      {.box = 0,
       .mail_id = 41,
       .read = 0,
       .mask = 0x6,
       .sender = "Bob",
       .expires_in_s = 3600,
       .title = "Un titre plus long"},
      {.box = 2,
       .mail_id = 42,
       .read = 1,
       .mask = 0x8,
       .sender = "Kafra",
       .expires_in_s = 60,
       .title = "Ok"},
      // Une boîte inconnue est lue, puis ignorée.
      {.box = 9, .mail_id = 43, .title = "?"},
      // Un an tout juste : la sentinelle « n'expire pas ». L'expéditeur garde
      // ses octets du fil.
      {.box = 0,
       .mail_id = 40,
       .sender = "Andr\xE9",
       .expires_in_s = ro::sim::kMailNeverExpiresSeconds,
       .title = "Sans fin"},
  });
  REQUIRE(ApplyMailFrame(FrameOf(frame), mailbox, kNowUnixS) == FrameOutcome::kApplied);
  REQUIRE(mailbox.headers().size() == 3);
  const ro::sim::MailHeader& endless = mailbox.headers()[2];
  CHECK(endless.mail_id == 40);
  CHECK(endless.never_expires);
  CHECK(endless.sender_wire == "Andr\xE9");
  CHECK(endless.sender == "Andr\xC3\xA9");
  const ro::sim::MailHeader& newest = mailbox.headers()[0];
  CHECK(newest.mail_id == 42);
  CHECK(newest.box == ro::sim::MailBox::kReturned);
  CHECK(newest.read);
  CHECK(newest.from_npc);
  CHECK(newest.title == "Ok");
  const ro::sim::MailHeader& oldest = mailbox.headers()[1];
  CHECK(oldest.title == "Un titre plus long");
  CHECK(oldest.sender == "Bob");
  CHECK(oldest.has_zeny);
  CHECK(oldest.has_items);
  CHECK(oldest.expires_at_unix_s == kNowUnixS + 3600);
  CHECK_FALSE(oldest.never_expires);
}

TEST_CASE("Courrier : une liste tronquée ou un titre trop long est rejeté entier", "[net][mail]") {
  Mailbox mailbox;
  const auto good = ListFrame({{.mail_id = 1, .title = "Garde"}});
  REQUIRE(ApplyMailFrame(FrameOf(good), mailbox, kNowUnixS) == FrameOutcome::kApplied);

  auto truncated = ListFrame({{.mail_id = 2, .title = "A"}, {.mail_id = 3, .title = "B"}});
  truncated.resize(truncated.size() - 20);
  REQUIRE(ByteWriter(truncated).PatchU16(2, static_cast<std::uint16_t>(truncated.size())));
  CHECK(ApplyMailFrame(FrameOf(truncated), mailbox, kNowUnixS) == FrameOutcome::kUndecodable);

  auto overlong = ListFrame({{.mail_id = 4, .title = "Court"}});
  // La longueur du titre suit l'en-tête de liste et la partie fixe de l'entrée.
  constexpr std::size_t kTitleLengthOffset = 5 + 1 + 8 + 1 + 1 + 24 + 4;
  REQUIRE(ByteWriter(overlong).PatchU16(kTitleLengthOffset, 200));
  CHECK(ApplyMailFrame(FrameOf(overlong), mailbox, kNowUnixS) == FrameOutcome::kUndecodable);

  // Une longueur annoncée qui ne colle pas à la trame.
  auto lying = ListFrame({{.mail_id = 5, .title = "X"}});
  REQUIRE(ByteWriter(lying).PatchU16(2, static_cast<std::uint16_t>(lying.size() + 1)));
  CHECK(ApplyMailFrame(FrameOf(lying), mailbox, kNowUnixS) == FrameOutcome::kUndecodable);

  REQUIRE(mailbox.headers().size() == 1);
  CHECK(mailbox.headers()[0].title == "Garde");
}

TEST_CASE("Courrier : un contenu à deux items et nul interne", "[net][mail]") {
  Mailbox mailbox;
  REQUIRE(ApplyMailFrame(FrameOf(ListFrame({{.mail_id = 77, .title = "T"}})), mailbox, kNowUnixS) ==
          FrameOutcome::kApplied);
  const std::string text("ligne1\0ligne2\0", 14);
  REQUIRE(ApplyMailFrame(FrameOf(ContentFrame(77, text, 2, 2)), mailbox, kNowUnixS) ==
          FrameOutcome::kApplied);
  const ro::sim::MailContent* content = mailbox.FindContent(77);
  REQUIRE(content != nullptr);
  CHECK(content->body == "ligne1\nligne2");
  CHECK(content->zeny == 300);
  REQUIRE(content->items.size() == 2);
  CHECK(content->items[0].item_id == 501);
  CHECK(content->items[0].amount == 2);
  CHECK(content->items[1].item_id == 502);
  CHECK(content->items[1].refine_level == 4);
  CHECK(mailbox.FindHeader(77)->read);

  // Plus d'items annoncés que de blocs présents : incohérent, rejeté.
  CHECK(ApplyMailFrame(FrameOf(ContentFrame(78, text, 3, 2)), mailbox, kNowUnixS) ==
        FrameOutcome::kUndecodable);
  CHECK(mailbox.FindContent(78) == nullptr);
  // Un texte annoncé au-delà de la trame.
  auto overlong = ContentFrame(79, "abc", 0, 0);
  REQUIRE(ByteWriter(overlong).PatchU16(13, 400));
  CHECK(ApplyMailFrame(FrameOf(overlong), mailbox, kNowUnixS) == FrameOutcome::kUndecodable);
  // Sept items annoncés et présents : cinq gardés.
  REQUIRE(ApplyMailFrame(FrameOf(ContentFrame(80, "x", 7, 7)), mailbox, kNowUnixS) ==
          FrameOutcome::kApplied);
  CHECK(mailbox.FindContent(80)->items.size() == ro::sim::kMailMostItems);
}

TEST_CASE("Courrier : accusés d'ajout cumulés, échec d'envoi puis retraits", "[net][mail]") {
  Mailbox mailbox = Writing();
  mailbox.NoteAttachSent(Potions(3), 3, 0);
  REQUIRE(ApplyMailFrame(FrameOf(AttachAck(kPotionSlot, 3)), mailbox, kNowUnixS) ==
          FrameOutcome::kApplied);
  mailbox.NoteAttachSent(Potions(2), 2, 0);
  REQUIRE(ApplyMailFrame(FrameOf(AttachAck(kPotionSlot, 2)), mailbox, kNowUnixS) ==
          FrameOutcome::kApplied);
  CHECK(mailbox.AttachedAmountAt(kPotionSlot) == 5);
  CHECK(mailbox.attached_weight() == 70);
  CHECK(mailbox.pending_attaches().empty());

  mailbox.NoteSendSent(0);
  ro::proto::ZcWriteMailResult failed;
  failed.result = ro::sim::kMailSendFailed;
  REQUIRE(ApplyMailFrame(FrameOf(ro::proto::Encode(failed)), mailbox, kNowUnixS) ==
          FrameOutcome::kApplied);
  REQUIRE(ApplyMailFrame(FrameOf(DetachAck(kPotionSlot, 5)), mailbox, kNowUnixS) ==
          FrameOutcome::kApplied);
  CHECK(mailbox.writing());
  CHECK(mailbox.attachment_count() == 0);
}

TEST_CASE("Courrier : encodage exact des demandes", "[net][mail]") {
  Wire wire;
  Mailbox mailbox;
  REQUIRE(
      ApplyMailFrame(FrameOf(ListFrame({{.box = 1, .mail_id = 0x0102, .mask = 0x6, .title = "T"}})),
                     mailbox, kNowUnixS) == FrameOutcome::kApplied);

  REQUIRE(ro::net::RequestMailList(wire.link, 5, 0, 0x0102).has_value());
  REQUIRE(ro::net::RequestReadMail(wire.link, mailbox, 0x0102, 0).has_value());
  REQUIRE(ro::net::RequestClaimZeny(wire.link, mailbox, 0x0102, 0).has_value());
  REQUIRE(ro::net::RequestClaimItems(wire.link, mailbox, 0x0102, 0).has_value());
  REQUIRE(wire.sent.size() == 4);
  CHECK(wire.sent[0] == std::vector<std::uint8_t>{0xC1, 0x0A, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                                  0,    0,    0, 0, 0, 2, 1, 0, 0, 0, 0, 0, 0});
  CHECK(wire.sent[1] == std::vector<std::uint8_t>{0xEA, 0x09, 1, 2, 1, 0, 0, 0, 0, 0, 0});
  CHECK(wire.sent[2] == std::vector<std::uint8_t>{0xF1, 0x09, 2, 1, 0, 0, 0, 0, 0, 0, 1});
  CHECK(wire.sent[3] == std::vector<std::uint8_t>{0xF3, 0x09, 2, 1, 0, 0, 0, 0, 0, 0, 1});
  // La même récupération ne repart pas tant qu'elle est en vol, ni la même
  // lecture.
  CHECK_FALSE(ro::net::RequestClaimItems(wire.link, mailbox, 0x0102, 0).has_value());
  CHECK_FALSE(ro::net::RequestReadMail(wire.link, mailbox, 0x0102, 0).has_value());
  CHECK(mailbox.ReadInFlight(0x0102));

  const auto attach = ro::net::EncodeAttachItem(kPotionSlot, 300);
  REQUIRE(attach.has_value());
  CHECK(*attach == std::vector<std::uint8_t>{0x04, 0x0A, 7, 0, 0x2C, 0x01});
}

TEST_CASE("Courrier : les noms partent et reviennent par leurs octets du fil", "[net][mail]") {
  Wire wire;
  // L'accord porte le nom en octets 1252 : la boîte garde les deux formes.
  Mailbox mailbox;
  ro::proto::ZcAckOpenWriteMail granted;
  granted.receiveName = "Andr\xE9";
  granted.result = 1;
  REQUIRE(ApplyMailFrame(FrameOf(ro::proto::Encode(granted)), mailbox, kNowUnixS) ==
          FrameOutcome::kApplied);
  CHECK(mailbox.write_recipient_wire() == "Andr\xE9");
  CHECK(mailbox.write_recipient() == "Andr\xC3\xA9");

  // L'ouverture émet les octets donnés, tels quels.
  REQUIRE(ro::net::RequestOpenWrite(wire.link, "Andr\xE9").has_value());
  REQUIRE(wire.sent.size() == 1);
  std::vector<std::uint8_t> open = {0x08, 0x0A, 'A', 'n', 'd', 'r', 0xE9};
  open.resize(2 + 24, 0);
  CHECK(wire.sent[0] == open);
  CHECK_FALSE(ro::net::RequestOpenWrite(wire.link, std::string(24, 'a')).has_value());
  CHECK(ro::net::MailRecipientWire("Andr\xC3\xA9") == std::optional<std::string>("Andr\xE9"));

  // La vérification garde le nom interrogé.
  REQUIRE(ro::net::RequestCheckRecipient(wire.link, mailbox, "Andr\xE9", 0).has_value());
  CHECK(mailbox.recipient().queried_wire == "Andr\xE9");
  CHECK_FALSE(ro::net::RequestCheckRecipient(wire.link, mailbox, "", 0).has_value());

  // Aucun retrait ne part pendant un envoi.
  mailbox.NoteAttachSent(Potions(3), 3, 0);
  REQUIRE(ApplyMailFrame(FrameOf(AttachAck(kPotionSlot, 3)), mailbox, kNowUnixS) ==
          FrameOutcome::kApplied);
  mailbox.NoteSendSent(0);
  const std::size_t before = wire.sent.size();
  CHECK_FALSE(ro::net::RequestDetachItem(wire.link, mailbox, kPotionSlot, 1).has_value());
  CHECK(wire.sent.size() == before);
}

TEST_CASE("Courrier : l'envoi, octet par octet", "[net][mail]") {
  const auto fields = ro::net::EncodeMailFields("Bob", "Hi", "Yo");
  REQUIRE(fields.has_value());
  const auto frame = ro::net::EncodeSendMail(*fields, 1000);
  REQUIRE(frame.has_value());
  std::vector<std::uint8_t> expected = {0x6E, 0x0A, 74, 0, 'B', 'o', 'b'};
  expected.resize(4 + 24 + 24, 0);
  const std::vector<std::uint8_t> tail = {0xE8, 0x03, 0, 0,   0,   0, 0, 0,  // zeny
                                          3,    0,    3, 0,  // longueurs, zéro compris
                                          0,    0,    0, 0,  // identifiant ignoré
                                          'H',  'i',  0, 'Y', 'o', 0};
  expected.insert(expected.end(), tail.begin(), tail.end());
  CHECK(*frame == expected);

  // Un corps vide part avec sa seule terminaison.
  const auto empty_body = ro::net::EncodeSendMail({.recipient = "Bob", .title = "T"}, 0);
  REQUIRE(empty_body.has_value());
  CHECK(empty_body->size() == 68 + 2 + 1);
  CHECK((*empty_body)[62] == 1);
}

TEST_CASE("Courrier : les émissions dangereuses ne partent pas", "[net][mail]") {
  using ro::net::EncodeSendMail;
  using ro::net::MailWireFields;
  CHECK_FALSE(EncodeSendMail({.recipient = "Bob", .title = ""}, 0).has_value());
  CHECK_FALSE(EncodeSendMail({.recipient = "Bob", .title = std::string("\0x", 2)}, 0).has_value());
  CHECK_FALSE(EncodeSendMail({.recipient = "Bob", .title = std::string(40, 't')}, 0).has_value());
  CHECK(EncodeSendMail({.recipient = "Bob", .title = std::string(39, 't')}, 0).has_value());
  CHECK_FALSE(EncodeSendMail({.recipient = "", .title = "T"}, 0).has_value());
  CHECK_FALSE(EncodeSendMail({.recipient = std::string(24, 'a'), .title = "T"}, 0).has_value());
  CHECK_FALSE(EncodeSendMail({.recipient = "Bob", .title = "T", .body = std::string(500, 'b')}, 0)
                  .has_value());
  CHECK_FALSE(EncodeSendMail({.recipient = "Bob", .title = "T"}, -1).has_value());
  CHECK_FALSE(
      EncodeSendMail({.recipient = "Bob", .title = "T"}, ro::sim::kMailMostZeny + 1).has_value());

  CHECK_FALSE(ro::net::EncodeAttachItem(0, 1).has_value());
  CHECK_FALSE(ro::net::EncodeAttachItem(1, 1).has_value());
  CHECK_FALSE(ro::net::EncodeAttachItem(kPotionSlot, 0).has_value());
  CHECK_FALSE(
      ro::net::EncodeAttachItem(kPotionSlot, ro::sim::kMailMostAttachAmount + 1).has_value());

  // Les verbes refusent sans rien émettre.
  Wire wire;
  Mailbox closed;
  const MailWireFields fields{.recipient = "Bob", .title = "T"};
  CHECK_FALSE(ro::net::RequestSendMail(wire.link, closed, fields, 0, 1000, 0).has_value());
  Mailbox mailbox = Writing();
  CHECK_FALSE(
      ro::net::RequestSendMail(wire.link, mailbox, {.recipient = "Bob", .title = ""}, 0, 1000, 0)
          .has_value());
  CHECK_FALSE(ro::net::RequestSendMail(wire.link, mailbox, fields, 0, std::nullopt, 0).has_value());
  CHECK_FALSE(ro::net::RequestAttachItem(wire.link, mailbox, Potions(5, 0), 1, 0).has_value());
  InventoryItem worn = Potions(1);
  worn.worn_at = 2;
  CHECK_FALSE(ro::net::RequestAttachItem(wire.link, mailbox, worn, 1, 0).has_value());
  CHECK_FALSE(ro::net::RequestReadMail(wire.link, mailbox, 999, 0).has_value());
  CHECK(wire.sent.empty());

  REQUIRE(ro::net::RequestSendMail(wire.link, mailbox, fields, 0, 1000, 0).has_value());
  CHECK(wire.sent.size() == 1);
  CHECK(mailbox.send_in_flight());
}

TEST_CASE("Courrier : l'ordre d'ouvrir ou de fermer la boîte (`@mail`)", "[net][mail]") {
  const auto order_frame = [](std::uint32_t type) {
    std::vector<std::uint8_t> bytes;
    ByteWriter writer(bytes);
    writer.U16(0x0260);
    writer.U32(type);
    return bytes;
  };
  Mailbox mailbox;

  // Zéro ouvre : c'est ce que le serveur envoie pour `@mail`.
  REQUIRE(ApplyMailFrame(FrameOf(order_frame(0)), mailbox, kNowUnixS) == FrameOutcome::kApplied);
  CHECK(mailbox.TakeWindowOrder() == ro::sim::MailWindowOrder::kOpen);

  REQUIRE(ApplyMailFrame(FrameOf(order_frame(1)), mailbox, kNowUnixS) == FrameOutcome::kApplied);
  CHECK(mailbox.TakeWindowOrder() == ro::sim::MailWindowOrder::kClose);

  // Un type inconnu, une trame tronquée ou trop longue : aucun ordre.
  CHECK(ApplyMailFrame(FrameOf(order_frame(2)), mailbox, kNowUnixS) == FrameOutcome::kUndecodable);
  std::vector<std::uint8_t> truncated = order_frame(0);
  truncated.pop_back();
  CHECK(ApplyMailFrame(FrameOf(truncated), mailbox, kNowUnixS) == FrameOutcome::kUndecodable);
  std::vector<std::uint8_t> padded = order_frame(0);
  padded.push_back(0);
  CHECK(ApplyMailFrame(FrameOf(padded), mailbox, kNowUnixS) == FrameOutcome::kUndecodable);
  CHECK_FALSE(mailbox.TakeWindowOrder().has_value());
}
