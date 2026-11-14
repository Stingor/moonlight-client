// LE CLASSEMENT SURVIVORS À L'ÉCRAN : service fermé, aucune vue ne le porte ;
// service ouvert, du jeton à l'écran de fin contre le serveur simulé (rang,
// meilleur, record local), le tableau demandé une fois dans sa cadence, le
// compteur des envois en attente, et les lignes de conversation (Q3, Q5, Q6).

#include "game/survivors_ranking_desk.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/net/survivors_ranking.h"
#include "ro/net/world_update.h"
#include "ro/sim/survivors_score.h"
#include "ro/sim/survivors_week.h"
#include "ro/ui/survivors_ranking_window.h"
#include "ro/ui/survivors_summary_window.h"

#include "game/survivors_book_mode.h"
#include "game/survivors_meta_desk.h"
#include "game/survivors_ranking_mode.h"
#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/survivors_fixture.h"
#include "support/survivors_ranking_server.h"
#include "support/temp_directory.h"
#include "survivors/survivors_ranking_session.h"
#include "survivors/survivors_save.h"
#include "survivors/survivors_stage.h"

namespace {

namespace rn = ro::net;
namespace sim = ro::sim;
namespace ts = ro::test_support;
namespace ui = ro::ui;
using moonclient::SurvivorsBoardKind;
using moonclient::SurvivorsBookMode;
using moonclient::SurvivorsRankingAsks;
using moonclient::SurvivorsRankingFacts;
using moonclient::SurvivorsRankingMode;
using moonclient::SurvivorsRankingNotice;
using moonclient::SurvivorsRankingNoticeKind;
using moonclient::SurvivorsRankingNow;
using moonclient::SurvivorsRankingRowsHeld;
using moonclient::SurvivorsRankingSession;
using moonclient::SurvivorsScreen;
using moonclient::SurvivorsStage;

constexpr const char* kServerKey = "127.0.0.1:6900";
constexpr std::uint32_t kAlice = 150001;
constexpr std::uint32_t kAccount = 2000001;
constexpr std::uint64_t kLocalSeed = 77;
constexpr std::int64_t kMsPerSecond = 1000;
constexpr std::int32_t kArenaSideCells = 64;
constexpr int kMostFrames = 20000;
// Le mercredi 21 octobre 2026, à midi UTC : loin d'un changement de semaine.
constexpr sim::CivilDate kBenchDay{.year = 2026, .month = 10, .day = 21};
constexpr std::int64_t kBenchHour = 12;
constexpr std::int16_t kKnownLength = 25;
// Une seconde : moins que la cadence du tableau.
constexpr std::int64_t kWithinCadenceMs = 1000;
constexpr const char* kRunLineHead = "Survivants de Midgard : votre partie du ";

[[nodiscard]] std::int64_t BenchStartUtcS() {
  return sim::DayOfCivilDate(kBenchDay) * sim::kSecondsPerDay + kBenchHour * sim::kSecondsPerHour;
}

[[nodiscard]] rn::Frame FrameOf(std::vector<std::uint8_t> bytes) {
  constexpr unsigned kBitsPerByte = 8;
  const auto opcode = static_cast<std::uint16_t>(bytes[0] | (bytes[1] << kBitsPerByte));
  return rn::Frame{.opcode = opcode, .bytes = std::move(bytes)};
}

[[nodiscard]] std::uint16_t OpcodeOf(const std::vector<std::uint8_t>& frame) {
  return FrameOf(frame).opcode;
}

[[nodiscard]] bool StartsWith(const std::string& text, const std::string& head) {
  return text.rfind(head, 0) == 0;
}

// LE BANC : le classement, une liaison en boucle locale dont chaque trame
// émise est relevée, le serveur simulé et le carnet, sur une même horloge.
struct Bench {
  explicit Bench(ts::SurvivorsRankingServerSetup setup)
      : server(std::move(setup)),
        mode(sim::kSurvivorsRulesetVersion,
             [](std::uint16_t) -> std::optional<std::int16_t> { return kKnownLength; }),
        book([this] { return utc_s(); }),
        link(ro::test::Connect(loopback)) {
    server.AddCharacter(kAlice, kAccount, "Alice");
    link.set_send_observer([this](std::span<const std::uint8_t> frame) {
      sent.emplace_back(frame.begin(), frame.end());
    });
    stage.Open();
    Pump();
  }

