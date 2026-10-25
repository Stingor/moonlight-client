#include "ro/sim/survivors_records.h"

#include <cstddef>
#include <optional>
#include <vector>

namespace ro::sim {

bool RanksAbove(const SurvivorsRecord& first, const SurvivorsRecord& second) noexcept {
  if (first.score != second.score) return first.score > second.score;
  if (first.survived_ms != second.survived_ms) return first.survived_ms > second.survived_ms;
  return first.played_at_utc_s < second.played_at_utc_s;
}

std::optional<std::size_t> PlaceRecord(std::vector<SurvivorsRecord>& board,
                                       const SurvivorsRecord& record) {
  // La première ligne que la partie dépasse strictement : les égales restent
  // devant elle.
  std::size_t rank = 0;
  while (rank < board.size() && !RanksAbove(record, board[rank])) ++rank;
  if (rank >= kRecordsPerBoard) {
    if (board.size() > kRecordsPerBoard) board.resize(kRecordsPerBoard);
    return std::nullopt;
  }
  board.insert(board.begin() + static_cast<std::ptrdiff_t>(rank), record);
  if (board.size() > kRecordsPerBoard) board.resize(kRecordsPerBoard);
  return rank;
}

}  // namespace ro::sim
