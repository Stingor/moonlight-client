#include "world/item_obtain_rules.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace moonclient {

bool SameObtainedLine(const ObtainedLine& first, const ObtainedLine& second) {
  return first.item_id == second.item_id && first.refine_level == second.refine_level &&
         first.identified == second.identified && first.damaged == second.damaged;
}

std::size_t MergeOrPushObtained(std::vector<ObtainedLine>& stack, ObtainedLine arriving,
                                bool merge_same) {
  if (merge_same) {
    for (std::size_t at = 0; at < stack.size(); ++at) {
      if (!SameObtainedLine(stack[at], arriving)) {
        continue;
      }
      stack[at].amount += arriving.amount;
      // Le regroupement fait repartir le minuteur : une ligne qu'on alimente
      // encore n'est pas une ligne qu'on a fini de lire.
      stack[at].born_ms = arriving.born_ms;
      return at;
    }
  }
  stack.push_back(std::move(arriving));
  return stack.size() - 1;
}

void ExpireObtainedLines(std::vector<ObtainedLine>& stack, std::uint32_t now_ms,
                         std::uint32_t life_ms) {
  const auto expired = [now_ms, life_ms](const ObtainedLine& line) {
    return now_ms - line.born_ms >= life_ms;
  };
  stack.erase(std::remove_if(stack.begin(), stack.end(), expired), stack.end());
}

void CapObtainedLines(std::vector<ObtainedLine>& stack, int most_lines) {
  const auto cap = static_cast<std::size_t>(ObtainedLineCap(most_lines));
  if (stack.size() <= cap) {
    return;
  }
  stack.erase(stack.begin(), stack.end() - static_cast<std::ptrdiff_t>(cap));
}

std::uint32_t ObtainedLifeMs(int duration_ms) {
  return static_cast<std::uint32_t>(std::max(duration_ms, ro::ui::kLeastToastDurationMs));
}

int ObtainedLineCap(int most_lines) {
  return std::clamp(most_lines, ro::ui::kLeastToastLines, ro::ui::kMostToastLines);
}

}  // namespace moonclient
