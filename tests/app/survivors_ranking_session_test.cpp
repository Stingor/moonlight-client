// LA SESSION DE CLASSEMENT contre le serveur simulé : muette contre le serveur
// d'aujourd'hui, le jeton attendu 3 s, la fin de partie envoyée, recalculée et
// rangée, les résultats en attente redits au seul personnage qui les a joués.
// Aucun essai ne finit sur une coupure.

#include "survivors/survivors_ranking_session.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "ro/net/survivors_ranking.h"
#include "ro/net/ui_caps.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_ranked.h"
#include "ro/sim/survivors_score.h"
#include "ro/sim/survivors_state.h"
#include "ro/sim/survivors_week.h"

#include "support/survivors_golden.h"
#include "support/survivors_golden_bot.h"
#include "support/survivors_ranking_server.h"

namespace {

namespace rn = ro::net;
namespace sim = ro::sim;
namespace ts = ro::test_support;
using moonclient::SurvivorsPendingChangeKind;
using moonclient::SurvivorsPendingResult;
using moonclient::SurvivorsRankingNotice;
using moonclient::SurvivorsRankingNoticeKind;
using moonclient::SurvivorsRankingNow;
using moonclient::SurvivorsRankingSession;
using moonclient::SurvivorsRunEnd;
using moonclient::SurvivorsRunRanking;
using moonclient::SurvivorsTicketRequest;

constexpr const char* kServerKey = "127.0.0.1:5121";
constexpr std::uint32_t kAlice = 150001;
constexpr std::uint32_t kBob = 150002;
constexpr std::uint32_t kAccount = 2000001;
constexpr std::int64_t kMsPerSecond = 1000;
// Une image à 60 Hz, arrondie.
constexpr std::int64_t kFrameMs = 16;
// Le mercredi 21 octobre 2026, à midi UTC : loin d'un changement de semaine.
constexpr sim::CivilDate kBenchDay{.year = 2026, .month = 10, .day = 21};
constexpr std::int64_t kBenchHour = 12;

[[nodiscard]] std::int64_t BenchStartUtcS() {
  return sim::DayOfCivilDate(kBenchDay) * sim::kSecondsPerDay + kBenchHour * sim::kSecondsPerHour;
}

[[nodiscard]] SurvivorsTicketRequest GoldenRequest(
    rn::SurvivorsBoardKind board = rn::SurvivorsBoardKind::kRanked) {
  return SurvivorsTicketRequest{.arena_id = ts::kGoldenArenaId,
                                .class_id = ts::kGoldenClassId,
                                .board = board,
                                .arena_key = "pillar_arena",
                                .class_key = "mage",
                                .neutral_start = true};
}

[[nodiscard]] rn::SurvivorsBoardAsk GoldenBoard(
    rn::SurvivorsBoardKind board = rn::SurvivorsBoardKind::kRanked) {
  return rn::SurvivorsBoardAsk{.ruleset = ts::kGoldenRulesetVersion,
                               .arena_id = ts::kGoldenArenaId,
                               .class_id = ts::kGoldenClassId,
                               .board = board};
}

// La partie en or courte, arrêtée par une chute à son dernier pas.
[[nodiscard]] sim::SurvivorsTally ShortTally() {
  sim::SurvivorsTally tally = ts::kGoldenTally;
  tally.outcome = sim::SurvivorsOutcome::kFell;
  return tally;
}

[[nodiscard]] std::int64_t PlayedMs(const sim::SurvivorsTally& tally) {
  return (std::int64_t{tally.survived_steps / sim::kStepsPerSecond} + 1) * kMsPerSecond;
}

[[nodiscard]] std::uint16_t OpcodeOf(std::span<const std::uint8_t> frame) {
  constexpr unsigned kBitsPerByte = 8;
  return static_cast<std::uint16_t>(frame[0] | (frame[1] << kBitsPerByte));
}

// LE BANC : la session et le serveur simulé sur une même horloge, le carnet
// réduit à la liste des changements et des lignes rendus.
struct Bench {
  explicit Bench(ts::SurvivorsRankingServerSetup setup,
                 std::uint16_t ruleset = ts::kGoldenRulesetVersion)
      : server(std::move(setup)), session(ruleset) {
    server.AddCharacter(kAlice, kAccount, "Alice");
    server.AddCharacter(kBob, kAccount, "Bob");
    Advance(0);
  }

  [[nodiscard]] SurvivorsRankingNow Now() const {
    return SurvivorsRankingNow{.steady_ms = elapsed_ms,
                               .utc_s = BenchStartUtcS() + elapsed_ms / kMsPerSecond};
  }

  void Advance(std::int64_t ms) {
    elapsed_ms += ms;
    server.SetNowMs(BenchStartUtcS() * kMsPerSecond + elapsed_ms);
    session.Tick(Now());
    Collect();
  }

