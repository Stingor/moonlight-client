// Le classement mondial du mode survivors sur le fil : codage et décodage, sans
// émettre. Voir l'en-tête pour la forme des trames.

#include "ro/net/survivors_ranking.h"

#include <utility>

#include "ro/core/byte_reader.h"
#include "ro/core/text.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/survivors_state.h"

namespace ro::net {
namespace {

// Le contrat de l'en-tête est celui des structures que `packetgen` engendre des
// en-têtes du serveur : un écart arrête la bâtisse.
static_assert(kSurvivorsBeginOpcode == proto::CzMoonlightSurvivorsBegin::kOpcode);
static_assert(kSurvivorsTicketOpcode == proto::ZcMoonlightSurvivorsTicket::kOpcode);
static_assert(kSurvivorsEndOpcode == proto::CzMoonlightSurvivorsEnd::kOpcode);
static_assert(kSurvivorsResultOpcode == proto::ZcMoonlightSurvivorsResult::kOpcode);
static_assert(kSurvivorsBoardAskOpcode == proto::CzMoonlightSurvivorsBoard::kOpcode);
static_assert(kSurvivorsBoardOpcode == proto::ZcMoonlightSurvivorsBoard::kOpcode);
static_assert(kSurvivorsJournalOpcode == proto::CzMoonlightSurvivorsJournal::kOpcode);
static_assert(kSurvivorsServiceOpcode == proto::ZcMoonlightSurvivorsService::kOpcode);
static_assert(kSurvivorsBeginSize == proto::CzMoonlightSurvivorsBegin::kWireSize);
static_assert(kSurvivorsTicketSize == proto::ZcMoonlightSurvivorsTicket::kWireSize);
static_assert(kSurvivorsEndSize == proto::CzMoonlightSurvivorsEnd::kWireSize);
static_assert(kSurvivorsResultSize == proto::ZcMoonlightSurvivorsResult::kWireSize);
static_assert(kSurvivorsBoardAskSize == proto::CzMoonlightSurvivorsBoard::kWireSize);
static_assert(kSurvivorsBoardHeaderSize == proto::ZcMoonlightSurvivorsBoard::kWireSize);
static_assert(kSurvivorsBoardLineSize == proto::ZcMoonlightSurvivorsBoardEntry::kWireSize);
static_assert(kSurvivorsServiceSize == proto::ZcMoonlightSurvivorsService::kWireSize);

// L'issue d'`END` est la valeur numérique de l'issue de la simulation.
static_assert(kSurvivorsWireOutcomeFell == static_cast<std::uint8_t>(sim::SurvivorsOutcome::kFell));
static_assert(kSurvivorsWireOutcomeVictory ==
              static_cast<std::uint8_t>(sim::SurvivorsOutcome::kVictory));

[[nodiscard]] bool IsSendableBoard(SurvivorsBoardKind board) {
  return board == SurvivorsBoardKind::kRanked || board == SurvivorsBoardKind::kChallenge;
}

[[nodiscard]] SurvivorsBoardKind BoardOf(std::uint8_t wire) {
  switch (wire) {
    case static_cast<std::uint8_t>(SurvivorsBoardKind::kRanked):
      return SurvivorsBoardKind::kRanked;
    case static_cast<std::uint8_t>(SurvivorsBoardKind::kChallenge):
      return SurvivorsBoardKind::kChallenge;
    default:
      break;
  }
  return SurvivorsBoardKind::kUnknown;
}

[[nodiscard]] SurvivorsTicketResult TicketResultOf(std::uint8_t wire) {
  switch (wire) {
    case static_cast<std::uint8_t>(SurvivorsTicketResult::kGranted):
      return SurvivorsTicketResult::kGranted;
    case static_cast<std::uint8_t>(SurvivorsTicketResult::kPlaceRefused):
      return SurvivorsTicketResult::kPlaceRefused;
    case static_cast<std::uint8_t>(SurvivorsTicketResult::kUnlinkedAccount):
      return SurvivorsTicketResult::kUnlinkedAccount;
    case static_cast<std::uint8_t>(SurvivorsTicketResult::kUnknownRules):
      return SurvivorsTicketResult::kUnknownRules;
    case static_cast<std::uint8_t>(SurvivorsTicketResult::kTooSoon):
      return SurvivorsTicketResult::kTooSoon;
    default:
      break;
  }
  return SurvivorsTicketResult::kUnknown;
}

[[nodiscard]] SurvivorsVerdict VerdictOf(std::uint8_t wire) {
  switch (wire) {
    case static_cast<std::uint8_t>(SurvivorsVerdict::kAccepted):
      return SurvivorsVerdict::kAccepted;
    case static_cast<std::uint8_t>(SurvivorsVerdict::kPersonalBest):
      return SurvivorsVerdict::kPersonalBest;
    case static_cast<std::uint8_t>(SurvivorsVerdict::kUnknownTicket):
      return SurvivorsVerdict::kUnknownTicket;
    case static_cast<std::uint8_t>(SurvivorsVerdict::kImplausible):
      return SurvivorsVerdict::kImplausible;
    default:
      break;
  }
  return SurvivorsVerdict::kUnknown;
}

// Une demande de jeton et une demande de tableau ont la même forme.
template <typename Packet>
[[nodiscard]] std::optional<std::vector<std::uint8_t>> EncodeBoardKey(
    const SurvivorsBoardKey& key) {
  if (!IsSendableBoard(key.board)) return std::nullopt;
  Packet packet;
  // ⚠ LA TAILLE EST ÉCRITE, le serveur la LIT.
  packet.packetLength = static_cast<std::int16_t>(Packet::kWireSize);
  packet.ruleset = key.ruleset;
  packet.arena = key.arena_id;
  packet.class_id = key.class_id;
  packet.board = static_cast<std::uint8_t>(key.board);
  return proto::Encode(packet);
}

// Un lecteur posé après l'en-tête, quand la trame porte cet opcode et que son
// `len` dit sa taille ; rien sinon.
[[nodiscard]] std::optional<core::ByteReader> BodyOf(std::span<const std::uint8_t> frame,
                                                     std::uint16_t opcode) {
  core::ByteReader reader(frame);
  const std::uint16_t read_opcode = reader.U16();
  const std::uint16_t length = reader.U16();
  if (!reader.ok() || read_opcode != opcode || length != frame.size()) return std::nullopt;
  return reader;
}

// Un ZC de taille fixe : la table des longueurs le tient pour variable (sa
// structure porte `len`), donc la taille exacte et `len` se vérifient ici avant
// que la structure engendrée ne le lise.
template <typename Packet>
[[nodiscard]] std::optional<Packet> DecodeFixed(std::span<const std::uint8_t> frame) {
  if (frame.size() != Packet::kWireSize || !BodyOf(frame, Packet::kOpcode).has_value()) {
    return std::nullopt;
  }
  Packet packet;
  if (!proto::Decode(frame, packet)) return std::nullopt;
  return packet;
}

[[nodiscard]] SurvivorsBoardKey ReadBoardKey(core::ByteReader& reader) {
  SurvivorsBoardKey key;
  key.ruleset = reader.U16();
  key.arena_id = reader.U16();
  key.class_id = reader.U16();
  key.board = BoardOf(reader.U8());
  return key;
}

}  // namespace

std::optional<std::vector<std::uint8_t>> EncodeSurvivorsBegin(const SurvivorsTicketAsk& ask) {
  return EncodeBoardKey<proto::CzMoonlightSurvivorsBegin>(ask);
}

std::optional<std::vector<std::uint8_t>> EncodeSurvivorsBoardAsk(const SurvivorsBoardAsk& ask) {
  return EncodeBoardKey<proto::CzMoonlightSurvivorsBoard>(ask);
}

std::optional<std::vector<std::uint8_t>> EncodeSurvivorsEnd(const SurvivorsEndReport& report) {
  if (report.outcome < kSurvivorsWireOutcomeFell || report.outcome > kSurvivorsWireOutcomeVictory) {
    return std::nullopt;
  }
  proto::CzMoonlightSurvivorsEnd packet;
  packet.packetLength = static_cast<std::int16_t>(proto::CzMoonlightSurvivorsEnd::kWireSize);
  packet.ticket = report.ticket;
  packet.survived_steps = report.survived_steps;
  packet.kill_points = report.kill_points;
  packet.kills = report.kills;
  packet.boss_kills = report.boss_kills;
  packet.level = report.level;
  packet.outcome = report.outcome;
  packet.journal_bytes = report.journal_bytes;
  return proto::Encode(packet);
}

std::optional<SurvivorsTicket> DecodeSurvivorsTicket(std::span<const std::uint8_t> frame) {
  const std::optional<proto::ZcMoonlightSurvivorsTicket> packet =
      DecodeFixed<proto::ZcMoonlightSurvivorsTicket>(frame);
  if (!packet.has_value()) return std::nullopt;
  SurvivorsTicket ticket;
  ticket.result = TicketResultOf(packet->result);
  ticket.ticket = packet->ticket;
  ticket.seed = packet->seed;
  ticket.issued_at_utc_s = packet->issued_at;
  return ticket;
}

std::optional<SurvivorsResult> DecodeSurvivorsResult(std::span<const std::uint8_t> frame) {
  const std::optional<proto::ZcMoonlightSurvivorsResult> packet =
      DecodeFixed<proto::ZcMoonlightSurvivorsResult>(frame);
  if (!packet.has_value()) return std::nullopt;
  SurvivorsResult result;
  result.ticket = packet->ticket;
  result.verdict = VerdictOf(packet->verdict);
  result.score = packet->score;
  result.rank = packet->rank;
  result.best = packet->best;
  return result;
}

std::optional<SurvivorsService> DecodeSurvivorsService(std::span<const std::uint8_t> frame) {
  const std::optional<proto::ZcMoonlightSurvivorsService> packet =
      DecodeFixed<proto::ZcMoonlightSurvivorsService>(frame);
  if (!packet.has_value()) return std::nullopt;
  SurvivorsService service;
  service.ruleset_low = packet->ruleset_low;
  service.ruleset_high = packet->ruleset_high;
  service.server_utc_s = packet->server_utc;
  return service;
}

// Le tableau se lit à la main : la structure engendrée exige des entrées de
// taille exacte, alors qu'un serveur plus récent peut les allonger en fin.
std::optional<SurvivorsBoardPage> DecodeSurvivorsBoard(std::span<const std::uint8_t> frame) {
  if (frame.size() < kSurvivorsBoardHeaderSize) return std::nullopt;
  std::optional<core::ByteReader> reader = BodyOf(frame, kSurvivorsBoardOpcode);
  if (!reader.has_value()) return std::nullopt;
  SurvivorsBoardPage page;
  page.key = ReadBoardKey(*reader);
  page.own_rank = reader->U16();
  page.own_score = reader->U64();
  const std::size_t count = reader->U8();
  if (!reader->ok()) return std::nullopt;

  const std::size_t tail = frame.size() - kSurvivorsBoardHeaderSize;
  if (count == 0) {
    if (tail != 0) return std::nullopt;
    return page;
  }
  if (tail % count != 0 || tail / count < kSurvivorsBoardLineSize) return std::nullopt;
  const std::size_t stride = tail / count;
  page.lines.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    reader->Seek(kSurvivorsBoardHeaderSize + index * stride);
    SurvivorsBoardLine line;
    line.rank = reader->U16();
    line.score = reader->U64();
    line.survived_steps = reader->U32();
    line.level = reader->U16();
    line.verified = reader->U8() != 0;
    // Le serveur écrit le nom dans la page de codes du jeu.
    line.name = core::Utf8FromWire(reader->FixedString(kSurvivorsBoardNameSize));
    page.lines.push_back(std::move(line));
  }
  if (!reader->ok()) return std::nullopt;
  return page;
}

}  // namespace ro::net
