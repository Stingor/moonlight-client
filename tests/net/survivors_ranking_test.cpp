// LE CLASSEMENT SURVIVORS SUR LE FIL : chaque CZ épinglé à l'octet, chaque ZC
// décodé depuis des octets écrits à la main, et ce qu'une trame abîmée donne ;
// le tableau aussi depuis la structure engendrée, qui épingle sa disposition.

#include "ro/net/survivors_ranking.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/proto/map_packets.h"

#include "support/wire_bytes.h"

using ro::net::DecodeSurvivorsBoard;
using ro::net::DecodeSurvivorsResult;
using ro::net::DecodeSurvivorsService;
using ro::net::DecodeSurvivorsTicket;
using ro::net::EncodeSurvivorsBegin;
using ro::net::EncodeSurvivorsBoardAsk;
using ro::net::EncodeSurvivorsEnd;
using ro::net::SurvivorsBoardKind;
using ro::net::SurvivorsBoardPage;
using ro::net::SurvivorsEndReport;
using ro::net::SurvivorsTicketAsk;
using ro::net::SurvivorsTicketResult;
using ro::net::SurvivorsVerdict;
using ro::test_support::PutU16;
using ro::test_support::PutU32;
using ro::test_support::PutU64;
using ro::test_support::ReadU16;
using ro::test_support::ReadU32;