  [[nodiscard]] std::int64_t utc_s() const { return BenchStartUtcS() + elapsed_ms / kMsPerSecond; }

  void Pump(std::int64_t ms = 0) { Deliver(PumpHeld(ms)); }

  [[nodiscard]] ts::SurvivorsRankingServer::Frames PumpHeld(std::int64_t ms) {
    elapsed_ms += ms;
    server.SetNowMs(BenchStartUtcS() * kMsPerSecond + elapsed_ms);
    const std::size_t before = sent.size();
    mode.Pump(link,
              SurvivorsRankingFacts{
                  .verified = true,
                  .map_entries = 1,
                  .server_key = kServerKey,
                  .character_id = kAlice,
                  .character_name = "Alice",
                  .now = SurvivorsRankingNow{.steady_ms = elapsed_ms, .utc_s = utc_s()}},
              root.path());
    ts::SurvivorsRankingServer::Frames replies;
    for (std::size_t index = before; index < sent.size(); ++index) {
      for (std::vector<std::uint8_t>& reply : server.Receive(kAlice, sent[index])) {
        replies.push_back(std::move(reply));
      }
    }
    return replies;
  }

  void Deliver(const ts::SurvivorsRankingServer::Frames& frames) {
    for (const std::vector<std::uint8_t>& frame : frames) {
      CHECK(mode.ApplyFrame(FrameOf(frame), elapsed_ms) == rn::FrameOutcome::kApplied);
    }
  }

  void Announce() { Deliver(server.AnnounceCaps(kAlice, mode.caps())); }

  // La partie choisie à l'accueil, commencée, son jeton reçu — ou perdu, et
  // l'échéance passée —, puis jouée jusqu'à son bilan, comme le mode la conduit.
  void PlayRun(SurvivorsBoardKind kind, bool ticket_answered = true) {
    book.Choose(kind);
    Begin(mode.Launch(book, ts::TinyRules(), root.path(), kLocalSeed).value());
    if (ticket_answered) {
      Pump();
    } else {
      static_cast<void>(PumpHeld(0));
      Pump(moonclient::kSurvivorsTicketWaitMs);
    }
    if (const std::optional<SurvivorsBookMode::Begun> settled =
            mode.Settle(book, ts::TinyRules(), root.path())) {
      Begin(*settled);
    }
    stage.Loaded();
    hud_ranked_seen = moonclient::HudRankedOf(mode.session(), stage.board().kind);
    for (int frames = 0; stage.entered() != SurvivorsScreen::kSummary; ++frames) {
      REQUIRE(frames < kMostFrames);
      moonclient::SurvivorsIntent intent{.proceed = true};
      if (stage.screen() == SurvivorsScreen::kChoosing) {
        intent.answer = sim::SurvivorsAnswer{.kind = sim::AnswerKind::kPick, .offer_index = 0};
      }
      stage.Advance(sim::kSurvivorsStepMs, intent);
      elapsed_ms += sim::kSurvivorsStepMs;
    }
    // Le classement suit chaque image du jeu : il sait l'heure de la fin.
    Pump();
    mode.FollowEnded(stage, false);
    book.FollowEntered(stage, ts::TinyRules(), root.path());
  }

  void Begin(const SurvivorsBookMode::Begun& begun) {
    REQUIRE(stage.Begin(ts::TinyRules(), ts::PillarArena(kArenaSideCells), begun.start, begun.board)
                .has_value());
  }

  // Les lignes de conversation de la session depuis la dernière lecture.
  [[nodiscard]] std::vector<std::string> Said() {
    std::vector<std::string> lines;
    moonclient::SayRankingNotices(mode, stage,
                                  [&lines](std::string line) { lines.push_back(std::move(line)); });
    return lines;
  }

  [[nodiscard]] std::size_t BoardAsksSent() const {
    std::size_t count = 0;
    for (const std::vector<std::uint8_t>& frame : sent) {
      if (OpcodeOf(frame) == rn::kSurvivorsBoardAskOpcode) ++count;
    }
    return count;
  }

  ts::SurvivorsRankingServer server;
  SurvivorsRankingMode mode;
  SurvivorsBookMode book;
  SurvivorsStage stage;
  const ts::TempDirectory root;
  ro::test::LoopbackServer loopback;
  rn::Connection link;
  std::vector<std::vector<std::uint8_t>> sent;
  std::int64_t elapsed_ms = 0;
  bool hud_ranked_seen = false;
};

}  // namespace