  // Le personnage entre en jeu : poignée de main, capacités, entrée en carte.
  void Connect(std::uint32_t character_id, const std::string& name) {
    character = character_id;
    session.NoteCharacter(kServerKey, character_id, name);
    session.NoteHandshakeVerified();
    Deliver(server.AnnounceCaps(character_id, rn::kUiCapSurvivors));
    session.NoteMapEntered();
    Collect();
  }

  // Ce que la session émet, reçu par le serveur ; ses réponses, gardées.
  [[nodiscard]] ts::SurvivorsRankingServer::Frames Ship() {
    ts::SurvivorsRankingServer::Frames replies;
    for (const std::vector<std::uint8_t>& frame : session.TakeOutgoing()) {
      ++sent;
      for (std::vector<std::uint8_t>& reply : server.Receive(character, frame)) {
        replies.push_back(std::move(reply));
      }
    }
    return replies;
  }

  void Deliver(const ts::SurvivorsRankingServer::Frames& frames) {
    for (const std::vector<std::uint8_t>& frame : frames) {
      switch (OpcodeOf(frame)) {
        case rn::kSurvivorsTicketOpcode:
          session.NoteFrame(rn::DecodeSurvivorsTicket(frame).value());
          break;
        case rn::kSurvivorsResultOpcode:
          session.NoteFrame(rn::DecodeSurvivorsResult(frame).value());
          break;
        case rn::kSurvivorsBoardOpcode:
          session.NoteFrame(rn::DecodeSurvivorsBoard(frame).value());
          break;
        case rn::kSurvivorsServiceOpcode:
          session.NoteService(rn::DecodeSurvivorsService(frame).value());
          break;
        default:
          FAIL("trame inattendue");
      }
    }
    Collect();
  }

  void Exchange() { Deliver(Ship()); }

  void Collect() {
    for (auto& change : session.TakePendingChanges()) changes.push_back(std::move(change));
    for (auto& notice : session.TakeNotices()) notices.push_back(std::move(notice));
  }

  [[nodiscard]] std::size_t CountOf(SurvivorsRankingNoticeKind kind) const {
    return static_cast<std::size_t>(
        std::count_if(notices.begin(), notices.end(),
                      [&](const SurvivorsRankingNotice& notice) { return notice.kind == kind; }));
  }

  // Une partie classée demandée et accordée.
  void StartRanked(rn::SurvivorsBoardKind board = rn::SurvivorsBoardKind::kRanked) {
    REQUIRE(session.AskTicket(GoldenRequest(board)) == SurvivorsRunRanking::kAwaiting);
    Exchange();
    REQUIRE(session.run_state() == SurvivorsRunRanking::kRanked);
  }

  // La partie jouée jusqu'à son bilan, puis rapportée ; les réponses gardées.
  [[nodiscard]] ts::SurvivorsRankingServer::Frames FinishRanked(const sim::SurvivorsTally& tally) {
    Advance(PlayedMs(tally));
    session.ReportEnd(SurvivorsRunEnd{.tally = tally});
    Collect();
    return Ship();
  }

  ts::SurvivorsRankingServer server;
  SurvivorsRankingSession session;
  std::int64_t elapsed_ms = 0;
  std::uint32_t character = kAlice;
  std::size_t sent = 0;
  std::vector<moonclient::SurvivorsPendingChange> changes;
  std::vector<SurvivorsRankingNotice> notices;
};

[[nodiscard]] ts::SurvivorsRankingServerSetup Serving() {
  return ts::ServingSetup(ts::kGoldenRulesetVersion, ts::kGoldenArenaId, ts::kGoldenClassId);
}

// Une entrée en attente d'Alice, d'un jeton que le serveur ne connaît pas.
[[nodiscard]] SurvivorsPendingResult AliceEntry(std::uint32_t ticket, std::int64_t issued_at_s) {
  return SurvivorsPendingResult{.server_key = kServerKey,
                                .character_id = kAlice,
                                .character_name = "Alice",
                                .ticket = ticket,
                                .issued_at_utc_s = issued_at_s,
                                .ruleset = ts::kGoldenRulesetVersion,
                                .arena_key = "pillar_arena",
                                .class_key = "mage",
                                .board_key = "ranked",
                                .seed = ts::kGoldenSeed,
                                .components = sim::EndComponentsOf(ShortTally(), 0).value(),
                                .local_score = sim::SurvivorsScore(ShortTally()),
                                .ended_at_utc_s = issued_at_s};
}

}  // namespace

