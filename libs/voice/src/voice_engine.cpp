#include "ro/voice/voice_engine.h"

#include <chrono>
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

VoiceStatus VoiceEngine::status() const {
  const std::lock_guard<std::mutex> lock(guard_);
  return status_;
}

void VoiceEngine::Run() {
  using Clock = std::chrono::steady_clock;
  const Clock::time_point started = Clock::now();
  while (!stopping_) {
    const auto now_ms = static_cast<std::uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - started).count());
    VoiceControls controls;
    {
      const std::lock_guard<std::mutex> lock(guard_);
      controls = controls_;
    }
    link_.Steer(controls);
    link_.Step(now_ms);
    VoiceStatus status = link_.status(now_ms);
    {
      const std::lock_guard<std::mutex> lock(guard_);
      status_ = std::move(status);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(kVoiceStepMs));
  }
}

}  // namespace ro::voice
