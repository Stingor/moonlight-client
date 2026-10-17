// Le courrier. Voir l'en-tête pour le décodage borné et l'encodage des chaînes.

#include "ro/net/mail.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <span>
#include <utility>

#include "ro/core/byte_reader.h"
#include "ro/core/byte_writer.h"
#include "ro/core/text.h"
#include "ro/proto/map_packets.h"

namespace ro::net {
namespace {

using core::ErrorCode;

// ── LES OPCODES QUE LES STRUCTURES GÉNÉRÉES NE PORTENT PAS ──────────────────

constexpr std::uint16_t kListOpcode = 0x0AC2;
constexpr std::uint16_t kZenyClaimedOpcode = 0x09F2;
constexpr std::uint16_t kItemsClaimedOpcode = 0x09F4;
constexpr std::uint16_t kWindowOrderOpcode = 0x0260;

constexpr std::uint16_t kOpenMailboxOpcode = 0x0AC1;
constexpr std::uint16_t kReadMailOpcode = 0x09EA;
constexpr std::uint16_t kClaimZenyOpcode = 0x09F1;
constexpr std::uint16_t kClaimItemsOpcode = 0x09F3;
constexpr std::uint16_t kDeleteMailOpcode = 0x09F5;
constexpr std::uint16_t kCancelWriteOpcode = 0x0A03;
constexpr std::uint16_t kAttachOpcode = 0x0A04;
constexpr std::uint16_t kDetachOpcode = 0x0A06;
constexpr std::uint16_t kOpenWriteOpcode = 0x0A08;
constexpr std::uint16_t kSendOpcode = 0x0A6E;

// ── LES DISPOSITIONS ────────────────────────────────────────────────────────

// Opcode, puis longueur pour une trame variable.
constexpr std::size_t kOpcodeBytes = 2;
constexpr std::size_t kVariableHeadBytes = 4;
constexpr std::size_t kNameBytes = 24;

// La liste : en-tête variable, un octet que le serveur pose à un, puis les
// entrées. Chacune : boîte, identifiant, lu, masque, expéditeur, secondes
// restantes, longueur du titre — puis le titre.
constexpr std::size_t kListHeadBytes = kVariableHeadBytes + 1;
constexpr std::size_t kListEntryFixedBytes = 1 + 8 + 1 + 1 + kNameBytes + 4 + 2;

// Le contenu : en-tête variable, boîte, identifiant, longueur du texte, zeny,
// nombre d'items — puis le texte, puis les blocs d'item.
constexpr std::size_t kContentHeadBytes = kVariableHeadBytes + 1 + 8 + 2 + 8 + 1;
constexpr std::size_t kContentItemBytes = 60;

// Une récupération accusée : identifiant, boîte, résultat.
constexpr std::size_t kClaimAckBytes = kOpcodeBytes + 8 + 1 + 1;
// La pastille « non lu ».
constexpr std::size_t kUnreadBytes = kOpcodeBytes + 1;
// L'ordre sur la boîte : son type, sur trente-deux bits.
constexpr std::size_t kWindowOrderBytes = kOpcodeBytes + 4;
constexpr std::uint32_t kWireWindowOpen = 0;
constexpr std::uint32_t kWireWindowClose = 1;

// L'envoi : en-tête variable, destinataire, expéditeur (ignoré), zeny, deux
// longueurs, identifiant du destinataire (ignoré) — puis titre et corps.
constexpr std::size_t kSendFixedBytes =
    kVariableHeadBytes + kNameBytes + kNameBytes + 8 + 2 + 2 + 4;

// Le masque d'un courrier listé.
constexpr std::uint8_t kMailTypeZeny = 0x2;
constexpr std::uint8_t kMailTypeItems = 0x4;
constexpr std::uint8_t kMailTypeNpc = 0x8;

// La boîte, sur le fil.
constexpr std::uint8_t kWireBoxNormal = 0;
constexpr std::uint8_t kWireBoxAccount = 1;
constexpr std::uint8_t kWireBoxReturned = 2;

// L'accord d'écriture et le retrait réussi.
constexpr std::int8_t kWriteGranted = 1;
constexpr std::int8_t kDetachSucceeded = 1;

std::optional<sim::MailBox> BoxFromWire(std::uint8_t wire) noexcept {
  switch (wire) {
    case kWireBoxNormal:
      return sim::MailBox::kNormal;
    case kWireBoxAccount:
      return sim::MailBox::kAccount;
    case kWireBoxReturned:
      return sim::MailBox::kReturned;
    default:
      return std::nullopt;
  }
}

std::uint8_t WireFromBox(sim::MailBox box) noexcept {
  switch (box) {
    case sim::MailBox::kNormal:
      return kWireBoxNormal;
    case sim::MailBox::kAccount:
      return kWireBoxAccount;
    case sim::MailBox::kReturned:
      return kWireBoxReturned;
  }
  return kWireBoxNormal;
}

std::string_view AsText(std::span<const std::uint8_t> raw) noexcept {
  return {reinterpret_cast<const char*>(raw.data()), raw.size()};
}

// Un champ de texte borné : coupé au premier nul.
std::string_view UpToNul(std::string_view field) noexcept {
  return field.substr(0, std::min(field.find('\0'), field.size()));
}

// Le corps : les nuls de fin retirés, ceux de l'intérieur devenus des sauts de
// ligne.
std::string BodyFromWire(std::string_view raw) {
  const std::size_t last = raw.find_last_not_of('\0');
  std::string body(raw.substr(0, last == std::string_view::npos ? 0 : last + 1));
  std::ranges::replace(body, '\0', '\n');
  return body;
}

std::int16_t ClampAmount(std::int32_t amount) noexcept {
  return static_cast<std::int16_t>(
      std::clamp(amount, std::int32_t{0}, std::int32_t{std::numeric_limits<std::int16_t>::max()}));
}

// LA LISTE. Rien n'est posé tant que la trame entière n'a pas été lue.
FrameOutcome ApplyList(const Frame& frame, sim::Mailbox& mailbox, std::int64_t now_unix_s) {
  core::ByteReader reader(frame.bytes);
  reader.Skip(kOpcodeBytes);
  const std::uint16_t declared = reader.U16();
  reader.Skip(1);
  if (!reader.ok() || declared != frame.bytes.size()) {
    return FrameOutcome::kUndecodable;
  }
  std::vector<sim::MailHeader> headers;
  while (reader.remaining() > 0 && headers.size() < sim::kMailMostListed) {
    if (reader.remaining() < kListEntryFixedBytes) {
      return FrameOutcome::kUndecodable;
    }
    const std::uint8_t wire_box = reader.U8();
    sim::MailHeader header;
    header.mail_id = reader.I64();
    header.read = reader.U8() != 0;
    const std::uint8_t mask = reader.U8();
    header.has_zeny = (mask & kMailTypeZeny) != 0;
    header.has_items = (mask & kMailTypeItems) != 0;
    header.from_npc = (mask & kMailTypeNpc) != 0;
    header.sender_wire = std::string(reader.FixedString(kNameBytes));
    header.sender = core::Utf8FromWire(header.sender_wire);
    const std::uint32_t seconds_left = reader.U32();
    header.expires_at_unix_s = now_unix_s + static_cast<std::int64_t>(seconds_left);
    header.never_expires = seconds_left == sim::kMailNeverExpiresSeconds;
    const std::uint16_t title_bytes = reader.U16();
    if (title_bytes > reader.remaining()) {
      return FrameOutcome::kUndecodable;
    }
    header.title = core::Utf8FromWire(UpToNul(AsText(reader.Bytes(title_bytes))));
    if (!reader.ok()) {
      return FrameOutcome::kUndecodable;
    }
    // Une boîte inconnue ne désigne rien qu'on puisse redemander.
    if (const std::optional<sim::MailBox> box = BoxFromWire(wire_box); box.has_value()) {
      header.box = *box;
      headers.push_back(std::move(header));
    }
  }
  mailbox.TellList(std::move(headers));
  return FrameOutcome::kApplied;
}

sim::InventoryItem ItemFromContentBlock(core::ByteReader& reader) {
  sim::InventoryItem item;
  item.amount = ClampAmount(reader.I16());
  item.item_id = reader.U32();
  item.identified = reader.U8() != 0;
  item.damaged = reader.U8() != 0;
  for (std::uint32_t& card : item.cards) {
    card = reader.U32();
  }
  item.wearable_at = reader.U32();
  item.type = reader.U8();
  item.view_id = reader.U16();
  item.bind_on_equip = reader.U16();
  for (sim::ItemOption& option : item.options) {
    option.id = reader.U16();
    option.value = reader.I16();
    option.param = reader.U8();
  }
  item.refine_level = reader.U8();
  item.grade = reader.U8();
  return item;
}

// LE CONTENU LU. Un nombre d'items que les blocs présents ne couvrent pas est
// une trame incohérente ; au-delà de cinq, les items sont ignorés.
FrameOutcome ApplyContent(const Frame& frame, sim::Mailbox& mailbox) {
  core::ByteReader reader(frame.bytes);
  reader.Skip(kOpcodeBytes);
  const std::uint16_t declared = reader.U16();
  const std::uint8_t wire_box = reader.U8();
  sim::MailContent content;
  content.mail_id = reader.I64();
  const std::uint16_t text_bytes = reader.U16();
  content.zeny = reader.I64();
  const std::uint8_t item_count = reader.U8();
  if (!reader.ok() || declared != frame.bytes.size() || text_bytes > reader.remaining()) {
    return FrameOutcome::kUndecodable;
  }
  content.body = core::Utf8FromWire(BodyFromWire(AsText(reader.Bytes(text_bytes))));
  if (static_cast<std::size_t>(item_count) * kContentItemBytes > reader.remaining()) {
    return FrameOutcome::kUndecodable;
  }
  const std::size_t kept = std::min<std::size_t>(item_count, sim::kMailMostItems);
  for (std::size_t rank = 0; rank < kept; ++rank) {
    content.items.push_back(ItemFromContentBlock(reader));
  }
  const std::optional<sim::MailBox> box = BoxFromWire(wire_box);
  if (!reader.ok() || !box.has_value()) {
    return FrameOutcome::kUndecodable;
  }
  content.box = *box;
  mailbox.TellContent(std::move(content));
  return FrameOutcome::kApplied;
}

sim::InventoryItem ItemFrom(const proto::ZcAckAddItemRodex& packet) {
  sim::InventoryItem item;
  item.client_slot = static_cast<std::uint16_t>(packet.index);
  item.item_id = packet.itemId;
  item.type = static_cast<std::uint8_t>(packet.type);
  item.amount = packet.count;
  item.identified = packet.IsIdentified != 0;
  item.damaged = packet.IsDamaged != 0;
  item.cards = packet.slot.card;
  for (std::size_t rank = 0; rank < sim::kItemOptionSlots; ++rank) {
    item.options[rank].id = static_cast<std::uint16_t>(packet.optionData[rank].index);
    item.options[rank].value = packet.optionData[rank].value;
    item.options[rank].param = packet.optionData[rank].param;
  }
  item.favorite = packet.favorite != 0;
  item.wearable_at = packet.location;
  item.refine_level = static_cast<std::uint8_t>(packet.refiningLevel);
  item.grade = static_cast<std::uint8_t>(packet.grade);
  return item;
}

FrameOutcome ApplyClaimAck(const Frame& frame, sim::Mailbox& mailbox, sim::MailClaim claim) {
  if (frame.bytes.size() != kClaimAckBytes) {
    return FrameOutcome::kUndecodable;
  }
  core::ByteReader reader(frame.bytes);
  reader.Skip(kOpcodeBytes);
  const std::int64_t mail_id = reader.I64();
  reader.Skip(1);
  const std::uint8_t result = reader.U8();
  if (!reader.ok()) {
    return FrameOutcome::kUndecodable;
  }
  mailbox.TellClaimResult(mail_id, claim, result);
  return FrameOutcome::kApplied;
}

core::Result<void> Refuse(std::string_view why) {
  return core::Fail(ErrorCode::kInvalidArgument, std::string("courrier : ") + std::string(why));
}

// Un nom tel qu'il part : non vide si `allow_empty` est faux, au plus
// `sim::kMailMostNameBytes` octets, sans octet nul.
bool WireNameFits(std::string_view wire, bool allow_empty) noexcept {
  return (allow_empty || !wire.empty()) && wire.size() <= sim::kMailMostNameBytes &&
         wire.find('\0') == std::string_view::npos;
}

// Les trames de onze octets qui visent un courrier : boîte puis identifiant, ou
// l'inverse pour les récupérations.
std::vector<std::uint8_t> BoxThenId(std::uint16_t opcode, sim::MailBox box, std::int64_t mail_id) {
  std::vector<std::uint8_t> bytes;
  core::ByteWriter writer(bytes);
  writer.U16(opcode);
  writer.U8(WireFromBox(box));
  writer.I64(mail_id);
  return bytes;
}

std::vector<std::uint8_t> IdThenBox(std::uint16_t opcode, sim::MailBox box, std::int64_t mail_id) {
  std::vector<std::uint8_t> bytes;
  core::ByteWriter writer(bytes);
  writer.U16(opcode);
  writer.I64(mail_id);
  writer.U8(WireFromBox(box));
  return bytes;
}

const char* SendFaultText(sim::MailSendFault fault) {
  switch (fault) {
    case sim::MailSendFault::kNone:
      return "";
    case sim::MailSendFault::kNotWriting:
      return "aucune écriture n'est ouverte";
    case sim::MailSendFault::kSendInFlight:
      return "un envoi est déjà en route";
    case sim::MailSendFault::kReopenDue:
      return "le brouillon est à rouvrir après l'envoi précédent";
    case sim::MailSendFault::kAttachInFlight:
      return "un ajout n'est pas encore accusé";
    case sim::MailSendFault::kRecipientEmpty:
      return "destinataire vide";
    case sim::MailSendFault::kRecipientTooLong:
      return "destinataire trop long";
    case sim::MailSendFault::kTitleEmpty:
      return "titre vide : le serveur détruirait les pièces jointes";
    case sim::MailSendFault::kTitleTooLong:
      return "titre trop long";
    case sim::MailSendFault::kBodyTooLong:
      return "corps trop long";
    case sim::MailSendFault::kEmbeddedNul:
      return "octet nul dans un champ";
    case sim::MailSendFault::kZenyNegative:
      return "zeny négatif";
    case sim::MailSendFault::kZenyAboveMost:
      return "zeny au-delà du plafond";
    case sim::MailSendFault::kPurseUnknown:
      return "bourse inconnue";
    case sim::MailSendFault::kPurseShort:
      return "bourse insuffisante pour le zeny et les frais";
  }
  return "envoi refusé";
}

const char* AttachFaultText(sim::MailAttachFault fault) {
  switch (fault) {
    case sim::MailAttachFault::kNone:
      return "";
    case sim::MailAttachFault::kNotWriting:
      return "aucune écriture n'est ouverte";
    case sim::MailAttachFault::kSendInFlight:
      return "un envoi est en route";
    case sim::MailAttachFault::kReopenDue:
      return "le brouillon est à rouvrir après l'envoi précédent";
    case sim::MailAttachFault::kAttachInFlight:
      return "un ajout attend encore son accusé";
    case sim::MailAttachFault::kBadSlot:
      return "case hors du sac";
    case sim::MailAttachFault::kEquipped:
      return "item porté";
    case sim::MailAttachFault::kRented:
      return "item loué";
    case sim::MailAttachFault::kAllAttached:
      return "toute la pile est déjà jointe";
    case sim::MailAttachFault::kBadAmount:
      return "quantité hors de la pile";
    case sim::MailAttachFault::kAttachmentsFull:
      return "cinq items déjà joints";
  }
  return "ajout refusé";
}

core::Result<void> RequestClaim(Connection& link, sim::Mailbox& mailbox, std::int64_t mail_id,
                                sim::MailClaim claim, std::uint32_t now_ms) {
  const sim::MailHeader* header = mailbox.FindHeader(mail_id);
  if (header == nullptr) {
    return Refuse("récupération d'un courrier inconnu");
  }
  const bool carries = claim == sim::MailClaim::kZeny ? header->has_zeny : header->has_items;
  if (!carries) {
    return Refuse("rien à récupérer");
  }
  if (mailbox.ClaimInFlight(mail_id, claim)) {
    return Refuse("récupération déjà en route");
  }
  const std::uint16_t opcode =
      claim == sim::MailClaim::kZeny ? kClaimZenyOpcode : kClaimItemsOpcode;
  link.Send(IdThenBox(opcode, header->box, mail_id));
  mailbox.NoteClaimSent(mail_id, claim, now_ms);
  return {};
}

FrameOutcome ApplyWindowOrder(const Frame& frame, sim::Mailbox& mailbox) {
  if (frame.bytes.size() != kWindowOrderBytes) {
    return FrameOutcome::kUndecodable;
  }
  core::ByteReader reader(frame.bytes);
  reader.Skip(kOpcodeBytes);
  const std::uint32_t wire = reader.U32();
  if (!reader.ok()) {
    return FrameOutcome::kUndecodable;
  }
  switch (wire) {
    case kWireWindowOpen:
      mailbox.TellWindowOrder(sim::MailWindowOrder::kOpen);
      return FrameOutcome::kApplied;
    case kWireWindowClose:
      mailbox.TellWindowOrder(sim::MailWindowOrder::kClose);
      return FrameOutcome::kApplied;
    default:
      return FrameOutcome::kUndecodable;
  }
}

}  // namespace

FrameOutcome ApplyMailFrame(const Frame& frame, sim::Mailbox& mailbox, std::int64_t now_unix_s) {
  switch (frame.opcode) {
    case kListOpcode:
      return ApplyList(frame, mailbox, now_unix_s);

    case proto::ZcAckReadRodex::kOpcode:
      return ApplyContent(frame, mailbox);

    case proto::ZcAckOpenWriteMail::kOpcode: {
      proto::ZcAckOpenWriteMail packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      const std::string_view wire = UpToNul(packet.receiveName);
      mailbox.TellWriteOpened(packet.result == kWriteGranted, core::Utf8FromWire(wire),
                              std::string(wire));
      return FrameOutcome::kApplied;
    }

    case proto::ZcWriteMailResult::kOpcode: {
      proto::ZcWriteMailResult packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      mailbox.TellSendResult(static_cast<std::uint8_t>(packet.result));
      return FrameOutcome::kApplied;
    }

    case proto::ZcAckAddItemRodex::kOpcode: {
      proto::ZcAckAddItemRodex packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      mailbox.TellAttachResult(static_cast<std::uint8_t>(packet.result), ItemFrom(packet),
                               packet.count, packet.weight);
      return FrameOutcome::kApplied;
    }

    case proto::ZcAckRemoveItemMail::kOpcode: {
      proto::ZcAckRemoveItemMail packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      if (packet.result == kDetachSucceeded) {
        mailbox.TellDetached(static_cast<std::uint16_t>(packet.index), packet.cnt, packet.weight);
      }
      return FrameOutcome::kApplied;
    }

    case proto::ZcCheckname::kOpcode: {
      proto::ZcCheckname packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      mailbox.TellRecipient(
          static_cast<std::uint32_t>(packet.CharId), static_cast<std::uint16_t>(packet.Class),
          static_cast<std::uint16_t>(packet.BaseLevel), core::Utf8FromWire(packet.Name));
      return FrameOutcome::kApplied;
    }

    case proto::ZcNotifyUnreadmail::kOpcode: {
      if (frame.bytes.size() != kUnreadBytes) {
        return FrameOutcome::kUndecodable;
      }
      mailbox.TellUnreadBadge(frame.bytes[kOpcodeBytes] != 0);
      return FrameOutcome::kApplied;
    }

    case kWindowOrderOpcode:
      return ApplyWindowOrder(frame, mailbox);

    case kZenyClaimedOpcode:
      return ApplyClaimAck(frame, mailbox, sim::MailClaim::kZeny);

    case kItemsClaimedOpcode:
      return ApplyClaimAck(frame, mailbox, sim::MailClaim::kItems);

    case proto::ZcAckDeleteMail::kOpcode: {
      proto::ZcAckDeleteMail packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      mailbox.TellDeleted(packet.MailID);
      return FrameOutcome::kApplied;
    }

    case proto::ZcRodexReturn::kOpcode: {
      proto::ZcRodexReturn packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      mailbox.TellReturnResult(packet.msgId, packet.result);
      return FrameOutcome::kApplied;
    }

    case proto::ZcNpcackMapmove::kOpcode:
    case proto::ZcNpcackServermove::kOpcode:
      mailbox.CloseWriteForMapChange();
      return FrameOutcome::kUnhandled;

    default:
      return FrameOutcome::kUnhandled;
  }
}

std::optional<std::string> MailRecipientWire(std::string_view recipient_utf8) {
  return core::ExactCp1252FromUtf8(recipient_utf8);
}

core::Result<MailWireFields> EncodeMailFields(std::string_view recipient_utf8,
                                              std::string_view title_utf8,
                                              std::string_view body_utf8) {
  std::optional<std::string> recipient = MailRecipientWire(recipient_utf8);
  if (!recipient.has_value()) {
    return core::Fail(ErrorCode::kInvalidArgument,
                      "courrier : destinataire inexprimable sur le fil");
  }
  return MailWireFields{.recipient = std::move(*recipient),
                        .title = core::WireFromUtf8Text(title_utf8),
                        .body = core::WireFromUtf8Text(body_utf8)};
}

core::Result<std::vector<std::uint8_t>> EncodeSendMail(const MailWireFields& fields,
                                                       std::int64_t zeny) {
  constexpr char kNul = '\0';
  if (fields.recipient.empty() || fields.recipient.size() > sim::kMailMostNameBytes ||
      fields.recipient.find(kNul) != std::string::npos) {
    return core::Fail(ErrorCode::kInvalidArgument, "courrier : destinataire");
  }
  // 🔴 Le serveur détruirait les pièces jointes d'un titre vide.
  if (fields.title.empty() || fields.title.size() > sim::kMailMostTitleBytes ||
      fields.title.find(kNul) != std::string::npos) {
    return core::Fail(ErrorCode::kInvalidArgument, "courrier : titre");
  }
  if (fields.body.size() > sim::kMailMostBodyBytes || fields.body.find(kNul) != std::string::npos) {
    return core::Fail(ErrorCode::kInvalidArgument, "courrier : corps");
  }
  if (zeny < 0 || zeny > sim::kMailMostZeny) {
    return core::Fail(ErrorCode::kInvalidArgument, "courrier : zeny");
  }
  // Les longueurs comptent le zéro final.
  const auto title_bytes = static_cast<std::uint16_t>(fields.title.size() + 1);
  const auto body_bytes = static_cast<std::uint16_t>(fields.body.size() + 1);
  std::vector<std::uint8_t> bytes;
  bytes.reserve(kSendFixedBytes + title_bytes + body_bytes);
  core::ByteWriter writer(bytes);
  writer.U16(kSendOpcode);
  writer.U16(static_cast<std::uint16_t>(kSendFixedBytes + title_bytes + body_bytes));
  writer.FixedString(fields.recipient, kNameBytes);
  writer.FixedString({}, kNameBytes);
  writer.I64(zeny);
  writer.U16(title_bytes);
  writer.U16(body_bytes);
  writer.U32(0);
  writer.Text(fields.title);
  writer.U8(0);
  writer.Text(fields.body);
  writer.U8(0);
  return bytes;
}

core::Result<std::vector<std::uint8_t>> EncodeAttachItem(std::uint16_t client_slot,
                                                         std::int32_t amount) {
  if (client_slot < sim::kMailFirstBagSlot) {
    return core::Fail(ErrorCode::kInvalidArgument, "courrier : case hors du sac (zeny)");
  }
  if (amount <= 0 || amount > sim::kMailMostAttachAmount) {
    return core::Fail(ErrorCode::kInvalidArgument, "courrier : quantité");
  }
  std::vector<std::uint8_t> bytes;
  core::ByteWriter writer(bytes);
  writer.U16(kAttachOpcode);
  writer.U16(client_slot);
  writer.U16(static_cast<std::uint16_t>(amount));
  return bytes;
}

core::Result<void> RequestMailList(Connection& link, std::int64_t newest_normal,
                                   std::int64_t newest_returned, std::int64_t newest_account) {
  std::vector<std::uint8_t> bytes;
  core::ByteWriter writer(bytes);
  writer.U16(kOpenMailboxOpcode);
  writer.I64(newest_normal);
  writer.I64(newest_returned);
  writer.I64(newest_account);
  link.Send(bytes);
  return {};
}

core::Result<void> RequestReadMail(Connection& link, sim::Mailbox& mailbox, std::int64_t mail_id,
                                   std::uint32_t now_ms) {
  const sim::MailHeader* header = mailbox.FindHeader(mail_id);
  if (header == nullptr) {
    return Refuse("lecture d'un courrier inconnu");
  }
  if (mailbox.ReadInFlight(mail_id)) {
    return Refuse("lecture déjà en route");
  }
  link.Send(BoxThenId(kReadMailOpcode, header->box, mail_id));
  mailbox.NoteReadSent(mail_id, now_ms);
  return {};
}

core::Result<void> RequestClaimItems(Connection& link, sim::Mailbox& mailbox, std::int64_t mail_id,
                                     std::uint32_t now_ms) {
  return RequestClaim(link, mailbox, mail_id, sim::MailClaim::kItems, now_ms);
}

core::Result<void> RequestClaimZeny(Connection& link, sim::Mailbox& mailbox, std::int64_t mail_id,
                                    std::uint32_t now_ms) {
  return RequestClaim(link, mailbox, mail_id, sim::MailClaim::kZeny, now_ms);
}

core::Result<void> RequestDeleteMail(Connection& link, const sim::Mailbox& mailbox,
                                     std::int64_t mail_id) {
  const sim::MailHeader* header = mailbox.FindHeader(mail_id);
  if (header == nullptr) {
    return Refuse("suppression d'un courrier inconnu");
  }
  if (header->has_attachments()) {
    return Refuse("suppression refusée : des pièces jointes restent à récupérer");
  }
  link.Send(BoxThenId(kDeleteMailOpcode, header->box, mail_id));
  return {};
}

core::Result<void> RequestReturnMail(Connection& link, sim::Mailbox& mailbox,
                                     std::int64_t mail_id) {
  const sim::MailHeader* header = mailbox.FindHeader(mail_id);
  if (header == nullptr) {
    return Refuse("retour d'un courrier inconnu");
  }
  if (header->box != sim::MailBox::kNormal || header->from_npc) {
    return Refuse("seul le courrier d'un joueur, dans la boîte normale, se retourne");
  }
  if (mailbox.ReturnInFlight(mail_id)) {
    return Refuse("retour déjà en route");
  }
  proto::CzRodexReturn request;
  request.msgId = static_cast<std::uint32_t>(mail_id);
  link.Send(proto::Encode(request));
  mailbox.NoteReturnSent(mail_id);
  return {};
}

core::Result<void> RequestOpenWrite(Connection& link, std::string_view recipient_wire) {
  if (!WireNameFits(recipient_wire, true)) {
    return Refuse("destinataire trop long ou porteur d'un octet nul");
  }
  std::vector<std::uint8_t> bytes;
  core::ByteWriter writer(bytes);
  writer.U16(kOpenWriteOpcode);
  writer.FixedString(recipient_wire, kNameBytes);
  link.Send(bytes);
  return {};
}

core::Result<void> RequestCancelWrite(Connection& link, sim::Mailbox& mailbox) {
  std::vector<std::uint8_t> bytes;
  core::ByteWriter writer(bytes);
  writer.U16(kCancelWriteOpcode);
  link.Send(bytes);
  mailbox.NoteWriteCancelled();
  return {};
}

core::Result<void> RequestAttachItem(Connection& link, sim::Mailbox& mailbox,
                                     const sim::InventoryItem& bag_item, std::int32_t amount,
                                     std::uint32_t now_ms) {
  if (const sim::MailAttachFault fault = mailbox.CheckAttach(bag_item, amount);
      fault != sim::MailAttachFault::kNone) {
    return Refuse(std::string("ajout refusé : ") + AttachFaultText(fault));
  }
  const core::Result<std::vector<std::uint8_t>> frame =
      EncodeAttachItem(bag_item.client_slot, amount);
  if (!frame) {
    return std::unexpected(frame.error());
  }
  link.Send(*frame);
  mailbox.NoteAttachSent(bag_item, static_cast<std::int16_t>(amount), now_ms);
  return {};
}

core::Result<void> RequestDetachItem(Connection& link, const sim::Mailbox& mailbox,
                                     std::uint16_t client_slot, std::int32_t amount) {
  if (!mailbox.CanDetach(client_slot, amount)) {
    return Refuse("retrait hors de ce qui est joint, ou pendant un envoi");
  }
  std::vector<std::uint8_t> bytes;
  core::ByteWriter writer(bytes);
  writer.U16(kDetachOpcode);
  writer.U16(client_slot);
  writer.U16(static_cast<std::uint16_t>(amount));
  link.Send(bytes);
  return {};
}

core::Result<void> RequestCheckRecipient(Connection& link, sim::Mailbox& mailbox,
                                         std::string_view recipient_wire, std::uint32_t now_ms) {
  if (!WireNameFits(recipient_wire, false)) {
    return Refuse("destinataire vide, trop long ou porteur d'un octet nul");
  }
  proto::CzCheckname2 request;
  request.Name = std::string(recipient_wire);
  link.Send(proto::Encode(request));
  mailbox.NoteRecipientCheckSent(now_ms, std::string(recipient_wire));
  return {};
}

core::Result<void> RequestSendMail(Connection& link, sim::Mailbox& mailbox,
                                   const MailWireFields& fields, std::int64_t zeny,
                                   std::optional<std::int64_t> purse, std::uint32_t now_ms) {
  if (const sim::MailSendFault fault =
          mailbox.CheckSend(fields.recipient, fields.title, fields.body, zeny, purse);
      fault != sim::MailSendFault::kNone) {
    return Refuse(std::string("envoi refusé : ") + SendFaultText(fault));
  }
  const core::Result<std::vector<std::uint8_t>> frame = EncodeSendMail(fields, zeny);
  if (!frame) {
    return std::unexpected(frame.error());
  }
  link.Send(*frame);
  mailbox.NoteSendSent(now_ms);
  return {};
}

}  // namespace ro::net
