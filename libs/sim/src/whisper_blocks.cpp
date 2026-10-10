#include "ro/sim/whisper_blocks.h"

#include <algorithm>
#include <utility>

namespace ro::sim {

bool WhisperBlockList::Blocks(std::string_view name) const {
  return std::ranges::find(names_, name) != names_.end();
}

void WhisperBlockList::Expect(std::string name, WhisperBlockWay way) {
  if (pending_.size() >= kMostPendingWhisperBlocks) {
    pending_.pop_front();
  }
  pending_.push_back(Pending{.name = std::move(name), .way = way});
}

WhisperBlockAnswer WhisperBlockList::Settle(WhisperBlockWay way, std::uint8_t result) {
  WhisperBlockAnswer answer{.way = way, .result = result};
  if (pending_.empty() || pending_.front().way != way) {
    pending_.clear();
    return answer;
  }
  answer.name = std::move(pending_.front().name);
  pending_.pop_front();
  const auto code = static_cast<WhisperBlockResult>(result);
  if (way == WhisperBlockWay::kBlock && code == WhisperBlockResult::kDone) {
    Add(answer.name);
  } else if (way == WhisperBlockWay::kAllow &&
             (code == WhisperBlockResult::kDone || code == WhisperBlockResult::kFailed)) {
    Remove(answer.name);
  }
  return answer;
}

void WhisperBlockList::SettleEveryone(WhisperBlockWay way, bool failed) {
  if (way == WhisperBlockWay::kBlock) {
    everyone_ = true;
    return;
  }
  if (failed) {
    everyone_ = false;
    names_.clear();
    return;
  }
  if (everyone_) {
    everyone_ = false;
  } else {
    names_.clear();
  }
}

void WhisperBlockList::Replace(std::vector<std::string> names) {
  names_.clear();
  for (std::string& name : names) {
    if (!name.empty() && !Blocks(name)) {
      names_.push_back(std::move(name));
    }
  }
}

void WhisperBlockList::Clear() noexcept {
  names_.clear();
  pending_.clear();
  everyone_ = false;
}

void WhisperBlockList::Add(const std::string& name) {
  if (!name.empty() && !Blocks(name)) {
    names_.push_back(name);
  }
}

void WhisperBlockList::Remove(std::string_view name) { std::erase(names_, name); }

}  // namespace ro::sim