namespace {

constexpr std::uint16_t kRuleset = 3;
constexpr std::uint16_t kArena = 1;
constexpr std::uint16_t kClass = 2;
constexpr std::uint32_t kTicket = 0xA1B2C3D4;
constexpr std::uint64_t kSeed = 0x0102'0304'0506'0708ULL;
constexpr std::int64_t kIssuedAt = 1'761'523'200;  // 2025-10-27 00:00:00 UTC
constexpr std::uint64_t kScore = 48'210;
constexpr std::uint64_t kBest = 51'000;
constexpr std::uint16_t kRank = 14;
constexpr std::size_t kNameSize = ro::net::kSurvivorsBoardNameSize;

// L'en-tête d'un ZC : l'opcode, puis la taille annoncée.
void PutHeader(std::vector<std::uint8_t>& into, std::uint16_t opcode, std::size_t size) {
  PutU16(into, opcode);
  PutU16(into, static_cast<std::uint16_t>(size));
}

void PutName(std::vector<std::uint8_t>& into, const std::string& name) {
  for (std::size_t index = 0; index < kNameSize; ++index) {
    into.push_back(index < name.size() ? static_cast<std::uint8_t>(name[index]) : 0);
  }
}

[[nodiscard]] std::vector<std::uint8_t> TicketFrame(std::uint8_t result) {
  std::vector<std::uint8_t> frame;
  PutHeader(frame, ro::net::kSurvivorsTicketOpcode, ro::net::kSurvivorsTicketSize);
  frame.push_back(result);
  PutU32(frame, kTicket);
  PutU64(frame, kSeed);
  PutU64(frame, static_cast<std::uint64_t>(kIssuedAt));
  return frame;
}

[[nodiscard]] std::vector<std::uint8_t> ResultFrame(std::uint8_t verdict) {
  std::vector<std::uint8_t> frame;
  PutHeader(frame, ro::net::kSurvivorsResultOpcode, ro::net::kSurvivorsResultSize);
  PutU32(frame, kTicket);
  frame.push_back(verdict);
  PutU64(frame, kScore);
  PutU16(frame, kRank);
  PutU64(frame, kBest);
  return frame;
}

// Une page du tableau dont chaque entrée porte `extra` octets de plus que la
// forme connue ; `count` lignes, le compte écrit valant `written_count`.
[[nodiscard]] std::vector<std::uint8_t> BoardFrame(std::size_t count, std::size_t extra,
                                                   std::uint8_t written_count) {
  const std::size_t stride = ro::net::kSurvivorsBoardLineSize + extra;
  std::vector<std::uint8_t> frame;
  PutHeader(frame, ro::net::kSurvivorsBoardOpcode,
            ro::net::kSurvivorsBoardHeaderSize + count * stride);
  PutU16(frame, kRuleset);
  PutU16(frame, kArena);
  PutU16(frame, kClass);
  frame.push_back(1);  // défi
  PutU16(frame, kRank);
  PutU64(frame, kScore);
  frame.push_back(written_count);
  for (std::size_t line = 0; line < count; ++line) {
    PutU16(frame, static_cast<std::uint16_t>(line + 1));
    PutU64(frame, kBest - line);
    PutU32(frame, static_cast<std::uint32_t>(45'000 + line));
    PutU16(frame, static_cast<std::uint16_t>(40 + line));
    frame.push_back(0);
    PutName(frame, "Joueur" + std::to_string(line));
    frame.insert(frame.end(), extra, std::uint8_t{0xEE});
  }
  return frame;
}

[[nodiscard]] std::uint64_t ReadU64(const std::vector<std::uint8_t>& bytes, std::size_t at) {
  constexpr unsigned kHalfBits = 32;
  return static_cast<std::uint64_t>(ReadU32(bytes, at)) |
         (static_cast<std::uint64_t>(ReadU32(bytes, at + sizeof(std::uint32_t))) << kHalfBits);
}

}  // namespace

TEST_CASE("Classement survivors : BEGIN et BOARD épinglés à l'octet", "[net][survivors]") {
  const SurvivorsTicketAsk ask{.ruleset = kRuleset,
                               .arena_id = kArena,
                               .class_id = kClass,
                               .board = SurvivorsBoardKind::kChallenge};
  const std::optional<std::vector<std::uint8_t>> begin = EncodeSurvivorsBegin(ask);
  REQUIRE(begin.has_value());
  CHECK(*begin == std::vector<std::uint8_t>{0x4C, 0x0F, 11, 0, 3, 0, 1, 0, 2, 0, 1});

  const std::optional<std::vector<std::uint8_t>> board =
      EncodeSurvivorsBoardAsk({.ruleset = kRuleset,
                               .arena_id = kArena,
                               .class_id = kClass,
                               .board = SurvivorsBoardKind::kRanked});
  REQUIRE(board.has_value());
  CHECK(*board == std::vector<std::uint8_t>{0x50, 0x0F, 11, 0, 3, 0, 1, 0, 2, 0, 0});

  // Un tableau inconnu ne part jamais.
  SurvivorsTicketAsk unknown = ask;
  unknown.board = SurvivorsBoardKind::kUnknown;
  CHECK_FALSE(EncodeSurvivorsBegin(unknown).has_value());
  CHECK_FALSE(EncodeSurvivorsBoardAsk(unknown).has_value());
}

TEST_CASE("Classement survivors : END épinglé à l'octet, seule une issue finie part",
          "[net][survivors]") {
  const SurvivorsEndReport report{.ticket = kTicket,
                                  .survived_steps = 48'300,
                                  .kill_points = 279'966,
                                  .kills = 2'345,
                                  .boss_kills = 2,
                                  .level = 49,
                                  .outcome = 3,
                                  .journal_bytes = 6'144};
  const std::optional<std::vector<std::uint8_t>> end = EncodeSurvivorsEnd(report);
  REQUIRE(end.has_value());
  const std::vector<std::uint8_t>& bytes = *end;
  REQUIRE(bytes.size() == ro::net::kSurvivorsEndSize);
  CHECK(ReadU16(bytes, 0) == 0x0F4E);
  CHECK(ReadU16(bytes, 2) == 29);
  CHECK(ReadU32(bytes, 4) == kTicket);
  CHECK(ReadU32(bytes, 8) == 48'300);
  CHECK(ReadU32(bytes, 12) == 279'966);
  CHECK(ReadU32(bytes, 16) == 2'345);
  CHECK(ReadU16(bytes, 20) == 2);
  CHECK(ReadU16(bytes, 22) == 49);
  CHECK(bytes[24] == 3);
  CHECK(ReadU32(bytes, 25) == 6'144);
  // Le jeton part petit-boutiste, l'octet de poids faible d'abord.
  CHECK(bytes[4] == 0xD4);

  SurvivorsEndReport running = report;
  running.outcome = 0;
  CHECK_FALSE(EncodeSurvivorsEnd(running).has_value());
  SurvivorsEndReport abandoned = report;
  abandoned.outcome = 4;
  CHECK_FALSE(EncodeSurvivorsEnd(abandoned).has_value());
  SurvivorsEndReport fell = report;
  fell.outcome = 1;
  CHECK(EncodeSurvivorsEnd(fell).has_value());
}

TEST_CASE("Classement survivors : TICKET décodé, un résultat inconnu se lit inconnu",
          "[net][survivors]") {
  const auto granted = DecodeSurvivorsTicket(TicketFrame(0));
  REQUIRE(granted.has_value());
  CHECK(granted->result == SurvivorsTicketResult::kGranted);
  CHECK(granted->ticket == kTicket);
  CHECK(granted->seed == kSeed);
  CHECK(granted->issued_at_utc_s == kIssuedAt);

  CHECK(DecodeSurvivorsTicket(TicketFrame(4))->result == SurvivorsTicketResult::kTooSoon);
  CHECK(DecodeSurvivorsTicket(TicketFrame(9))->result == SurvivorsTicketResult::kUnknown);
}

TEST_CASE("Classement survivors : RESULT décodé, un verdict inconnu se lit inconnu",
          "[net][survivors]") {
  const auto result = DecodeSurvivorsResult(ResultFrame(1));
  REQUIRE(result.has_value());
  CHECK(result->ticket == kTicket);
  CHECK(result->verdict == SurvivorsVerdict::kPersonalBest);
  CHECK(result->score == kScore);
  CHECK(result->rank == kRank);
  CHECK(result->best == kBest);
  CHECK(DecodeSurvivorsResult(ResultFrame(3))->verdict == SurvivorsVerdict::kImplausible);
  CHECK(DecodeSurvivorsResult(ResultFrame(7))->verdict == SurvivorsVerdict::kUnknown);
}

TEST_CASE("Classement survivors : SERVICE décodé", "[net][survivors]") {
  std::vector<std::uint8_t> frame;
  PutHeader(frame, ro::net::kSurvivorsServiceOpcode, ro::net::kSurvivorsServiceSize);
  PutU16(frame, 2);
  PutU16(frame, 3);
  PutU64(frame, static_cast<std::uint64_t>(kIssuedAt));
  const auto service = DecodeSurvivorsService(frame);
  REQUIRE(service.has_value());
  CHECK(service->ruleset_low == 2);
  CHECK(service->ruleset_high == 3);
  CHECK(service->server_utc_s == kIssuedAt);
}

TEST_CASE("Classement survivors : trame tronquée, opcode étranger ou len qui ment : rien",
          "[net][survivors]") {
  std::vector<std::uint8_t> truncated = TicketFrame(0);
  truncated.pop_back();
  CHECK_FALSE(DecodeSurvivorsTicket(truncated).has_value());

  // Un RESULT n'est pas un TICKET, ni l'inverse.
  CHECK_FALSE(DecodeSurvivorsTicket(ResultFrame(0)).has_value());
  CHECK_FALSE(DecodeSurvivorsResult(TicketFrame(0)).has_value());
  CHECK_FALSE(DecodeSurvivorsService(TicketFrame(0)).has_value());
  CHECK_FALSE(DecodeSurvivorsBoard(TicketFrame(0)).has_value());

  // La trame a la bonne taille, mais son len en annonce une autre.
  std::vector<std::uint8_t> lying = ResultFrame(0);
  lying[2] = static_cast<std::uint8_t>(ro::net::kSurvivorsResultSize + 1);
  CHECK_FALSE(DecodeSurvivorsResult(lying).has_value());
  std::vector<std::uint8_t> lying_board = BoardFrame(2, 0, 2);
  lying_board[2] = static_cast<std::uint8_t>(lying_board[2] - 1);
  CHECK_FALSE(DecodeSurvivorsBoard(lying_board).has_value());

  CHECK_FALSE(DecodeSurvivorsTicket({}).has_value());
  CHECK_FALSE(DecodeSurvivorsBoard({}).has_value());
}

TEST_CASE("Classement survivors : une page du tableau, la taille d'entrée lue dans la trame",
          "[net][survivors]") {
  const std::optional<SurvivorsBoardPage> page = DecodeSurvivorsBoard(BoardFrame(3, 0, 3));
  REQUIRE(page.has_value());
  CHECK(page->key.ruleset == kRuleset);
  CHECK(page->key.arena_id == kArena);
  CHECK(page->key.class_id == kClass);
  CHECK(page->key.board == SurvivorsBoardKind::kChallenge);
  CHECK(page->own_rank == kRank);
  CHECK(page->own_score == kScore);
  REQUIRE(page->lines.size() == 3);
  CHECK(page->lines[0].rank == 1);
  CHECK(page->lines[0].score == kBest);
  CHECK(page->lines[2].survived_steps == 45'002);
  CHECK(page->lines[2].level == 42);
  CHECK_FALSE(page->lines[2].verified);
  CHECK(page->lines[1].name == "Joueur1");

  // Une entrée allongée de huit octets se lit à l'identique.
  const std::optional<SurvivorsBoardPage> longer = DecodeSurvivorsBoard(BoardFrame(3, 8, 3));
  REQUIRE(longer.has_value());
  CHECK(*longer == *page);

  // Le plus grand tableau : vingt lignes, 842 octets.
  const std::vector<std::uint8_t> full = BoardFrame(20, 0, 20);
  CHECK(full.size() == 842);
  CHECK(DecodeSurvivorsBoard(full)->lines.size() == 20);
}

TEST_CASE("Classement survivors : un tableau vide, et un compte qui ne tombe pas juste",
          "[net][survivors]") {
  const std::optional<SurvivorsBoardPage> empty = DecodeSurvivorsBoard(BoardFrame(0, 0, 0));
  REQUIRE(empty.has_value());
  CHECK(empty->lines.empty());

  // Un compte nul exige une trame sans entrée.
  std::vector<std::uint8_t> tail_without_count = BoardFrame(1, 0, 1);
  tail_without_count[ro::net::kSurvivorsBoardHeaderSize - 1] = 0;
  CHECK_FALSE(DecodeSurvivorsBoard(tail_without_count).has_value());
  // Deux entrées annoncées pour trois portées : 3 × 41 ne se divise pas par 2.
  CHECK_FALSE(DecodeSurvivorsBoard(BoardFrame(3, 0, 2)).has_value());
  // Quarante et une annoncées pour deux portées : 82 octets se divisent, mais
  // en entrées de deux octets.
  CHECK_FALSE(DecodeSurvivorsBoard(BoardFrame(2, 0, 41)).has_value());
}

TEST_CASE("Classement survivors : un nom de 24 octets sans zéro se lit entier",
          "[net][survivors]") {
  std::vector<std::uint8_t> frame = BoardFrame(1, 0, 1);
  const std::string full_name(kNameSize, 'N');
  constexpr std::size_t kNameAt = ro::net::kSurvivorsBoardHeaderSize + 17;
  for (std::size_t index = 0; index < kNameSize; ++index) {
    frame[kNameAt + index] = static_cast<std::uint8_t>(full_name[index]);
  }
  const auto page = DecodeSurvivorsBoard(frame);
  REQUIRE(page.has_value());
  REQUIRE(page->lines.size() == 1);
  CHECK(page->lines[0].name == full_name);
  CHECK(ReadU64(frame, ro::net::kSurvivorsBoardHeaderSize + 2) == kBest);
}

// La lecture à la main du tableau, épinglée contre la disposition du serveur :
// une page codée par la structure engendrée se relit champ à champ. Chaque
// champ porte une valeur qu'aucun autre de même taille ne porte, pour qu'une
// permutation ne passe pas.
TEST_CASE("Classement survivors : la page codée par la structure engendrée se relit champ à champ",
          "[net][survivors]") {
  constexpr std::uint16_t kDistinctRuleset = 0x0103;
  constexpr std::uint16_t kDistinctArena = 0x0205;
  constexpr std::uint16_t kDistinctClass = 0x0307;
  constexpr std::uint16_t kDistinctOwnRank = 0x0409;
  constexpr std::uint64_t kDistinctOwnScore = 0x0A0B0C0D0E0F1011;
  ro::proto::ZcMoonlightSurvivorsBoard sent;
  sent.ruleset = kDistinctRuleset;
  sent.arena = kDistinctArena;
  sent.class_id = kDistinctClass;
  sent.board = static_cast<std::uint8_t>(SurvivorsBoardKind::kChallenge);
  sent.own_rank = kDistinctOwnRank;
  sent.own_score = kDistinctOwnScore;
  sent.entries.push_back({.rank = 0x0511,
                          .score = 0x1213141516171819,
                          .survived_steps = 0x1A1B1C1D,
                          .level = 0x0613,
                          .verified = 0,
                          .name = "Premier"});
  sent.entries.push_back({.rank = 0x0715,
                          .score = 0x2223242526272829,
                          .survived_steps = 0x2A2B2C2D,
                          .level = 0x0817,
                          .verified = 1,
                          .name = "Second"});
  sent.count = static_cast<std::uint8_t>(sent.entries.size());
  const std::vector<std::uint8_t> frame = ro::proto::Encode(sent);
  REQUIRE_FALSE(frame.empty());

  const std::optional<SurvivorsBoardPage> page = DecodeSurvivorsBoard(frame);
  REQUIRE(page.has_value());
  CHECK(page->key.ruleset == kDistinctRuleset);
  CHECK(page->key.arena_id == kDistinctArena);
  CHECK(page->key.class_id == kDistinctClass);
  CHECK(page->key.board == SurvivorsBoardKind::kChallenge);
  CHECK(page->own_rank == kDistinctOwnRank);
  CHECK(page->own_score == kDistinctOwnScore);
  REQUIRE(page->lines.size() == sent.entries.size());
  for (std::size_t index = 0; index < sent.entries.size(); ++index) {
    const ro::proto::ZcMoonlightSurvivorsBoardEntry& entry = sent.entries[index];
    const ro::net::SurvivorsBoardLine& line = page->lines[index];
    CHECK(line.rank == entry.rank);
    CHECK(line.score == entry.score);
    CHECK(line.survived_steps == entry.survived_steps);
    CHECK(line.level == entry.level);
    CHECK(line.verified == (entry.verified != 0));
    CHECK(line.name == entry.name);
  }
}
