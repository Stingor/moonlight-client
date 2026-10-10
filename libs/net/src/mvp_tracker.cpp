// Le carnet de chasse MVP sur le fil. Voir l'en-tête pour la forme des trames.

#include "ro/net/mvp_tracker.h"

#include <algorithm>
#include <format>
#include <limits>
#include <optional>
#include <utility>

#include "ro/core/byte_reader.h"
#include "ro/core/byte_writer.h"
#include "ro/core/log.h"
#include "ro/core/text.h"
#include "ro/proto/map_packets.h"

namespace ro::net {

static_assert(kMvpCommandMostBytes <= std::numeric_limits<std::uint16_t>::max(),
              "une commande du carnet tient dans la longueur 16 bits du fil");

namespace {

// UN CHAMP TEXTE DE TAILLE FIXE : coupé au premier NUL, et jamais plus long que
// le champ moins son terminateur — le serveur en pose toujours un, et un champ
// qui n'en porterait pas ne déborde pas pour autant.
[[nodiscard]] std::string_view FieldText(core::ByteReader& reader, std::size_t field_bytes) {
  return reader.FixedString(field_bytes).substr(0, field_bytes - 1);
}

// Une heure du fil, bornée : voir `kMvpLatestWireSeconds`.
[[nodiscard]] std::int64_t WireSeconds(std::int64_t raw_s) noexcept {
  return std::clamp<std::int64_t>(raw_s, 0, kMvpLatestWireSeconds);
}

// Une source inconnue se garde en brut ; l'énumération, elle, retombe sur la
// saisie, la moins précise.
[[nodiscard]] sim::MvpSource SourceOf(std::uint8_t raw) noexcept {
  return raw <= static_cast<std::uint8_t>(sim::MvpSource::kMirror)
             ? static_cast<sim::MvpSource>(raw)
             : sim::MvpSource::kManual;
}

// Une origine d'invitation inconnue de cette version se lit « inconnue » : la
// règle de repli reprend la main.
[[nodiscard]] sim::MvpInviteOrigin InviteOriginOf(std::uint8_t raw) noexcept {
  return raw <= static_cast<std::uint8_t>(sim::MvpInviteOrigin::kPushed)
             ? static_cast<sim::MvpInviteOrigin>(raw)
             : sim::MvpInviteOrigin::kUnknown;
}

[[nodiscard]] std::vector<sim::MvpSlot> ReadCatalogue(core::ByteReader& reader, std::size_t count) {
  std::vector<sim::MvpSlot> slots;
  slots.reserve(count);
  for (std::size_t rank = 0; rank < count; ++rank) {
    sim::MvpSlot slot;
    slot.slot_id = reader.U16();
    slot.mob_id = reader.U16();
    slot.kind = reader.U8();
    slot.delay1_ms = reader.U32();
    slot.delay2_ms = reader.U32();
    slot.map_cells_x = reader.U16();
    slot.map_cells_y = reader.U16();
    // La carte est un nom interne ASCII : ni conversion ni casse.
    slot.map = std::string(FieldText(reader, kMvpMapNameBytes));
    slot.name = core::Utf8FromWire(FieldText(reader, kMvpNameBytes));
    slots.push_back(std::move(slot));
  }
  return slots;
}

[[nodiscard]] std::vector<std::pair<std::uint16_t, sim::MvpObservation>> ReadObservations(
    core::ByteReader& reader, std::size_t count) {
  std::vector<std::pair<std::uint16_t, sim::MvpObservation>> entries;
  entries.reserve(count);
  for (std::size_t rank = 0; rank < count; ++rank) {
    const std::uint16_t slot_id = reader.U16();
    sim::MvpObservation observation;
    observation.source_raw = reader.U8();
    observation.source = SourceOf(observation.source_raw);
    observation.mob_id = reader.U16();
    observation.kill_time_s = WireSeconds(reader.I64());
    observation.exact_respawn_s = WireSeconds(reader.I64());
    observation.tomb_x = reader.I16();
    observation.tomb_y = reader.I16();
    observation.by_user_id = reader.U32();
    observation.reported_at_s = WireSeconds(reader.I64());
    observation.by_name = core::Utf8FromWire(FieldText(reader, kMvpNameBytes));
    entries.emplace_back(slot_id, std::move(observation));
  }
  return entries;
}

[[nodiscard]] std::vector<std::uint16_t> ReadFavorites(core::ByteReader& reader,
                                                       std::size_t count) {
  std::vector<std::uint16_t> slot_ids;
  slot_ids.reserve(count);
  for (std::size_t rank = 0; rank < count; ++rank) {
    slot_ids.push_back(reader.U16());
  }
  return slot_ids;
}

[[nodiscard]] FrameOutcome ApplyStateFrame(const Frame& frame, sim::MvpTrackerBook& book,
                                           std::int64_t local_now_s) {
  proto::ZcMoonlightMvpState packet;
  if (!proto::Decode(frame.bytes, packet)) {
    return FrameOutcome::kUndecodable;
  }
  // 🔴 L'HORLOGE SE RECALE AVANT TOUT CONTRÔLE DU CORPS : chaque paquet d'état
  // porte l'heure du serveur, celui qu'on ne sait pas lire compris.
  book.NoteServerTime(WireSeconds(packet.server_time), local_now_s);

  const std::size_t available = frame.bytes.size() - kMvpStateFixedSize;
  const std::size_t count = packet.count;
  core::ByteReader reader(frame.bytes);
  if (!reader.Seek(kMvpStateFixedSize)) {
    return FrameOutcome::kUndecodable;
  }
  // ⚠ UNE QUEUE PLUS COURTE QUE SON COMPTE NE TOUCHE À RIEN : l'état d'avant
  // reste entier. Un corps plus long, lui, passe ; l'excédent est ignoré.
  const auto Fits = [&](std::size_t entry_bytes) { return available >= count * entry_bytes; };

  const std::uint8_t kind = packet.kind;
  if (kind == static_cast<std::uint8_t>(MvpStateKind::kCatalogue)) {
    if (!Fits(kMvpCatalogueEntrySize)) {
      return FrameOutcome::kUndecodable;
    }
    std::vector<sim::MvpSlot> slots = ReadCatalogue(reader, count);
    core::LogInfo("carnet MVP : catalogue de {} créneau(x)", slots.size());
    book.ReceiveCatalogue(std::move(slots));
    return FrameOutcome::kApplied;
  }
  if (kind == static_cast<std::uint8_t>(MvpStateKind::kSnapshot) ||
      kind == static_cast<std::uint8_t>(MvpStateKind::kDelta)) {
    if (!Fits(kMvpObservationEntrySize)) {
      return FrameOutcome::kUndecodable;
    }
    // Un instantané à zéro entrée vide tout : c'est la réponse « sans groupe ».
    book.ReceiveObservations(ReadObservations(reader, count),
                             kind == static_cast<std::uint8_t>(MvpStateKind::kSnapshot));
    return FrameOutcome::kApplied;
  }
  if (kind == static_cast<std::uint8_t>(MvpStateKind::kFavorites)) {
    if (!Fits(kMvpFavoriteEntrySize)) {
      return FrameOutcome::kUndecodable;
    }
    book.ReceiveFavorites(ReadFavorites(reader, count));
    return FrameOutcome::kApplied;
  }
  return FrameOutcome::kIgnored;
}

[[nodiscard]] FrameOutcome ApplyGroupFrame(const Frame& frame, sim::MvpTrackerBook& book,
                                           std::uint32_t now_ms) {
  proto::ZcMoonlightMvpGroup packet;
  if (!proto::Decode(frame.bytes, packet)) {
    return FrameOutcome::kUndecodable;
  }
  const std::string group_name =
      core::Utf8FromWire(std::string_view(packet.name).substr(0, kMvpGroupNameBytes - 1));

  // LES QUEUES DE L'INVITATION ET DU RÉSULTAT suivent l'en-tête fixe : elles se
  // lisent quand la trame est assez longue pour les porter.
  core::ByteReader trailer(frame.bytes);
  const bool header_skipped = trailer.Seek(kMvpGroupFixedSize);
  const std::size_t after_header = frame.bytes.size() - kMvpGroupFixedSize;

  // LE RÉSULTAT D'UNE COMMANDE ne touche ni au groupe ni à l'invitation.
  if (packet.kind == static_cast<std::uint8_t>(MvpGroupKind::kResult)) {
    std::optional<sim::MvpResultTag> tag;
    if (header_skipped && after_header >= kMvpResultTagTrailerSize) {
      tag = sim::MvpResultTag{.request_id = trailer.U32(), .command = trailer.U8()};
    }
    book.ReceiveResult(packet.result, now_ms, tag);
    return FrameOutcome::kApplied;
  }
  // L'INVITATION EN ATTENTE, sans test sur le groupe : zéro l'efface de fait.
  if (packet.kind == static_cast<std::uint8_t>(MvpGroupKind::kInvitation)) {
    std::optional<sim::MvpInviteOrigin> origin;
    if (header_skipped && after_header >= kMvpInviteOriginTrailerSize) {
      origin = InviteOriginOf(trailer.U8());
    }
    book.ReceiveInvitation(packet.group_id, group_name, origin);
    return FrameOutcome::kApplied;
  }

  // ⚠ TOUT AUTRE KIND EST LE GROUPE, comme le serveur l'entend aujourd'hui.
  // Un groupe zéro veut dire « dans aucun groupe » : c'est une réponse, et le
  // panneau se vide.
  sim::MvpGroup group;
  group.group_id = packet.group_id;
  group.owner_user_id = packet.owner_user_id;
  group.name = group_name;

  const std::size_t count = packet.count;
  if (frame.bytes.size() - kMvpGroupFixedSize < count * kMvpMemberEntrySize) {
    // Le groupe est su, ses membres non : il reste sans membres, et ce que le
    // groupe savait n'est pas jeté sur une réponse illisible.
    book.ReceiveGroup(std::move(group), false);
    return FrameOutcome::kUndecodable;
  }
  core::ByteReader reader(frame.bytes);
  if (!reader.Seek(kMvpGroupFixedSize)) {
    return FrameOutcome::kUndecodable;
  }
  group.members.reserve(count);
  for (std::size_t rank = 0; rank < count; ++rank) {
    sim::MvpMember member;
    member.user_id = reader.U32();
    member.level = reader.I16();
    member.online = reader.U8() != 0;
    member.name = core::Utf8FromWire(FieldText(reader, kMvpNameBytes));
    group.members.push_back(std::move(member));
  }
  // QUI NOUS SOMMES, derrière le dernier membre.
  if (frame.bytes.size() - kMvpGroupFixedSize >=
      count * kMvpMemberEntrySize + kMvpGroupSelfTrailerSize) {
    group.self_user_id = reader.U32();
  }
  book.ReceiveGroup(std::move(group), true);
  return FrameOutcome::kApplied;
}

}  // namespace

FrameOutcome ApplyMvpTrackerFrame(const Frame& frame, sim::MvpTrackerBook& book,
                                  std::int64_t local_now_s, std::uint32_t now_ms) {
  if (frame.opcode == proto::ZcMoonlightMvpState::kOpcode) {
    return ApplyStateFrame(frame, book, local_now_s);
  }
  if (frame.opcode == proto::ZcMoonlightMvpGroup::kOpcode) {
    return ApplyGroupFrame(frame, book, now_ms);
  }
  return FrameOutcome::kUnhandled;
}

std::vector<std::uint8_t> EncodeMvpCommand(MvpCommand command, std::uint32_t a, std::uint32_t b,
                                           std::string_view text_wire, std::uint32_t request_id) {
  // Le texte n'est pas terminé sur le fil : sa longueur est celle de la trame.
  // 🔴 COUPÉ À SON PREMIER NUL : un NUL dans le texte serait lu comme le début
  // du suffixe d'identifiant.
  const std::string_view kept =
      text_wire.substr(0, std::min(text_wire.find('\0'), kMvpCommandTextMost));
  const std::size_t suffix_bytes = request_id != 0 ? kMvpCommandRequestIdSize : 0;
  const std::size_t total_bytes = kMvpCommandFixedSize + kept.size() + suffix_bytes;
  std::vector<std::uint8_t> bytes;
  bytes.reserve(total_bytes);
  core::ByteWriter writer(bytes);
  writer.U16(proto::CzMoonlightMvpCmd::kOpcode);
  writer.U16(static_cast<std::uint16_t>(total_bytes));
  writer.U8(static_cast<std::uint8_t>(command));
  writer.U32(a);
  writer.U32(b);
  writer.Text(kept);
  if (request_id != 0) {
    writer.U8(0);
    writer.U32(request_id);
  }
  return bytes;
}

void RequestMvpCommand(Connection& link, MvpCommand command, std::uint32_t a, std::uint32_t b,
                       std::string_view text_utf8, std::uint32_t request_id) {
  const std::string text_wire = text_utf8.empty() ? std::string() : core::Cp1252FromUtf8(text_utf8);
  link.Send(EncodeMvpCommand(command, a, b, text_wire, request_id));
}

std::string MvpManualReportText(int tomb_x, int tomb_y, std::string_view shared_by_utf8) {
  // « x,y|nom », les deux moitiés facultatives. Le nom vient EN DERNIER et le
  // serveur ne l'analyse pas : il peut tout porter, sauf la barre qui le précède.
  std::string text;
  if (tomb_x >= 0 && tomb_y >= 0) {
    text = std::format("{},{}", tomb_x, tomb_y);
  }
  if (!shared_by_utf8.empty()) {
    text += '|';
    text += shared_by_utf8;
  }
  return text;
}

}  // namespace ro::net
