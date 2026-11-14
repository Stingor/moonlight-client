// LE CLASSEMENT SURVIVORS DANS LA PARTIE : le bit gardé par la table des
// longueurs, une partie jouée service fermé sans un octet émis, et une partie
// classée de bout en bout contre le serveur simulé — jeton, départ refait sur
// sa graine, fin envoyée, résultat en attente écrit puis retiré au verdict.
// Sur les règles livrées, la partie libre, le défi local et la partie classée
// partent sur le sol généré de la prairie, sans installation.

#include "game/survivors_ranking_mode.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/net/survivors_ranking.h"
#include "ro/net/ui_caps.h"
#include "ro/net/world_update.h"
#include "ro/proto/packet_lengths.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_ranked.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_score.h"
#include "ro/sim/survivors_terrain.h"
#include "ro/sim/survivors_week.h"

#include "game/surface_announcer.h"
#include "game/survivors_book_mode.h"
#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/survivors_fixture.h"
#include "support/survivors_ranking_server.h"
#include "support/temp_directory.h"
#include "survivors/arena_choice.h"
#include "survivors/survivors_book.h"
#include "survivors/survivors_save.h"
#include "survivors/survivors_stage.h"

namespace {

namespace rn = ro::net;
namespace sim = ro::sim;
namespace ts = ro::test_support;
using moonclient::SurvivorsBoardKind;
using moonclient::SurvivorsBookMode;
using moonclient::SurvivorsRankingFacts;
using moonclient::SurvivorsRankingMode;
using moonclient::SurvivorsRankingNow;
using moonclient::SurvivorsRunRanking;
using moonclient::SurvivorsScreen;
using moonclient::SurvivorsStage;

constexpr const char* kServerKey = "127.0.0.1:6900";
constexpr std::uint32_t kAlice = 150001;
constexpr std::uint32_t kAccount = 2000001;
constexpr std::uint64_t kLocalSeed = 77;
constexpr std::int64_t kMsPerSecond = 1000;
constexpr std::int32_t kArenaSideCells = 64;
// Une partie minuscule dure une minute ; l'arrivée, le voile et le bandeau en
// ajoutent quelques secondes.
constexpr int kMostFrames = 20000;
// Le mercredi 21 octobre 2026, à midi UTC : loin d'un changement de semaine.
constexpr sim::CivilDate kBenchDay{.year = 2026, .month = 10, .day = 21};
constexpr std::int64_t kBenchHour = 12;
// La longueur qu'une table d'essai donne à toute trame qu'elle connaît.
constexpr std::int16_t kKnownLength = 25;

[[nodiscard]] std::int64_t BenchStartUtcS() {
  return sim::DayOfCivilDate(kBenchDay) * sim::kSecondsPerDay + kBenchHour * sim::kSecondsPerHour;
}

// Une table des longueurs qui connaît tout, ou tout sauf `missing`.
[[nodiscard]] moonclient::PacketLengthOf TableWithout(std::optional<std::uint16_t> missing) {
  return [missing](std::uint16_t opcode) -> std::optional<std::int16_t> {
    if (missing == opcode) return std::nullopt;
    return kKnownLength;
  };
}

[[nodiscard]] rn::Frame FrameOf(std::vector<std::uint8_t> bytes) {
  constexpr unsigned kBitsPerByte = 8;
  const auto opcode = static_cast<std::uint16_t>(bytes[0] | (bytes[1] << kBitsPerByte));
  return rn::Frame{.opcode = opcode, .bytes = std::move(bytes)};
}

// LE BANC : le classement, une liaison en boucle locale dont chaque trame
// émise est relevée, le serveur simulé et le carnet, sur une même horloge.
struct Bench {
  explicit Bench(ts::SurvivorsRankingServerSetup setup)
      : server(std::move(setup)),
        mode(sim::kSurvivorsRulesetVersion, TableWithout(std::nullopt)),
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

  // Une image : l'horloge avance, le classement suit, ce qu'il émet arrive au
  // serveur, et les réponses reviennent par le flux.
  void Pump(std::int64_t ms = 0) { Deliver(PumpHeld(ms)); }