TEST_CASE("survivors_ranking_session : le serveur d'aujourd'hui ne reçoit pas un octet",
          "[app][survivors]") {
  Bench bench(ts::TodaySetup());
  bench.Connect(kAlice, "Alice");
  CHECK_FALSE(bench.session.service_open());

  CHECK(bench.session.AskTicket(GoldenRequest()) == SurvivorsRunRanking::kFree);
  bench.Advance(PlayedMs(ShortTally()));
  bench.session.ReportEnd(SurvivorsRunEnd{.tally = ShortTally()});
  CHECK(bench.session.AskTicket(GoldenRequest(rn::SurvivorsBoardKind::kChallenge)) ==
        SurvivorsRunRanking::kFree);
  CHECK_FALSE(bench.session.AskBoard(GoldenBoard()));
  bench.session.NoteHandshakeVerified();
  bench.session.NoteMapEntered();
  bench.Advance(kFrameMs);
  bench.Advance(kFrameMs);
  CHECK(bench.Ship().empty());

  CHECK(bench.sent == 0);
  CHECK(bench.server.frames_received() == 0);
  CHECK(bench.server.cuts() == 0);
  CHECK(bench.changes.empty());
  CHECK(bench.session.pending().empty());
  CHECK(bench.notices.size() == 1);
  CHECK(bench.CountOf(SurvivorsRankingNoticeKind::kServiceAbsent) == 1);
}

TEST_CASE("survivors_ranking_session : des règles que le serveur ne classe pas restent muettes",
          "[app][survivors]") {
  constexpr std::uint16_t kServedRuleset = 2;
  constexpr std::uint16_t kSessionRuleset = 3;
  Bench bench(ts::ServingSetup(kServedRuleset, ts::kGoldenArenaId, ts::kGoldenClassId),
              kSessionRuleset);
  bench.Connect(kAlice, "Alice");
  bench.Connect(kAlice, "Alice");
  CHECK_FALSE(bench.session.service_open());
  CHECK(bench.session.AskTicket(GoldenRequest()) == SurvivorsRunRanking::kFree);
  CHECK_FALSE(bench.session.AskBoard(GoldenBoard()));
  CHECK(bench.Ship().empty());
  CHECK(bench.server.frames_received() == 0);
  CHECK(bench.CountOf(SurvivorsRankingNoticeKind::kRulesetNotServed) == 1);
  CHECK(bench.CountOf(SurvivorsRankingNoticeKind::kServiceAbsent) == 0);
  CHECK(bench.server.cuts() == 0);
}

TEST_CASE("survivors_ranking_session : le jeton s'attend 3 s", "[app][survivors]") {
  constexpr std::int64_t kJustInTimeMs = 2900;
  constexpr std::int64_t kTooLateMs = 3100;

  SECTION("à 2,9 s, la partie se classe") {
    Bench bench(Serving());
    bench.Connect(kAlice, "Alice");
    REQUIRE(bench.session.AskTicket(GoldenRequest()) == SurvivorsRunRanking::kAwaiting);
    const auto replies = bench.Ship();
    bench.Advance(kJustInTimeMs);
    CHECK(bench.session.run_state() == SurvivorsRunRanking::kAwaiting);
    bench.Deliver(replies);
    CHECK(bench.session.run_state() == SurvivorsRunRanking::kRanked);
    CHECK(bench.session.ranked_seed().has_value());
    CHECK(bench.notices.empty());
    CHECK(bench.server.cuts() == 0);
  }
  SECTION("à 3,1 s, la partie part libre et le jeton s'ignore") {
    Bench bench(Serving());
    bench.Connect(kAlice, "Alice");
    REQUIRE(bench.session.AskTicket(GoldenRequest()) == SurvivorsRunRanking::kAwaiting);
    const auto replies = bench.Ship();
    bench.Advance(kTooLateMs);
    CHECK(bench.session.run_state() == SurvivorsRunRanking::kFree);
    CHECK(bench.CountOf(SurvivorsRankingNoticeKind::kTicketSilent) == 1);
    bench.Deliver(replies);
    CHECK(bench.session.run_state() == SurvivorsRunRanking::kFree);
    CHECK_FALSE(bench.session.ranked_seed().has_value());
    // Une partie libre ne rapporte rien.
    bench.session.ReportEnd(SurvivorsRunEnd{.tally = ShortTally()});
    CHECK(bench.Ship().empty());
    CHECK(bench.server.cuts() == 0);
  }
}