TEST_CASE("Classement survivors à l'écran : service fermé, ni bouton, ni onglet, ni ligne",
          "[app][survivors]") {
  Bench bench(ts::TodaySetup());
  SurvivorsRankingRowsHeld held;
  CHECK_FALSE(
      moonclient::PrepareRankingOf(bench.mode.session(), held, ts::kTinyClassId).has_value());

  // Le bouton « Classement » n'existe pas ; une intention forcée n'ouvre rien.
  ui::SurvivorsRankingState state;
  moonclient::FollowRankingAsks(bench.mode, state, SurvivorsRankingAsks{.open = true},
                                ts::TinyRules(), ts::kTinyClassId);
  CHECK_FALSE(state.open);
  state.open = true;
  ui::SurvivorsRankingView view;
  moonclient::ComposeRankingWindow(bench.mode, state, view, held, ts::kTinyClassId, "");
  CHECK_FALSE(state.open);

  // Une partie classée demandée part libre : ni marque au HUD, ni section à
  // l'écran de fin, et rien dans la conversation — « service absent » reste
  // au journal.
  bench.PlayRun(SurvivorsBoardKind::kRanked);
  CHECK_FALSE(bench.hud_ranked_seen);
  CHECK_FALSE(
      moonclient::SummaryRankOf(bench.mode.session(), bench.stage.board().kind).has_value());
  bench.Pump();
  CHECK(bench.Said().empty());
  CHECK(bench.sent.empty());
  CHECK(bench.server.frames_received() == 0);
  CHECK(bench.server.cuts() == 0);
}

TEST_CASE("Classement survivors à l'écran : du jeton à l'écran de fin, rang et meilleur",
          "[app][survivors]") {
  Bench bench(ts::ServingSetup(sim::kSurvivorsRulesetVersion, ts::kTinyArenaId, ts::kTinyClassId));
  bench.Announce();
  SurvivorsRankingRowsHeld held;
  REQUIRE(moonclient::PrepareRankingOf(bench.mode.session(), held, ts::kTinyClassId).has_value());

  bench.PlayRun(SurvivorsBoardKind::kRanked);
  CHECK(bench.hud_ranked_seen);
  const SurvivorsBoardKind board = bench.stage.board().kind;
  REQUIRE(board == SurvivorsBoardKind::kRanked);
  const sim::SurvivorsTally tally = bench.stage.ending()->tally;

  // L'ENVOI PART, LE VERDICT EST GARDÉ : « Envoi au serveur… », et le compteur
  // de l'accueil compte la partie.
  const ts::SurvivorsRankingServer::Frames verdict = bench.PumpHeld(0);
  REQUIRE(verdict.size() == 1);
  std::optional<ui::SurvivorsSummaryRanking> summary =
      moonclient::SummaryRankOf(bench.mode.session(), board);
  REQUIRE(summary.has_value());
  CHECK(summary->ranked);
  CHECK(summary->tab == ui::SurvivorsRankingTab::kRanked);
  CHECK(summary->standing.status == ui::SurvivorsRankStatus::kSending);
  CHECK(summary->board_reachable);
  CHECK_FALSE(summary->server_score.has_value());
  CHECK(moonclient::PendingCountOf(bench.mode.session()) == 1);

  // LE VERDICT TARDE : passé son délai, l'écran de fin dit l'entrée en attente.
  bench.Pump(moonclient::kSurvivorsVerdictWaitS * kMsPerSecond);
  summary = moonclient::SummaryRankOf(bench.mode.session(), board);
  REQUIRE(summary.has_value());
  CHECK(summary->standing.status == ui::SurvivorsRankStatus::kPending);
  const std::optional<ui::SurvivorsPrepareRanking> waiting =
      moonclient::PrepareRankingOf(bench.mode.session(), held, ts::kTinyClassId);
  REQUIRE(waiting.has_value());
  REQUIRE(waiting->pending.size() == 1);
  CHECK(std::string(waiting->pending.front().character) == "Alice");

  // LE VERDICT : le rang, le meilleur et le score du serveur, que l'écran de
  // fin montre seul ; la conversation se tait, et le compteur retombe à zéro.
  bench.Deliver(verdict);
  bench.Pump();
  summary = moonclient::SummaryRankOf(bench.mode.session(), board);
  REQUIRE(summary.has_value());
  CHECK((summary->standing.status == ui::SurvivorsRankStatus::kAccepted ||
         summary->standing.status == ui::SurvivorsRankStatus::kPersonalBest));
  CHECK(summary->standing.rank == 1);
  CHECK(summary->standing.best_score == sim::SurvivorsScore(tally));
  REQUIRE(summary->server_score.has_value());
  CHECK(*summary->server_score == sim::SurvivorsScore(tally));
  CHECK(bench.Said().empty());
  CHECK(moonclient::PendingCountOf(bench.mode.session()) == 0);
  const std::optional<ui::SurvivorsPrepareRanking> emptied =
      moonclient::PrepareRankingOf(bench.mode.session(), held, ts::kTinyClassId);
  REQUIRE(emptied.has_value());
  CHECK(emptied->pending.empty());

  // LE RECORD LOCAL DES PARTIES CLASSÉES (Q8), sous son onglet.
  moonclient::SurvivorsRecordRowsHeld rows;
  const ui::SurvivorsRecordsView records = moonclient::RecordsViewOf(
      ts::TinyRules(), moonclient::SurvivorsSaveFile().Load(bench.root.path()).save, bench.utc_s(),
      std::nullopt, rows);
  CHECK(records.ranked_runs.size() == 1);
  CHECK(records.free_runs.empty());

  // UNE PARTIE LIBRE ENSUITE ne montre pas le verdict de la précédente.
  bench.stage.Close();
  bench.stage.Open();
  bench.PlayRun(SurvivorsBoardKind::kFree);
  summary = moonclient::SummaryRankOf(bench.mode.session(), bench.stage.board().kind);
  REQUIRE(summary.has_value());
  CHECK_FALSE(summary->ranked);
  CHECK(bench.server.cuts() == 0);
}

