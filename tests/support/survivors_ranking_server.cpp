#include "support/survivors_ranking_server.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "ro/core/byte_reader.h"
#include "ro/net/survivors_ranking.h"
#include "ro/net/ui_caps.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_score.h"
#include "ro/sim/survivors_state.h"
#include "ro/sim/survivors_week.h"

#include "support/wire_bytes.h"

namespace ro::test_support {
namespace {

namespace rn = ro::net;

constexpr std::int64_t kMsPerSecond = 1000;
// Les cadences du serveur, par personnage : un jeton par 10 s ; son refus
// « trop tôt » deux fois par seconde ; un tableau par 2 s ; l'annonce du
// service deux fois par 2 s.
constexpr std::int64_t kTicketCadenceMs = 10000;
constexpr std::int64_t kRefusalCadenceMs = 1000;
constexpr std::int64_t kRefusalBurst = 2;
constexpr std::int64_t kBoardCadenceMs = 2000;
constexpr std::int64_t kServiceCadenceMs = 2000;
constexpr std::int64_t kServiceBurst = 2;
// La marge que la borne de durée laisse entre l'heure du jeton et la fin.
constexpr std::int64_t kDurationSlackS = 5;
constexpr std::size_t kBoardLinesMost = 20;
constexpr std::uint8_t kBoardRanked = 0;
constexpr std::uint8_t kBoardChallenge = 1;
// Le compte d'une semaine : année ISO × 100 + semaine.
constexpr std::uint32_t kWeeksPerYearSlot = 100;
// Le mélange qui tire la graine d'une partie classée du numéro de son jeton.
constexpr std::uint64_t kSeedGolden = 0x9E3779B97F4A7C15ULL;
constexpr std::uint64_t kSeedMixA = 0xBF58476D1CE4E5B9ULL;
constexpr std::uint64_t kSeedMixB = 0x94D049BB133111EBULL;
constexpr unsigned kSeedShiftA = 30;
constexpr unsigned kSeedShiftB = 27;
constexpr unsigned kSeedShiftC = 31;

[[nodiscard]] std::uint64_t SeedOfTicket(std::uint32_t ticket) noexcept {
  std::uint64_t mixed = std::uint64_t{ticket} * kSeedGolden;
  mixed = (mixed ^ (mixed >> kSeedShiftA)) * kSeedMixA;
  mixed = (mixed ^ (mixed >> kSeedShiftB)) * kSeedMixB;
  mixed ^= mixed >> kSeedShiftC;
  return mixed == 0 ? 1 : mixed;
}

[[nodiscard]] bool Contains(const std::vector<std::uint16_t>& ids, std::uint16_t id) {
  return std::find(ids.begin(), ids.end(), id) != ids.end();
}

// L'en-tête d'une trame : l'opcode et la taille de la trame entière.
[[nodiscard]] std::vector<std::uint8_t> Header(std::uint16_t opcode, std::size_t size) {
  std::vector<std::uint8_t> frame;
  frame.reserve(size);
  PutU16(frame, opcode);
  PutU16(frame, static_cast<std::uint16_t>(size));
  return frame;
}

[[nodiscard]] std::vector<std::uint8_t> TicketFrame(rn::SurvivorsTicketResult result,
                                                    std::uint32_t ticket, std::uint64_t seed,
                                                    std::int64_t issued_at_s) {
  std::vector<std::uint8_t> frame = Header(rn::kSurvivorsTicketOpcode, rn::kSurvivorsTicketSize);
  frame.push_back(static_cast<std::uint8_t>(result));
  PutU32(frame, ticket);
  PutU64(frame, seed);
  PutU64(frame, static_cast<std::uint64_t>(issued_at_s));
  return frame;
}

[[nodiscard]] std::vector<std::uint8_t> RefusalFrame(rn::SurvivorsTicketResult result) {
  return TicketFrame(result, 0, 0, 0);
}

[[nodiscard]] std::vector<std::uint8_t> ResultFrame(std::uint32_t ticket,
                                                    rn::SurvivorsVerdict verdict,
                                                    std::uint64_t score, std::uint16_t rank,
                                                    std::uint64_t best) {
  std::vector<std::uint8_t> frame = Header(rn::kSurvivorsResultOpcode, rn::kSurvivorsResultSize);
  PutU32(frame, ticket);
  frame.push_back(static_cast<std::uint8_t>(verdict));
  PutU64(frame, score);
  PutU16(frame, rank);
  PutU64(frame, best);
  return frame;
}

// Le corps d'une trame de taille fixe, ou rien quand l'opcode ou la taille ne
// vont pas : le serveur coupe alors la session.
[[nodiscard]] std::optional<ro::core::ByteReader> FixedBody(std::span<const std::uint8_t> frame,
                                                            std::size_t size) {
  if (frame.size() != size) return std::nullopt;
  ro::core::ByteReader reader(frame);
  reader.Skip(sizeof(std::uint16_t));
  if (reader.U16() != size) return std::nullopt;
  return reader;
}

}  // namespace

SurvivorsRankingServerSetup ServingSetup(std::uint16_t ruleset, std::uint16_t arena_id,
                                         std::uint16_t class_id) {
  return ServingSetup(ruleset, arena_id, std::vector<std::uint16_t>{class_id});
}

SurvivorsRankingServerSetup ServingSetup(std::uint16_t ruleset, std::uint16_t arena_id,
                                         std::vector<std::uint16_t> class_ids) {
  SurvivorsRankingServerSetup setup;
  setup.ruleset_low = ruleset;
  setup.ruleset_high = ruleset;
  setup.arena_ids = {arena_id};
  setup.class_ids = std::move(class_ids);
  return setup;
}

SurvivorsRankingServerSetup TodaySetup() {
  SurvivorsRankingServerSetup setup;
  setup.knows_service = false;
  return setup;
}

SurvivorsRankingServer::SurvivorsRankingServer(SurvivorsRankingServerSetup setup)
    : setup_(std::move(setup)) {}

void SurvivorsRankingServer::AddCharacter(std::uint32_t character_id, std::uint32_t user_id,
                                          std::string name) {
  characters_.push_back(Character{
      .character_id = character_id,
      .user_id = user_id,
      .name = std::move(name),
      .ticket_cadence = Cadence{.interval_ms = kTicketCadenceMs},
      .refusal_cadence = Cadence{.interval_ms = kRefusalCadenceMs, .burst = kRefusalBurst},
      .board_cadence = Cadence{.interval_ms = kBoardCadenceMs},
      .service_cadence = Cadence{.interval_ms = kServiceCadenceMs, .burst = kServiceBurst}});
}

bool SurvivorsRankingServer::Cadence::Pass(std::int64_t now_ms) {
  const std::int64_t start_ms = (!due_ms || *due_ms < now_ms) ? now_ms : *due_ms;
  if (start_ms - now_ms > interval_ms * (burst - 1)) return false;
  due_ms = start_ms + interval_ms;
  return true;
}

void SurvivorsRankingServer::SetPlace(std::uint32_t character_id, SurvivorsServerPlace place) {
  if (Character* who = Find(character_id)) who->place = place;
}

SurvivorsRankingServer::Character* SurvivorsRankingServer::Find(std::uint32_t character_id) {
  for (Character& who : characters_) {
    if (who.character_id == character_id) return &who;
  }
  return nullptr;
}

bool SurvivorsRankingServer::Ahead(const BestRow& left, const BestRow& right) noexcept {
  if (left.score != right.score) return left.score > right.score;
  if (left.survived_steps != right.survived_steps) {
    return left.survived_steps > right.survived_steps;
  }
  return left.arrival < right.arrival;
}

std::int64_t SurvivorsRankingServer::now_s() const noexcept { return now_ms_ / kMsPerSecond; }

std::uint32_t SurvivorsRankingServer::WeekOf(std::uint8_t board, std::int64_t utc_s) const {
  if (board != kBoardChallenge || !setup_.weekly_challenge) return 0;
  const sim::IsoWeek week = sim::IsoWeekOfDay(sim::DayOfCivilDate(sim::UtcInstantOf(utc_s).date));
  return static_cast<std::uint32_t>(week.year) * kWeeksPerYearSlot + week.week;
}

SurvivorsRankingServer::Frames SurvivorsRankingServer::AnnounceCaps(std::uint32_t character_id,
                                                                    std::uint32_t caps) {
  Character* who = Find(character_id);
  if (who == nullptr || !setup_.knows_service) return {};
  who->announced = (caps & rn::kUiCapSurvivors) != 0;
  if (!who->announced || !who->service_cadence.Pass(now_ms_)) return {};
  std::vector<std::uint8_t> frame = Header(rn::kSurvivorsServiceOpcode, rn::kSurvivorsServiceSize);
  PutU16(frame, setup_.ruleset_low);
  PutU16(frame, setup_.ruleset_high);
  PutU64(frame, static_cast<std::uint64_t>(now_s()));
  return {std::move(frame)};
}

SurvivorsRankingServer::Frames SurvivorsRankingServer::Receive(
    std::uint32_t character_id, std::span<const std::uint8_t> frame) {
  ++frames_received_;
  Character* who = Find(character_id);
  if (frame.size() < sizeof(std::uint16_t) || who == nullptr) {
    ++cuts_;
    return {};
  }
  ro::core::ByteReader reader(frame);
  const std::uint16_t opcode = reader.U16();
  const bool in_range =
      opcode >= rn::kSurvivorsBeginOpcode && opcode <= rn::kSurvivorsServiceOpcode;
  // Le serveur d'aujourd'hui ne connaît aucun opcode du service.
  if (!setup_.knows_service && in_range) {
    ++cuts_;
    return {};
  }
  // Une trame connue d'un personnage qui n'a pas annoncé le bit : tue, sans
  // couper.
  const bool heard_request = opcode == rn::kSurvivorsBeginOpcode ||
                             opcode == rn::kSurvivorsEndOpcode ||
                             opcode == rn::kSurvivorsBoardAskOpcode;
  if (heard_request && !who->announced) {
    ++unheard_;
    return {};
  }
  switch (opcode) {
    case rn::kSurvivorsBeginOpcode:
      return Begin(*who, frame);
    case rn::kSurvivorsEndOpcode:
      return End(*who, frame);
    case rn::kSurvivorsBoardAskOpcode:
      return Board(*who, frame);
    default:
      // Un ZC renvoyé, le journal réservé ou un opcode étranger : refusés.
      ++cuts_;
      return {};
  }
}

SurvivorsRankingServer::Frames SurvivorsRankingServer::Begin(Character& who,
                                                             std::span<const std::uint8_t> frame) {
  std::optional<ro::core::ByteReader> body = FixedBody(frame, rn::kSurvivorsBeginSize);
  if (!body) {
    ++cuts_;
    return {};
  }
  BoardKey key;
  key.ruleset = body->U16();
  key.arena_id = body->U16();
  key.class_id = body->U16();
  key.board = body->U8();

  if (!who.ticket_cadence.Pass(now_ms_)) {
    if (!who.refusal_cadence.Pass(now_ms_)) return {};
    return {RefusalFrame(rn::SurvivorsTicketResult::kTooSoon)};
  }
  const bool known_rules =
      setup_.ruleset_low <= key.ruleset && key.ruleset <= setup_.ruleset_high &&
      Contains(setup_.arena_ids, key.arena_id) && Contains(setup_.class_ids, key.class_id) &&
      (key.board == kBoardRanked || key.board == kBoardChallenge);
  if (!known_rules) return {RefusalFrame(rn::SurvivorsTicketResult::kUnknownRules)};
  if (who.user_id == 0) return {RefusalFrame(rn::SurvivorsTicketResult::kUnlinkedAccount)};
  const bool place_accepted =
      who.place == SurvivorsServerPlace::kTown ||
      (who.place == SurvivorsServerPlace::kIndoor && setup_.indoors_accepted);
  if (!place_accepted) return {RefusalFrame(rn::SurvivorsTicketResult::kPlaceRefused)};

  const std::uint32_t ticket = next_ticket_++;
  const std::int64_t issued_at_s = now_s();
  std::uint64_t seed = SeedOfTicket(ticket);
  if (key.board == kBoardChallenge) {
    const sim::IsoWeek week =
        sim::IsoWeekOfDay(sim::DayOfCivilDate(sim::UtcInstantOf(issued_at_s).date));
    seed = sim::SeedOfWeek(week.year, week.week);
  }
  key.week = WeekOf(key.board, issued_at_s);
  who.ticket = ticket;
  who.issued_at_s = issued_at_s;
  who.key = key;
  return {TicketFrame(rn::SurvivorsTicketResult::kGranted, ticket, seed, issued_at_s)};
}

SurvivorsRankingServer::Frames SurvivorsRankingServer::End(Character& who,
                                                           std::span<const std::uint8_t> frame) {
  std::optional<ro::core::ByteReader> body = FixedBody(frame, rn::kSurvivorsEndSize);
  if (!body) {
    ++cuts_;
    return {};
  }
  const std::uint32_t ticket = body->U32();
  sim::SurvivorsTally tally;
  tally.survived_steps = body->U32();
  tally.kill_points = body->U32();
  tally.kills = body->U32();
  tally.boss_kills = body->U16();
  tally.level = body->U16();
  const std::uint8_t outcome = body->U8();
  static_cast<void>(body->U32());  // la taille du journal, que rien ne lit encore

  if (setup_.idempotent_end && who.consumed_ticket == ticket) return {who.consumed_reply};
  const bool expired =
      setup_.ticket_lifetime_s > 0 && now_s() - who.issued_at_s > setup_.ticket_lifetime_s;
  if (who.ticket != ticket || expired) {
    return {ResultFrame(ticket, rn::SurvivorsVerdict::kUnknownTicket, 0, 0, 0)};
  }
  const BoardKey key = who.key;
  const std::int64_t issued_at_s = who.issued_at_s;
  who.ticket.reset();

  const SurvivorsServerBounds& bounds = setup_.bounds;
  const std::uint32_t survived_seconds = tally.survived_steps / sim::kStepsPerSecond;
  const bool plausible =
      outcome >= rn::kSurvivorsWireOutcomeFell && outcome <= rn::kSurvivorsWireOutcomeVictory &&
      tally.survived_steps <= bounds.most_survived_steps &&
      std::int64_t{survived_seconds} <= now_s() - issued_at_s + kDurationSlackS &&
      std::uint64_t{tally.kill_points} <=
          std::uint64_t{bounds.most_kill_points_per_second} * (survived_seconds + 1U) &&
      tally.boss_kills <= bounds.most_boss_kills && tally.level <= bounds.most_level;
  tally.outcome = static_cast<sim::SurvivorsOutcome>(outcome);
  const std::uint64_t score = sim::SurvivorsScore(tally);

  auto best_of = [&](std::uint32_t user_id) -> BestRow* {
    for (BestRow& row : best_) {
      if (row.user_id == user_id && row.key == key) return &row;
    }
    return nullptr;
  };
  rn::SurvivorsVerdict verdict = rn::SurvivorsVerdict::kImplausible;
  if (plausible) {
    BestRow* best = best_of(who.user_id);
    verdict = rn::SurvivorsVerdict::kAccepted;
    if (best == nullptr || score > best->score) {
      verdict = rn::SurvivorsVerdict::kPersonalBest;
      if (best == nullptr) best = &best_.emplace_back();
      *best = BestRow{.user_id = who.user_id,
                      .name = who.name,
                      .key = key,
                      .score = score,
                      .survived_steps = tally.survived_steps,
                      .level = tally.level,
                      .arrival = next_arrival_++};
    }
  }
  // Le rang du meilleur score du compte : un de plus que les lignes devant.
  std::uint16_t rank = 0;
  std::uint64_t best_score = 0;
  if (const BestRow* best = best_of(who.user_id)) {
    best_score = best->score;
    rank = 1;
    for (const BestRow& row : best_) {
      if (!(row.key == key) || &row == best) continue;
      if (Ahead(row, *best)) ++rank;
    }
  }
  std::vector<std::uint8_t> reply = ResultFrame(ticket, verdict, score, rank, best_score);
  who.consumed_ticket = ticket;
  who.consumed_reply = reply;
  return {std::move(reply)};
}

SurvivorsRankingServer::Frames SurvivorsRankingServer::Board(Character& who,
                                                             std::span<const std::uint8_t> frame) {
  std::optional<ro::core::ByteReader> body = FixedBody(frame, rn::kSurvivorsBoardAskSize);
  if (!body) {
    ++cuts_;
    return {};
  }
  if (!who.board_cadence.Pass(now_ms_)) {
    ++throttled_;
    return {};
  }
  BoardKey key;
  key.ruleset = body->U16();
  key.arena_id = body->U16();
  key.class_id = body->U16();
  key.board = body->U8();
  key.week = WeekOf(key.board, now_s());

  std::vector<const BestRow*> rows;
  for (const BestRow& row : best_) {
    if (row.key == key) rows.push_back(&row);
  }
  std::sort(rows.begin(), rows.end(),
            [](const BestRow* left, const BestRow* right) { return Ahead(*left, *right); });
  std::uint16_t own_rank = 0;
  std::uint64_t own_score = 0;
  for (std::size_t index = 0; index < rows.size(); ++index) {
    if (rows[index]->user_id == who.user_id) {
      own_rank = static_cast<std::uint16_t>(index + 1);
      own_score = rows[index]->score;
    }
  }
  const std::size_t count = std::min(rows.size(), kBoardLinesMost);
  std::vector<std::uint8_t> reply =
      Header(rn::kSurvivorsBoardOpcode,
             rn::kSurvivorsBoardHeaderSize + count * rn::kSurvivorsBoardLineSize);
  PutU16(reply, key.ruleset);
  PutU16(reply, key.arena_id);
  PutU16(reply, key.class_id);
  reply.push_back(key.board);
  PutU16(reply, own_rank);
  PutU64(reply, own_score);
  reply.push_back(static_cast<std::uint8_t>(count));
  for (std::size_t index = 0; index < count; ++index) {
    const BestRow& row = *rows[index];
    PutU16(reply, static_cast<std::uint16_t>(index + 1));
    PutU64(reply, row.score);
    PutU32(reply, row.survived_steps);
    PutU16(reply, row.level);
    reply.push_back(0);  // jamais vérifiée : aucun rejeu
    std::string name = row.name.substr(0, rn::kSurvivorsBoardNameSize);
    name.resize(rn::kSurvivorsBoardNameSize, '\0');
    reply.insert(reply.end(), name.begin(), name.end());
  }
  return {std::move(reply)};
}

void SurvivorsRankingServer::Restart() {
  for (Character& who : characters_) {
    who.ticket.reset();
    who.consumed_ticket.reset();
    who.consumed_reply.clear();
    who.ticket_cadence.due_ms.reset();
    who.refusal_cadence.due_ms.reset();
    who.board_cadence.due_ms.reset();
    who.service_cadence.due_ms.reset();
    who.announced = false;
  }
}

}  // namespace ro::test_support