TEST_CASE("survivors_ranking_session : le jeton d'une demande échue ne sert pas la suivante",
          "[app][survivors]") {
  constexpr std::int64_t kReplayAfterMs = 3000;
  Bench bench(Serving());
  bench.Connect(kAlice, "Alice");
  REQUIRE(bench.session.AskTicket(GoldenRequest()) == SurvivorsRunRanking::kAwaiting);
  const auto late = bench.Ship();
  bench.Advance(moonclient::kSurvivorsTicketWaitMs);
  REQUIRE(bench.session.run_state() == SurvivorsRunRanking::kFree);
  // Le joueur rejoue ; le jeton de la première demande arrive pendant la
  // seconde, et la réponse de la seconde décide seule.
  bench.Advance(kReplayAfterMs);
  REQUIRE(bench.session.AskTicket(GoldenRequest()) == SurvivorsRunRanking::kAwaiting);
  const auto second = bench.Ship();
  bench.Deliver(late);
  CHECK(bench.session.run_state() == SurvivorsRunRanking::kAwaiting);
  CHECK_FALSE(bench.session.ranked_seed().has_value());
  bench.Deliver(second);
  CHECK(bench.session.run_state() != SurvivorsRunRanking::kAwaiting);
  CHECK(bench.server.cuts() == 0);
}

TEST_CASE("survivors_ranking_session : un jeton tardif se juge à son arrivée", "[app][survivors]") {
  constexpr std::int64_t kPastDeadlineMs = 50;
  Bench bench(Serving());
  bench.Connect(kAlice, "Alice");
  REQUIRE(bench.session.AskTicket(GoldenRequest()) == SurvivorsRunRanking::kAwaiting);
  const auto replies = bench.Ship();
  // Aucune image n'a passé l'échéance : la trame porte son propre instant.
  bench.session.NoteReceivedAt(moonclient::kSurvivorsTicketWaitMs + kPastDeadlineMs);
  bench.Deliver(replies);
  CHECK(bench.session.run_state() == SurvivorsRunRanking::kFree);
  CHECK(bench.CountOf(SurvivorsRankingNoticeKind::kTicketSilent) == 1);
  CHECK(bench.server.cuts() == 0);
}

TEST_CASE("survivors_ranking_session : chaque refus de jeton fait une partie libre",
          "[app][survivors]") {
  auto refused_with = [](Bench& bench, const SurvivorsTicketRequest& request) {
    REQUIRE(bench.session.AskTicket(request) == SurvivorsRunRanking::kAwaiting);
    bench.Exchange();
    CHECK(bench.session.run_state() == SurvivorsRunRanking::kFree);
    REQUIRE(bench.CountOf(SurvivorsRankingNoticeKind::kTicketRefused) == 1);
    CHECK(bench.server.cuts() == 0);
    return bench.notices.back().ticket_result;
  };

  SECTION("1 : le lieu") {
    Bench bench(Serving());
    bench.server.SetPlace(kAlice, ts::SurvivorsServerPlace::kField);
    bench.Connect(kAlice, "Alice");
    CHECK(refused_with(bench, GoldenRequest()) == rn::SurvivorsTicketResult::kPlaceRefused);
  }
  SECTION("1 : un intérieur, quand le serveur ne le vaut pas une ville") {
    ts::SurvivorsRankingServerSetup setup = Serving();
    setup.indoors_accepted = false;
    Bench bench(setup);
    bench.server.SetPlace(kAlice, ts::SurvivorsServerPlace::kIndoor);
    bench.Connect(kAlice, "Alice");
    CHECK(refused_with(bench, GoldenRequest()) == rn::SurvivorsTicketResult::kPlaceRefused);
  }
  SECTION("2 : le compte non rattaché") {
    constexpr std::uint32_t kUnlinked = 150009;
    Bench bench(Serving());
    bench.server.AddCharacter(kUnlinked, 0, "Nobody");
    bench.Connect(kUnlinked, "Nobody");
    CHECK(refused_with(bench, GoldenRequest()) == rn::SurvivorsTicketResult::kUnlinkedAccount);
  }
  SECTION("3 : une arène que le serveur ne classe pas") {
    constexpr std::uint16_t kOtherArena = 7;
    Bench bench(Serving());
    bench.Connect(kAlice, "Alice");
    SurvivorsTicketRequest request = GoldenRequest();
    request.arena_id = kOtherArena;
    CHECK(refused_with(bench, request) == rn::SurvivorsTicketResult::kUnknownRules);
  }
  SECTION("4 : trop tôt") {
    Bench bench(Serving());
    bench.Connect(kAlice, "Alice");
    bench.StartRanked();
    bench.Advance(kMsPerSecond);
    CHECK(refused_with(bench, GoldenRequest()) == rn::SurvivorsTicketResult::kTooSoon);
  }
}