  // Une image dont les réponses du serveur sont gardées au lieu d'arriver.
  [[nodiscard]] ts::SurvivorsRankingServer::Frames PumpHeld(std::int64_t ms, bool verified = true,
                                                            std::uint32_t map_entries = 1) {
    elapsed_ms += ms;
    server.SetNowMs(BenchStartUtcS() * kMsPerSecond + elapsed_ms);
    const std::size_t before = sent.size();
    mode.Pump(link,
              SurvivorsRankingFacts{
                  .verified = verified,
                  .map_entries = map_entries,
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

  // Les trames reçues à l'instant courant, avant l'image qui les suit.
  void Deliver(const ts::SurvivorsRankingServer::Frames& frames) {
    for (const std::vector<std::uint8_t>& frame : frames) {
      CHECK(mode.ApplyFrame(FrameOf(frame), elapsed_ms) == rn::FrameOutcome::kApplied);
    }
  }

  // Le départ de l'accueil, commencé sur l'étage.
  void Launch(SurvivorsBoardKind kind) {
    book.Choose(kind);
    const std::optional<SurvivorsBookMode::Begun> begun =
        mode.Launch(book, ts::TinyRules(), root.path(), kLocalSeed);
    REQUIRE(begun.has_value());
    Begin(*begun);
  }

  void Begin(const SurvivorsBookMode::Begun& begun) {
    REQUIRE(stage.Begin(ts::TinyRules(), ts::PillarArena(kArenaSideCells), begun.start, begun.board)
                .has_value());
  }

  // La partie jouée jusqu'à son bilan, chaque offre prise à la première carte,
  // puis rapportée comme le mode la rapporte ; le carnet la crédite.
  void PlayToSummary() {
    stage.Loaded();
    for (int frames = 0; stage.entered() != SurvivorsScreen::kSummary; ++frames) {
      REQUIRE(frames < kMostFrames);
      moonclient::SurvivorsIntent intent{.proceed = true};
      if (stage.screen() == SurvivorsScreen::kChoosing) {
        intent.answer = sim::SurvivorsAnswer{.kind = sim::AnswerKind::kPick, .offer_index = 0};
      }
      stage.Advance(sim::kSurvivorsStepMs, intent);
      elapsed_ms += sim::kSurvivorsStepMs;
    }
    mode.FollowEnded(stage, false);
    book.FollowEntered(stage, ts::TinyRules(), root.path());
  }

  [[nodiscard]] moonclient::SurvivorsSave Save() const {
    return moonclient::SurvivorsSaveFile().Load(root.path()).save;
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
};

}  // namespace

TEST_CASE("Classement survivors : le bit n'entre au masque que si la table connaît les ZC",
          "[app][survivors]") {
  // La table livrée, engendrée du serveur, les connaît tous : le bit entre dans
  // le masque, à côté des surfaces, sans en recouvrir aucune.
  for (const std::uint16_t opcode : rn::kSurvivorsReplyOpcodes) {
    CHECK(ro::proto::PacketLength(opcode).has_value());
  }
  CHECK(SurvivorsRankingMode().caps() == rn::kUiCapSurvivors);
  CHECK((moonclient::ShownSurfaces(true) & rn::kUiCapSurvivors) == 0);
  CHECK((moonclient::ShownSurfaces(true) | SurvivorsRankingMode().caps()) ==
        (moonclient::ShownSurfaces(true) | rn::kUiCapSurvivors));
  // Une table d'essai qui les connaît tous : le bit y est.
  const SurvivorsRankingMode knowing(sim::kSurvivorsRulesetVersion, TableWithout(std::nullopt));
  CHECK(knowing.caps() == rn::kUiCapSurvivors);
  // Il suffit d'un seul ZC inconnu pour le taire.
  for (const std::uint16_t opcode : rn::kSurvivorsReplyOpcodes) {
    CHECK_FALSE(moonclient::SurvivorsRepliesKnown(TableWithout(opcode)));
    CHECK(SurvivorsRankingMode(sim::kSurvivorsRulesetVersion, TableWithout(opcode)).caps() == 0);
  }
}

TEST_CASE("Classement survivors : service fermé, une partie entière n'émet aucun octet",
          "[app][survivors]") {
  Bench bench(ts::TodaySetup());
  // La partie classée n'existe pas : elle part libre, et rien n'attend.
  bench.book.Choose(SurvivorsBoardKind::kRanked);
  const SurvivorsBookMode::Begun free_run =
      bench.mode.Launch(bench.book, ts::TinyRules(), bench.root.path(), kLocalSeed).value();
  CHECK(free_run.board.kind == SurvivorsBoardKind::kFree);
  CHECK_FALSE(bench.mode.holding());
  // Le défi reste un défi local, sans demande.
  bench.Launch(SurvivorsBoardKind::kChallenge);
  CHECK_FALSE(bench.mode.holding());
  CHECK(bench.stage.board().kind == SurvivorsBoardKind::kChallenge);
  bench.Pump();
  bench.PlayToSummary();
  bench.Pump();
  // Une poignée de main et une entrée en carte de plus : toujours rien.
  bench.mode.Pump(
      bench.link,
      SurvivorsRankingFacts{
          .verified = false, .map_entries = 2, .server_key = kServerKey, .character_id = kAlice},
      bench.root.path());
  bench.Pump();
  CHECK(bench.sent.empty());
  CHECK(bench.server.cuts() == 0);
  CHECK(bench.server.frames_received() == 0);
  CHECK(bench.Save().pending_results.empty());
  CHECK_FALSE(bench.mode.session().service_open());
}

TEST_CASE("Classement survivors : service annoncé, une partie classée part et se range",
          "[app][survivors]") {
  Bench bench(ts::ServingSetup(sim::kSurvivorsRulesetVersion, ts::kTinyArenaId, ts::kTinyClassId));
  // L'annonce du service, comme le serveur la rend au bit de `0x0F24`.
  bench.Deliver(bench.server.AnnounceCaps(kAlice, bench.mode.caps()));
  REQUIRE(bench.mode.session().service_open());

  // LE DÉPART ATTEND SON JETON, puis se refait sur la graine du serveur.
  bench.book.Choose(SurvivorsBoardKind::kRanked);
  const SurvivorsBookMode::Begun asked =
      bench.mode.Launch(bench.book, ts::TinyRules(), bench.root.path(), kLocalSeed).value();
  CHECK(asked.board.kind == SurvivorsBoardKind::kRanked);
  CHECK(bench.mode.holding());
  bench.Begin(asked);
  bench.Pump();
  REQUIRE(bench.sent.size() == 1);
  CHECK(bench.sent.front().size() == rn::kSurvivorsBeginSize);
  CHECK_FALSE(bench.mode.holding());
  REQUIRE(bench.mode.session().run_state() == SurvivorsRunRanking::kRanked);
  const std::optional<SurvivorsBookMode::Begun> settled =
      bench.mode.Settle(bench.book, ts::TinyRules(), bench.root.path());
  REQUIRE(settled.has_value());
  CHECK(settled->start.seed == *bench.mode.session().ranked_seed());
  CHECK(settled->board.kind == SurvivorsBoardKind::kRanked);
  CHECK(sim::IsNeutralStart(settled->start));
  REQUIRE(bench.stage.screen() == SurvivorsScreen::kLoading);
  bench.Begin(*settled);
  CHECK_FALSE(bench.mode.Settle(bench.book, ts::TinyRules(), bench.root.path()).has_value());

  // LA FIN PART, ET L'ENTRÉE EN ATTENTE S'ÉCRIT AVANT LE VERDICT.
  bench.PlayToSummary();
  const sim::SurvivorsTally tally = bench.stage.ending()->tally;
  // Le verdict revient dans la même image que l'envoi : on relit le carnet
  // entre les deux, au retour de la session.
  bench.server.SetNowMs(BenchStartUtcS() * kMsPerSecond + bench.elapsed_ms);
  bench.mode.Pump(bench.link,
                  SurvivorsRankingFacts{.verified = true,
                                        .map_entries = 1,
                                        .server_key = kServerKey,
                                        .character_id = kAlice,
                                        .character_name = "Alice",
                                        .now = SurvivorsRankingNow{.steady_ms = bench.elapsed_ms,
                                                                   .utc_s = bench.utc_s()}},
                  bench.root.path());
  REQUIRE(bench.sent.size() == 2);
  CHECK(bench.sent.back().size() == rn::kSurvivorsEndSize);
  const moonclient::SurvivorsSave pending = bench.Save();
  REQUIRE(pending.pending_results.size() == 1);
  CHECK(pending.pending_results.front().character_id == kAlice);
  CHECK(pending.pending_results.front().server_key == kServerKey);
  CHECK(pending.pending_results.front().board_key == moonclient::kRankedBoardKey);

  // LE VERDICT RETIRE L'ENTRÉE, et le score du serveur est celui du client.
  bench.Deliver(bench.server.Receive(kAlice, bench.sent.back()));
  bench.Pump();
  REQUIRE(bench.mode.session().run_result().has_value());
  const rn::SurvivorsResult& result = *bench.mode.session().run_result();
  CHECK((result.verdict == rn::SurvivorsVerdict::kAccepted ||
         result.verdict == rn::SurvivorsVerdict::kPersonalBest));
  CHECK(result.score == sim::SurvivorsScore(tally));
  CHECK(bench.Save().pending_results.empty());
  CHECK(bench.server.cuts() == 0);

  // LE TABLEAU LOCAL DES PARTIES CLASSÉES (Q8) la garde.
  const moonclient::SurvivorsSave credited = bench.Save();
  const moonclient::SurvivorsBoard* board =
      moonclient::FindBoard(credited, "tiny", "tiny", moonclient::kRankedBoardKey, {});
  REQUIRE(board != nullptr);
  CHECK(board->runs.size() == 1);
}

TEST_CASE("Classement survivors : un défi au jeton accordé garde son départ", "[app][survivors]") {
  Bench bench(ts::ServingSetup(sim::kSurvivorsRulesetVersion, ts::kTinyArenaId, ts::kTinyClassId));
  bench.Deliver(bench.server.AnnounceCaps(kAlice, bench.mode.caps()));
  bench.Launch(SurvivorsBoardKind::kChallenge);
  CHECK(bench.mode.holding());
  bench.Pump();
  REQUIRE(bench.mode.session().run_state() == SurvivorsRunRanking::kRanked);
  // La graine du serveur est celle de la semaine que l'accueil a jouée.
  CHECK_FALSE(bench.mode.Settle(bench.book, ts::TinyRules(), bench.root.path()).has_value());
  CHECK(bench.stage.board().kind == SurvivorsBoardKind::kChallenge);
  CHECK(bench.server.cuts() == 0);
}

TEST_CASE("Classement survivors : un jeton accordé ne classe que son propre départ",
          "[app][survivors]") {
  Bench bench(ts::ServingSetup(sim::kSurvivorsRulesetVersion, ts::kTinyArenaId, ts::kTinyClassId));
  bench.Deliver(bench.server.AnnounceCaps(kAlice, bench.mode.caps()));
  bench.Launch(SurvivorsBoardKind::kRanked);
  bench.Pump();
  REQUIRE(bench.mode.session().run_state() == SurvivorsRunRanking::kRanked);
  REQUIRE(bench.sent.size() == 1);

  SECTION("le départ n'aboutit pas, la partie libre suivante n'envoie rien") {
    // L'étage se ferme pendant le chargement, sans bilan.
    bench.stage.Close();
    bench.stage.Open();
    bench.Launch(SurvivorsBoardKind::kFree);
    CHECK(bench.mode.session().run_state() != SurvivorsRunRanking::kRanked);
    bench.PlayToSummary();
    bench.Pump();
  }
  SECTION("une partie libre commencée sans le classement n'envoie rien") {
    bench.Begin(bench.book.StartAs(SurvivorsBoardKind::kFree, ts::TinyRules(), bench.root.path(),
                                   kLocalSeed));
    bench.PlayToSummary();
    bench.Pump();
    CHECK(bench.mode.session().run_state() != SurvivorsRunRanking::kRanked);
  }
  CHECK(bench.sent.size() == 1);
  CHECK(bench.Save().pending_results.empty());
  CHECK(bench.server.cuts() == 0);
}

TEST_CASE("Classement survivors : un jeton reçu après l'échéance fait une partie libre",
          "[app][survivors]") {
  constexpr std::int64_t kQuietMarginMs = 10;
  constexpr std::int64_t kLastQuietFrameMs = moonclient::kSurvivorsTicketWaitMs - kQuietMarginMs;
  constexpr std::int64_t kPastDeadlineMs = 60;
  Bench bench(ts::ServingSetup(sim::kSurvivorsRulesetVersion, ts::kTinyArenaId, ts::kTinyClassId));
  bench.Deliver(bench.server.AnnounceCaps(kAlice, bench.mode.caps()));
  bench.book.Choose(SurvivorsBoardKind::kRanked);
  bench.Begin(
      bench.mode.Launch(bench.book, ts::TinyRules(), bench.root.path(), kLocalSeed).value());
  const ts::SurvivorsRankingServer::Frames ticket = bench.PumpHeld(0);
  REQUIRE(bench.sent.size() == 1);
  REQUIRE(ticket.size() == 1);
  CHECK(bench.PumpHeld(kLastQuietFrameMs).empty());
  CHECK(bench.mode.holding());
  // Le jeton arrive dans l'image qui franchit l'échéance, avant son `Pump`.
  bench.elapsed_ms += kPastDeadlineMs;
  bench.Deliver(ticket);
  CHECK(bench.mode.session().run_state() == SurvivorsRunRanking::kFree);
  bench.Pump();
  CHECK_FALSE(bench.mode.holding());
  const std::optional<SurvivorsBookMode::Begun> settled =
      bench.mode.Settle(bench.book, ts::TinyRules(), bench.root.path());
  REQUIRE(settled.has_value());
  CHECK(settled->board.kind == SurvivorsBoardKind::kFree);
  CHECK(bench.server.cuts() == 0);
}

TEST_CASE("Classement survivors : hors d'une poignée de main vérifiée, rien ne se compte envoyé",
          "[app][survivors]") {
  constexpr std::uint32_t kZoneChangeEntries = 2;
  constexpr std::uint32_t kBackEntries = 3;
  Bench bench(ts::ServingSetup(sim::kSurvivorsRulesetVersion, ts::kTinyArenaId, ts::kTinyClassId));
  bench.Deliver(bench.server.AnnounceCaps(kAlice, bench.mode.caps()));
  bench.Launch(SurvivorsBoardKind::kRanked);
  bench.Pump();
  const std::optional<SurvivorsBookMode::Begun> settled =
      bench.mode.Settle(bench.book, ts::TinyRules(), bench.root.path());
  REQUIRE(settled.has_value());
  bench.Begin(*settled);
  bench.PlayToSummary();
  // L'`END` part ; son verdict est gardé.
  static_cast<void>(bench.PumpHeld(0));
  REQUIRE(bench.sent.size() == 2);
  REQUIRE(bench.Save().pending_results.size() == 1);
  CHECK(bench.Save().pending_results.front().send_count == 1);

  // Changement de zone : la poignée de main n'est plus reconnue, l'entrée en
  // carte arme le renvoi, mais rien ne part ni ne se compte.
  CHECK(bench.PumpHeld(0, false, kZoneChangeEntries).empty());
  CHECK_FALSE(bench.mode.session().service_open());
  CHECK(bench.sent.size() == 2);
  CHECK(bench.Save().pending_results.front().send_count == 1);

  // La zone suivante : rien avant l'annonce, puis le renvoi, compté.
  CHECK(bench.PumpHeld(0, true, kBackEntries).empty());
  bench.Deliver(bench.server.AnnounceCaps(kAlice, bench.mode.caps()));
  static_cast<void>(bench.PumpHeld(0));
  CHECK(bench.sent.size() == 3);
  CHECK(bench.Save().pending_results.front().send_count == 2);
  CHECK(bench.server.cuts() == 0);
}

TEST_CASE("Classement survivors : une classée sans jeton repart libre sur l'arène demandée",
          "[app][survivors]") {
  // L'ACCUEIL A RETENU une arène libre pour la partie libre : une partie
  // classée qui part libre n'y va pas, qu'elle n'ait pas de service ou que le
  // serveur refuse son jeton. Il ne classe ici que cette arène, et refuse la
  // prairie.
  const sim::SurvivorsRules& rules = sim::ShippedSurvivorsRules();
  const sim::ArenaRule* plains = moonclient::DefaultArenaOf(rules);
  REQUIRE(plains != nullptr);
  const sim::ArenaRule* field = nullptr;
  for (const sim::ArenaRule& arena : rules.arenas) {
    if (arena.arena_id != plains->arena_id) field = &arena;
  }
  REQUIRE(field != nullptr);
  const sim::ClassRule& mage = rules.classes.front();
  Bench bench(ts::ServingSetup(rules.ruleset_version, field->arena_id, mage.class_id));
  bench.book.ChooseArena(rules, field->arena_id);
  bench.book.Choose(SurvivorsBoardKind::kFree);
  CHECK(bench.mode.Launch(bench.book, rules, bench.root.path(), kLocalSeed)->start.arena_id ==
        field->arena_id);

  // SERVICE FERMÉ : la classée part libre aussitôt, sur la prairie.
  bench.book.Choose(SurvivorsBoardKind::kRanked);
  const SurvivorsBookMode::Begun closed =
      bench.mode.Launch(bench.book, rules, bench.root.path(), kLocalSeed).value();
  CHECK(closed.board.kind == SurvivorsBoardKind::kFree);
  CHECK(closed.start.arena_id == plains->arena_id);
  CHECK_FALSE(bench.mode.holding());

  // JETON REFUSÉ : le départ refait au règlement part libre, sur la prairie.
  bench.Deliver(bench.server.AnnounceCaps(kAlice, bench.mode.caps()));
  REQUIRE(bench.mode.session().service_open());
  const SurvivorsBookMode::Begun asked =
      bench.mode.Launch(bench.book, rules, bench.root.path(), kLocalSeed).value();
  CHECK(asked.board.kind == SurvivorsBoardKind::kRanked);
  REQUIRE(bench.mode.holding());
  bench.Pump();
  CHECK_FALSE(bench.mode.holding());
  CHECK(bench.mode.session().run_state() != SurvivorsRunRanking::kRanked);
  const std::optional<SurvivorsBookMode::Begun> refused =
      bench.mode.Settle(bench.book, rules, bench.root.path());
  REQUIRE(refused.has_value());
  CHECK(refused->board.kind == SurvivorsBoardKind::kFree);
  CHECK(refused->start.arena_id == plains->arena_id);
  CHECK(bench.server.cuts() == 0);
}

TEST_CASE("Classement survivors : libre, défi local et classée partent sur le sol généré",
          "[app][survivors]") {
  // LES RÈGLES LIVRÉES, le serveur simulé qui les sert sur la
  // prairie seule (Q6 amendée) : une autre arène y serait refusée.
  const sim::SurvivorsRules& rules = sim::ShippedSurvivorsRules();
  const sim::ArenaRule* plains = moonclient::DefaultArenaOf(rules);
  REQUIRE(plains != nullptr);
  REQUIRE(plains->terrain_id != 0);
  const sim::ClassRule& mage = rules.classes.front();
  Bench bench(ts::ServingSetup(rules.ruleset_version, plains->arena_id, mage.class_id));

  // LA PARTIE COMMENCE SANS INSTALLATION, sur le terrain de son arène sous sa
  // graine, et arrive jusqu'à ses premiers pas.
  constexpr std::uint32_t kPlayedMs = 2000;
  const auto play_on_ground = [&](const SurvivorsBookMode::Begun& begun) {
    CHECK(begun.start.arena_id == plains->arena_id);
    const sim::ArenaRule* arena = sim::FindArena(rules, begun.start.arena_id);
    REQUIRE(arena != nullptr);
    auto walk = moonclient::ArenaWalkOf(nullptr, *arena);
    REQUIRE(walk.has_value());
    CHECK_FALSE(walk->has_value());
    REQUIRE(bench.stage.Begin(rules, std::move(*walk), begun.start, begun.board).has_value());
    const sim::SurvivorsField& field = bench.stage.play()->run().field();
    CHECK_FALSE(field.ground.bounded());
    CHECK(field.ground.run_seed() == begun.start.seed);
    CHECK(sim::CellOf(field.hero.position.x) == sim::kTerrainStartCell);
    CHECK(sim::CellOf(field.hero.position.y) == sim::kTerrainStartCell);
    bench.stage.Loaded();
    for (std::uint32_t played_ms = 0; played_ms < moonclient::kArrivalMs + kPlayedMs;
         played_ms += sim::kSurvivorsStepMs) {
      bench.stage.Advance(sim::kSurvivorsStepMs, moonclient::SurvivorsIntent{});
    }
    CHECK(bench.stage.screen() == SurvivorsScreen::kPlaying);
    CHECK(bench.stage.play()->run().field().step > 0);
    bench.stage.Close();
    bench.stage.Open();
  };

  // LA PARTIE LIBRE, sur la prairie par défaut, et LE DÉFI LOCAL, service
  // fermé : ni l'une ni l'autre ne demande rien.
  bench.book.Choose(SurvivorsBoardKind::kFree);
  play_on_ground(bench.mode.Launch(bench.book, rules, bench.root.path(), kLocalSeed).value());
  bench.book.Choose(SurvivorsBoardKind::kChallenge);
  const SurvivorsBookMode::Begun challenge =
      bench.mode.Launch(bench.book, rules, bench.root.path(), kLocalSeed).value();
  CHECK(challenge.board.kind == SurvivorsBoardKind::kChallenge);
  CHECK_FALSE(bench.mode.holding());
  play_on_ground(challenge);
  CHECK(bench.sent.empty());

  // LA PARTIE CLASSÉE : le jeton de la prairie, puis le départ refait sur la
  // graine du serveur, toujours sur le sol généré.
  bench.Deliver(bench.server.AnnounceCaps(kAlice, bench.mode.caps()));
  REQUIRE(bench.mode.session().service_open());
  bench.book.Choose(SurvivorsBoardKind::kRanked);
  const SurvivorsBookMode::Begun asked =
      bench.mode.Launch(bench.book, rules, bench.root.path(), kLocalSeed).value();
  CHECK(asked.board.kind == SurvivorsBoardKind::kRanked);
  REQUIRE(bench.mode.holding());
  bench.Pump();
  REQUIRE(bench.mode.session().run_state() == SurvivorsRunRanking::kRanked);
  const std::optional<SurvivorsBookMode::Begun> settled =
      bench.mode.Settle(bench.book, rules, bench.root.path());
  REQUIRE(settled.has_value());
  CHECK(settled->board.kind == SurvivorsBoardKind::kRanked);
  CHECK(settled->start.seed == *bench.mode.session().ranked_seed());
  play_on_ground(*settled);
  CHECK(bench.server.cuts() == 0);
}

namespace {

constexpr std::uint16_t kMageClass = 1;
constexpr std::uint16_t kKnightClass = 2;
constexpr std::uint16_t kThiefClass = 3;
// La place de la classe dans `CZ 0x0F4C` et `CZ 0x0F50`.
constexpr std::size_t kWireClassOffset = 8;

[[nodiscard]] std::uint16_t WireClassOf(const std::vector<std::uint8_t>& frame) {
  constexpr unsigned kBitsPerByte = 8;
  return static_cast<std::uint16_t>(frame[kWireClassOffset] |
                                    (frame[kWireClassOffset + 1] << kBitsPerByte));
}

// Le serveur simulé qui classe les règles livrées sur la prairie, pour `class_ids`.
[[nodiscard]] ts::SurvivorsRankingServerSetup PlainsServing(std::vector<std::uint16_t> class_ids) {
  const sim::SurvivorsRules& rules = sim::ShippedSurvivorsRules();
  return ts::ServingSetup(rules.ruleset_version, moonclient::DefaultArenaOf(rules)->arena_id,
                          std::move(class_ids));
}

// Le carnet du banc, la classe `class_id` débloquée.
void UnlockClass(Bench& bench, std::uint16_t class_id) {
  const sim::SurvivorsRules& rules = sim::ShippedSurvivorsRules();
  const std::string feat(moonclient::ClassUnlockFeat(*sim::FindClass(rules, class_id)));
  REQUIRE(moonclient::SurvivorsSaveFile()
              .Mutate(bench.root.path(),
                      [&feat](moonclient::SurvivorsSave& save) {
                        save.album.feats.push_back(feat);
                        return true;
                      })
              .has_value());
}

void UnlockKnight(Bench& bench) { UnlockClass(bench, kKnightClass); }

}  // namespace

TEST_CASE("Classement survivors : le Voleur rejoint part classé et en défi, sous sa classe",
          "[app][survivors]") {
  const sim::SurvivorsRules& rules = sim::ShippedSurvivorsRules();
  REQUIRE(sim::FindClass(rules, kThiefClass) != nullptr);
  for (const SurvivorsBoardKind kind :
       {SurvivorsBoardKind::kRanked, SurvivorsBoardKind::kChallenge}) {
    Bench bench(PlainsServing({kMageClass, kKnightClass, kThiefClass}));
    UnlockClass(bench, kThiefClass);
    bench.Deliver(bench.server.AnnounceCaps(kAlice, bench.mode.caps()));
    bench.book.ChooseClass(rules, kThiefClass);
    bench.book.Choose(kind);
    const std::optional<SurvivorsBookMode::Begun> asked =
        bench.mode.Launch(bench.book, rules, bench.root.path(), kLocalSeed);
    REQUIRE(asked.has_value());
    CHECK(asked->board.kind == kind);
    CHECK(asked->start.class_id == kThiefClass);
    CHECK(sim::SurvivorsRun::Start(rules, asked->start).has_value());
    bench.Pump();
    REQUIRE(bench.sent.size() == 1);
    CHECK(WireClassOf(bench.sent.front()) == kThiefClass);
    CHECK(bench.mode.session().run_state() == SurvivorsRunRanking::kRanked);
    CHECK(bench.server.cuts() == 0);
  }
}

TEST_CASE(
    "Classement survivors : la classe retenue part classée, le jeton et le règlement la gardent",
    "[app][survivors]") {
  const sim::SurvivorsRules& rules = sim::ShippedSurvivorsRules();
  const sim::ClassRule* knight = sim::FindClass(rules, kKnightClass);
  REQUIRE(knight != nullptr);

  Bench bench(PlainsServing({kMageClass, kKnightClass}));
  UnlockKnight(bench);
  bench.Deliver(bench.server.AnnounceCaps(kAlice, bench.mode.caps()));
  bench.book.ChooseClass(rules, kKnightClass);
  bench.book.Choose(SurvivorsBoardKind::kRanked);
  const std::optional<SurvivorsBookMode::Begun> asked =
      bench.mode.Launch(bench.book, rules, bench.root.path(), kLocalSeed);
  REQUIRE(asked.has_value());
  CHECK(asked->board.kind == SurvivorsBoardKind::kRanked);
  CHECK(asked->start.class_id == kKnightClass);
  // La partie classée part de l'auto-attaque seule.
  CHECK(asked->start.first_skill_id == 0);
  CHECK(sim::IsNeutralStart(asked->start));
  CHECK(sim::SurvivorsRun::Start(rules, asked->start).has_value());
  REQUIRE(bench.mode.holding());

  // La demande de jeton porte la classe, et le serveur l'accepte.
  bench.Pump();
  REQUIRE(bench.sent.size() == 1);
  CHECK(WireClassOf(bench.sent.front()) == kKnightClass);
  REQUIRE(bench.mode.session().run_state() == SurvivorsRunRanking::kRanked);

  // LE DÉPART RÉGLÉ PAR LE JETON reste celui du Chevalier, sur la graine du serveur.
  const std::optional<SurvivorsBookMode::Begun> settled =
      bench.mode.Settle(bench.book, rules, bench.root.path());
  REQUIRE(settled.has_value());
  CHECK(settled->board.kind == SurvivorsBoardKind::kRanked);
  CHECK(settled->start.class_id == kKnightClass);
  CHECK(settled->start.seed == *bench.mode.session().ranked_seed());
  CHECK(settled->start.first_skill_id == 0);
  CHECK(sim::SurvivorsRun::Start(rules, settled->start).has_value());
  CHECK(bench.server.cuts() == 0);
}

TEST_CASE("Classement survivors : le défi joue la classe retenue, jusqu'à la semaine du serveur",
          "[app][survivors]") {
  const sim::SurvivorsRules& rules = sim::ShippedSurvivorsRules();
  const sim::ClassRule* knight = sim::FindClass(rules, kKnightClass);
  REQUIRE(knight != nullptr);
  Bench bench(PlainsServing({kMageClass, kKnightClass}));
  UnlockKnight(bench);
  bench.Deliver(bench.server.AnnounceCaps(kAlice, bench.mode.caps()));
  // UNE MACHINE EN RETARD D'UNE SEMAINE : le règlement refait le départ, sur la
  // semaine du serveur.
  SurvivorsBookMode late_book([] { return BenchStartUtcS() - sim::kSecondsPerDay * 7; });
  late_book.ChooseClass(rules, kKnightClass);
  late_book.Choose(SurvivorsBoardKind::kChallenge);
  const std::optional<SurvivorsBookMode::Begun> asked =
      bench.mode.Launch(late_book, rules, bench.root.path(), kLocalSeed);
  REQUIRE(asked.has_value());
  CHECK(asked->start.class_id == kKnightClass);
  bench.Pump();
  REQUIRE(bench.sent.size() == 1);
  CHECK(WireClassOf(bench.sent.front()) == kKnightClass);
  const std::optional<SurvivorsBookMode::Begun> settled =
      bench.mode.Settle(late_book, rules, bench.root.path());
  REQUIRE(settled.has_value());
  const sim::IsoWeek week = moonclient::IsoWeekOfUtc(bench.utc_s());
  CHECK(settled->board.kind == SurvivorsBoardKind::kChallenge);
  CHECK(settled->start.class_id == kKnightClass);
  CHECK(settled->start.seed == sim::SeedOfWeek(week.year, week.week));
  CHECK(settled->start.first_skill_id == moonclient::ChallengeFirstSkill(rules, week));
  CHECK(bench.server.cuts() == 0);
}

TEST_CASE("Classement survivors : refusée par le serveur, la classée part libre avec sa classe",
          "[app][survivors]") {
  const sim::SurvivorsRules& rules = sim::ShippedSurvivorsRules();
  const sim::ArenaRule* plains = moonclient::DefaultArenaOf(rules);
  // Le serveur d'aujourd'hui ne classe que le Magicien.
  Bench bench(PlainsServing({kMageClass}));
  UnlockKnight(bench);
  bench.book.ChooseClass(rules, kKnightClass);
  bench.book.Choose(SurvivorsBoardKind::kRanked);

  // SERVICE FERMÉ : libre tout de suite, en Chevalier.
  const std::optional<SurvivorsBookMode::Begun> closed =
      bench.mode.Launch(bench.book, rules, bench.root.path(), kLocalSeed);
  REQUIRE(closed.has_value());
  CHECK(closed->board.kind == SurvivorsBoardKind::kFree);
  CHECK(closed->start.class_id == kKnightClass);
  CHECK(closed->start.arena_id == plains->arena_id);

  // JETON REFUSÉ : le départ réglé part libre, toujours en Chevalier.
  bench.Deliver(bench.server.AnnounceCaps(kAlice, bench.mode.caps()));
  REQUIRE(bench.mode.Launch(bench.book, rules, bench.root.path(), kLocalSeed).has_value());
  REQUIRE(bench.mode.holding());
  bench.Pump();
  CHECK(bench.mode.session().run_state() != SurvivorsRunRanking::kRanked);
  const std::optional<SurvivorsBookMode::Begun> refused =
      bench.mode.Settle(bench.book, rules, bench.root.path());
  REQUIRE(refused.has_value());
  CHECK(refused->board.kind == SurvivorsBoardKind::kFree);
  CHECK(refused->start.class_id == kKnightClass);
  CHECK(refused->start.arena_id == plains->arena_id);
  CHECK(bench.server.cuts() == 0);
}

TEST_CASE(
    "Classement survivors : une classe verrouillée ne part ni classée ni en défi, et rien ne part",
    "[app][survivors]") {
  const sim::SurvivorsRules& rules = sim::ShippedSurvivorsRules();
  Bench bench(PlainsServing({kMageClass, kKnightClass}));
  bench.book.ChooseClass(rules, kKnightClass);
  const auto launch = [&](SurvivorsBoardKind kind) {
    bench.book.Choose(kind);
    return bench.mode.Launch(bench.book, rules, bench.root.path(), kLocalSeed);
  };

  // SERVICE FERMÉ COMME OUVERT : aucun repli sur le Magicien.
  CHECK_FALSE(launch(SurvivorsBoardKind::kRanked).has_value());
  CHECK_FALSE(launch(SurvivorsBoardKind::kChallenge).has_value());
  bench.Deliver(bench.server.AnnounceCaps(kAlice, bench.mode.caps()));
  REQUIRE(bench.mode.session().service_open());
  CHECK_FALSE(launch(SurvivorsBoardKind::kRanked).has_value());
  CHECK_FALSE(launch(SurvivorsBoardKind::kChallenge).has_value());
  CHECK_FALSE(bench.mode.holding());
  bench.Pump();
  CHECK(bench.sent.empty());
  CHECK(bench.server.frames_received() == 0);

  // Sans classement non plus.
  bench.book.Choose(SurvivorsBoardKind::kRanked);
  CHECK_FALSE(bench.book.StartAlone(rules, bench.root.path(), kLocalSeed).has_value());
  bench.book.Choose(SurvivorsBoardKind::kChallenge);
  CHECK_FALSE(bench.book.StartAlone(rules, bench.root.path(), kLocalSeed).has_value());

  // LA PARTIE LIBRE garde son repli, que l'accueil montre coché.
  const std::optional<SurvivorsBookMode::Begun> free_run = launch(SurvivorsBoardKind::kFree);
  REQUIRE(free_run.has_value());
  CHECK(free_run->start.class_id == kMageClass);

  // L'ACCUEIL QUI RELIT UN CARNET VERROUILLÉ revient à la première classe, et ce
  // qu'il montre part : le Magicien classé part.
  bench.book.SettleClass(rules, moonclient::SurvivorsSave{});
  CHECK(bench.book.chosen_class_id() == 0);
  CHECK(bench.book.board_class_id(rules) == kMageClass);
  const std::optional<SurvivorsBookMode::Begun> mage = launch(SurvivorsBoardKind::kRanked);
  REQUIRE(mage.has_value());
  CHECK(mage->start.class_id == kMageClass);
  bench.Pump();
  REQUIRE(bench.sent.size() == 1);
  CHECK(WireClassOf(bench.sent.front()) == kMageClass);

  // DÉBLOQUÉ, le Chevalier se retient et part.
  moonclient::SurvivorsSave unlocked;
  unlocked.album.feats.emplace_back(
      moonclient::ClassUnlockFeat(*sim::FindClass(rules, kKnightClass)));
  bench.book.ChooseClass(rules, kKnightClass);
  bench.book.SettleClass(rules, unlocked);
  CHECK(bench.book.chosen_class_id() == kKnightClass);
  UnlockKnight(bench);
  bench.book.Choose(SurvivorsBoardKind::kChallenge);
  const std::optional<SurvivorsBookMode::Begun> alone =
      bench.book.StartAlone(rules, bench.root.path(), kLocalSeed);
  REQUIRE(alone.has_value());
  CHECK(alone->start.class_id == kKnightClass);
  CHECK(bench.server.cuts() == 0);
}
