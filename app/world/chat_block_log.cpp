#include "world/chat_block_log.h"

#include <array>
#include <cstdio>
#include <ctime>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/log.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::net::ChatBlocksStatus;
using ro::ui::ChatBlockLogPhase;

// `std::tm` compte les années depuis 1900 et les mois depuis zéro.
constexpr int kFirstYear = 1900;
constexpr int kFirstMonth = 1;
// « 2026-10-11 15:41 », et de la marge.
constexpr std::size_t kDateBytes = 32;
// « 2147483647 min », traduit, et de la marge.
constexpr std::size_t kDurationBytes = 48;

[[nodiscard]] ChatBlockLogPhase PhaseOf(ChatBlocksStatus status) {
  switch (status) {
    case ChatBlocksStatus::kFound:
      return ChatBlockLogPhase::kReady;
    case ChatBlocksStatus::kMissing:
      return ChatBlockLogPhase::kMissing;
    case ChatBlocksStatus::kDenied:
      return ChatBlockLogPhase::kDenied;
    case ChatBlocksStatus::kLogOff:
      return ChatBlockLogPhase::kLogOff;
    case ChatBlocksStatus::kReadFailed:
      break;
  }
  return ChatBlockLogPhase::kReadFailed;
}

}  // namespace

std::string ChatBlockDateText(std::uint32_t date_s) {
  const auto seconds = static_cast<std::time_t>(date_s);
  std::tm local{};
#ifdef _WIN32
  if (localtime_s(&local, &seconds) != 0) {
    return {};
  }
#else
  if (localtime_r(&seconds, &local) == nullptr) {
    return {};
  }
#endif
  std::array<char, kDateBytes> text{};
  std::snprintf(text.data(), text.size(), "%04d-%02d-%02d %02d:%02d", local.tm_year + kFirstYear,
                local.tm_mon + kFirstMonth, local.tm_mday, local.tm_hour, local.tm_min);
  return text.data();
}

ro::ui::ChatBlockLogRow ChatBlockRowOf(const ro::net::ChatBlock& block) {
  ro::ui::ChatBlockLogRow row;
  row.when = ChatBlockDateText(block.date_s);
  row.author = block.author;
  if (block.kind == ro::net::ChatBlockKind::kUnmute) {
    row.command = "@unmute";
    return row;
  }
  row.command = "@mute";
  std::array<char, kDurationBytes> duration{};
  std::snprintf(duration.data(), duration.size(), Tr("%d min"), block.minutes);
  row.duration = duration.data();
  return row;
}

void ChatBlockLog::Open(ro::ui::ChatBlockLogWindowState& state, std::uint32_t aid,
                        std::string subject, std::uint32_t now_ms) {
  aid_ = aid;
  view_ = ro::ui::ChatBlockLogView{};
  view_.subject = std::move(subject);
  Ask(now_ms);
  state.open = true;
  state.raise = true;
}

ro::net::FrameOutcome ChatBlockLog::ApplyFrame(const ro::net::Frame& frame) {
  const std::optional<ro::net::ChatBlocksReply> reply = ro::net::ReadChatBlocks(frame);
  if (!reply.has_value()) {
    return frame.opcode == ro::net::kChatBlocksReplyOpcode ? ro::net::FrameOutcome::kUndecodable
                                                           : ro::net::FrameOutcome::kUnhandled;
  }
  if (aid_ == 0 || reply->aid != aid_) {
    return ro::net::FrameOutcome::kIgnored;
  }
  Note(*reply);
  return ro::net::FrameOutcome::kApplied;
}

void ChatBlockLog::Note(const ro::net::ChatBlocksReply& reply) {
  if (aid_ == 0 || reply.aid != aid_) {
    return;
  }
  view_.phase = PhaseOf(reply.status);
  view_.rows.clear();
  for (const ro::net::ChatBlock& block : reply.blocks) {
    view_.rows.push_back(ChatBlockRowOf(block));
  }
}

void ChatBlockLog::Advance(ro::ui::ChatBlockLogWindowState& state,
                           const ro::ui::ChatBlockLogAnswer& answer, std::uint32_t now_ms) {
  if (aid_ == 0) {
    state.open = false;
    return;
  }
  if (!state.open) {
    Forget();
    return;
  }
  if (answer.refresh && view_.phase != ChatBlockLogPhase::kAsking) {
    Ask(now_ms);
  }
  if (view_.phase == ChatBlockLogPhase::kAsking && now_ms - asked_ms_ >= kChatBlockLogWaitMs) {
    view_.phase = ChatBlockLogPhase::kSilent;
  }
}

std::optional<std::uint32_t> ChatBlockLog::TakeRequest() {
  return std::exchange(pending_request_, std::nullopt);
}

void ChatBlockLog::SendRequest(ro::net::Connection& link) {
  const std::optional<std::uint32_t> aid = TakeRequest();
  if (!aid.has_value()) {
    return;
  }
  if (auto sent = ro::net::RequestChatBlocks(link, *aid); !sent) {
    ro::core::LogWarning("journal des blocages : demande non émise — {}",
                         ro::core::Describe(sent.error()));
  }
}

void ChatBlockLog::Ask(std::uint32_t now_ms) {
  pending_request_ = aid_;
  asked_ms_ = now_ms;
  view_.phase = ChatBlockLogPhase::kAsking;
  view_.rows.clear();
}

void ChatBlockLog::Forget() {
  aid_ = 0;
  pending_request_.reset();
  view_ = ro::ui::ChatBlockLogView{};
}

}  // namespace moonclient