TEST_CASE("survivors_ranking_session : au-delà de la cadence de son refus, le serveur se tait",
          "[app][survivors]") {
  constexpr std::int64_t kTooLateMs = 3100;
  constexpr std::size_t kSpokenRefusals = 2;
  Bench bench(Serving());
  bench.Connect(kAlice, "Alice");
  bench.StartRanked();
  // Deux refus « trop tôt » dans la même seconde, puis plus rien.
  for (std::size_t refusal = 0; refusal < kSpokenRefusals; ++refusal) {
    REQUIRE(bench.session.AskTicket(GoldenRequest()) == SurvivorsRunRanking::kAwaiting);
    bench.Exchange();
    CHECK(bench.session.run_state() == SurvivorsRunRanking::kFree);
  }
  CHECK(bench.CountOf(SurvivorsRankingNoticeKind::kTicketRefused) == kSpokenRefusals);
  REQUIRE(bench.session.AskTicket(GoldenRequest()) == SurvivorsRunRanking::kAwaiting);
  CHECK(bench.Ship().empty());
  // Le silence fait une partie libre à l'échéance, comme un serveur muet.
  bench.Advance(kTooLateMs);
  CHECK(bench.session.run_state() == SurvivorsRunRanking::kFree);
  CHECK(bench.CountOf(SurvivorsRankingNoticeKind::kTicketSilent) == 1);
  CHECK(bench.server.cuts() == 0);
}

TEST_CASE("survivors_ranking_session : sans le bit annoncé, le serveur tait le service",
          "[app][survivors]") {
  Bench bench(Serving());
  // Le personnage n'a rien annoncé : sa demande est tue, sans couper.
  const std::optional<std::vector<std::uint8_t>> begin = rn::EncodeSurvivorsBegin(GoldenBoard());
  REQUIRE(begin.has_value());
  CHECK(bench.server.Receive(kAlice, *begin).empty());
  CHECK(bench.server.unheard() == 1);
  CHECK(bench.server.cuts() == 0);
}

TEST_CASE("survivors_ranking_session : le défi se classe sur la graine de sa semaine",
          "[app][survivors]") {
  const sim::IsoWeek week =
      sim::IsoWeekOfDay(sim::DayOfCivilDate(sim::UtcInstantOf(BenchStartUtcS()).date));

  SECTION("la graine de la semaine") {
    Bench bench(Serving());
    bench.Connect(kAlice, "Alice");
    bench.StartRanked(rn::SurvivorsBoardKind::kChallenge);
    CHECK(bench.session.ranked_seed() == sim::SeedOfWeek(week.year, week.week));
    CHECK(bench.server.cuts() == 0);
  }
  SECTION("la graine d'une autre semaine") {
    Bench bench(Serving());
    bench.Connect(kAlice, "Alice");
    REQUIRE(bench.session.AskTicket(GoldenRequest(rn::SurvivorsBoardKind::kChallenge)) ==
            SurvivorsRunRanking::kAwaiting);
    static_cast<void>(bench.Ship());
    const auto other_week = static_cast<std::uint8_t>(week.week + 1);
    bench.session.NoteFrame(rn::SurvivorsTicket{.result = rn::SurvivorsTicketResult::kGranted,
                                                .ticket = 1,
                                                .seed = sim::SeedOfWeek(week.year, other_week),
                                                .issued_at_utc_s = BenchStartUtcS()});
    bench.Collect();
    CHECK(bench.session.run_state() == SurvivorsRunRanking::kFree);
    CHECK(bench.CountOf(SurvivorsRankingNoticeKind::kChallengeSeedMismatch) == 1);
    CHECK(bench.server.cuts() == 0);
  }
}

TEST_CASE("survivors_ranking_session : le score du serveur est celui du bilan",
          "[app][survivors]") {
  const sim::SurvivorsTally tally = GENERATE(ShortTally(), ts::kGoldenBotTally);
  Bench bench(Serving());
  bench.Connect(kAlice, "Alice");
  bench.StartRanked();
  bench.Deliver(bench.FinishRanked(tally));

  REQUIRE(bench.session.run_result().has_value());
  const rn::SurvivorsResult& result = *bench.session.run_result();
  CHECK(result.verdict == rn::SurvivorsVerdict::kPersonalBest);
  CHECK(result.score == sim::SurvivorsScore(tally));
  CHECK(result.rank == 1);
  CHECK(result.best == result.score);
  CHECK(bench.session.pending().empty());
  REQUIRE(bench.changes.size() == 2);
  CHECK(bench.changes[0].kind == SurvivorsPendingChangeKind::kAdd);
  CHECK(bench.changes[0].entry.send_count == 1);
  CHECK(bench.changes[1].kind == SurvivorsPendingChangeKind::kRemove);
  CHECK(bench.CountOf(SurvivorsRankingNoticeKind::kVerdict) == 1);
  CHECK(bench.CountOf(SurvivorsRankingNoticeKind::kScoreMismatch) == 0);
  CHECK(bench.server.cuts() == 0);
}

