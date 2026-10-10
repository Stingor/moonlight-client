#include "ro/net/progress_bar.h"

#include <cstdint>
#include <vector>

#include "ro/proto/map_packets.h"
#include "ro/proto/progress_packets.h"
#include "ro/sim/world.h"

#include "frame_apply.h"

namespace ro::net {

FrameOutcome ApplyProgressFrame(const Frame& frame, sim::World& world, sim::ProgressReport& report,
                                std::uint32_t now_ms) {
  switch (frame.opcode) {
    case proto::ProgressStart::kOpcode: {
      const core::Result<proto::ProgressStart> start = proto::DecodeProgressStart(frame.bytes);
      if (!start) {
        return FrameOutcome::kUndecodable;
      }
      return sim::StartOwnProgress(world, report, start->seconds, now_ms) ? FrameOutcome::kApplied
                                                                          : FrameOutcome::kIgnored;
    }
    case proto::kProgressActorOpcode:
      return Decoded<proto::ZcProgressActor>(frame, [&world, now_ms](const auto& packet) {
        return sim::StartActorProgress(world, static_cast<std::uint32_t>(packet.GID), packet.time,
                                       now_ms)
                   ? FrameOutcome::kApplied
                   : FrameOutcome::kIgnored;
      });
    case proto::ProgressCancel::kOpcode:
      if (frame.bytes.size() != proto::ProgressCancel::kWireSize) {
        return FrameOutcome::kUndecodable;
      }
      sim::CancelOwnProgress(world, report);
      return FrameOutcome::kApplied;
    default:
      return FrameOutcome::kUnhandled;
  }
}

core::Result<void> ReportProgressDone(Connection& link, const sim::World& world,
                                      sim::ProgressReport& report, std::uint32_t now_ms) {
  sim::NoteOwnProgressEnd(world, report, now_ms);
  if (!report.TakeDue()) {
    return {};
  }
  core::Result<std::vector<std::uint8_t>> done = proto::EncodeProgressDone();
  if (!done) {
    return std::unexpected(done.error());
  }
  link.Send(*done);
  return {};
}

}  // namespace ro::net
