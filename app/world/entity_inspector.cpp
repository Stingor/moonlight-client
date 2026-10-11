#include "world/entity_inspector.h"

#include <utility>

#include "ro/core/log.h"

namespace moonclient {
namespace {

using ro::ui::EntityInspectorPhase;

[[nodiscard]] EntityInspectorPhase PhaseOf(ro::net::EntityPropsStatus status) {
  switch (status) {
    case ro::net::EntityPropsStatus::kFound:
      return EntityInspectorPhase::kReady;
    case ro::net::EntityPropsStatus::kDenied:
      return EntityInspectorPhase::kDenied;
    case ro::net::EntityPropsStatus::kMissing:
      break;
  }
  return EntityInspectorPhase::kMissing;
}

}  // namespace

void EntityInspector::Open(ro::ui::EntityInspectorWindowState& state, std::uint32_t gid,
                           std::string subject, std::string_view map_name, std::uint32_t now_ms) {
  gid_ = gid;
  map_name_ = map_name;
  view_ = ro::ui::EntityInspectorView{};
  view_.subject = std::move(subject);
  Ask(now_ms);
  state.open = true;
  state.raise = true;
}

ro::net::FrameOutcome EntityInspector::ApplyFrame(const ro::net::Frame& frame) {
  const std::optional<ro::net::EntityPropsReply> reply = ro::net::ReadEntityProps(frame);
  if (!reply.has_value()) {
    return frame.opcode == ro::net::kEntityPropsReplyOpcode ? ro::net::FrameOutcome::kUndecodable
                                                            : ro::net::FrameOutcome::kUnhandled;
  }
  if (gid_ == 0 || reply->gid != gid_) {
    return ro::net::FrameOutcome::kIgnored;
  }
  Note(*reply);
  return ro::net::FrameOutcome::kApplied;
}

void EntityInspector::Note(const ro::net::EntityPropsReply& reply) {
  if (gid_ == 0 || reply.gid != gid_) {
    return;
  }
  view_.phase = PhaseOf(reply.status);
  view_.rows.clear();
  for (const ro::net::EntityProp& prop : reply.props) {
    view_.rows.push_back(ro::ui::EntityInspectorRow{.key = prop.key, .value = prop.value});
  }
}

void EntityInspector::Advance(ro::ui::EntityInspectorWindowState& state,
                              const ro::ui::EntityInspectorAnswer& answer,
                              std::string_view map_name, std::uint32_t now_ms) {
  if (gid_ == 0) {
    state.open = false;
    return;
  }
  if (!state.open || map_name != map_name_) {
    state.open = false;
    Forget();
    return;
  }
  if (answer.refresh && view_.phase != EntityInspectorPhase::kAsking) {
    Ask(now_ms);
  }
  if (view_.phase == EntityInspectorPhase::kAsking &&
      now_ms - asked_ms_ >= kEntityInspectorWaitMs) {
    view_.phase = EntityInspectorPhase::kSilent;
  }
}

std::optional<std::uint32_t> EntityInspector::TakeRequest() {
  return std::exchange(pending_request_, std::nullopt);
}

void EntityInspector::SendRequest(ro::net::Connection& link) {
  const std::optional<std::uint32_t> gid = TakeRequest();
  if (!gid.has_value()) {
    return;
  }
  if (auto sent = ro::net::RequestEntityProps(link, *gid); !sent) {
    ro::core::LogWarning("inspecteur : demande non émise — {}", ro::core::Describe(sent.error()));
  }
}

void EntityInspector::Ask(std::uint32_t now_ms) {
  pending_request_ = gid_;
  asked_ms_ = now_ms;
  view_.phase = EntityInspectorPhase::kAsking;
  view_.rows.clear();
}

void EntityInspector::Forget() {
  gid_ = 0;
  map_name_.clear();
  pending_request_.reset();
  view_ = ro::ui::EntityInspectorView{};
}

}  // namespace moonclient
