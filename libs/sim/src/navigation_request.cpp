#include "ro/sim/navigation_request.h"

#include <utility>

namespace ro::sim {

void NavigationRequests::Push(NavigationRequest request) {
  if (queue_.size() >= kMaxPendingNavigationRequests) {
    queue_.pop_front();
  }
  queue_.push_back(std::move(request));
}

std::optional<NavigationRequest> NavigationRequests::Take() {
  if (queue_.empty()) {
    return std::nullopt;
  }
  NavigationRequest oldest = std::move(queue_.front());
  queue_.pop_front();
  return oldest;
}

}  // namespace ro::sim
