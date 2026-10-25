#include "survivors/survivors_ranking_session.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ro/net/survivors_ranking.h"
#include "ro/sim/survivors_ranked.h"
#include "ro/sim/survivors_score.h"

#include "survivors/survivors_save.h"

namespace moonclient {
namespace {

namespace rn = ro::net;

[[nodiscard]] std::string BoardKeyOf(rn::SurvivorsBoardKind board) {
  return std::string(board == rn::SurvivorsBoardKind::kChallenge ? kChallengeBoardKey
                                                                 : kRankedBoardKey);
}

[[nodiscard]] rn::SurvivorsEndReport EndReportOf(const SurvivorsPendingResult& entry) {
  const ro::sim::SurvivorsEndComponents& parts = entry.components;
  return rn::SurvivorsEndReport{.ticket = entry.ticket,
                                .survived_steps = parts.survived_steps,
                                .kill_points = parts.kill_points,
                                .kills = parts.kills,
                                .boss_kills = parts.boss_kills,
                                .level = parts.level,
                                .outcome = static_cast<std::uint8_t>(parts.outcome),
                                .journal_bytes = parts.journal_bytes};
}

}  // namespace

SurvivorsRankingSession::SurvivorsRankingSession(std::uint16_t ruleset) : ruleset_(ruleset) {}

void SurvivorsRankingSession::LoadPending(std::vector<SurvivorsPendingResult> entries) {
  pending_ = std::move(entries);
  resend_.clear();
}

void SurvivorsRankingSession::NoteCharacter(std::string server_key, std::uint32_t character_id,
                                            std::string character_name) {
  server_key_ = std::move(server_key);
  character_id_ = character_id;
  character_name_ = std::move(character_name);
  resend_.clear();
}

void SurvivorsRankingSession::NoteHandshakeVerified() {
  // Une liaison neuve : le service se tait jusqu'à la prochaine annonce.
  service_open_ = false;
  DropOutgoing();
  tickets_owed_ = 0;
  ArmResend();
}

void SurvivorsRankingSession::NoteHandshakeLost() {
  service_open_ = false;
  DropOutgoing();
  tickets_owed_ = 0;
}

void SurvivorsRankingSession::NoteMapEntered() { ArmResend(); }

void SurvivorsRankingSession::NoteService(const rn::SurvivorsService& service) {
  service_seen_ = true;
  server_offset_s_ = service.server_utc_s - now_.utc_s;
  const bool served = ro::sim::RulesetServed(
      ro::sim::SurvivorsRulesetRange{.low = service.ruleset_low, .high = service.ruleset_high},
      ruleset_);
  service_open_ = served;
  // L'annonce se redit à chaque entrée en carte : la ligne ne se dit qu'une fois.
  if (!served && !told_unserved_) {
    told_unserved_ = true;
    Say({.kind = SurvivorsRankingNoticeKind::kRulesetNotServed});
  }
}

void SurvivorsRankingSession::NoteFrame(const rn::SurvivorsTicket& ticket) {
  // Un jeton que rien n'a demandé, ou qui répond à une demande plus ancienne,
  // s'ignore ; de même un jeton que rien n'attend plus.
  if (tickets_owed_ == 0) return;
  --tickets_owed_;
  if (tickets_owed_ != 0 || run_state_ != SurvivorsRunRanking::kAwaiting) return;
  // L'échéance se juge à l'arrivée : un jeton tardif fait une partie libre.
  if (now_.steady_ms >= run_deadline_ms_) {
    FreeRun({.kind = SurvivorsRankingNoticeKind::kTicketSilent});
    return;
  }
  if (ticket.result != rn::SurvivorsTicketResult::kGranted) {
    FreeRun({.kind = SurvivorsRankingNoticeKind::kTicketRefused, .ticket_result = ticket.result});
    return;
  }
  if (run_request_.board == rn::SurvivorsBoardKind::kChallenge &&
      !ro::sim::ChallengeTicketMatches(ticket.seed, ticket.issued_at_utc_s)) {
    FreeRun({.kind = SurvivorsRankingNoticeKind::kChallengeSeedMismatch});
    return;
  }
  run_ticket_ = ticket;
  run_state_ = SurvivorsRunRanking::kRanked;
}

void SurvivorsRankingSession::NoteFrame(const rn::SurvivorsResult& result) {
  const auto found =
      std::find_if(pending_.begin(), pending_.end(), [&](const SurvivorsPendingResult& entry) {
        return IsPendingResultOf(entry, server_key_, character_id_, result.ticket);
      });
  if (found == pending_.end()) return;
  const SurvivorsPendingResult entry = *found;
  Remove(static_cast<std::size_t>(found - pending_.begin()));
  if (submitted_ticket_ == result.ticket) run_result_ = result;

  SurvivorsRankingNotice notice{
      .result = result, .local_score = entry.local_score, .ended_at_utc_s = entry.ended_at_utc_s};
  if (result.verdict == rn::SurvivorsVerdict::kUnknownTicket) {
    notice.kind = SurvivorsRankingNoticeKind::kResultLost;
    Say(notice);
    return;
  }
  notice.kind = SurvivorsRankingNoticeKind::kVerdict;
  Say(notice);
  const bool accepted = result.verdict == rn::SurvivorsVerdict::kAccepted ||
                        result.verdict == rn::SurvivorsVerdict::kPersonalBest;
  if (accepted && result.score != entry.local_score) {
    notice.kind = SurvivorsRankingNoticeKind::kScoreMismatch;
    Say(notice);
  }
}

void SurvivorsRankingSession::NoteFrame(const rn::SurvivorsBoardPage& page) {
  SurvivorsHeldPage held{.page = page, .received_at_ms = now_.steady_ms};
  switch (page.key.board) {
    case rn::SurvivorsBoardKind::kRanked:
      ranked_page_ = std::move(held);
      break;
    case rn::SurvivorsBoardKind::kChallenge:
      challenge_page_ = std::move(held);
      break;
    case rn::SurvivorsBoardKind::kUnknown:
      break;
  }
}

void SurvivorsRankingSession::Tick(const SurvivorsRankingNow& now) {
  now_ = now;
  if (run_state_ == SurvivorsRunRanking::kAwaiting && now_.steady_ms >= run_deadline_ms_) {
    FreeRun({.kind = SurvivorsRankingNoticeKind::kTicketSilent});
  }
  if (!service_open_) return;
  // Un renvoi au plus par image ; une entrée morte s'oublie sans partir.
  while (!resend_.empty()) {
    const std::uint32_t ticket = resend_.front();
    resend_.erase(resend_.begin());
    const auto found =
        std::find_if(pending_.begin(), pending_.end(), [&](const SurvivorsPendingResult& entry) {
          return IsPendingResultOf(entry, server_key_, character_id_, ticket);
        });
    if (found == pending_.end()) continue;
    const std::int64_t server_now_s = now_.utc_s + server_offset_s_;
    if (found->issued_at_utc_s + kSurvivorsTicketLifetimeS <= server_now_s) {
      const SurvivorsPendingResult entry = *found;
      Remove(static_cast<std::size_t>(found - pending_.begin()));
      Say({.kind = SurvivorsRankingNoticeKind::kResultExpired,
           .local_score = entry.local_score,
           .ended_at_utc_s = entry.ended_at_utc_s});
      continue;
    }
    Send(*found);
    return;
  }
}

SurvivorsRunRanking SurvivorsRankingSession::AskTicket(const SurvivorsTicketRequest& request) {
  run_request_ = request;
  run_ticket_.reset();
  submitted_ticket_.reset();
  run_result_.reset();
  if (!request.neutral_start || request.board == rn::SurvivorsBoardKind::kUnknown) {
    run_state_ = SurvivorsRunRanking::kFree;
    return run_state_;
  }
  if (!service_open_) {
    run_state_ = SurvivorsRunRanking::kFree;
    if (!service_seen_ && !told_absent_) {
      told_absent_ = true;
      Say({.kind = SurvivorsRankingNoticeKind::kServiceAbsent});
    }
    return run_state_;
  }
  run_state_ = SurvivorsRunRanking::kAwaiting;
  run_deadline_ms_ = now_.steady_ms + kSurvivorsTicketWaitMs;
  if (Push(rn::EncodeSurvivorsBegin(rn::SurvivorsTicketAsk{.ruleset = ruleset_,
                                                           .arena_id = request.arena_id,
                                                           .class_id = request.class_id,
                                                           .board = request.board}))) {
    ++begins_queued_;
  }
  return run_state_;
}

void SurvivorsRankingSession::ForgetRun() noexcept {
  run_state_ = SurvivorsRunRanking::kNotAsked;
  run_ticket_.reset();
  // L'écran de fin de la partie suivante ne montre pas le verdict de celle-ci.
  submitted_ticket_.reset();
  run_result_.reset();
}

void SurvivorsRankingSession::ReportEnd(const SurvivorsRunEnd& end) {
  if (run_state_ != SurvivorsRunRanking::kRanked || !run_ticket_) return;
  const rn::SurvivorsTicket ticket = *run_ticket_;
  run_state_ = SurvivorsRunRanking::kNotAsked;
  run_ticket_.reset();
  if (end.frozen_by_link_loss) return;
  const std::optional<ro::sim::SurvivorsEndComponents> components =
      ro::sim::EndComponentsOf(end.tally, end.journal_bytes);
  if (!components) return;

  SurvivorsPendingResult entry{.server_key = server_key_,
                               .character_id = character_id_,
                               .character_name = character_name_,
                               .ticket = ticket.ticket,
                               .issued_at_utc_s = ticket.issued_at_utc_s,
                               .ruleset = ruleset_,
                               .arena_key = run_request_.arena_key,
                               .class_key = run_request_.class_key,
                               .board_key = BoardKeyOf(run_request_.board),
                               .seed = ticket.seed,
                               .components = *components,
                               .local_score = ro::sim::SurvivorsScore(end.tally),
                               .ended_at_utc_s = now_.utc_s};
  if (!IsSendablePendingResult(entry)) return;
  submitted_ticket_ = ticket.ticket;
  pending_.push_back(entry);
  if (service_open_) {
    Send(pending_.back());
    return;
  }
  // Service fermé : l'entrée part à la prochaine annonce.
  resend_.push_back(entry.ticket);
  changes_.push_back({.kind = SurvivorsPendingChangeKind::kAdd, .entry = std::move(entry)});
}

bool SurvivorsRankingSession::AskBoard(const rn::SurvivorsBoardAsk& ask) {
  if (!service_open_ || ask.ruleset != ruleset_) return false;
  if (last_board_ask_ms_ && now_.steady_ms < board_ready_at_ms()) return false;
  std::optional<std::vector<std::uint8_t>> frame = rn::EncodeSurvivorsBoardAsk(ask);
  if (!frame) return false;
  last_board_ask_ms_ = now_.steady_ms;
  Push(std::move(frame));
  return true;
}

std::vector<std::vector<std::uint8_t>> SurvivorsRankingSession::TakeOutgoing() {
  if (!service_open_) DropOutgoing();
  tickets_owed_ += std::exchange(begins_queued_, 0);
  return std::exchange(outgoing_, {});
}

std::vector<SurvivorsPendingChange> SurvivorsRankingSession::TakePendingChanges() {
  return std::exchange(changes_, {});
}

std::vector<SurvivorsRankingNotice> SurvivorsRankingSession::TakeNotices() {
  return std::exchange(notices_, {});
}

std::optional<std::uint64_t> SurvivorsRankingSession::ranked_seed() const noexcept {
  if (run_state_ != SurvivorsRunRanking::kRanked || !run_ticket_) return std::nullopt;
  return run_ticket_->seed;
}

const rn::SurvivorsTicket* SurvivorsRankingSession::ranked_ticket() const noexcept {
  if (run_state_ != SurvivorsRunRanking::kRanked || !run_ticket_) return nullptr;
  return &*run_ticket_;
}

const SurvivorsHeldPage* SurvivorsRankingSession::page(
    rn::SurvivorsBoardKind board) const noexcept {
  const std::optional<SurvivorsHeldPage>& held =
      board == rn::SurvivorsBoardKind::kChallenge ? challenge_page_ : ranked_page_;
  if (board == rn::SurvivorsBoardKind::kUnknown || !held) return nullptr;
  return &*held;
}

std::int64_t SurvivorsRankingSession::board_ready_at_ms() const noexcept {
  return last_board_ask_ms_ ? *last_board_ask_ms_ + kSurvivorsBoardCadenceMs : 0;
}

bool SurvivorsRankingSession::IsOurs(const SurvivorsPendingResult& entry) const {
  return entry.server_key == server_key_ && entry.character_id == character_id_;
}

bool SurvivorsRankingSession::Push(std::optional<std::vector<std::uint8_t>> frame) {
  if (!service_open_ || !frame) return false;
  outgoing_.push_back(std::move(*frame));
  return true;
}

void SurvivorsRankingSession::DropOutgoing() noexcept {
  outgoing_.clear();
  begins_queued_ = 0;
}

void SurvivorsRankingSession::Say(SurvivorsRankingNotice notice) {
  notices_.push_back(std::move(notice));
}

void SurvivorsRankingSession::FreeRun(SurvivorsRankingNotice notice) {
  run_state_ = SurvivorsRunRanking::kFree;
  run_ticket_.reset();
  notice.board = run_request_.board;
  Say(std::move(notice));
}

void SurvivorsRankingSession::ArmResend() {
  resend_.clear();
  for (const SurvivorsPendingResult& entry : pending_) {
    if (IsOurs(entry)) resend_.push_back(entry.ticket);
  }
}

void SurvivorsRankingSession::Send(SurvivorsPendingResult& entry) {
  Push(rn::EncodeSurvivorsEnd(EndReportOf(entry)));
  ++entry.send_count;
  entry.last_sent_utc_s = now_.utc_s;
  changes_.push_back({.kind = SurvivorsPendingChangeKind::kAdd, .entry = entry});
}

void SurvivorsRankingSession::Remove(std::size_t index) {
  SurvivorsPendingResult entry = pending_[index];
  pending_.erase(pending_.begin() + static_cast<std::ptrdiff_t>(index));
  std::erase(resend_, entry.ticket);
  changes_.push_back({.kind = SurvivorsPendingChangeKind::kRemove, .entry = std::move(entry)});
}

}  // namespace moonclient
