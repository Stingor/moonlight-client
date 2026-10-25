#include "game/survivors_ranking_mode.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ro/core/error.h"
#include "ro/core/log.h"
#include "ro/net/survivors_ranking.h"
#include "ro/net/ui_caps.h"
#include "ro/proto/packet_lengths.h"
#include "ro/sim/survivors_journal.h"
#include "ro/sim/survivors_play.h"
#include "ro/sim/survivors_ranked.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_week.h"

#include "survivors/survivors_book.h"

namespace moonclient {
namespace {

namespace rn = ro::net;

// La clé d'un serveur dont l'adresse de connexion manque.
constexpr const char* kUnnamedServerKey = "local";

[[nodiscard]] rn::SurvivorsBoardKind WireBoardOf(SurvivorsBoardKind kind) noexcept {
  return kind == SurvivorsBoardKind::kChallenge ? rn::SurvivorsBoardKind::kChallenge
                                                : rn::SurvivorsBoardKind::kRanked;
}

[[nodiscard]] unsigned CodeOf(auto value) noexcept { return static_cast<unsigned>(value); }

// CE QUE LA SESSION A À DIRE, au journal du client ; la conversation n'en
// reprend que ce que `RankingLineOf` choisit.
void LogNotice(const SurvivorsRankingNotice& notice, std::uint16_t ruleset) {
  switch (notice.kind) {
    case SurvivorsRankingNoticeKind::kServiceAbsent:
      ro::core::LogInfo("classement : service absent");
      return;
    case SurvivorsRankingNoticeKind::kRulesetNotServed:
      ro::core::LogInfo("classement : règles {} non classées par ce serveur", ruleset);
      return;
    case SurvivorsRankingNoticeKind::kTicketRefused:
      ro::core::LogInfo("classement : jeton refusé (raison {}), partie libre",
                        CodeOf(notice.ticket_result));
      return;
    case SurvivorsRankingNoticeKind::kTicketSilent:
      ro::core::LogInfo("classement : le serveur n'a pas répondu, partie libre");
      return;
    case SurvivorsRankingNoticeKind::kChallengeSeedMismatch:
      ro::core::LogWarning(
          "classement : la graine du défi n'est pas celle de sa semaine, partie "
          "libre");
      return;
    case SurvivorsRankingNoticeKind::kVerdict:
      ro::core::LogInfo("classement : verdict {} — score {}, rang {}, meilleur {}",
                        CodeOf(notice.result.verdict), notice.result.score, notice.result.rank,
                        notice.result.best);
      return;
    case SurvivorsRankingNoticeKind::kResultLost:
      ro::core::LogInfo("classement : résultat inconnu du serveur — déjà compté, ou perdu");
      return;
    case SurvivorsRankingNoticeKind::kResultExpired:
      ro::core::LogInfo("classement : résultat oublié, son jeton a expiré");
      return;
    case SurvivorsRankingNoticeKind::kScoreMismatch:
      ro::core::LogWarning("classement : désaccord de score — serveur {}, client {}",
                           notice.result.score, notice.local_score);
      return;
  }
}

}  // namespace

bool SurvivorsRepliesKnown(const PacketLengthOf& length_of) {
  return std::ranges::all_of(rn::kSurvivorsReplyOpcodes,
                             [&](std::uint16_t opcode) { return length_of(opcode).has_value(); });
}

std::string SurvivorsServerKeyOf(const std::string& login_address) {
  return login_address.empty() ? std::string(kUnnamedServerKey) : login_address;
}

SurvivorsRankingMode::SurvivorsRankingMode(std::uint16_t ruleset, const PacketLengthOf& length_of,
                                           SurvivorsSaveFile file)
    : session_(ruleset), file_(std::move(file)), ruleset_(ruleset) {
  const bool known = length_of ? SurvivorsRepliesKnown(length_of)
                               : SurvivorsRepliesKnown(PacketLengthOf(ro::proto::PacketLength));
  caps_ = known ? rn::kUiCapSurvivors : 0U;
}

ro::net::FrameOutcome SurvivorsRankingMode::ApplyFrame(const ro::net::Frame& frame,
                                                       std::int64_t received_at_ms) {
  using ro::net::FrameOutcome;
  switch (frame.opcode) {
    case rn::kSurvivorsTicketOpcode:
      if (const std::optional<rn::SurvivorsTicket> ticket =
              rn::DecodeSurvivorsTicket(frame.bytes)) {
        session_.NoteReceivedAt(received_at_ms);
        session_.NoteFrame(*ticket);
        return FrameOutcome::kApplied;
      }
      return FrameOutcome::kUndecodable;
    case rn::kSurvivorsResultOpcode:
      if (const std::optional<rn::SurvivorsResult> result =
              rn::DecodeSurvivorsResult(frame.bytes)) {
        session_.NoteFrame(*result);
        return FrameOutcome::kApplied;
      }
      return FrameOutcome::kUndecodable;
    case rn::kSurvivorsBoardOpcode:
      if (const std::optional<rn::SurvivorsBoardPage> page =
              rn::DecodeSurvivorsBoard(frame.bytes)) {
        session_.NoteFrame(*page);
        return FrameOutcome::kApplied;
      }
      return FrameOutcome::kUndecodable;
    case rn::kSurvivorsServiceOpcode:
      if (const std::optional<rn::SurvivorsService> service =
              rn::DecodeSurvivorsService(frame.bytes)) {
        session_.NoteService(*service);
        return FrameOutcome::kApplied;
      }
      return FrameOutcome::kUndecodable;
    default:
      return FrameOutcome::kUnhandled;
  }
}

void SurvivorsRankingMode::Pump(ro::net::Connection& link, const SurvivorsRankingFacts& facts,
                                const std::filesystem::path& client_root) {
  // LES RÉSULTATS EN ATTENTE DU CARNET, une fois par installation.
  if (loaded_root_ != client_root) {
    loaded_root_ = client_root;
    session_.LoadPending(file_.Load(client_root).save.pending_results);
  }
  // Le personnage avant la poignée de main et l'entrée en carte : un renvoi ne
  // s'arme que pour le personnage connu.
  if (server_key_ != facts.server_key || character_id_ != facts.character_id) {
    server_key_ = facts.server_key;
    character_id_ = facts.character_id;
    session_.NoteCharacter(facts.server_key, facts.character_id, facts.character_name);
  }
  if (facts.verified && !verified_) session_.NoteHandshakeVerified();
  if (!facts.verified && verified_) session_.NoteHandshakeLost();
  verified_ = facts.verified;
  if (map_entries_ != facts.map_entries) {
    map_entries_ = facts.map_entries;
    session_.NoteMapEntered();
  }
  now_ = facts.now;
  session_.Tick(facts.now);
  // Hors d'une poignée de main vérifiée, rien ne part : la suivante referme le
  // service et vide ce qui attendait.
  if (facts.verified && link.is_open()) {
    for (const std::vector<std::uint8_t>& frame : session_.TakeOutgoing()) link.Send(frame);
  }
  Write(client_root);
}

SurvivorsBookMode::Begun SurvivorsRankingMode::Launch(SurvivorsBookMode& book,
                                                      const ro::sim::SurvivorsRules& rules,
                                                      const std::filesystem::path& client_root,
                                                      std::uint64_t seed,
                                                      std::uint16_t first_skill_id) {
  launch_.reset();
  // CHAQUE DÉPART OUBLIE LE PRÉCÉDENT : un jeton accordé à un départ qui n'a
  // jamais atteint son bilan ne classe pas la partie suivante.
  session_.ForgetRun();
  SurvivorsBoardKind kind = book.chosen();
  // La partie classée n'existe que service ouvert : sans lui, elle part libre.
  if (kind == SurvivorsBoardKind::kRanked && !session_.service_open()) {
    kind = SurvivorsBoardKind::kFree;
  }
  SurvivorsBookMode::Begun begun = book.StartAs(kind, rules, client_root, seed, first_skill_id);
  if (kind == SurvivorsBoardKind::kFree) return begun;
  const ro::sim::ArenaRule* arena = ro::sim::FindArena(rules, begun.start.arena_id);
  const ro::sim::ClassRule* class_rule = ro::sim::FindClass(rules, begun.start.class_id);
  const SurvivorsRunRanking asked = session_.AskTicket(
      SurvivorsTicketRequest{.arena_id = begun.start.arena_id,
                             .class_id = begun.start.class_id,
                             .board = WireBoardOf(kind),
                             .arena_key = arena != nullptr ? arena->key : "",
                             .class_key = class_rule != nullptr ? class_rule->key : "",
                             .neutral_start = ro::sim::IsNeutralStart(begun.start)});
  if (asked == SurvivorsRunRanking::kAwaiting) {
    launch_ = PendingLaunch{.kind = kind,
                            .start_seed = begun.start.seed,
                            .local_seed = seed,
                            .first_skill_id = first_skill_id};
    return begun;
  }
  // Un défi sans jeton reste un défi local ; une partie classée part libre.
  if (kind == SurvivorsBoardKind::kRanked) {
    return book.StartAs(SurvivorsBoardKind::kFree, rules, client_root, seed, first_skill_id);
  }
  return begun;
}

std::optional<SurvivorsBookMode::Begun> SurvivorsRankingMode::Settle(
    SurvivorsBookMode& book, const ro::sim::SurvivorsRules& rules,
    const std::filesystem::path& client_root) {
  if (!launch_.has_value() || holding()) return std::nullopt;
  const PendingLaunch launch = *launch_;
  launch_.reset();
  if (const rn::SurvivorsTicket* ticket = session_.ranked_ticket()) {
    if (ticket->seed == launch.start_seed) return std::nullopt;
    if (launch.kind == SurvivorsBoardKind::kRanked) {
      return SurvivorsBookMode::Begun{
          .start = ro::sim::RankedStartOf(rules, ticket->seed, launch.first_skill_id),
          .board = SurvivorsRunBoard{.kind = SurvivorsBoardKind::kRanked}};
    }
    // LE DÉFI SE JOUE À LA SEMAINE DU SERVEUR, dont la graine vient d'être
    // vérifiée : la machine a pu changer de semaine avant ou après lui.
    const ro::sim::IsoWeek week = IsoWeekOfUtc(ticket->issued_at_utc_s);
    return SurvivorsBookMode::Begun{
        .start = ChallengeStartOf(rules, week),
        .board = SurvivorsRunBoard{.kind = SurvivorsBoardKind::kChallenge, .week = week}};
  }
  if (launch.kind == SurvivorsBoardKind::kRanked) {
    return book.StartAs(SurvivorsBoardKind::kFree, rules, client_root, launch.local_seed,
                        launch.first_skill_id);
  }
  return std::nullopt;
}

void SurvivorsRankingMode::FollowEnded(const SurvivorsStage& stage, bool frozen_by_link_loss) {
  launch_.reset();
  const SurvivorsEnding* ending = stage.ending();
  if (ending == nullptr || session_.run_state() != SurvivorsRunRanking::kRanked) return;
  // SEULE SE RAPPORTE LA PARTIE JOUÉE au tableau classé ou au défi, à départ
  // neutre : une autre oublie le jeton sans rien envoyer.
  const ro::sim::SurvivorsPlay* play = stage.play();
  if (play == nullptr || stage.board().kind == SurvivorsBoardKind::kFree ||
      !ro::sim::IsNeutralStart(play->run().field().start)) {
    session_.ForgetRun();
    return;
  }
  const std::size_t journal_bytes = ro::sim::EncodeSurvivorsJournal(play->journal()).size();
  session_.ReportEnd(
      SurvivorsRunEnd{.tally = ending->tally,
                      .journal_bytes = static_cast<std::uint32_t>(std::min<std::size_t>(
                          journal_bytes, std::numeric_limits<std::uint32_t>::max())),
                      .frozen_by_link_loss = frozen_by_link_loss});
}

void SurvivorsRankingMode::Write(const std::filesystem::path& client_root) {
  for (const SurvivorsPendingChange& change : session_.TakePendingChanges()) {
    const SurvivorsSaveDelta delta =
        change.kind == SurvivorsPendingChangeKind::kAdd
            ? AddPendingResult(change.entry)
            : RemovePendingResult(change.entry.server_key, change.entry.character_id,
                                  change.entry.ticket);
    if (const ro::core::Result<SurvivorsSaveRead> written = file_.Mutate(client_root, delta);
        !written) {
      ro::core::LogWarning("classement : résultat en attente non écrit au carnet — {}",
                           ro::core::Describe(written.error()));
    }
  }
  for (SurvivorsRankingNotice& notice : session_.TakeNotices()) {
    LogNotice(notice, ruleset_);
    told_.push_back(std::move(notice));
  }
}

bool SurvivorsRankingMode::AskBoard(const ro::sim::SurvivorsRules& rules,
                                    rn::SurvivorsBoardKind board) {
  if (rules.arenas.empty() || rules.classes.empty()) return false;
  return session_.AskBoard(rn::SurvivorsBoardAsk{.ruleset = ruleset_,
                                                 .arena_id = rules.arenas.front().arena_id,
                                                 .class_id = rules.classes.front().class_id,
                                                 .board = board});
}

}  // namespace moonclient
