#pragma once

// LE GABARIT DE NOS PROPRES PHRASES : chaque « {} » reçoit la valeur suivante,
// et un « {} » qui n'en a plus reste tel quel. ⚠ PAS DE `std::vformat` : une
// traduction mal formée lèverait une exception.

#include <cstddef>
#include <initializer_list>
#include <string>
#include <string_view>

namespace moonclient {

[[nodiscard]] inline std::string BracesFilled(std::string_view pattern,
                                              std::initializer_list<std::string_view> values) {
  std::string out;
  out.reserve(pattern.size());
  auto next = values.begin();
  for (std::size_t at = 0; at < pattern.size(); ++at) {
    if (pattern[at] == '{' && at + 1 < pattern.size() && pattern[at + 1] == '}' &&
        next != values.end()) {
      out.append(*next++);
      ++at;
      continue;
    }
    out.push_back(pattern[at]);
  }
  return out;
}

}  // namespace moonclient
