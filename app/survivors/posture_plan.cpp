#include "survivors/posture_plan.h"

namespace moonclient {

bool PosturePlan::SitOnEntry(bool sitting_now) noexcept {
  seated_by_us_ = !sitting_now;
  return seated_by_us_;
}

bool PosturePlan::StandOnExit(bool sitting_now) noexcept {
  const bool stand = seated_by_us_ && sitting_now;
  seated_by_us_ = false;
  return stand;
}

}  // namespace moonclient