TEST_CASE("survivors_ranking_session : une partie invraisemblable est refusée",
          "[app][survivors]") {
  constexpr std::uint16_t kUnreachableLevel = 200;
  sim::SurvivorsTally tally = ShortTally();
  tally.level = kUnreachableLevel;
  Bench bench(Serving());
  bench.Connect(kAlice, "Alice");
  bench.StartRanked();
  bench.Deliver(bench.FinishRanked(tally));
  REQUIRE(bench.session.run_result().has_value());
  CHECK(bench.session.run_result()->verdict == rn::SurvivorsVerdict::kImplausible);
  CHECK(bench.CountOf(SurvivorsRankingNoticeKind::kVerdict) == 1);
  CHECK(bench.session.pending().empty());
  CHECK(bench.server.cuts() == 0);
}

TEST_CASE("survivors_ranking_session : un résultat perdu se redit à la reconnexion",
          "[app][survivors]") {
  SECTION("A2 : le même verdict") {
    Bench bench(Serving());
    bench.Connect(kAlice, "Alice");
    bench.StartRanked();
    const auto lost = bench.FinishRanked(ShortTally());
    REQUIRE(lost.size() == 1);
    CHECK(bench.session.pending().size() == 1);
    CHECK_FALSE(bench.session.run_result().has_value());

    bench.Connect(kAlice, "Alice");
    bench.Advance(kFrameMs);
    bench.Exchange();
    REQUIRE(bench.session.run_result().has_value());
    CHECK(*bench.session.run_result() == rn::DecodeSurvivorsResult(lost.front()).value());
    CHECK(bench.session.pending().empty());
    CHECK(bench.sent == 3);
    CHECK(bench.CountOf(SurvivorsRankingNoticeKind::kResultLost) == 0);
    CHECK(bench.server.cuts() == 0);
  }
  SECTION("sans A2 : déjà comptée ou perdue, sans parler de refus") {
    ts::SurvivorsRankingServerSetup setup = Serving();
    setup.idempotent_end = false;
    Bench bench(setup);
    bench.Connect(kAlice, "Alice");
    bench.StartRanked();
    static_cast<void>(bench.FinishRanked(ShortTally()));
    bench.Connect(kAlice, "Alice");
    bench.Advance(kFrameMs);
    bench.Exchange();
    REQUIRE(bench.session.run_result().has_value());
    CHECK(bench.session.run_result()->verdict == rn::SurvivorsVerdict::kUnknownTicket);
    CHECK(bench.CountOf(SurvivorsRankingNoticeKind::kResultLost) == 1);
    CHECK(bench.CountOf(SurvivorsRankingNoticeKind::kVerdict) == 0);
    CHECK(bench.session.pending().empty());
    CHECK(bench.server.cuts() == 0);
  }
}

TEST_CASE("survivors_ranking_session : le redémarrage du serveur perd le jeton",
          "[app][survivors]") {
  Bench bench(Serving());
  bench.Connect(kAlice, "Alice");
  bench.StartRanked();
  bench.server.Restart();
  // Le serveur n'entend le service qu'après l'annonce que redit la reconnexion.
  bench.Deliver(bench.server.AnnounceCaps(kAlice, rn::kUiCapSurvivors));
  bench.Deliver(bench.FinishRanked(ShortTally()));
  CHECK(bench.CountOf(SurvivorsRankingNoticeKind::kResultLost) == 1);
  CHECK(bench.session.pending().empty());
  REQUIRE_FALSE(bench.changes.empty());
  CHECK(bench.changes.back().kind == SurvivorsPendingChangeKind::kRemove);
  CHECK(bench.server.cuts() == 0);
}

TEST_CASE("survivors_ranking_session : un résultat ne part qu'avec son personnage",
          "[app][survivors]") {
  Bench bench(Serving());
  bench.Connect(kAlice, "Alice");
  bench.StartRanked();
  static_cast<void>(bench.FinishRanked(ShortTally()));
  const std::size_t sent_by_alice = bench.sent;

  bench.Connect(kBob, "Bob");
  bench.Advance(kFrameMs);
  bench.Advance(kFrameMs);
  CHECK(bench.Ship().empty());
  CHECK(bench.sent == sent_by_alice);
  CHECK(bench.session.pending().size() == 1);

  bench.Connect(kAlice, "Alice");
  bench.Advance(kFrameMs);
  bench.Exchange();
  CHECK(bench.sent == sent_by_alice + 1);
  CHECK(bench.session.pending().empty());
  CHECK(bench.CountOf(SurvivorsRankingNoticeKind::kVerdict) == 1);
  CHECK(bench.server.cuts() == 0);
}