TEST_CASE("Classement survivors à l'écran : un verdict arrivé écran fermé se dit en conversation",
          "[app][survivors]") {
  Bench bench(ts::ServingSetup(sim::kSurvivorsRulesetVersion, ts::kTinyArenaId, ts::kTinyClassId));
  bench.Announce();
  bench.PlayRun(SurvivorsBoardKind::kRanked);
  const ts::SurvivorsRankingServer::Frames verdict = bench.PumpHeld(0);
  REQUIRE(verdict.size() == 1);
  CHECK(bench.Said().empty());
  // Le joueur a quitté l'écran de fin avant le verdict.
  bench.stage.Close();
  bench.Deliver(verdict);
  bench.Pump();
  const std::vector<std::string> said = bench.Said();
  REQUIRE(said.size() == 1);
  CHECK(StartsWith(said.front(), kRunLineHead));
  CHECK(said.front().find("1e") != std::string::npos);
}

TEST_CASE("Classement survivors à l'écran : ouvrir le tableau le demande une fois par cadence",
          "[app][survivors]") {
  Bench bench(ts::ServingSetup(sim::kSurvivorsRulesetVersion, ts::kTinyArenaId, ts::kTinyClassId));
  bench.Announce();
  ui::SurvivorsRankingState state;
  SurvivorsRankingRowsHeld held;
  ui::SurvivorsRankingView view;

  // « Classement » à l'accueil : la fenêtre s'ouvre sur le tableau classé et
  // demande sa page.
  moonclient::FollowRankingAsks(bench.mode, state, SurvivorsRankingAsks{.open = true},
                                ts::TinyRules(), ts::kTinyClassId);
  CHECK(state.open);
  CHECK(state.tab == ui::SurvivorsRankingTab::kRanked);
  moonclient::ComposeRankingWindow(bench.mode, state, view, held, ts::kTinyClassId, "");
  CHECK_FALSE(view.ranked.received);
  bench.Pump();
  CHECK(bench.BoardAsksSent() == 1);

  // Une seconde après : ni « Actualiser » ni un autre onglet ne repartent.
  bench.Pump(kWithinCadenceMs);
  moonclient::ComposeRankingWindow(bench.mode, state, view, held, ts::kTinyClassId, "");
  CHECK(view.ranked.received);
  CHECK(view.ranked.own_rank == 0);
  CHECK_FALSE(view.refresh_ready);
  CHECK(view.ranked.age_s == kWithinCadenceMs / kMsPerSecond);
  CHECK(view.ruleset == sim::kSurvivorsRulesetVersion);
  moonclient::FollowRankingAsks(
      bench.mode, state, SurvivorsRankingAsks{.window = ui::SurvivorsRankingAsk{.refresh = true}},
      ts::TinyRules(), ts::kTinyClassId);
  state.tab = ui::SurvivorsRankingTab::kChallenge;
  moonclient::FollowRankingAsks(
      bench.mode, state,
      SurvivorsRankingAsks{.window = ui::SurvivorsRankingAsk{.tab_changed = true}}, ts::TinyRules(),
      ts::kTinyClassId);
  bench.Pump();
  CHECK(bench.BoardAsksSent() == 1);

  // Pendant la cadence, la page du défi attend sa demande.
  moonclient::ComposeRankingWindow(bench.mode, state, view, held, ts::kTinyClassId, "");
  CHECK_FALSE(view.challenge.received);

  // LA CADENCE PASSÉE, LA DEMANDE RETENUE PART D'ELLE-MÊME, une fois : la
  // fenêtre n'a plus rien demandé.
  bench.Pump(moonclient::kSurvivorsBoardCadenceMs);
  moonclient::FollowRankingAsks(bench.mode, state, SurvivorsRankingAsks{}, ts::TinyRules(),
                                ts::kTinyClassId);
  bench.Pump();
  CHECK(bench.BoardAsksSent() == 2);
  bench.Pump(moonclient::kSurvivorsBoardCadenceMs);
  moonclient::FollowRankingAsks(bench.mode, state, SurvivorsRankingAsks{}, ts::TinyRules(),
                                ts::kTinyClassId);
  bench.Pump();
  CHECK(bench.BoardAsksSent() == 2);
  moonclient::ComposeRankingWindow(bench.mode, state, view, held, ts::kTinyClassId, "");
  CHECK(view.refresh_ready);
  CHECK(view.challenge.received);
  const sim::IsoWeek week = moonclient::IsoWeekOfUtc(bench.utc_s());
  CHECK(view.week_year == week.year);
  CHECK(view.week_number == week.week);
  CHECK(bench.server.cuts() == 0);
}

