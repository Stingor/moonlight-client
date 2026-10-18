#include "ro/formats/bgm_table.h"

#include "ro/core/ascii.h"

namespace ro::formats {

std::string_view BgmTable::TrackFor(std::string_view map_name) const {
  const auto found = tracks_.find(core::AsciiLower(map_name));
  return found == tracks_.end() ? std::string_view{} : found->second;
}

}  // namespace ro::formats
