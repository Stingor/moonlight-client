#include "world/player_report.h"

#include <array>
#include <cstdio>

#include "ro/core/i18n.h"
#include "ro/core/log.h"

namespace moonclient {
namespace {

using ro::core::Tr;
using ro::net::PlayerReportResult;

// Assez pour la plus longue phrase, dans toutes les langues, et un nom.
constexpr std::size_t kLineBytes = 256;

}  // namespace

std::optional<std::string> PlayerReportLine(const ro::net::PlayerReportAck& ack) {
  switch (static_cast<PlayerReportResult>(ack.result)) {
    case PlayerReportResult::kCountLimit:
      return Tr("Le nombre de signalements du jour est dépassé : vous ne pouvez plus signaler.");
    case PlayerReportResult::kRecorded: {
      std::array<char, kLineBytes> line{};
      std::snprintf(line.data(), line.size(), Tr("Signalement de %s enregistré."),
                    ack.target_name.c_str());
      return std::string(line.data());
    }
    case PlayerReportResult::kCoolTime:
      return Tr("Vous avez déjà signalé ce joueur récemment.");
    case PlayerReportResult::kTargetProtected:
      return Tr("Ce joueur est protégé contre les signalements.");
    case PlayerReportResult::kDailyMaxReached:
      return Tr("Vous avez atteint le nombre de signalements permis aujourd'hui.");
    case PlayerReportResult::kReportedByOther:
      return Tr("Un autre joueur vous a signalé.");
    case PlayerReportResult::kAreaForbidden:
      return Tr("On ne peut pas signaler dans cette zone.");
    case PlayerReportResult::kTargetNotFound:
      return Tr("Cible introuvable...");
    case PlayerReportResult::kFailed:
      return Tr("Le signalement n'a pas pu être enregistré (erreur serveur).");
  }
  return std::nullopt;
}

void PlayerReportDesk::Open(ro::ui::PlayerReportWindowState& state, std::uint32_t target_aid,
                            const std::string& target_name) {
  // Un autre joueur, ou une fenêtre refermée : on repart d'une page blanche.
  if (!state.open || target_aid != target_aid_) {
    state.motive = ro::ui::PlayerReportMotive::kIllegalProgram;
    state.message.fill('\0');
  }
  target_aid_ = target_aid;
  state.target_name = target_name;
  state.open = true;
  state.raise = true;
}

ro::net::PlayerReport PlayerReportDesk::ReportOf(const ro::ui::PlayerReportWindowState& state,
                                                 std::uint32_t reporter_aid) const {
  return ro::net::PlayerReport{.reporter_aid = reporter_aid,
                               .target_aid = target_aid_,
                               .target_name = state.target_name,
                               .reason = state.motive == ro::ui::PlayerReportMotive::kBugAbuse
                                             ? ro::net::PlayerReportReason::kBugAbuse
                                             : ro::net::PlayerReportReason::kIllegalProgram,
                               .message = state.message.data()};
}

void PlayerReportDesk::Advance(const ro::ui::PlayerReportWindowState& state,
                               const ro::ui::PlayerReportAnswer& answer, std::uint32_t reporter_aid,
                               ro::net::Connection& link) {
  if (answer.send && target_aid_ != 0) {
    if (const auto sent = ro::net::SendPlayerReport(link, ReportOf(state, reporter_aid)); !sent) {
      ro::core::LogWarning("signalement : {} — il n'est pas parti",
                           ro::core::Describe(sent.error()));
    }
  }
  if (!state.open) {
    target_aid_ = 0;
  }
}

ro::net::FrameOutcome PlayerReportDesk::ApplyFrame(const ro::net::Frame& frame,
                                                   ro::sim::World& world, std::uint32_t now_ms) {
  if (frame.opcode != ro::net::kPlayerReportAckOpcode) {
    return ro::net::FrameOutcome::kUnhandled;
  }
  const std::optional<ro::net::PlayerReportAck> ack = ro::net::ReadPlayerReportAck(frame);
  if (!ack.has_value()) {
    return ro::net::FrameOutcome::kIgnored;
  }
  Note(*ack, world, now_ms);
  return ro::net::FrameOutcome::kApplied;
}

void PlayerReportDesk::Note(const ro::net::PlayerReportAck& ack, ro::sim::World& world,
                            std::uint32_t now_ms) {
  const std::optional<std::string> line = PlayerReportLine(ack);
  if (!line.has_value()) {
    ro::core::LogDebug("signalement : verdict {} inconnu, rien ne se dit", ack.result);
    return;
  }
  world.RecordChat(ro::sim::ChatKind::kServer, 0, *line, now_ms, kPlayerReportSaysColor);
  if (static_cast<PlayerReportResult>(ack.result) == PlayerReportResult::kRecorded) {
    static_cast<void>(world.RecordEffect(world.player_id(), kPlayerReportRecordedEffect, now_ms));
  }
}

}  // namespace moonclient