TEST_CASE("Classement survivors à l'écran : un défi sans réponse se joue en local, sans marque",
          "[app][survivors]") {
  Bench bench(ts::ServingSetup(sim::kSurvivorsRulesetVersion, ts::kTinyArenaId, ts::kTinyClassId));
  bench.Announce();
  bench.PlayRun(SurvivorsBoardKind::kChallenge, false);
  CHECK_FALSE(bench.hud_ranked_seen);
  REQUIRE(bench.stage.board().kind == SurvivorsBoardKind::kChallenge);
  // Ni « Partie libre » ni « Partie classée » à l'écran de fin.
  CHECK_FALSE(
      moonclient::SummaryRankOf(bench.mode.session(), SurvivorsBoardKind::kChallenge).has_value());
  // La conversation le dit une fois, avec la ligne propre au défi.
  bench.Pump();
  const std::vector<std::string> said = bench.Said();
  REQUIRE(said.size() == 1);
  CHECK(said.front() == "Défi de la semaine non classé : il se joue en local.");
  CHECK(bench.Said().empty());
  for (const std::vector<std::uint8_t>& frame : bench.sent) {
    CHECK(OpcodeOf(frame) != rn::kSurvivorsEndOpcode);
  }
  CHECK(bench.server.cuts() == 0);
}

TEST_CASE("Classement survivors à l'écran : une page de plus de vingt lignes en montre vingt",
          "[app][survivors]") {
  SurvivorsRankingSession session;
  session.NoteService(
      rn::SurvivorsService{.ruleset_low = session.ruleset(), .ruleset_high = session.ruleset()});
  const rn::SurvivorsBoardKey key{.ruleset = session.ruleset(),
                                  .class_id = ts::kTinyClassId,
                                  .board = rn::SurvivorsBoardKind::kRanked};
  REQUIRE(session.AskBoard(key));
  rn::SurvivorsBoardPage page{.key = key};
  for (std::size_t index = 0; index <= ui::kSurvivorsRankingMostRows; ++index) {
    page.lines.push_back(rn::SurvivorsBoardLine{.rank = static_cast<std::uint16_t>(index + 1),
                                                .name = "Survivant" + std::to_string(index + 1)});
  }
  session.NoteFrame(page);
  SurvivorsRankingRowsHeld held;
  const ui::SurvivorsRankingView view =
      moonclient::RankingViewOf(session, {}, held, ts::kTinyClassId, "");
  REQUIRE(view.ranked.rows.size() == ui::kSurvivorsRankingMostRows);
  CHECK(std::string(view.ranked.rows.back().name) == "Survivant20");
}

