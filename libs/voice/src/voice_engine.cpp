#include "ro/voice/voice_engine.h"

#include <chrono>
#include <iterator>
#include <utility>

namespace ro::voice {

VoiceEngine::VoiceEngine(VoiceTarget target, VoiceDevices devices)
    : link_(std::move(target), std::move(devices)), worker_([this] { Run(); }) {}

VoiceEngine::~VoiceEngine() {
  stopping_ = true;
  if (worker_.joinable()) {
    worker_.join();
  }
}

void VoiceEngine::Steer(const VoiceControls& controls) {
  const std::lock_guard<std::mutex> lock(guard_);
  controls_ = controls;
}

void VoiceEngine::Block(std::uint32_t character_id, bool blocked) {
  const std::lock_guard<std::mutex> lock(guard_);
  block_orders_.push_back(BlockOrder{.character_id = character_id, .blocked = blocked});
}

VoiceStatus VoiceEngine::status() const {
  const std::lock_guard<std::mutex> lock(guard_);
  return status_;
}

void VoiceEngine::PostCoop(std::vector<std::uint8_t> payload) {
  const std::lock_guard<std::mutex> lock(guard_);
  coop_to_send_.push_back(std::move(payload));
}

std::vector<CoopRelayed> VoiceEngine::TakeCoop() {
  const std::lock_guard<std::mutex> lock(guard_);
  return std::exchange(coop_received_, std::vector<CoopRelayed>{});
}

void VoiceEngine::Run() {
  using Clock = std::chrono::steady_clock;
  const Clock::time_point started = Clock::now();
  while (!stopping_) {
    const auto now_ms = static_cast<std::uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - started).count());
    VoiceControls controls;
    std::vector<BlockOrder> orders;
    std::vector<std::vector<std::uint8_t>> coop_out;
    {
      const std::lock_guard<std::mutex> lock(guard_);
      controls = controls_;
      orders.swap(block_orders_);
      coop_out.swap(coop_to_send_);
    }
    link_.Steer(controls);
    for (const BlockOrder& order : orders) {
      link_.Block(order.character_id, order.blocked);
    }
    for (const std::vector<std::uint8_t>& payload : coop_out) link_.SendCoop(payload);
    link_.Step(now_ms);
    VoiceStatus status = link_.status(now_ms);
    std::vector<CoopRelayed> coop_in = link_.TakeCoop();
    {
      const std::lock_guard<std::mutex> lock(guard_);
      status_ = std::move(status);
      if (coop_received_.size() < kVoiceMostCoopHeld) {
        coop_received_.insert(coop_received_.end(), std::make_move_iterator(coop_in.begin()),
                              std::make_move_iterator(coop_in.end()));
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(kVoiceStepMs));
  }
}

}  // namespace ro::voice