TEST_CASE("survivors_ranking_session : les entrées en attente se redisent une par image",
          "[app][survivors]") {
  constexpr std::uint32_t kFirstTicket = 900;
  constexpr std::uint32_t kSecondTicket = 901;
  constexpr std::uint32_t kStaleTicket = 902;
  constexpr std::int64_t kHourS = 3600;
  Bench bench(Serving());
  bench.session.LoadPending({AliceEntry(kFirstTicket, BenchStartUtcS() - kHourS),
                             AliceEntry(kStaleTicket, BenchStartUtcS() - 2 * sim::kSecondsPerDay),
                             AliceEntry(kSecondTicket, BenchStartUtcS() - kHourS)});
  bench.Connect(kAlice, "Alice");
  bench.Advance(kFrameMs);
  CHECK(bench.session.TakeOutgoing().size() == 1);
  bench.Advance(kFrameMs);
  // L'entrée morte s'oublie sans partir ; la suivante part.
  CHECK(bench.CountOf(SurvivorsRankingNoticeKind::kResultExpired) == 1);
  CHECK(bench.session.TakeOutgoing().size() == 1);
  bench.Advance(kFrameMs);
  CHECK(bench.session.TakeOutgoing().empty());
  CHECK(bench.server.cuts() == 0);
}

TEST_CASE("survivors_ranking_session : abandon, liaison perdue et départ chargé n'envoient rien",
          "[app][survivors]") {
  Bench bench(Serving());
  bench.Connect(kAlice, "Alice");

  bench.StartRanked();
  sim::SurvivorsTally abandoned = ShortTally();
  abandoned.outcome = sim::SurvivorsOutcome::kAbandoned;
  CHECK(bench.FinishRanked(abandoned).empty());

  bench.StartRanked();
  bench.Advance(PlayedMs(ShortTally()));
  bench.session.ReportEnd(SurvivorsRunEnd{.tally = ShortTally(), .frozen_by_link_loss = true});
  CHECK(bench.Ship().empty());

  const std::size_t sent_before = bench.sent;
  SurvivorsTicketRequest loaded = GoldenRequest();
  loaded.neutral_start = false;
  CHECK(bench.session.AskTicket(loaded) == SurvivorsRunRanking::kFree);
  CHECK(bench.Ship().empty());
  CHECK(bench.sent == sent_before);
  CHECK(bench.changes.empty());
  CHECK(bench.session.pending().empty());
  CHECK(bench.server.cuts() == 0);
}

TEST_CASE("survivors_ranking_session : le tableau se demande toutes les 2 s au plus",
          "[app][survivors]") {
  Bench bench(Serving());
  bench.Connect(kAlice, "Alice");
  CHECK(bench.session.AskBoard(GoldenBoard()));
  bench.Exchange();
  bench.Advance(kMsPerSecond);
  CHECK_FALSE(bench.session.AskBoard(GoldenBoard()));
  CHECK(bench.Ship().empty());
  CHECK(bench.sent == 1);

  const moonclient::SurvivorsHeldPage* held =
      bench.session.page(rn::SurvivorsBoardKind::kRanked, ts::kGoldenClassId);
  REQUIRE(held != nullptr);
  CHECK(held->received_at_ms == 0);
  CHECK(held->page.lines.empty());
  CHECK_FALSE(moonclient::IsRankedOnPage(held->page));
  CHECK(bench.session.page(rn::SurvivorsBoardKind::kChallenge, ts::kGoldenClassId) == nullptr);

  // Une partie classée, puis la page qui la porte.
  bench.StartRanked();
  bench.Deliver(bench.FinishRanked(ShortTally()));
  CHECK(bench.session.AskBoard(GoldenBoard()));
  bench.Exchange();
  held = bench.session.page(rn::SurvivorsBoardKind::kRanked, ts::kGoldenClassId);
  REQUIRE(held != nullptr);
  CHECK(held->page.own_rank == 1);
  CHECK(moonclient::IsRankedOnPage(held->page));
  REQUIRE(held->page.lines.size() == 1);
  CHECK(held->page.lines.front().name == "Alice");
  CHECK(held->page.lines.front().score == sim::SurvivorsScore(ShortTally()));
  CHECK(bench.server.throttled() == 0);
  CHECK(bench.server.cuts() == 0);
}

