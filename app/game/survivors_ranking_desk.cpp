#include "game/survivors_ranking_desk.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <span>
#include <string>

#include "ro/core/i18n.h"
#include "ro/net/survivors_ranking.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_week.h"

#include "survivors/survivors_book.h"

namespace moonclient {
namespace {

namespace rn = ro::net;
namespace ui = ro::ui;

constexpr std::int64_t kMsPerSecond = 1000;
// La ligne de la conversation : la phrase, sa date et un rang.
constexpr std::size_t kLineTextSize = 256;
// « 2026-10-25 » et son zéro.
constexpr std::size_t kDateTextSize = 16;

[[nodiscard]] rn::SurvivorsBoardKind WireBoardOf(ui::SurvivorsRankingTab tab) noexcept {
  return tab == ui::SurvivorsRankingTab::kChallenge ? rn::SurvivorsBoardKind::kChallenge
                                                    : rn::SurvivorsBoardKind::kRanked;
}

[[nodiscard]] ui::SurvivorsRankingTab TabOf(SurvivorsBoardKind board) noexcept {
  return board == SurvivorsBoardKind::kChallenge ? ui::SurvivorsRankingTab::kChallenge
                                                 : ui::SurvivorsRankingTab::kRanked;
}

[[nodiscard]] ui::SurvivorsTicketRefusal RefusalOf(rn::SurvivorsTicketResult result) noexcept {
  switch (result) {
    case rn::SurvivorsTicketResult::kPlaceRefused:
      return ui::SurvivorsTicketRefusal::kPlace;
    case rn::SurvivorsTicketResult::kUnlinkedAccount:
      return ui::SurvivorsTicketRefusal::kUnlinked;
    case rn::SurvivorsTicketResult::kUnknownRules:
      return ui::SurvivorsTicketRefusal::kRules;
    case rn::SurvivorsTicketResult::kTooSoon:
      return ui::SurvivorsTicketRefusal::kTooSoon;
    case rn::SurvivorsTicketResult::kGranted:
    case rn::SurvivorsTicketResult::kUnknown:
      break;
  }
  return ui::SurvivorsTicketRefusal::kUnknown;
}

[[nodiscard]] ui::SurvivorsRankStanding StandingOf(const rn::SurvivorsResult& result) noexcept {
  ui::SurvivorsRankStanding standing{.rank = result.rank, .best_score = result.best};
  switch (result.verdict) {
    case rn::SurvivorsVerdict::kAccepted:
      standing.status = ui::SurvivorsRankStatus::kAccepted;
      break;
    case rn::SurvivorsVerdict::kPersonalBest:
      standing.status = ui::SurvivorsRankStatus::kPersonalBest;
      break;
    case rn::SurvivorsVerdict::kUnknownTicket:
      standing.status = ui::SurvivorsRankStatus::kLost;
      break;
    case rn::SurvivorsVerdict::kImplausible:
      standing.status = ui::SurvivorsRankStatus::kImplausible;
      break;
    case rn::SurvivorsVerdict::kUnknown:
      standing.status = ui::SurvivorsRankStatus::kRefused;
      break;
  }
  return standing;
}

[[nodiscard]] bool Accepted(rn::SurvivorsVerdict verdict) noexcept {
  return verdict == rn::SurvivorsVerdict::kAccepted ||
         verdict == rn::SurvivorsVerdict::kPersonalBest;
}

// LE VERDICT PEUT ENCORE VENIR : service ouvert, et la partie n'est pas encore
// partie, ou son dernier envoi date de moins de `kSurvivorsVerdictWaitS`.
[[nodiscard]] bool AwaitingVerdict(const SurvivorsRankingSession& session) {
  if (!session.service_open()) return false;
  const std::optional<std::uint32_t>& ticket = session.submitted_ticket();
  if (!ticket.has_value()) return true;
  const std::span<const SurvivorsPendingResult> pending = session.pending();
  const auto found = std::ranges::find_if(pending, [&](const SurvivorsPendingResult& entry) {
    return entry.server_key == session.server_key() && entry.ticket == *ticket;
  });
  return found != pending.end() &&
         session.now().utc_s - found->last_sent_utc_s < kSurvivorsVerdictWaitS;
}

// Une page tenue en lignes de la fenêtre, vingt au plus ; les noms recopiés
// dans `names`.
[[nodiscard]] ui::SurvivorsRankingPage PageOf(const SurvivorsHeldPage* held,
                                              const SurvivorsRankingNow& now,
                                              std::vector<std::string>& names,
                                              std::vector<ui::SurvivorsRankingRow>& rows) {
  names.clear();
  rows.clear();
  if (held == nullptr) return {};
  const rn::SurvivorsBoardPage& page = held->page;
  const std::size_t shown = std::min(page.lines.size(), ui::kSurvivorsRankingMostRows);
  names.reserve(shown);
  for (std::size_t index = 0; index < shown; ++index) names.push_back(page.lines[index].name);
  for (std::size_t index = 0; index < shown; ++index) {
    const rn::SurvivorsBoardLine& line = page.lines[index];
    rows.push_back(ui::SurvivorsRankingRow{
        .rank = line.rank,
        .name = names[index].c_str(),
        .score = line.score,
        .survived_ms = std::uint64_t{line.survived_steps} * ro::sim::kSurvivorsStepMs,
        .level = line.level});
  }
  return ui::SurvivorsRankingPage{
      .received = true,
      .rows = rows,
      .own_rank = page.own_rank,
      .own_score = page.own_score,
      .age_s = std::max<std::int64_t>(0, (now.steady_ms - held->received_at_ms) / kMsPerSecond)};
}

// « 2026-10-25 », le jour UTC de l'instant.
[[nodiscard]] std::array<char, kDateTextSize> DateOfUtc(std::int64_t utc_s) {
  std::int64_t days = utc_s / ro::sim::kSecondsPerDay;
  if (utc_s % ro::sim::kSecondsPerDay < 0) --days;
  const std::chrono::year_month_day date{std::chrono::sys_days{std::chrono::days{days}}};
  std::array<char, kDateTextSize> text = {};
  std::snprintf(text.data(), text.size(), "%04d-%02u-%02u", static_cast<int>(date.year()),
                static_cast<unsigned>(date.month()), static_cast<unsigned>(date.day()));
  return text;
}

// La ligne d'une partie finie, sa date en tête : un format littéral par cas.
[[nodiscard]] std::string RunLineOf(const SurvivorsRankingNotice& notice) {
  const std::array<char, kDateTextSize> date = DateOfUtc(notice.ended_at_utc_s);
  std::array<char, kLineTextSize> text = {};
  char* out = text.data();
  const std::size_t size = text.size();
  switch (notice.kind) {
    case SurvivorsRankingNoticeKind::kVerdict:
      if (!Accepted(notice.result.verdict)) {
        std::snprintf(
            out, size,
            ro::core::Tr("Survivants de Midgard : votre partie du %s est refusée par le serveur."),
            date.data());
      } else if (notice.result.rank > 0) {
        std::snprintf(out, size,
                      ro::core::Tr("Survivants de Midgard : votre partie du %s est classée %s."),
                      date.data(), ui::SurvivorsRankText(notice.result.rank).data());
      } else {
        std::snprintf(
            out, size,
            ro::core::Tr("Survivants de Midgard : votre partie du %s est comptée, hors tableau."),
            date.data());
      }
      break;
    case SurvivorsRankingNoticeKind::kResultLost:
      std::snprintf(out, size,
                    ro::core::Tr("Survivants de Midgard : votre partie du %s n'a pas été comptée "
                                 ": déjà reçue ou perdue par le serveur."),
                    date.data());
      break;
    case SurvivorsRankingNoticeKind::kResultExpired:
      std::snprintf(
          out, size,
          ro::core::Tr("Survivants de Midgard : votre partie du %s est oubliée : son jeton a "
                       "expiré."),
          date.data());
      break;
    case SurvivorsRankingNoticeKind::kServiceAbsent:
    case SurvivorsRankingNoticeKind::kRulesetNotServed:
    case SurvivorsRankingNoticeKind::kTicketRefused:
    case SurvivorsRankingNoticeKind::kTicketSilent:
    case SurvivorsRankingNoticeKind::kChallengeSeedMismatch:
    case SurvivorsRankingNoticeKind::kScoreMismatch:
      break;
  }
  return std::string(text.data());
}

// La raison d'un départ parti libre, ou du défi joué en local.
[[nodiscard]] std::string RefusalLineOf(const SurvivorsRankingNotice& notice,
                                        ui::SurvivorsTicketRefusal refusal) {
  if (notice.board == rn::SurvivorsBoardKind::kChallenge) {
    return ro::core::Tr("Défi de la semaine non classé : il se joue en local.");
  }
  return ui::SurvivorsTicketRefusalText(refusal);
}

}  // namespace

ui::SurvivorsRankingView RankingViewOf(const SurvivorsRankingSession& session,
                                       const SurvivorsRankingNow& now,
                                       SurvivorsRankingRowsHeld& held) {
  const ro::sim::IsoWeek week = IsoWeekOfUtc(now.utc_s + session.server_offset_s());
  return ui::SurvivorsRankingView{
      .ranked = PageOf(session.page(rn::SurvivorsBoardKind::kRanked), now, held.ranked_names,
                       held.ranked_rows),
      .challenge = PageOf(session.page(rn::SurvivorsBoardKind::kChallenge), now,
                          held.challenge_names, held.challenge_rows),
      .ruleset = session.ruleset(),
      .week_year = week.year,
      .week_number = week.week,
      .refresh_ready = now.steady_ms >= session.board_ready_at_ms()};
}

std::size_t PendingCountOf(const SurvivorsRankingSession& session) {
  return static_cast<std::size_t>(
      std::ranges::count_if(session.pending(), [&session](const SurvivorsPendingResult& entry) {
        return entry.server_key == session.server_key();
      }));
}

std::optional<ui::SurvivorsPrepareRanking> PrepareRankingOf(const SurvivorsRankingSession& session,
                                                            SurvivorsRankingRowsHeld& held) {
  held.pending_names.clear();
  held.pending_rows.clear();
  if (!session.service_open()) return std::nullopt;
  ui::SurvivorsPrepareRanking ranking;
  if (const SurvivorsHeldPage* page = session.page(rn::SurvivorsBoardKind::kRanked)) {
    ranking.best_rank = page->page.own_rank;
    ranking.best_score = page->page.own_score;
  }
  held.pending_names.reserve(session.pending().size());
  std::vector<std::int64_t> ended_at;
  for (const SurvivorsPendingResult& entry : session.pending()) {
    if (entry.server_key != session.server_key()) continue;
    held.pending_names.push_back(entry.character_name);
    ended_at.push_back(entry.ended_at_utc_s);
  }
  for (std::size_t index = 0; index < held.pending_names.size(); ++index) {
    held.pending_rows.push_back(ui::SurvivorsPendingRow{
        .character = held.pending_names[index].c_str(), .ended_at_utc_s = ended_at[index]});
  }
  ranking.pending = held.pending_rows;
  return ranking;
}

std::optional<ui::SurvivorsSummaryRanking> SummaryRankOf(const SurvivorsRankingSession& session,
                                                         SurvivorsBoardKind board) {
  const bool sent = session.run_submitted();
  if (!session.service_open() && !sent) return std::nullopt;
  const bool ranked = sent || HudRankedOf(session, board);
  // SEULE LA PARTIE LIBRE SE DIT « Partie libre » : un défi joué en local, ou
  // une partie classée qui n'a pas pu partir, ne porte aucune marque.
  if (!ranked && board != SurvivorsBoardKind::kFree) return std::nullopt;
  ui::SurvivorsSummaryRanking ranking{
      .ranked = ranked, .tab = TabOf(board), .board_reachable = session.service_open()};
  if (!ranking.ranked) return ranking;
  if (const std::optional<rn::SurvivorsResult>& result = session.run_result()) {
    ranking.standing = StandingOf(*result);
    if (Accepted(result->verdict)) ranking.server_score = result->score;
    return ranking;
  }
  ranking.standing.status = AwaitingVerdict(session) ? ui::SurvivorsRankStatus::kSending
                                                     : ui::SurvivorsRankStatus::kPending;
  return ranking;
}

bool HudRankedOf(const SurvivorsRankingSession& session, SurvivorsBoardKind board) noexcept {
  return board != SurvivorsBoardKind::kFree && session.ranked_ticket() != nullptr;
}

std::optional<std::string> RankingLineOf(const SurvivorsRankingNotice& notice,
                                         const SurvivorsRankingSession& session, bool on_summary) {
  switch (notice.kind) {
    case SurvivorsRankingNoticeKind::kTicketRefused:
      return RefusalLineOf(notice, RefusalOf(notice.ticket_result));
    case SurvivorsRankingNoticeKind::kTicketSilent:
      return RefusalLineOf(notice, ui::SurvivorsTicketRefusal::kSilent);
    case SurvivorsRankingNoticeKind::kChallengeSeedMismatch:
      return RefusalLineOf(notice, ui::SurvivorsTicketRefusal::kChallengeSeed);
    case SurvivorsRankingNoticeKind::kVerdict:
    case SurvivorsRankingNoticeKind::kResultLost:
      // L'écran de fin de cette partie dit déjà son verdict.
      if (on_summary && session.submitted_ticket() == notice.result.ticket) return std::nullopt;
      return RunLineOf(notice);
    case SurvivorsRankingNoticeKind::kResultExpired:
      return RunLineOf(notice);
    case SurvivorsRankingNoticeKind::kServiceAbsent:
    case SurvivorsRankingNoticeKind::kRulesetNotServed:
    case SurvivorsRankingNoticeKind::kScoreMismatch:
      break;
  }
  return std::nullopt;
}

bool SummaryShown(const SurvivorsStage& stage) noexcept {
  switch (stage.screen()) {
    case SurvivorsScreen::kDying:
    case SurvivorsScreen::kWon:
    case SurvivorsScreen::kSummary:
      return true;
    case SurvivorsScreen::kClosed:
    case SurvivorsScreen::kPreparing:
    case SurvivorsScreen::kLoading:
    case SurvivorsScreen::kArriving:
    case SurvivorsScreen::kPlaying:
    case SurvivorsScreen::kPaused:
    case SurvivorsScreen::kChoosing:
    case SurvivorsScreen::kChest:
      return false;
  }
  return false;
}

void SayRankingNotices(SurvivorsRankingMode& ranking, const SurvivorsStage& stage,
                       const SurvivorsSay& say) {
  const bool on_summary = SummaryShown(stage);
  for (const SurvivorsRankingNotice& notice : ranking.TakeNotices()) {
    if (!say) continue;
    if (std::optional<std::string> line = RankingLineOf(notice, ranking.session(), on_summary)) {
      say(std::move(*line));
    }
  }
}

void FollowRankingAsks(SurvivorsRankingMode& ranking, ui::SurvivorsRankingState& state,
                       const SurvivorsRankingAsks& asks, const ro::sim::SurvivorsRules& rules) {
  if (!ranking.session().service_open()) {
    state.open = false;
    ranking.OweBoard(std::nullopt);
    return;
  }
  bool ask = state.open && (asks.window.refresh || asks.window.tab_changed);
  if (asks.open_on.has_value()) {
    state.open = true;
    state.tab = *asks.open_on;
    ask = true;
  } else if (asks.open) {
    state.open = true;
    ask = true;
  }
  // UNE DEMANDE RETENUE PAR LA CADENCE RESTE DUE : elle part dès que la
  // cadence le permet, tant que la fenêtre reste ouverte sur son onglet.
  const rn::SurvivorsBoardKind board = WireBoardOf(state.tab);
  if (!state.open || (ranking.board_owed().has_value() && *ranking.board_owed() != board)) {
    ranking.OweBoard(std::nullopt);
  }
  if (!state.open) return;
  if (ask) ranking.OweBoard(board);
  if (ranking.board_owed().has_value() && ranking.AskBoard(rules, board)) {
    ranking.OweBoard(std::nullopt);
  }
}

void ComposeRankingWindow(const SurvivorsRankingMode& ranking, ui::SurvivorsRankingState& state,
                          ui::SurvivorsRankingView& view, SurvivorsRankingRowsHeld& held) {
  if (!state.open) return;
  if (!ranking.session().service_open()) {
    state.open = false;
    return;
  }
  view = RankingViewOf(ranking.session(), ranking.now(), held);
}

}  // namespace moonclient
