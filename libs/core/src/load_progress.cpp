#include "ro/core/load_progress.h"

#include <algorithm>

namespace ro::core {
namespace {

[[nodiscard]] std::size_t RankOf(LoadStep step) noexcept { return static_cast<std::size_t>(step); }

}  // namespace

LoadProgress::LoadProgress(std::span<const LoadStepWeight> plan, LoadObserver* observer)
    : observer_(observer) {
  for (const LoadStepWeight& entry : plan) {
    const std::size_t rank = RankOf(entry.step);
    if (rank >= kLoadStepCount || entry.expected_ms <= 0.0F) {
      continue;
    }
    plan_ms_ += entry.expected_ms;
    expected_ms_[rank] += entry.expected_ms;
  }
}

void LoadProgress::Begin(LoadStep step, std::size_t total) {
  if (step_.has_value()) {
    done_share_[RankOf(*step_)] = 1.0F;
  }
  step_ = step;
  done_ = 0;
  total_ = total;
  if (RankOf(step) < kLoadStepCount) {
    done_share_[RankOf(step)] = 0.0F;
  }
  Notify();
}

void LoadProgress::Advance(std::size_t count) {
  if (!step_.has_value() || total_ == 0) {
    return;
  }
  done_ = std::min(total_, done_ + count);
  if (RankOf(*step_) < kLoadStepCount) {
    done_share_[RankOf(*step_)] = static_cast<float>(done_) / static_cast<float>(total_);
  }
  Notify();
}

void LoadProgress::End() {
  if (!step_.has_value()) {
    return;
  }
  if (RankOf(*step_) < kLoadStepCount) {
    done_share_[RankOf(*step_)] = 1.0F;
  }
  step_.reset();
  done_ = 0;
  total_ = 0;
  Notify();
}

float LoadProgress::fraction() const noexcept {
  if (plan_ms_ <= 0.0F) {
    return 0.0F;
  }
  float done_ms = 0.0F;
  for (std::size_t rank = 0; rank < kLoadStepCount; ++rank) {
    done_ms += expected_ms_[rank] * done_share_[rank];
  }
  return std::clamp(done_ms / plan_ms_, 0.0F, 1.0F);
}

void LoadProgress::Notify() const {
  if (observer_ != nullptr) {
    observer_->Changed(*this);
  }
}

LoadStepScope::LoadStepScope(LoadProgress* progress, LoadStep step, std::size_t total)
    : progress_(progress) {
  if (progress_ != nullptr) {
    progress_->Begin(step, total);
  }
}

LoadStepScope::~LoadStepScope() {
  if (progress_ != nullptr) {
    progress_->End();
  }
}

void LoadStepScope::Advance(std::size_t count) const {
  if (progress_ != nullptr) {
    progress_->Advance(count);
  }
}

void BeginLoadStep(LoadProgress* progress, LoadStep step, std::size_t total) {
  if (progress != nullptr) {
    progress->Begin(step, total);
  }
}

void EndLoadStep(LoadProgress* progress) {
  if (progress != nullptr) {
    progress->End();
  }
}

bool LoadPacing::Due(std::uint32_t now_ms) const noexcept {
  if (!already_shown_ && now_ms - started_ms_ < kLoadingScreenDelayMs) {
    return false;
  }
  return !presented_ms_.has_value() || now_ms - *presented_ms_ >= kLoadingPresentIntervalMs;
}

float LoadPacing::Opacity(std::uint32_t now_ms) const noexcept {
  if (already_shown_) {
    return 1.0F;
  }
  const std::uint32_t since_ms = now_ms - started_ms_;
  if (since_ms <= kLoadingScreenDelayMs) {
    return 0.0F;
  }
  return std::min(1.0F, static_cast<float>(since_ms - kLoadingScreenDelayMs) /
                            static_cast<float>(kLoadingScreenFadeMs));
}

}  // namespace ro::core