TEST_CASE("survivors_ranking_session : seule la page de la dernière demande de son genre se garde",
          "[app][survivors]") {
  constexpr std::uint16_t kOtherClass = ts::kGoldenClassId + 1;
  moonclient::SurvivorsRankingSession session(ts::kGoldenRulesetVersion);
  session.NoteService(rn::SurvivorsService{.ruleset_low = ts::kGoldenRulesetVersion,
                                           .ruleset_high = ts::kGoldenRulesetVersion});
  const rn::SurvivorsBoardAsk golden = GoldenBoard();
  rn::SurvivorsBoardAsk other = golden;
  other.class_id = kOtherClass;
  const auto note = [&session](const rn::SurvivorsBoardKey& key, std::uint16_t own_rank) {
    session.NoteFrame(rn::SurvivorsBoardPage{.key = key, .own_rank = own_rank});
  };

  // Une page que rien n'a demandée ne se garde pas.
  note(golden, 1);
  CHECK(session.page(rn::SurvivorsBoardKind::kRanked, ts::kGoldenClassId) == nullptr);

  // La demande faite, sa page se garde, et ne se montre que sous sa classe.
  REQUIRE(session.AskBoard(golden));
  note(golden, 1);
  CHECK(session.page(rn::SurvivorsBoardKind::kRanked, ts::kGoldenClassId) != nullptr);
  CHECK(session.page(rn::SurvivorsBoardKind::kRanked, kOtherClass) == nullptr);
  CHECK(session.page(rn::SurvivorsBoardKind::kChallenge, ts::kGoldenClassId) == nullptr);

  // UNE RÉPONSE EN RETARD D'UNE CLASSE QUITTÉE ne remplace rien.
  session.Tick(moonclient::SurvivorsRankingNow{.steady_ms = moonclient::kSurvivorsBoardCadenceMs});
  REQUIRE(session.AskBoard(other));
  note(golden, 2);
  const moonclient::SurvivorsHeldPage* held =
      session.page(rn::SurvivorsBoardKind::kRanked, ts::kGoldenClassId);
  REQUIRE(held != nullptr);
  CHECK(held->page.own_rank == 1);

  // Une autre arène ou une autre version non plus.
  rn::SurvivorsBoardKey other_arena = other;
  other_arena.arena_id = other.arena_id + 1;
  note(other_arena, 3);
  CHECK(session.page(rn::SurvivorsBoardKind::kRanked, kOtherClass) == nullptr);

  // La page attendue prend la place de l'ancienne, et l'autre classe ne se montre plus.
  note(other, 4);
  held = session.page(rn::SurvivorsBoardKind::kRanked, kOtherClass);
  REQUIRE(held != nullptr);
  CHECK(held->page.own_rank == 4);
  CHECK(session.page(rn::SurvivorsBoardKind::kRanked, ts::kGoldenClassId) == nullptr);
}

TEST_CASE("survivors_ranking_session : une partie de l'ancienne version survit à la montée",
          "[app][survivors]") {
  constexpr std::uint16_t kCurrent = ts::kGoldenRulesetVersion;
  constexpr std::uint16_t kOlder = kCurrent - 1;
  // Une partie jouée sous l'ancienne version, dont le verdict s'est perdu ; puis
  // le client monté, qui relit son fichier en attente.
  const auto played_then_climbed = [](Bench& bench) {
    bench.Connect(kAlice, "Alice");
    bench.StartRanked();
    static_cast<void>(bench.FinishRanked(ShortTally()));
    REQUIRE(bench.session.pending().size() == 1);
    CHECK(bench.session.pending().front().ruleset == kOlder);
    const std::vector<SurvivorsPendingResult> kept(bench.session.pending().begin(),
                                                   bench.session.pending().end());
    bench.session = SurvivorsRankingSession(kCurrent);
    bench.session.LoadPending(kept);
    // Le client tourne avant de se connecter : l'annonce du serveur se date sur son horloge.
    bench.Advance(0);
    bench.Connect(kAlice, "Alice");
    bench.Advance(kFrameMs);
    bench.Exchange();
  };

  SECTION("le serveur classe les deux versions : le jeton garde la sienne, le verdict arrive") {
    ts::SurvivorsRankingServerSetup setup =
        ts::ServingSetup(kCurrent, ts::kGoldenArenaId, ts::kGoldenClassId);
    setup.ruleset_low = kOlder;
    Bench bench(setup, kOlder);
    played_then_climbed(bench);
    CHECK(bench.session.service_open());
    CHECK(bench.session.pending().empty());
    CHECK(bench.CountOf(SurvivorsRankingNoticeKind::kVerdict) == 1);
    CHECK(bench.CountOf(SurvivorsRankingNoticeKind::kResultLost) == 0);
    CHECK(bench.CountOf(SurvivorsRankingNoticeKind::kRulesetNotServed) == 0);
    CHECK(bench.server.cuts() == 0);
  }
  SECTION(
      "le serveur n'est pas monté : le service se ferme et la partie attend, sans rien envoyer") {
    Bench bench(ts::ServingSetup(kOlder, ts::kGoldenArenaId, ts::kGoldenClassId), kOlder);
    played_then_climbed(bench);
    CHECK_FALSE(bench.session.service_open());
    CHECK(bench.session.pending().size() == 1);
    CHECK(bench.CountOf(SurvivorsRankingNoticeKind::kRulesetNotServed) == 1);
    CHECK(bench.CountOf(SurvivorsRankingNoticeKind::kVerdict) == 0);
    CHECK(bench.session.AskTicket(GoldenRequest()) == SurvivorsRunRanking::kFree);
    CHECK(bench.server.cuts() == 0);
  }
}