TEST_CASE("Classement survivors à l'écran : les lignes de conversation par cas",
          "[app][survivors]") {
  const SurvivorsRankingSession session;
  constexpr sim::CivilDate kEndedDay{.year = 2026, .month = 10, .day = 25};
  const std::int64_t ended_at_utc_s =
      sim::DayOfCivilDate(kEndedDay) * sim::kSecondsPerDay + kBenchHour * sim::kSecondsPerHour;
  // Au départ (Q3) : la partie classée part libre avec sa raison ; le défi se
  // joue en local.
  const std::optional<std::string> silent = moonclient::RankingLineOf(
      SurvivorsRankingNotice{.kind = SurvivorsRankingNoticeKind::kTicketSilent}, session, false);
  REQUIRE(silent.has_value());
  CHECK(*silent == ui::SurvivorsTicketRefusalText(ui::SurvivorsTicketRefusal::kSilent));
  const std::optional<std::string> place = moonclient::RankingLineOf(
      SurvivorsRankingNotice{.kind = SurvivorsRankingNoticeKind::kTicketRefused,
                             .ticket_result = rn::SurvivorsTicketResult::kPlaceRefused},
      session, true);
  REQUIRE(place.has_value());
  CHECK(*place == ui::SurvivorsTicketRefusalText(ui::SurvivorsTicketRefusal::kPlace));
  const std::optional<std::string> challenge = moonclient::RankingLineOf(
      SurvivorsRankingNotice{.kind = SurvivorsRankingNoticeKind::kTicketSilent,
                             .board = rn::SurvivorsBoardKind::kChallenge},
      session, false);
  REQUIRE(challenge.has_value());
  CHECK(*challenge != *silent);

  // Résultat perdu ou oublié (Q6) : une ligne datée.
  for (const SurvivorsRankingNoticeKind kind :
       {SurvivorsRankingNoticeKind::kResultLost, SurvivorsRankingNoticeKind::kResultExpired}) {
    const std::optional<std::string> line = moonclient::RankingLineOf(
        SurvivorsRankingNotice{.kind = kind, .ended_at_utc_s = ended_at_utc_s}, session, false);
    REQUIRE(line.has_value());
    CHECK(StartsWith(*line, std::string(kRunLineHead) + "2026-10-25"));
  }
  // Un refus au verdict, hors de l'écran de fin.
  const std::optional<std::string> refused = moonclient::RankingLineOf(
      SurvivorsRankingNotice{
          .kind = SurvivorsRankingNoticeKind::kVerdict,
          .result = rn::SurvivorsResult{.verdict = rn::SurvivorsVerdict::kImplausible},
          .ended_at_utc_s = ended_at_utc_s},
      session, false);
  REQUIRE(refused.has_value());
  CHECK(StartsWith(*refused, kRunLineHead));

  // Le journal seul : le service absent, les règles non classées, le désaccord.
  for (const SurvivorsRankingNoticeKind kind :
       {SurvivorsRankingNoticeKind::kServiceAbsent, SurvivorsRankingNoticeKind::kRulesetNotServed,
        SurvivorsRankingNoticeKind::kScoreMismatch}) {
    CHECK_FALSE(moonclient::RankingLineOf(SurvivorsRankingNotice{.kind = kind}, session, false)
                    .has_value());
  }
}

TEST_CASE("Classement survivors à l'écran : l'accueil lit le meilleur rang de sa classe",
          "[app][survivors]") {
  constexpr std::uint16_t kOtherClass = ts::kTinyClassId + 1;
  constexpr std::uint16_t kOwnRank = 3;
  SurvivorsRankingSession session;
  session.NoteService(
      rn::SurvivorsService{.ruleset_low = session.ruleset(), .ruleset_high = session.ruleset()});
  const rn::SurvivorsBoardKey key{.ruleset = session.ruleset(),
                                  .class_id = ts::kTinyClassId,
                                  .board = rn::SurvivorsBoardKind::kRanked};
  REQUIRE(session.AskBoard(key));
  session.NoteFrame(rn::SurvivorsBoardPage{.key = key, .own_rank = kOwnRank});
  SurvivorsRankingRowsHeld held;
  const std::optional<ui::SurvivorsPrepareRanking> own =
      moonclient::PrepareRankingOf(session, held, ts::kTinyClassId);
  REQUIRE(own.has_value());
  CHECK(own->best_known);
  CHECK(own->best_rank == kOwnRank);
  const std::optional<ui::SurvivorsPrepareRanking> other =
      moonclient::PrepareRankingOf(session, held, kOtherClass);
  REQUIRE(other.has_value());
  // La page tenue est celle d'une autre classe : son rang n'est pas su.
  CHECK_FALSE(other->best_known);
  CHECK(other->best_rank == 0);
}

TEST_CASE("Classement survivors à l'écran : un tableau par classe, redemandé quand elle change",
          "[app][survivors]") {
  constexpr std::uint16_t kMageClass = 1;
  constexpr std::uint16_t kKnightClass = 2;
  constexpr const char* kKnightName = "Chevalier";
  // La place de la classe dans `CZ 0x0F50`.
  constexpr std::size_t kWireClassOffset = 8;
  const sim::SurvivorsRules& rules = sim::ShippedSurvivorsRules();
  const sim::ArenaRule* ranked_arena = sim::FirstRankedArena(rules);
  REQUIRE(ranked_arena != nullptr);
  Bench bench(ts::ServingSetup(rules.ruleset_version, ranked_arena->arena_id,
                               std::vector<std::uint16_t>{kMageClass, kKnightClass}));
  bench.Announce();
  ui::SurvivorsRankingState state;
  SurvivorsRankingRowsHeld held;
  ui::SurvivorsRankingView view;
  const auto last_ask_class = [&bench] {
    std::uint16_t asked = 0;
    for (const std::vector<std::uint8_t>& frame : bench.sent) {
      if (OpcodeOf(frame) == rn::kSurvivorsBoardAskOpcode) asked = frame[kWireClassOffset];
    }
    return asked;
  };

  // LE TABLEAU DU MAGICIEN.
  moonclient::FollowRankingAsks(bench.mode, state, SurvivorsRankingAsks{.open = true}, rules,
                                kMageClass);
  bench.Pump();
  REQUIRE(bench.BoardAsksSent() == 1);
  CHECK(last_ask_class() == kMageClass);
  moonclient::ComposeRankingWindow(bench.mode, state, view, held, kMageClass, "Magicien");
  CHECK(view.ranked.received);
  CHECK(std::string(view.class_name) == "Magicien");

  // LA CLASSE CHANGE, FENÊTRE OUVERTE : la page du Magicien ne se montre pas sous
  // le Chevalier, et la demande attend la cadence, sans autre intention.
  moonclient::FollowRankingAsks(bench.mode, state, SurvivorsRankingAsks{}, rules, kKnightClass);
  bench.Pump();
  CHECK(bench.BoardAsksSent() == 1);
  moonclient::ComposeRankingWindow(bench.mode, state, view, held, kKnightClass, kKnightName);
  CHECK_FALSE(view.ranked.received);
  CHECK(std::string(view.class_name) == kKnightName);
  bench.Pump(moonclient::kSurvivorsBoardCadenceMs);
  moonclient::FollowRankingAsks(bench.mode, state, SurvivorsRankingAsks{}, rules, kKnightClass);
  bench.Pump();
  REQUIRE(bench.BoardAsksSent() == 2);
  CHECK(last_ask_class() == kKnightClass);
  moonclient::ComposeRankingWindow(bench.mode, state, view, held, kKnightClass, kKnightName);
  CHECK(view.ranked.received);

  // UNE DEMANDE RETENUE POUR UNE CLASSE QUITTÉE NE PART PAS : seule part la dernière.
  moonclient::FollowRankingAsks(bench.mode, state, SurvivorsRankingAsks{}, rules, kMageClass);
  moonclient::FollowRankingAsks(bench.mode, state, SurvivorsRankingAsks{}, rules, kKnightClass);
  bench.Pump(moonclient::kSurvivorsBoardCadenceMs);
  moonclient::FollowRankingAsks(bench.mode, state, SurvivorsRankingAsks{}, rules, kKnightClass);
  bench.Pump();
  CHECK(bench.BoardAsksSent() == 3);
  CHECK(last_ask_class() == kKnightClass);

  // Une classe que les règles ne connaissent pas ne demande rien.
  bench.Pump(moonclient::kSurvivorsBoardCadenceMs);
  CHECK_FALSE(bench.mode.AskBoard(rules, 0, rn::SurvivorsBoardKind::kRanked));
  CHECK(bench.server.cuts() == 0);
}
